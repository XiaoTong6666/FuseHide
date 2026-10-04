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

#include <android/dlext.h>
#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <link.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>

#include "dex_loader.hpp"
#include "dobby.h"
#include "native_hook_adapter.hpp"
#include "zygisk_log.hpp"

// module.cpp provides these process-local values.
extern JavaVM* gVm;
extern void* gFuseHideHandle;

namespace {

constexpr char kLogTag[] = "FuseHide";
constexpr char kFuseJniMarker[] = "libfuse_jni.so";
constexpr char kFuseHideMarker[] = "libfusehide.so";

using AndroidDlopenExt = void* (*)(const char*, int, const android_dlextinfo*);
using PublicDlopen = void* (*)(const char*, int);
using JniOnLoad = jint (*)(JavaVM*, void*);
using NativeInit = void* (*)(void*);
using PostNativeInit = void (*)(const char*, void*);

struct LoaderHookState {
    std::atomic<void*> original{nullptr};
    DobbyHookHandle handle = 0;
    bool recoveryRequired = false;
    bool needsAbort = false;
    bool everPublished = false;
};

LoaderHookState gAndroidDlopenExtHook;
LoaderHookState gPublicDlopenHook;
std::atomic_bool gDlopenMonitorActive{false};
std::atomic_bool gDlopenMonitorInstalling{false};
std::atomic_bool gObserverStarted{false};
std::atomic_bool gPublicHookAttempted{false};
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

bool FindLoadedFuseJni(std::string* path) {
    if (path == nullptr) {
        return false;
    }
    struct SearchState {
        std::string* path;
        bool found;
    } state{path, false};
    dl_iterate_phdr(
        [](dl_phdr_info* info, size_t, void* opaque) -> int {
            auto* search = static_cast<SearchState*>(opaque);
            if (info == nullptr || info->dlpi_name == nullptr) {
                return 0;
            }
            const std::string_view name(info->dlpi_name);
            if (name.find(kFuseJniMarker) == std::string_view::npos) {
                return 0;
            }
            search->path->assign(name.data(), name.size());
            search->found = true;
            return 1;
        },
        &state);
    return state.found;
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

void MaybeInitAfterLoad(const char* name, void* handle) {
    if (handle != nullptr && IsFuseJni(name)) {
        InitFuseHideOnFuseLoaded(name, handle);
    }
}

void* HookedAndroidDlopenExt(const char* name, int flags, const android_dlextinfo* extinfo) {
    const auto original = reinterpret_cast<AndroidDlopenExt>(
        gAndroidDlopenExtHook.original.load(std::memory_order_acquire));
    if (original == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "android_dlopen_ext callback without backup");
        return nullptr;
    }
    void* handle = original(name, flags, extinfo);
    MaybeInitAfterLoad(name, handle);
    return handle;
}

void* HookedPublicDlopen(const char* name, int flags) {
    const auto original =
        reinterpret_cast<PublicDlopen>(gPublicDlopenHook.original.load(std::memory_order_acquire));
    if (original == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "dlopen callback without backup");
        return nullptr;
    }
    void* handle = original(name, flags);
    MaybeInitAfterLoad(name, handle);
    return handle;
}

void StartFuseJniObserver() {
    bool expected = false;
    if (!gObserverStarted.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }
    gDlopenMonitorActive.store(true, std::memory_order_release);

    std::string loadedPath;
    if (FindLoadedFuseJni(&loadedPath)) {
        InitFuseHideOnFuseLoaded(loadedPath.c_str(), nullptr);
        return;
    }

    std::thread([]() {
        constexpr int kFastAttempts = 1000;
        constexpr int kMediumAttempts = 2200;
        bool slowObserverLogged = false;
        for (int attempt = 0;; ++attempt) {
            const auto state = gFuseHideInitState.load(std::memory_order_acquire);
            if (state == FuseHideInitState::kReady || state == FuseHideInitState::kFailedAfterJni) {
                return;
            }
            std::string path;
            if (FindLoadedFuseJni(&path)) {
                InitFuseHideOnFuseLoaded(path.c_str(), nullptr);
                const auto after = gFuseHideInitState.load(std::memory_order_acquire);
                if (after == FuseHideInitState::kReady ||
                    after == FuseHideInitState::kFailedAfterJni) {
                    return;
                }
            }
            if (attempt < kFastAttempts) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            } else if (attempt < kMediumAttempts) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            } else {
                if (!slowObserverLogged) {
                    __android_log_print(
                        ANDROID_LOG_INFO, kLogTag,
                        "libfuse_jni.so not loaded yet; continuing low-frequency phdr observer");
                    slowObserverLogged = true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }).detach();
}

void RecoverLoaderHookIfNeeded(const char* label, LoaderHookState* state) {
    if (state == nullptr || !state->recoveryRequired || state->handle == 0) {
        return;
    }
    DobbyHookResult recovered{};
    recovered.struct_size = sizeof(recovered);
    const int status = state->needsAbort ? DobbyAbortHook(state->handle, &recovered)
                                         : DobbyRecoverHook(state->handle, &recovered);
    if (status != RT_SUCCESS) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "%s monitor recovery deferred: status=%u cause=%u", label,
                            recovered.status, recovered.cause);
        return;
    }
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "%s monitor recovery completed", label);
    state->handle = 0;
    state->recoveryRequired = false;
    state->needsAbort = false;
}

bool InstallPublicLoaderHook(const char* label, void* target, dobby_dummy_func_t replacement,
                             LoaderHookState* state) {
    if (target == nullptr || state == nullptr || state->handle != 0 || state->recoveryRequired) {
        return false;
    }
#if defined(FUSEHIDE_LINKER_REVIEW_TEST)
    void* noTarget = nullptr;
    gDlopenReviewTarget.compare_exchange_strong(noTarget, target, std::memory_order_acq_rel);
#endif

    DobbyHookOptions options = {
        sizeof(DobbyHookOptions),
#if defined(__aarch64__)
        DOBBY_BRANCH_REQUIRE_NEAR,
        DOBBY_HOOK_REQUIRE_CONCURRENT_SAFE | DOBBY_HOOK_REQUIRE_PRISTINE_ENTRY |
            DOBBY_HOOK_VALIDATE_BACKUP | DOBBY_HOOK_PRESERVE_LANDING_PAD,
#elif defined(__x86_64__)
        DOBBY_BRANCH_FORCE_LONG,
        DOBBY_HOOK_REQUIRE_CONCURRENT_SAFE | DOBBY_HOOK_REQUIRE_PRISTINE_ENTRY,
#else
        DOBBY_BRANCH_LEGACY,
        DOBBY_HOOK_REQUIRE_PRISTINE_ENTRY,
#endif
        0,
        target,
        replacement,
    };
    DobbyHookResult transaction{};
    transaction.struct_size = sizeof(transaction);
    if (DobbyPrepareHook(&options, &transaction) != RT_SUCCESS || transaction.original == nullptr) {
        __android_log_print(
            ANDROID_LOG_WARN, kLogTag,
            "%s monitor not hooked: prepare status=%u cause=%u; phdr observer remains active",
            label, transaction.status, transaction.cause);
        if (transaction.handle != 0) {
            DobbyHookResult aborted{};
            aborted.struct_size = sizeof(aborted);
            if (DobbyAbortHook(transaction.handle, &aborted) != RT_SUCCESS) {
                state->handle = transaction.handle;
                state->recoveryRequired = true;
                state->needsAbort = true;
            }
        }
        return false;
    }

    const DobbyHookHandle ticket = transaction.handle;
    state->handle = ticket;
    state->original.store(reinterpret_cast<void*>(transaction.original), std::memory_order_release);
    if (DobbyCommitHook(ticket, &transaction) != RT_SUCCESS) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "%s monitor commit failed: status=%u cause=%u retained=%u; phdr "
                            "observer remains active",
                            label, transaction.status, transaction.cause, transaction.handle != 0);
        state->everPublished = transaction.ever_published;
        if (transaction.handle == ticket && !transaction.ever_published &&
            !transaction.target_may_be_patched &&
            transaction.status != DOBBY_HOOK_RECOVERY_REQUIRED) {
            DobbyHookResult aborted{};
            aborted.struct_size = sizeof(aborted);
            if (DobbyAbortHook(ticket, &aborted) == RT_SUCCESS) {
                transaction.handle = 0;
            } else {
                state->needsAbort = true;
            }
        }
        state->handle = transaction.handle;
        state->recoveryRequired = transaction.handle != 0;
        if (transaction.status == DOBBY_HOOK_RECOVERY_REQUIRED) {
            state->needsAbort = false;
        }
        if (!transaction.ever_published && transaction.handle == 0) {
            state->original.store(nullptr, std::memory_order_release);
        }
        return false;
    }

    state->everPublished = true;
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "hooked public %s at %p", label, target);
    return true;
}

}  // namespace

void InstallDlopenMonitor() {
    StartFuseJniObserver();
#if !defined(__aarch64__)
    // A live MediaProvider cannot provide the trusted process-wide quiescence
    // lease required for a multi-byte x86/x64 loader patch.  Other ABIs also
    // remain observer-only until they have an equally strong atomic-publication
    // proof.  The phdr observer is sufficient because PostNativeInit only
    // requires the mapped libfuse_jni path, not its dlopen handle.
    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "loader inline hooks disabled on this ABI; using phdr observer");
    return;
#endif
    RecoverLoaderHookIfNeeded("android_dlopen_ext", &gAndroidDlopenExtHook);
    RecoverLoaderHookIfNeeded("dlopen", &gPublicDlopenHook);
    if (gFuseHideInitState.load(std::memory_order_acquire) == FuseHideInitState::kReady) {
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
    if (gPublicHookAttempted.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    void* androidDlopenExt = dlsym(RTLD_DEFAULT, "android_dlopen_ext");
    void* publicDlopen = dlsym(RTLD_DEFAULT, "dlopen");
    const bool extHooked = InstallPublicLoaderHook(
        "android_dlopen_ext", androidDlopenExt,
        reinterpret_cast<dobby_dummy_func_t>(HookedAndroidDlopenExt), &gAndroidDlopenExtHook);
    const bool dlopenHooked = InstallPublicLoaderHook(
        "dlopen", publicDlopen, reinterpret_cast<dobby_dummy_func_t>(HookedPublicDlopen),
        &gPublicDlopenHook);
    if (!extHooked && !dlopenHooked) {
        __android_log_print(ANDROID_LOG_INFO, kLogTag,
                            "public loader hooks unavailable; using dl_iterate_phdr observer only");
    }
}

#if defined(FUSEHIDE_LINKER_REVIEW_TEST)
extern "C" int FuseHideDlopenMonitorReviewState() {
    return gDlopenMonitorActive.load(std::memory_order_acquire) ? 1 : 0;
}
extern "C" int FuseHideDlopenMonitorReviewRecoveryState() {
    const bool androidExt =
        gAndroidDlopenExtHook.recoveryRequired && gAndroidDlopenExtHook.handle != 0 &&
        gAndroidDlopenExtHook.original.load(std::memory_order_acquire) != nullptr;
    const bool dlopen = gPublicDlopenHook.recoveryRequired && gPublicDlopenHook.handle != 0 &&
                        gPublicDlopenHook.original.load(std::memory_order_acquire) != nullptr;
    return androidExt || dlopen ? 1 : 0;
}
extern "C" void* FuseHideDlopenMonitorReviewTarget() {
    return gDlopenReviewTarget.load(std::memory_order_acquire);
}
#endif
