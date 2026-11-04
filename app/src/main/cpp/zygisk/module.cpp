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

#include "zygisk.hpp"

#include <android/dlext.h>
#include <android/log.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "dex_loader.hpp"
#include "dlopen_monitor.hpp"
#include "dobby.h"
#include "lsposed_scope.hpp"
#include "zygisk_log.hpp"

// Used by dlopen_monitor.cpp / dex_loader.cpp.
JavaVM* gVm = nullptr;
void* gFuseHideHandle = nullptr;

namespace {

constexpr char kLogTag[] = "FuseHide";
constexpr char kDexRelPath[] = "dex";
constexpr char kFuseHideLibrary[] = "libfusehide.so";
constexpr char kLsposedScopeFlag[] = "lsp_scope_enabled";
constexpr char kLsposedDatabase[] = "/data/adb/lspd/config/modules_config.db";
constexpr char kMediaProviderPackage[] = "com.android.providers.media.module";
constexpr char kGoogleMediaProviderPackage[] = "com.google.android.providers.media.module";

#if defined(__aarch64__)
constexpr char kAbi[] = "arm64-v8a";
#elif defined(__arm__)
constexpr char kAbi[] = "armeabi-v7a";
#elif defined(__x86_64__)
constexpr char kAbi[] = "x86_64";
#elif defined(__i386__)
constexpr char kAbi[] = "x86";
#else
#error Unsupported Android ABI
#endif

int GetTargetProcess(JNIEnv* env, jstring niceName) {
    if (env == nullptr || niceName == nullptr) {
        return -1;
    }
    const char* processName = env->GetStringUTFChars(niceName, nullptr);
    if (processName == nullptr) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return -1;
    }
    const int target = strcmp(processName, kMediaProviderPackage) == 0         ? 0
                       : strcmp(processName, kGoogleMediaProviderPackage) == 0 ? 1
                                                                               : -1;
    env->ReleaseStringUTFChars(niceName, processName);
    return target;
}

bool PreloadModuleRuntime(zygisk::Api* api, JNIEnv* env) {
    if (gFuseHideHandle != nullptr) {
        return true;
    }
    if (api == nullptr || env == nullptr) {
        return false;
    }
    const int moduleDirFd = api->getModuleDir();
    if (moduleDirFd < 0) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "getModuleDir failed");
        return false;
    }

    const int dexDirFd = openat(moduleDirFd, kDexRelPath, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    char libraryPath[96];
    snprintf(libraryPath, sizeof(libraryPath), "lib/%s/libfusehide.so", kAbi);
    const int libraryFd = openat(moduleDirFd, libraryPath, O_RDONLY | O_CLOEXEC);
    close(moduleDirFd);

    if (dexDirFd < 0 || libraryFd < 0) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "failed to open module resources for abi=%s: %s", kAbi,
                            strerror(errno));
        if (dexDirFd >= 0) {
            close(dexDirFd);
        }
        if (libraryFd >= 0) {
            close(libraryFd);
        }
        return false;
    }

    const bool dexLoaded = PreloadInjectedDex(env, dexDirFd);
    close(dexDirFd);
    if (!dexLoaded) {
        close(libraryFd);
        return false;
    }

    android_dlextinfo extinfo{};
    extinfo.flags = ANDROID_DLEXT_USE_LIBRARY_FD;
    extinfo.library_fd = libraryFd;
    gFuseHideHandle = android_dlopen_ext(kFuseHideLibrary, RTLD_NOW | RTLD_LOCAL, &extinfo);
    close(libraryFd);
    if (gFuseHideHandle == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "preload libfusehide.so failed: %s",
                            dlerror());
        return false;
    }

    __android_log_print(ANDROID_LOG_INFO, kLogTag, "preloaded module runtime for abi=%s", kAbi);
    return true;
}

bool ShouldSkipForLsposedScope(zygisk::Api* api) {
    if (api == nullptr) {
        return false;
    }
    const int moduleDirFd = api->getModuleDir();
    if (moduleDirFd < 0) {
        return false;
    }
    const int flagFd = openat(moduleDirFd, kLsposedScopeFlag, O_RDONLY | O_CLOEXEC);
    close(moduleDirFd);
    if (flagFd < 0) {
        return false;
    }
    close(flagFd);
    __android_log_print(ANDROID_LOG_WARN, kLogTag,
                        "LSPosed scope flag detected; skip Zygisk backend");
    return true;
}

struct ScopeRequest {
    int32_t userId;
    int32_t target;
};

void SetScopeSocketTimeout(int fd) {
    const timeval timeout{1, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

void HandleScopeRequest(int client) {
    SetScopeSocketTimeout(client);
    ScopeRequest request{};
    auto scope = fusehide::zygisk::LsposedScope::kUnknown;
    if (recv(client, &request, sizeof(request), MSG_WAITALL) == sizeof(request) &&
        request.userId >= 0 && (request.target == 0 || request.target == 1)) {
        scope = fusehide::zygisk::QueryLsposedScope("libsqlite.so", kLsposedDatabase,
                                                    request.userId, request.target == 1);
    }
    send(client, &scope, sizeof(scope), MSG_NOSIGNAL);
}

bool ShouldSkipForLsposedDatabase(zygisk::Api* api, int uid, int target) {
    if (api == nullptr) {
        return false;
    }
    const int client = api->connectCompanion();
    auto scope = fusehide::zygisk::LsposedScope::kUnknown;
    if (client >= 0) {
        SetScopeSocketTimeout(client);
        const ScopeRequest request{uid / 100000, target};
        if (send(client, &request, sizeof(request), MSG_NOSIGNAL) != sizeof(request) ||
            recv(client, &scope, sizeof(scope), MSG_WAITALL) != sizeof(scope)) {
            scope = fusehide::zygisk::LsposedScope::kUnknown;
        }
        close(client);
    }
    if (scope == fusehide::zygisk::LsposedScope::kEnabled) {
        __android_log_print(ANDROID_LOG_INFO, kLogTag,
                            "LSPosed MediaProvider scope enabled for user=%d; skip Zygisk backend",
                            uid / 100000);
        return true;
    }
    if (scope != fusehide::zygisk::LsposedScope::kDisabled) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "LSPosed scope query unavailable; checking loaded native runtime");
    }
    return false;
}

}  // namespace

namespace {

class FuseHideZygiskModule : public zygisk::ModuleBase {
   public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        api_ = api;
        env->GetJavaVM(&gVm);
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        const int target = GetTargetProcess(gVmEnv(), args != nullptr ? args->nice_name : nullptr);
        shouldInject_ = target >= 0;
        if (!shouldInject_) {
            return;
        }
        if (ShouldSkipForLsposedScope(api_) ||
            ShouldSkipForLsposedDatabase(api_, args->uid, target)) {
            shouldInject_ = false;
            return;
        }
        if (!PreloadModuleRuntime(api_, gVmEnv())) {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                                "failed to preload module runtime for target process");
            shouldInject_ = false;
        } else {
            // Patch libnativeloader's import before it can load libfuse_jni.so.
            // Failure is reported as missing coverage, never silently accepted.
            InstallEarlyDlopenGotMonitor();
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs* /*args*/) override {
        if (!shouldInject_) {
            if (api_ != nullptr) {
                api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            }
            return;
        }
        InstallDlopenMonitor();
    }

    void postServerSpecialize(const zygisk::ServerSpecializeArgs* /*args*/) override {
        if (api_ != nullptr) {
            api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
        }
    }

   private:
    JNIEnv* gVmEnv() const {
        if (gVm == nullptr) {
            return nullptr;
        }
        JNIEnv* env = nullptr;
        if (gVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            return nullptr;
        }
        return env;
    }

    zygisk::Api* api_ = nullptr;
    bool shouldInject_ = false;
};

}  // namespace

REGISTER_ZYGISK_MODULE(FuseHideZygiskModule)
REGISTER_ZYGISK_COMPANION(HandleScopeRequest)
