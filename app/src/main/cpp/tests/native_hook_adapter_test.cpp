// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0

#include "zygisk/native_hook_adapter.hpp"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include "dobby.h"

namespace {
using TargetFn = int (*)();
TargetFn gOriginal = nullptr;
std::atomic<TargetFn> gStrictOriginal{nullptr};
void* gFaultTarget = nullptr;
void* gStrictTarget = nullptr;
std::atomic_bool gInjectFailure{false};
std::atomic_bool gStrictPatchBeforePublication{false};
std::atomic_bool gHeldCallbackEntered{false};
std::atomic_bool gReleaseHeldCallback{false};
std::atomic<TargetFn> gHeldOriginal{nullptr};

extern "C" MemoryOperationError __real_DobbyCodePatch(void*, uint8_t*, uint32_t);
extern "C" MemoryOperationError __wrap_DobbyCodePatch(void* target, uint8_t* bytes, uint32_t size) {
    if (target == gStrictTarget && gStrictOriginal.load(std::memory_order_acquire) == nullptr)
        gStrictPatchBeforePublication.store(true, std::memory_order_release);
    const auto status = __real_DobbyCodePatch(target, bytes, size);
    if (target == gFaultTarget && status == kMemoryOperationSuccess &&
        gInjectFailure.exchange(false, std::memory_order_acq_rel))
        return kMemoryOperationError;  // The replacement was already physically published.
    return status;
}

int Replacement() {
    return gOriginal ? gOriginal() + 92 : -1000;
}

int StrictReplacement() {
    const auto original = gStrictOriginal.load(std::memory_order_acquire);
    return original ? original() + 92 : -1000;
}

__attribute__((noinline)) int HeldReplacement() {
    gHeldCallbackEntered.store(true, std::memory_order_release);
    while (!gReleaseHeldCallback.load(std::memory_order_acquire))
        std::this_thread::yield();
    const auto original = gHeldOriginal.load(std::memory_order_acquire);
    return original ? original() + 92 : -1000;
}

void PublishStrictOriginal(void*, void* original) {
    gStrictOriginal.store(reinterpret_cast<TargetFn>(original), std::memory_order_release);
}

void* MakeTarget(size_t page_size) {
    auto* page = static_cast<uint8_t*>(mmap(nullptr, page_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (page == MAP_FAILED)
        return nullptr;
#if defined(__aarch64__)
    const uint32_t code[] = {0xd28000e0U, 0xd65f03c0U, 0xd503201fU, 0xd503201fU};
    memcpy(page, code, sizeof(code));  // mov x0,#7; ret; nop; nop
#elif defined(__x86_64__)
    const uint8_t code[] = {0xb8, 7,    0,    0,    0,    0xc3, 0x90, 0x90,
                            0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    memcpy(page, code, sizeof(code));
#else
    munmap(page, page_size);
    return nullptr;
#endif
    __builtin___clear_cache(reinterpret_cast<char*>(page), reinterpret_cast<char*>(page + 16));
    if (mprotect(page, page_size, PROT_READ | PROT_EXEC) != 0) {
        munmap(page, page_size);
        return nullptr;
    }
    return page;
}

bool Check(bool condition, const char* label) {
    if (!condition)
        std::fprintf(stderr, "native-adapter FAIL: %s\n", label);
    return condition;
}
}  // namespace

int main(int argc, char** argv) {
    const auto& api = fusehide::zygisk::GetNativeHookApi();
    if (!Check(api.version == 2 && api.hookFunc && api.unhookFunc, "complete v2 function table"))
        return 1;
    const long raw_page_size = sysconf(_SC_PAGESIZE);
    if (raw_page_size <= 0)
        return 2;
    const size_t page_size = static_cast<size_t>(raw_page_size);
    void* target = MakeTarget(page_size);
    void* foreign_target = MakeTarget(page_size);
    if (!target || !foreign_target)
        return 2;
    const auto fn = reinterpret_cast<TargetFn>(target);
    bool ok = Check(fn() == 7, "original executable target");

#if defined(__x86_64__)
    // The production MediaProvider cannot provide the trusted process-wide
    // quiescence lease required for an x64 multi-byte entry patch.  The
    // generic Zygisk adapter must therefore fail closed without publishing a
    // backup or modifying the target.  Dobby's opt-in x64 quiescence fixture
    // separately verifies the managed-host path where such a lease exists.
    void* x64_backup = reinterpret_cast<void*>(1);
    const int x64_installed =
        api.hookFunc(target, reinterpret_cast<void*>(Replacement), &x64_backup);
    ok &= Check(x64_installed != RT_SUCCESS && x64_backup == nullptr && fn() == 7,
                "x64 unmanaged hook fails closed without modifying target");

    const auto& strict_x64 = fusehide::zygisk::GetStrictNativeHookApi();
    gStrictOriginal.store(nullptr, std::memory_order_release);
    const int x64_strict = strict_x64.hookWithPublication(
        target, reinterpret_cast<void*>(StrictReplacement), nullptr, PublishStrictOriginal);
    ok &= Check(x64_strict != RT_SUCCESS &&
                    gStrictOriginal.load(std::memory_order_acquire) == nullptr && fn() == 7,
                "x64 strict hook with no quiescence lease fails closed");
    ok &= Check(api.unhookFunc(target) != RT_SUCCESS,
                "x64 rejected hook does not leave an owned ticket");

    munmap(target, page_size);
    munmap(foreign_target, page_size);
    std::printf("native-adapter x64-unmanaged result=%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
#endif

    void* original = nullptr;
    const int installed = api.hookFunc(target, reinterpret_cast<void*>(Replacement), &original);
    gOriginal = reinterpret_cast<TargetFn>(original);
    ok &= Check(installed == RT_SUCCESS && original && gOriginal() == 7 && fn() == 99,
                "hook publishes callable original and replacement");
    void* duplicate = reinterpret_cast<void*>(1);
    ok &= Check(
        api.hookFunc(target, reinterpret_cast<void*>(Replacement), &duplicate) != RT_SUCCESS &&
            duplicate == nullptr && fn() == 99,
        "duplicate install does not take ownership");
    ok &= Check(api.unhookFunc(target) == RT_SUCCESS && fn() == 7 &&
                    api.unhookFunc(target) != RT_SUCCESS && gOriginal() == 7,
                "owned unhook restores target and invalidates ticket");

    dobby_dummy_func_t foreign_original = nullptr;
    const int foreign_installed = DobbyHook(
        foreign_target, reinterpret_cast<dobby_dummy_func_t>(Replacement), &foreign_original);
    ok &= Check(foreign_installed == RT_SUCCESS && foreign_original &&
                    api.unhookFunc(foreign_target) != RT_SUCCESS &&
                    api.hookFunc(foreign_target, reinterpret_cast<void*>(Replacement),
                                 &duplicate) != RT_SUCCESS,
                "foreign owner cannot be unhooked");
    if (foreign_installed == RT_SUCCESS)
        ok &= Check(DobbyDestroy(foreign_target) == RT_SUCCESS,
                    "foreign owner still controls its own hook");

    gFaultTarget = target;
    gInjectFailure.store(true, std::memory_order_release);
    original = nullptr;
    const int partial = api.hookFunc(target, reinterpret_cast<void*>(Replacement), &original);
    gOriginal = reinterpret_cast<TargetFn>(original);
    ok &= Check(
        partial != RT_SUCCESS && original && gOriginal() == 7 &&
            api.hookFunc(target, reinterpret_cast<void*>(Replacement), &duplicate) != RT_SUCCESS,
        "partially published failure retains its exact ticket");
    ok &= Check(
        api.unhookFunc(target) == RT_SUCCESS && fn() == 7 && api.unhookFunc(target) != RT_SUCCESS,
        "explicit recovery restores and releases retained ticket");
    gFaultTarget = nullptr;

    int results[2]{RT_FAILED, RT_FAILED};
    void* backups[2]{nullptr, nullptr};
    std::thread a([&] {
        results[0] = api.hookFunc(target, reinterpret_cast<void*>(Replacement), &backups[0]);
    });
    std::thread b([&] {
        results[1] = api.hookFunc(target, reinterpret_cast<void*>(Replacement), &backups[1]);
    });
    a.join();
    b.join();
    const int winners = (results[0] == RT_SUCCESS) + (results[1] == RT_SUCCESS);
    gOriginal = reinterpret_cast<TargetFn>(results[0] == RT_SUCCESS ? backups[0] : backups[1]);
    ok &= Check(winners == 1 && gOriginal && gOriginal() == 7 && fn() == 99,
                "concurrent installers have exactly one owner");
    ok &= Check(api.unhookFunc(target) == RT_SUCCESS && fn() == 7, "concurrent winner unhooks");

    const auto& strict = fusehide::zygisk::GetStrictNativeHookApi();
    ok &= Check(strict.base.version == 3 && strict.base.hookFunc && strict.base.unhookFunc &&
                    strict.struct_size == sizeof(strict) && strict.hookWithPublication &&
                    strict.hookWithPublication(target, reinterpret_cast<void*>(StrictReplacement),
                                               nullptr, nullptr) != RT_SUCCESS,
                "v3 is an opt-in complete extension with required publisher");
    gStrictTarget = target;
    gStrictOriginal.store(nullptr, std::memory_order_release);
    gStrictPatchBeforePublication.store(false, std::memory_order_release);
    const int strict_installed = strict.hookWithPublication(
        target, reinterpret_cast<void*>(StrictReplacement), nullptr, PublishStrictOriginal);
    const auto published = gStrictOriginal.load(std::memory_order_acquire);
    ok &= Check(strict_installed == RT_SUCCESS && published &&
                    !gStrictPatchBeforePublication.load(std::memory_order_acquire) &&
                    published() == 7 && fn() == 99,
                "strict backup-ready callback runs before any physical entry write");
    ok &= Check(strict.base.unhookFunc(target) == RT_SUCCESS && fn() == 7,
                "strict owner uses compatible v2 Unhook");

    gFaultTarget = target;
    gInjectFailure.store(true, std::memory_order_release);
    gStrictOriginal.store(nullptr, std::memory_order_release);
    const int strict_partial = strict.hookWithPublication(
        target, reinterpret_cast<void*>(StrictReplacement), nullptr, PublishStrictOriginal);
    ok &= Check(strict_partial == fusehide::kNativeHookRecoveryRequired &&
                    gStrictOriginal.load(std::memory_order_acquire) != nullptr &&
                    !gStrictPatchBeforePublication.load(std::memory_order_acquire) &&
                    strict.base.unhookFunc(target) == RT_SUCCESS && fn() == 7,
                "strict post-publication failure retains callable backup until explicit recovery");
    gFaultTarget = nullptr;
    gStrictTarget = nullptr;
    gStrictOriginal.store(nullptr, std::memory_order_release);

    // The physical entry can be restored while an already-entered module
    // replacement is still executing. The published backup must remain
    // executable after its Dobby ticket is released; a host that cannot
    // unregister its callback must also keep replacement code mapped.
    void* heldTarget = MakeTarget(page_size);
    if (!heldTarget) {
        ok = Check(false, "held-callback target mapping");
    } else {
        auto heldFn = reinterpret_cast<TargetFn>(heldTarget);
        void* heldBackup = nullptr;
        if (api.hookFunc(heldTarget, reinterpret_cast<void*>(HeldReplacement), &heldBackup) !=
                RT_SUCCESS ||
            !heldBackup) {
            ok = Check(false, "held-callback installation");
        } else {
            gHeldOriginal.store(reinterpret_cast<TargetFn>(heldBackup), std::memory_order_release);
            gHeldCallbackEntered.store(false, std::memory_order_release);
            gReleaseHeldCallback.store(false, std::memory_order_release);
            int heldResult = -1;
            std::thread inFlight([&] { heldResult = heldFn(); });
            while (!gHeldCallbackEntered.load(std::memory_order_acquire))
                std::this_thread::yield();
            const bool removed = api.unhookFunc(heldTarget) == RT_SUCCESS && heldFn() == 7 &&
                                 gHeldOriginal.load()() == 7;
            gReleaseHeldCallback.store(true, std::memory_order_release);
            inFlight.join();
            ok &= Check(removed && heldResult == 99 && api.unhookFunc(heldTarget) != RT_SUCCESS,
                        "in-flight replacement keeps callable original across physical Unhook");
            gHeldOriginal.store(nullptr, std::memory_order_release);
        }
        munmap(heldTarget, page_size);
    }

    // An Unhook that reports a failure after restoring bytes must not release
    // its ticket. Dobby may have restored entry bytes but the caller still
    // owns a potentially in-flight trampoline and must explicitly retry.
    void* failedUnhookTarget = MakeTarget(page_size);
    if (!failedUnhookTarget) {
        ok = Check(false, "failed-unhook target mapping");
    } else {
        auto failedFn = reinterpret_cast<TargetFn>(failedUnhookTarget);
        void* failedBackup = nullptr;
        const int installedForRemoval =
            api.hookFunc(failedUnhookTarget, reinterpret_cast<void*>(Replacement), &failedBackup);
        gOriginal = reinterpret_cast<TargetFn>(failedBackup);
        ok &= Check(installedForRemoval == RT_SUCCESS && failedBackup && failedFn() == 99,
                    "failed-unhook setup");
        gFaultTarget = failedUnhookTarget;
        gInjectFailure.store(true, std::memory_order_release);
        const int firstRemoval = api.unhookFunc(failedUnhookTarget);
        void* rejectedBackup = nullptr;
        const int conflicting =
            api.hookFunc(failedUnhookTarget, reinterpret_cast<void*>(Replacement), &rejectedBackup);
        const bool retained = firstRemoval != RT_SUCCESS && conflicting != RT_SUCCESS &&
                              rejectedBackup == nullptr && gOriginal && gOriginal() == 7;
        gFaultTarget = nullptr;
        const int recovered = api.unhookFunc(failedUnhookTarget);
        ok &= Check(retained && recovered == RT_SUCCESS && failedFn() == 7 &&
                        api.unhookFunc(failedUnhookTarget) != RT_SUCCESS,
                    "failed Unhook retains exact owner until explicit retry");
        munmap(failedUnhookTarget, page_size);
    }

    // The real libfusehide.so must accept the exact v3 extension ABI. Do not
    // call PostNativeInit here: this standalone executable is not MediaProvider.
    if (argc == 2) {
        void* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        if (!module) {
            std::fprintf(stderr, "native-adapter FAIL: load real libfusehide.so: %s\n", dlerror());
            ok = false;
        } else {
            using NativeInitV3 = void* (*)(const fusehide::NativeApiEntriesV3*);
            auto* init = reinterpret_cast<NativeInitV3>(dlsym(module, "native_init_v3"));
            const bool accepted = init && init(&strict) != nullptr;
            ok &= Check(accepted && dlsym(module, "native_init") != nullptr,
                        "real libfusehide.so accepts versioned v3 and exports legacy v2");
            if (accepted)
                std::puts("native-init-v3 result=PASS");
            // Keep the library mapped: the strict API function pointers are
            // process-lifetime Zygisk callbacks in the real integration.
        }
    }

    munmap(target, page_size);
    munmap(foreign_target, page_size);
    std::printf("native-adapter result=%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
