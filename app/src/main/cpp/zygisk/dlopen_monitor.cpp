// Copyright (C) 2026 XiaoTong6666
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dlopen_monitor.hpp"

#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <thread>

#include "dex_loader.hpp"
#include "dobby.h"
#include "fusehide/elf/elf_utils.hpp"
#include "native_hook_adapter.hpp"
#include "zygisk_log.hpp"

// module.cpp provides these process-local values.
extern JavaVM* gVm;
extern void* gFuseHideHandle;

namespace {

constexpr char kLogTag[] = "FuseHide";
constexpr char kFuseJniMarker[] = "libfuse_jni.so";
constexpr char kFuseHideMarker[] = "libfusehide.so";
constexpr char kDoDlopenSymbol[] = "__dl__Z9do_dlopenPKciPK17android_dlextinfoPKv";

using DoDlopen = void* (*)(const char*, int, const void*, const void*);
using JniOnLoad = jint (*)(JavaVM*, void*);
using NativeInit = void* (*)(void*);
using PostNativeInit = void (*)(const char*, void*);

// The original must be published BEFORE committing the entry patch. The
// replacement runs on unrelated loader threads and reads this concurrently.
std::atomic<DoDlopen> gOriginalDoDlopen{nullptr};
std::atomic_bool gDlopenMonitorActive{false};
std::atomic_bool gDlopenMonitorInstalling{false};
// Only the thread that holds gDlopenMonitorInstalling accesses these two
// tickets. Never retry Prepare while a failed Commit still owns its target.
DobbyHookHandle gDlopenMonitorHandle = 0;
bool gDlopenMonitorRecoveryRequired = false;
bool gDlopenMonitorNeedsAbort = false;
std::atomic_bool gDlopenMonitorEverPublished{false};
#if defined(FUSEHIDE_LINKER_REVIEW_TEST)
std::atomic<void*> gDlopenReviewTarget{nullptr};
#endif
enum class FuseHideInitState : uint8_t {
    kNotStarted,
    kInitializing,
    kReady,
    // JNI_OnLoad may already have made irreversible changes. Do not rerun it
    // after a partial native initialization failure.
    kFailedAfterJni,
};
std::atomic<FuseHideInitState> gFuseHideInitState{FuseHideInitState::kNotStarted};

bool MapsContains(const char* needle) {
    FILE* maps = fopen("/proc/self/maps", "re");
    if (maps == nullptr) {
        return false;
    }
    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), maps) != nullptr) {
        if (strstr(line, needle) != nullptr) {
            found = true;
            break;
        }
    }
    fclose(maps);
    return found;
}

bool IsFuseJni(const char* name) {
    return name != nullptr && std::string_view(name).ends_with(kFuseJniMarker);
}

bool GetJniEnv(JNIEnv** env, bool* attached) {
    if (gVm == nullptr || env == nullptr || attached == nullptr) {
        return false;
    }
    *attached = false;
    const jint result = gVm->GetEnv(reinterpret_cast<void**>(env), JNI_VERSION_1_6);
    if (result == JNI_OK) {
        return true;
    }
    if (result != JNI_EDETACHED || gVm->AttachCurrentThread(env, nullptr) != JNI_OK) {
        return false;
    }
    *attached = true;
    return true;
}

void StartInjectedJavaWhenApplicationReady(JNIEnv* env) {
    if (StartInjectedJava(env)) {
        return;
    }
    std::thread([]() {
        JNIEnv* workerEnv = nullptr;
        if (gVm == nullptr || gVm->AttachCurrentThread(&workerEnv, nullptr) != JNI_OK ||
            workerEnv == nullptr) {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                                "failed to attach Java initialization worker");
            return;
        }
        constexpr int kMaxAttempts = 100;
        for (int attempt = 1; attempt <= kMaxAttempts; ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (StartInjectedJava(workerEnv)) {
                __android_log_print(ANDROID_LOG_INFO, kLogTag,
                                    "Application ready after %d Java initialization attempts",
                                    attempt);
                gVm->DetachCurrentThread();
                return;
            }
        }
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "timed out waiting for MediaProvider Application");
        gVm->DetachCurrentThread();
    }).detach();
}

void InitFuseHideOnFuseLoaded(const char* loadedLibrary, void* loadedHandle) {
    if (gFuseHideHandle == nullptr && MapsContains(kFuseHideMarker)) {
        __android_log_print(ANDROID_LOG_INFO, kLogTag,
                            "libfusehide.so already loaded; native framework owns initialization");
        return;
    }
    if (gFuseHideHandle == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "preloaded libfusehide.so is unavailable");
        return;
    }

    auto expected = FuseHideInitState::kNotStarted;
    if (!gFuseHideInitState.compare_exchange_strong(expected, FuseHideInitState::kInitializing,
                                                    std::memory_order_acq_rel)) {
        return;
    }

    JNIEnv* env = nullptr;
    bool attached = false;
    if (!GetJniEnv(&env, &attached)) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "failed to obtain JNIEnv");
        // No module initialization has started: a later loader event may retry.
        gFuseHideInitState.store(FuseHideInitState::kNotStarted, std::memory_order_release);
        return;
    }

    auto jniOnLoad = reinterpret_cast<JniOnLoad>(dlsym(gFuseHideHandle, "JNI_OnLoad"));
    if (jniOnLoad == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "JNI_OnLoad not found in libfusehide.so");
        gFuseHideInitState.store(FuseHideInitState::kFailedAfterJni, std::memory_order_release);
        if (attached) {
            gVm->DetachCurrentThread();
        }
        return;
    }
    // A failed JNI_OnLoad can already have registered methods or initialized
    // native globals, so retrying the same library is not demonstrably safe.
    if (jniOnLoad(gVm, nullptr) < JNI_VERSION_1_6) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "JNI_OnLoad failed for libfusehide.so");
        gFuseHideInitState.store(FuseHideInitState::kFailedAfterJni, std::memory_order_release);
        if (attached) {
            gVm->DetachCurrentThread();
        }
        return;
    }

    auto nativeInitStrict = reinterpret_cast<NativeInit>(dlsym(gFuseHideHandle, "native_init_v3"));
    auto nativeInit = reinterpret_cast<NativeInit>(dlsym(gFuseHideHandle, "native_init"));
    if (nativeInitStrict == nullptr && nativeInit == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "native_init not found in libfusehide.so");
        gFuseHideInitState.store(FuseHideInitState::kFailedAfterJni, std::memory_order_release);
        if (attached) {
            gVm->DetachCurrentThread();
        }
        return;
    }

    void* postInitAddress = nullptr;
    if (nativeInitStrict) {
        const auto& api = fusehide::zygisk::GetStrictNativeHookApi();
        postInitAddress = nativeInitStrict(const_cast<fusehide::NativeApiEntriesV3*>(&api));
    } else {
        const auto& api = fusehide::zygisk::GetNativeHookApi();
        postInitAddress = nativeInit(const_cast<fusehide::NativeApiEntries*>(&api));
    }
    auto postNativeInit = reinterpret_cast<PostNativeInit>(postInitAddress);
    if (postNativeInit == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "native_init returned null");
        gFuseHideInitState.store(FuseHideInitState::kFailedAfterJni, std::memory_order_release);
        if (attached) {
            gVm->DetachCurrentThread();
        }
        return;
    }

    RegisterAllNativeMethods(env);
    postNativeInit(loadedLibrary, loadedHandle);
    gFuseHideInitState.store(FuseHideInitState::kReady, std::memory_order_release);
    StartInjectedJavaWhenApplicationReady(env);
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "FuseHide initialized after %s", loadedLibrary);

    if (attached) {
        gVm->DetachCurrentThread();
    }
}

void* HookedDoDlopen(const char* name, int flags, const void* extinfo, const void* callerAddress) {
    const DoDlopen original = gOriginalDoDlopen.load(std::memory_order_acquire);
    // The publication order makes this unreachable during normal installation;
    // fail closed if a foreign patch ever reaches this callback without backup.
    if (original == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "do_dlopen callback without backup");
        return nullptr;
    }
    void* handle = original(name, flags, extinfo, callerAddress);
    if (handle != nullptr && IsFuseJni(name)) {
        InitFuseHideOnFuseLoaded(name, handle);
    }
    return handle;
}

}  // namespace

void InstallDlopenMonitor() {
    if (gDlopenMonitorActive.load(std::memory_order_acquire)) {
        return;
    }
    bool expected = false;
    if (!gDlopenMonitorInstalling.compare_exchange_strong(expected, true,
                                                          std::memory_order_acq_rel)) {
        return;
    }
    struct AttemptGuard {
        ~AttemptGuard() {
            gDlopenMonitorInstalling.store(false, std::memory_order_release);
        }
    } guard;
    if (gDlopenMonitorActive.load(std::memory_order_acquire))
        return;

    // Recovery is not equivalent to re-running Prepare. A failed sync-core
    // may have made this replacement visible before Commit returned. Retain
    // the backup, restore the exact old ticket, and only then try another
    // installation. A failed recovery leaves ownership intact.
    if (gDlopenMonitorRecoveryRequired) {
        DobbyHookResult recovered{};
        recovered.struct_size = sizeof(recovered);
        const int recovery_status = gDlopenMonitorNeedsAbort
                                        ? DobbyAbortHook(gDlopenMonitorHandle, &recovered)
                                        : DobbyRecoverHook(gDlopenMonitorHandle, &recovered);
        if (recovery_status != RT_SUCCESS) {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                                "cannot recover do_dlopen: status=%u cause=%u", recovered.status,
                                recovered.cause);
            return;
        }
        gDlopenMonitorHandle = 0;
        gDlopenMonitorRecoveryRequired = false;
        gDlopenMonitorNeedsAbort = false;
    }

    auto linker = fusehide::FindModuleFromMaps("/linker64");
    if (!linker.has_value()) {
        linker = fusehide::FindModuleFromMaps("/linker");
    }
    if (!linker.has_value()) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "dynamic linker mapping not found");
        return;
    }

    auto mapped = fusehide::MapReadOnlyFile(linker->path, linker->fileOffset);
    if (!mapped.has_value()) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "failed to map %s", linker->path.c_str());
        return;
    }
    auto offset = fusehide::FindSymbolOffset(*mapped, kDoDlopenSymbol);
    if (!offset.has_value()) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "linker symbol not found: %s",
                            kDoDlopenSymbol);
        return;
    }

    void* target = reinterpret_cast<void*>(linker->base + *offset);
#if defined(FUSEHIDE_LINKER_REVIEW_TEST)
    gDlopenReviewTarget.store(target, std::memory_order_release);
#endif
    DobbyHookOptions options = {
        sizeof(DobbyHookOptions),
#if defined(__aarch64__)
        DOBBY_BRANCH_REQUIRE_NEAR,
        DOBBY_HOOK_REQUIRE_CONCURRENT_SAFE,
#elif defined(__x86_64__)
        // The legacy x64 multi-byte patch is not safe against concurrent
        // linker callers. Require an exclusive host lease rather than
        // silently publishing a torn do_dlopen entry.
        DOBBY_BRANCH_FORCE_LONG,
        DOBBY_HOOK_REQUIRE_CONCURRENT_SAFE,
#else
        DOBBY_BRANCH_LEGACY,
        0,
#endif
        0,
        target,
        reinterpret_cast<dobby_dummy_func_t>(HookedDoDlopen),
    };
    DobbyHookResult transaction{};
    transaction.struct_size = sizeof(transaction);
    if (DobbyPrepareHook(&options, &transaction) != RT_SUCCESS || transaction.original == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "failed to prepare do_dlopen: status=%u cause=%u", transaction.status,
                            transaction.cause);
        // Prepare must never publish an entry. If it unexpectedly returned an
        // owned ticket, do not allow a subsequent install to alias that owner.
        if (transaction.handle) {
            DobbyHookResult aborted{};
            aborted.struct_size = sizeof(aborted);
            if (DobbyAbortHook(transaction.handle, &aborted) != RT_SUCCESS) {
                gDlopenMonitorHandle = transaction.handle;
                gDlopenMonitorRecoveryRequired = true;
                gDlopenMonitorNeedsAbort = true;
            }
        }
        return;
    }
    const DobbyHookHandle hook_handle = transaction.handle;
    gDlopenMonitorHandle = hook_handle;
    gOriginalDoDlopen.store(reinterpret_cast<DoDlopen>(transaction.original),
                            std::memory_order_release);
    if (DobbyCommitHook(hook_handle, &transaction) != RT_SUCCESS) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "failed to commit do_dlopen: status=%u cause=%u retained=%u",
                            transaction.status, transaction.cause, transaction.handle != 0);
        if (transaction.ever_published) {
            gDlopenMonitorEverPublished.store(true, std::memory_order_release);
        }
        // A TARGET_CHANGED failure remains Prepared. Abort it without
        // touching the foreign writer's bytes, rather than leaking the slot
        // or trying to Recover a transaction that never entered Commit.
        if (transaction.handle == hook_handle && !transaction.ever_published &&
            !transaction.target_may_be_patched &&
            transaction.status != DOBBY_HOOK_RECOVERY_REQUIRED) {
            DobbyHookResult aborted{};
            aborted.struct_size = sizeof(aborted);
            if (DobbyAbortHook(hook_handle, &aborted) == RT_SUCCESS)
                transaction.handle = 0;
            else
                gDlopenMonitorNeedsAbort = true;
        }
        gDlopenMonitorHandle = transaction.handle;
        gDlopenMonitorRecoveryRequired = transaction.handle != 0;
        if (!gDlopenMonitorRecoveryRequired)
            gDlopenMonitorNeedsAbort = false;
        // Never clear a backup that a previous or current failed install
        // could still need. Restoring bytes does not drain running callbacks.
        if (!gDlopenMonitorEverPublished.load(std::memory_order_acquire) &&
            !transaction.ever_published && !transaction.handle) {
            gOriginalDoDlopen.store(nullptr, std::memory_order_release);
        }
        return;
    }
    gDlopenMonitorEverPublished.store(true, std::memory_order_release);
    gDlopenMonitorActive.store(true, std::memory_order_release);

    __android_log_print(ANDROID_LOG_INFO, kLogTag, "hooked do_dlopen at %p from %s", target,
                        linker->path.c_str());
}

#if defined(FUSEHIDE_LINKER_REVIEW_TEST)
extern "C" int FuseHideDlopenMonitorReviewState() {
    return gDlopenMonitorActive.load(std::memory_order_acquire) &&
                   gOriginalDoDlopen.load(std::memory_order_acquire) != nullptr &&
                   gDlopenMonitorHandle != 0
               ? 1
               : 0;
}
extern "C" int FuseHideDlopenMonitorReviewRecoveryState() {
    return gDlopenMonitorRecoveryRequired && gDlopenMonitorHandle != 0 &&
                   gOriginalDoDlopen.load(std::memory_order_acquire) != nullptr
               ? 1
               : 0;
}
extern "C" void* FuseHideDlopenMonitorReviewTarget() {
    return gDlopenReviewTarget.load(std::memory_order_acquire);
}
#endif
