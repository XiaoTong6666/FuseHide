// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
//
// A real libfusehide.so must remain mapped when the legacy Vector host keeps
// its onModuleLoaded pointer after dlclose. This test never installs FUSE hooks.

#include <dlfcn.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

#include "fusehide/native_hook_api.hpp"
#include "zygisk/native_hook_adapter.hpp"

namespace {
using ModuleCallback = void (*)(const char*, void*);
using InitV2 = void* (*)(const fusehide::NativeApiEntries*);
using InitV3 = void* (*)(const fusehide::NativeApiEntriesV3*);

int HookA(void*, void*, void**) {
    return -1;
}
int HookB(void*, void*, void**) {
    return -1;
}
int UnhookA(void*) {
    return -1;
}
int UnhookB(void*) {
    return -1;
}
int StrictA(void*, void*, void*, fusehide::HookBackupPublisher) {
    return -1;
}
int StrictB(void*, void*, void*, fusehide::HookBackupPublisher) {
    return -1;
}

std::atomic_uint32_t gSyntheticOriginalCalls{0};
extern "C" __attribute__((noinline)) void SyntheticTarget(const char* name, void* handle) {
    // These arguments are part of the hook ABI even though the synthetic
    // original does not otherwise need them.  Keep them observable so LTO/
    // IPA cannot legally omit x0/x1 setup at direct call sites.  An inline
    // replacement must receive exactly the arguments the public function
    // signature promises, not whatever scratch values happened to be live.
    asm volatile("" : : "r"(name), "r"(handle) : "memory");
    gSyntheticOriginalCalls.fetch_add(1, std::memory_order_relaxed);
}
void PublishSyntheticBackup(void* user, void* original) {
    *static_cast<void**>(user) = original;
}

bool Check(bool condition, const char* label) {
    if (!condition)
        std::fprintf(stderr, "callback-lifetime FAIL: %s\n", label);
    return condition;
}

// Android's RTLD_NOLOAD is a name-based lookup. Verify the retained callback
// PC against an actual executable VMA, including after dlclose has dropped
// the last explicit handle. Never invoke an old callback if that VMA vanished.
bool CallbackTextMapped(ModuleCallback callback) {
    FILE* maps = std::fopen("/proc/self/maps", "r");
    if (!maps)
        return false;
    const auto pc = reinterpret_cast<uintptr_t>(callback);
    char line[2048];
    bool mapped = false;
    while (std::fgets(line, sizeof(line), maps)) {
        unsigned long long start = 0;
        unsigned long long end = 0;
        char permissions[5]{};
        if (std::sscanf(line, "%llx-%llx %4s", &start, &end, permissions) != 3)
            continue;
        if (pc >= start && pc < end && permissions[2] == 'x' &&
            std::strstr(line, "libfusehide.so")) {
            mapped = true;
            break;
        }
    }
    std::fclose(maps);
    return mapped;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[2], "v2") && std::strcmp(argv[2], "v2-no-unhook") &&
         std::strcmp(argv[2], "v3") && std::strcmp(argv[2], "v2-preclosed") &&
         std::strcmp(argv[2], "v3-preclosed") && std::strcmp(argv[2], "v2-init-close-race") &&
         std::strcmp(argv[2], "v3-init-close-race"))) {
        std::fprintf(stderr,
                     "Usage: %s /absolute/libfusehide.so "
                     "v2|v2-no-unhook|v3|v2-preclosed|v3-preclosed|"
                     "v2-init-close-race|v3-init-close-race\n",
                     argv[0]);
        return 2;
    }
    void* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) {
        std::fprintf(stderr, "callback-lifetime FAIL: dlopen: %s\n", dlerror());
        return 1;
    }
    const auto initV2 = reinterpret_cast<InitV2>(dlsym(module, "native_init"));
    const auto initV3 = reinterpret_cast<InitV3>(dlsym(module, "native_init_v3"));
    const auto closeAdmission =
        reinterpret_cast<void (*)()>(dlsym(module, "fusehide_close_loader_callback_admission"));
    const auto finishDrain =
        reinterpret_cast<bool (*)()>(dlsym(module, "fusehide_finish_loader_callback_drain"));
    const auto inFlight =
        reinterpret_cast<uint32_t (*)()>(dlsym(module, "fusehide_loader_callback_in_flight"));
    if (!Check(initV2 && initV3, "both entrypoints exported"))
        return 1;
    if (!Check(closeAdmission && finishDrain && inFlight,
               "logical loader callback drain controls exported"))
        return 1;
    const fusehide::NativeApiEntries v2{2, fusehide::zygisk::InstallNativeHook,
                                        fusehide::zygisk::UninstallNativeHook};
    const fusehide::NativeApiEntries compatibleV2{2, fusehide::zygisk::InstallNativeHook, nullptr};
    const fusehide::NativeApiEntries foreignV2{2, HookB, UnhookB};
    const fusehide::NativeApiEntriesV3 v3{
        {3, fusehide::zygisk::InstallNativeHook, fusehide::zygisk::UninstallNativeHook},
        sizeof(fusehide::NativeApiEntriesV3),
        0,
        fusehide::zygisk::InstallNativeHookWithPublication};
    const fusehide::NativeApiEntriesV3 foreignV3{
        {3, HookB, UnhookB}, sizeof(fusehide::NativeApiEntriesV3), 0, StrictB};

    const bool strict = std::strncmp(argv[2], "v3", 2) == 0;
    const auto* selectedV2 = std::strcmp(argv[2], "v2-no-unhook") == 0 ? &compatibleV2 : &v2;
    const bool preclosed = std::strstr(argv[2], "preclosed") != nullptr;
    if (preclosed) {
        closeAdmission();
        bool closed =
            Check(finishDrain() && inFlight() == 0, "closed-before-init has no admitted callbacks");
        closed &= Check((strict ? initV3(&v3) : initV2(selectedV2)) == nullptr,
                        "closed module cannot register a new host callback");
        dlclose(module);
        void* resident = dlopen(argv[1], RTLD_NOW | RTLD_NOLOAD);
        const bool mapped = CallbackTextMapped(reinterpret_cast<ModuleCallback>(initV2));
        closed &= Check(mapped, "closed module still keeps callback text mapped");
        if (resident)
            dlclose(resident);
        std::printf(
            "callback-lifetime backend=%s callbacks=0 physical=0 nodelete=%u noload=%u result=%s\n",
            argv[2], mapped, resident != nullptr, closed ? "PASS" : "FAIL");
        return closed ? 0 : 1;
    }
    const bool initCloseRace = std::strstr(argv[2], "init-close-race") != nullptr;
    if (initCloseRace) {
        std::atomic_bool launch{false};
        std::atomic<void*> published{nullptr};
        std::atomic_bool drained{false};
        std::thread initializing([&] {
            while (!launch.load(std::memory_order_acquire)) {
            }
            published.store(strict ? initV3(&v3) : initV2(selectedV2), std::memory_order_release);
        });
        std::thread retiring([&] {
            launch.store(true, std::memory_order_release);
            closeAdmission();
            for (uint32_t attempt = 0; attempt < 100000; ++attempt) {
                if (inFlight() == 0 && finishDrain()) {
                    drained.store(true, std::memory_order_release);
                    return;
                }
                std::this_thread::yield();
            }
        });
        initializing.join();
        retiring.join();
        bool raced = Check(drained.load(std::memory_order_acquire) && inFlight() == 0,
                           "init/close race reaches a closed idle state");
        auto callback = reinterpret_cast<ModuleCallback>(published.load(std::memory_order_acquire));
        if (callback != nullptr) {
            callback("libnot_fuse_jni.so", nullptr);
            raced &= Check(inFlight() == 0 && finishDrain(),
                           "callback admitted before close becomes a retired no-op");
        }
        raced &= Check((strict ? initV3(&v3) : initV2(selectedV2)) == nullptr,
                       "closed gate rejects post-drain initialization");
        dlclose(module);
        const bool mapped = CallbackTextMapped(
            callback != nullptr ? callback : reinterpret_cast<ModuleCallback>(initV2));
        raced &= Check(mapped, "init/close race retains module text");
        std::printf(
            "callback-lifetime backend=%s callbacks=0 physical=0 nodelete=%u noload=0 "
            "published=%u result=%s\n",
            argv[2], mapped, callback != nullptr, raced ? "PASS" : "FAIL");
        return raced ? 0 : 1;
    }
    // Invalid handshakes must not poison the process-global first-owner
    // binding, or a later valid host will be incorrectly rejected.
    bool ok = true;
    if (strict) {
        auto bad = v3;
        bad.base.version = 2;
        ok &= Check(initV3(&bad) == nullptr, "v3 rejects unsupported version");
        bad = v3;
        bad.struct_size = sizeof(bad) - 1;
        ok &= Check(initV3(&bad) == nullptr, "v3 rejects truncated ABI");
        bad = v3;
        bad.reserved = 1;
        ok &= Check(initV3(&bad) == nullptr, "v3 rejects reserved fields");
        bad = v3;
        bad.hookWithPublication = nullptr;
        ok &= Check(initV3(&bad) == nullptr, "v3 requires backup publisher");
        bad = v3;
        bad.base.unhookFunc = nullptr;
        ok &= Check(initV3(&bad) == nullptr, "v3 requires owned unhook");
    } else {
        auto bad = *selectedV2;
        bad.version = 1;
        ok &= Check(initV2(&bad) == nullptr, "v2 rejects unsupported version");
        bad = *selectedV2;
        bad.version = 3;
        ok &= Check(initV2(&bad) == nullptr, "v2 rejects an unknown ABI version");
        bad = *selectedV2;
        bad.hookFunc = nullptr;
        ok &= Check(initV2(&bad) == nullptr, "v2 rejects missing Hook");
    }
    if (!ok)
        return 1;
    const auto callback =
        reinterpret_cast<ModuleCallback>(strict ? initV3(&v3) : initV2(selectedV2));
    ok &= Check(callback != nullptr, "initial callback published after rejected handshakes");
    const auto repeated =
        reinterpret_cast<ModuleCallback>(strict ? initV3(&v3) : initV2(selectedV2));
    ok &= Check(repeated == nullptr, "same owner does not register a duplicate callback");
    ok &= Check((strict ? initV3(&foreignV3) : initV2(&foreignV2)) == nullptr,
                "foreign host cannot rebind published callback");
    ok &= Check((strict ? initV2(selectedV2) : initV3(&v3)) == nullptr,
                "other API family cannot downgrade or upgrade a live host");
    if (!ok)
        return 1;

    // A different DSO supplies the replacement callback. The physical hook
    // belongs to the host adapter, so uninstalling it must not imply that the
    // module text can be freed while a racing CPU holds its old branch target.
    void* syntheticBackup = nullptr;
    auto* target = reinterpret_cast<void*>(+SyntheticTarget);
    const bool physicallyInstalled =
        (strict ? fusehide::zygisk::InstallNativeHookWithPublication(
                      target, reinterpret_cast<void*>(callback), &syntheticBackup,
                      PublishSyntheticBackup)
                : fusehide::zygisk::InstallNativeHook(target, reinterpret_cast<void*>(callback),
                                                      &syntheticBackup)) == 0;
#if defined(__x86_64__)
    // The standalone process has no verified quiescence lease. A long x64
    // entry patch MUST be rejected rather than used for this lifetime test.
    ok &= Check(!physicallyInstalled && syntheticBackup == nullptr,
                "unmanaged x64 cross-DSO Hook fails closed");
#else
    ok &= Check(physicallyInstalled && syntheticBackup,
                "physical Dobby Hook targets the external module callback");
    if (!ok)
        return 1;
    const auto originalTarget = reinterpret_cast<ModuleCallback>(syntheticBackup);
    originalTarget("libnot_fuse_jni.so", nullptr);
    ok &= Check(gSyntheticOriginalCalls.load(std::memory_order_relaxed) == 1,
                "cross-DSO backup remains independently callable");
    SyntheticTarget("libnot_fuse_jni.so", nullptr);
    ok &= Check(gSyntheticOriginalCalls.load(std::memory_order_relaxed) == 1,
                "patched host target dispatches into the external module");
#endif
    if (!ok)
        return 1;

    std::atomic_bool start{false};
    std::atomic_uint32_t count{0};
    std::atomic_uint32_t physicalCount{0};
    std::atomic_bool physicalPaused{false};
    std::atomic_bool physicalResume{false};
    std::thread active([&] {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (unsigned i = 0; i < 50000; ++i) {
            callback("libnot_fuse_jni.so", nullptr);  // Deliberately benign event.
            count.fetch_add(1, std::memory_order_relaxed);
        }
    });
    std::thread closing([&] {
        start.store(true, std::memory_order_release);
        // Retirement order is deliberate:
        //   1. reject new logical callback work;
        //   2. restore the physical entry;
        //   3. wait for already-admitted callbacks to drain;
        //   4. release only the dlopen handle, never the retained DSO mapping.
        // A CPU that fetched the old branch before Unhook may still enter the
        // replacement, but it observes Draining and becomes a no-op.
        while (!physicalPaused.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        closeAdmission();
#if defined(__x86_64__)
        if (fusehide::zygisk::UninstallNativeHook(target) == 0)
            count.store(UINT32_MAX, std::memory_order_release);
#else
        if (fusehide::zygisk::UninstallNativeHook(target) != 0)
            count.store(UINT32_MAX, std::memory_order_release);
#endif
        // Always release the target thread, even on an assertion path, so a
        // failed Unhook is reported instead of deadlocking the fixture.
        physicalResume.store(true, std::memory_order_release);
        // Close may still observe an admitted callback. Keep the gate in
        // Draining; never reopen it to accommodate a retry.
        (void)finishDrain();
        dlclose(module);
    });
    std::thread physical([&] {
        while (!start.load(std::memory_order_acquire)) {
        }
        // The barrier makes "before Unhook" and "after Unhook" executions
        // deterministic even when translated ARM64 runs much faster/slower
        // than the closing thread on the host scheduler.
        constexpr unsigned kBeforeUnhook = 1024;
        for (unsigned i = 0; i < kBeforeUnhook; ++i) {
            SyntheticTarget("libnot_fuse_jni.so", nullptr);
            physicalCount.fetch_add(1, std::memory_order_release);
        }
        physicalPaused.store(true, std::memory_order_release);
        while (!physicalResume.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        for (unsigned i = kBeforeUnhook; i < 50000; ++i) {
            SyntheticTarget("libnot_fuse_jni.so", nullptr);
            physicalCount.fetch_add(1, std::memory_order_release);
        }
    });
    active.join();
    physical.join();
    closing.join();
    ok &= Check(physicalCount.load(std::memory_order_relaxed) == 50000 &&
                    gSyntheticOriginalCalls.load(std::memory_order_relaxed) > 1,
#if defined(__x86_64__)
                "unmanaged x64 target remains unmodified across callback drain");
#else
                "physical Unhook restores host target while callbacks run");
    const auto beforeOriginalCall = gSyntheticOriginalCalls.load(std::memory_order_relaxed);
    SyntheticTarget("libnot_fuse_jni.so", nullptr);
    ok &= Check(gSyntheticOriginalCalls.load(std::memory_order_relaxed) == beforeOriginalCall + 1,
                "host original entry is callable after physical Unhook");
#endif
    ok &= Check(fusehide::zygisk::UninstallNativeHook(target) != 0,
                "physical owner ticket cannot be destroyed twice");
    ok &= Check(inFlight() == 0 && finishDrain(),
                "all admitted callbacks drained and loader admission closed");

    // The legacy host cannot unregister the callback. A NODELETE mapping is
    // required even if no physical Hook remains.
    void* stillLoaded = dlopen(argv[1], RTLD_NOW | RTLD_NOLOAD);
    const bool callbackMapped = CallbackTextMapped(callback);
    ok &= Check(callbackMapped, "retained callback PC remains in executable DSO mapping");
    if (callbackMapped) {
        for (int i = 0; i < 1000; ++i)
            callback("libnot_fuse_jni.so", nullptr);
        // If callback admission is truly closed, even a filename matching the
        // real target library cannot invoke InstallFuseHooks after retirement.
        callback("libfuse_jni.so", nullptr);
        ok &= Check(inFlight() == 0 && finishDrain(),
                    "late callback pointer is callable but logically retired");
        ok &= Check((strict ? initV3(&v3) : initV2(selectedV2)) == nullptr,
                    "retired module cannot publish a duplicate callback");
    }
    if (stillLoaded)
        dlclose(stillLoaded);
    ok &= Check(count.load(std::memory_order_relaxed) == 50000,
                "in-flight callback completes across dlclose");
    std::printf(
        "callback-lifetime backend=%s callbacks=%u physical=%u nodelete=%u noload=%u result=%s\n",
        argv[2], count.load(std::memory_order_relaxed), physicallyInstalled, callbackMapped,
        stillLoaded != nullptr, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
