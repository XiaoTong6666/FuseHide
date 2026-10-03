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

#include "fusehide/hooks/wrappers.hpp"
#include "fusehide/native_callback_gate.hpp"

#include <atomic>

namespace {
enum class NativeModuleInitState : uint8_t {
    kUninitialized,
    kInitializingV2,
    kInitializingV3,
    kReadyV2,
    kReadyV3,
};

std::atomic<NativeModuleInitState> gNativeModuleInitState{NativeModuleInitState::kUninitialized};
int (*gNativeOwnerUnhook)(void*) = nullptr;
fusehide::NativeCallbackGate gNativeCallbackGate;

bool BeginNativeModuleInit(NativeModuleInitState initializing, NativeModuleInitState ready) {
    auto expected = NativeModuleInitState::kUninitialized;
    if (gNativeModuleInitState.compare_exchange_strong(expected, initializing,
                                                       std::memory_order_acq_rel)) {
        return true;
    }
    if (expected != ready) {
        // Do not let another native framework overwrite a live host's hook
        // pointers or downgrade the strict v3 publisher to legacy v2.
        __android_log_print(ANDROID_LOG_ERROR, fusehide::kLogTag,
                            "native_init rejected conflicting or unfinished API binding");
    }
    return false;
}
}  // namespace

extern "C" void PostNativeInit(const char* loadedLibrary, void*) {
    if (!gNativeCallbackGate.TryEnter())
        return;
    struct CallbackExit {
        ~CallbackExit() {
            gNativeCallbackGate.Exit();
        }
    } exit;
    if (loadedLibrary == nullptr ||
        std::strstr(loadedLibrary, fusehide::kTargetLibrary) == nullptr) {
        return;
    }
    fusehide::InstallFuseHooks();
}

extern "C" void fusehide_close_loader_callback_admission() {
    gNativeCallbackGate.CloseAdmission();
}

extern "C" bool fusehide_finish_loader_callback_drain() {
    return gNativeCallbackGate.FinishDrain();
}

extern "C" uint32_t fusehide_loader_callback_in_flight() {
    return gNativeCallbackGate.InFlight();
}

std::vector<std::string> JStringArrayToVector(JNIEnv* env, jobjectArray values) {
    std::vector<std::string> out;
    if (env == nullptr || values == nullptr) {
        return out;
    }
    const jsize count = env->GetArrayLength(values);
    out.reserve(static_cast<size_t>(count));
    for (jsize i = 0; i < count; ++i) {
        jstring value = static_cast<jstring>(env->GetObjectArrayElement(values, i));
        if (value == nullptr) {
            continue;
        }
        const char* chars = env->GetStringUTFChars(value, nullptr);
        if (chars != nullptr) {
            out.emplace_back(chars);
            env->ReleaseStringUTFChars(value, chars);
        }
        env->DeleteLocalRef(value);
    }
    return out;
}

std::vector<std::string> SplitLines(std::string_view text) {
    std::vector<std::string> out;
    size_t begin = 0;
    while (begin <= text.size()) {
        const size_t end = text.find('\n', begin);
        std::string_view line =
            end == std::string_view::npos ? text.substr(begin) : text.substr(begin, end - begin);
        while (!line.empty() &&
               (line.front() == ' ' || line.front() == '\t' || line.front() == '\r')) {
            line.remove_prefix(1);
        }
        while (!line.empty() &&
               (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.remove_suffix(1);
        }
        if (!line.empty()) {
            out.emplace_back(line);
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return out;
}

jobjectArray VectorToJavaStringArray(JNIEnv* env, const std::vector<std::string>& values) {
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray array =
        env->NewObjectArray(static_cast<jsize>(values.size()), stringClass, nullptr);
    for (jsize i = 0; i < static_cast<jsize>(values.size()); ++i) {
        jstring value = env->NewStringUTF(values[static_cast<size_t>(i)].c_str());
        env->SetObjectArrayElement(array, i, value);
        env->DeleteLocalRef(value);
    }
    env->DeleteLocalRef(stringClass);
    return array;
}

std::vector<std::string> PackageRulePackages(const fusehide::HideConfig& config) {
    std::vector<std::string> out;
    out.reserve(config.packageRules.size());
    for (const auto& rule : config.packageRules) {
        out.push_back(rule.packageName);
    }
    return out;
}

std::string JoinLines(const std::vector<std::string>& values) {
    std::string out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out.push_back('\n');
        }
        out.append(values[i]);
    }
    return out;
}

std::vector<std::string> PackageRuleRootEntries(const fusehide::HideConfig& config) {
    std::vector<std::string> out;
    out.reserve(config.packageRules.size());
    for (const auto& rule : config.packageRules) {
        out.push_back(JoinLines(rule.hiddenRootEntryNames));
    }
    return out;
}

std::vector<std::string> PackageRuleRelativePaths(const fusehide::HideConfig& config) {
    std::vector<std::string> out;
    out.reserve(config.packageRules.size());
    for (const auto& rule : config.packageRules) {
        out.push_back(JoinLines(rule.hiddenRelativePaths));
    }
    return out;
}

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    fusehide::gJavaVm = vm;
    return JNI_VERSION_1_6;
}

JNIEXPORT jboolean JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_getDefaultEnableHideAllRootEntries(
    JNIEnv*, jclass) {
    return fusehide::DefaultHideConfig().enableHideAllRootEntries ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobjectArray JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_getDefaultHideAllRootEntriesExemptions(
    JNIEnv* env, jclass) {
    return VectorToJavaStringArray(env, fusehide::DefaultHideConfig().hideAllRootEntriesExemptions);
}

JNIEXPORT jobjectArray JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_getDefaultHiddenRootEntryNames(
    JNIEnv* env, jclass) {
    return VectorToJavaStringArray(env, fusehide::DefaultHideConfig().hiddenRootEntryNames);
}

JNIEXPORT jobjectArray JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_getDefaultHiddenRelativePaths(
    JNIEnv* env, jclass) {
    return VectorToJavaStringArray(env, fusehide::DefaultHideConfig().hiddenRelativePaths);
}

JNIEXPORT jobjectArray JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_getDefaultHiddenPackages(
    JNIEnv* env, jclass) {
    return VectorToJavaStringArray(env, fusehide::DefaultHideConfig().hiddenPackages);
}

JNIEXPORT jboolean JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_getCurrentEnableHideAllRootEntries(
    JNIEnv*, jclass) {
    return fusehide::CurrentHideConfig()->enableHideAllRootEntries ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobjectArray JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_getCurrentHideAllRootEntriesExemptions(
    JNIEnv* env, jclass) {
    return VectorToJavaStringArray(env,
                                   fusehide::CurrentHideConfig()->hideAllRootEntriesExemptions);
}

JNIEXPORT jobjectArray JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_getCurrentHiddenRootEntryNames(
    JNIEnv* env, jclass) {
    return VectorToJavaStringArray(env, fusehide::CurrentHideConfig()->hiddenRootEntryNames);
}

JNIEXPORT jobjectArray JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_getCurrentHiddenRelativePaths(
    JNIEnv* env, jclass) {
    return VectorToJavaStringArray(env, fusehide::CurrentHideConfig()->hiddenRelativePaths);
}

JNIEXPORT jobjectArray JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_getCurrentHiddenPackages(
    JNIEnv* env, jclass) {
    return VectorToJavaStringArray(env, fusehide::CurrentHideConfig()->hiddenPackages);
}

JNIEXPORT jobjectArray JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_getCurrentPackageRulePackages(
    JNIEnv* env, jclass) {
    return VectorToJavaStringArray(env, PackageRulePackages(*fusehide::CurrentHideConfig()));
}

JNIEXPORT jobjectArray JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_getCurrentPackageRuleHiddenRootEntryNames(
    JNIEnv* env, jclass) {
    return VectorToJavaStringArray(env, PackageRuleRootEntries(*fusehide::CurrentHideConfig()));
}

JNIEXPORT jobjectArray JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_getCurrentPackageRuleHiddenRelativePaths(
    JNIEnv* env, jclass) {
    return VectorToJavaStringArray(env, PackageRuleRelativePaths(*fusehide::CurrentHideConfig()));
}

JNIEXPORT void JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_applyHideConfig(
    JNIEnv* env, jclass, jboolean enableHideAllRootEntries,
    jobjectArray hideAllRootEntriesExemptions, jobjectArray hiddenRootEntryNames,
    jobjectArray hiddenRelativePaths, jobjectArray hiddenPackages, jobjectArray packageRulePackages,
    jobjectArray packageRuleHiddenRootEntryNames, jobjectArray packageRuleHiddenRelativePaths) {
    fusehide::HideConfig config;
    config.enableHideAllRootEntries = enableHideAllRootEntries == JNI_TRUE;
    config.hideAllRootEntriesExemptions = JStringArrayToVector(env, hideAllRootEntriesExemptions);
    config.hiddenRootEntryNames = JStringArrayToVector(env, hiddenRootEntryNames);
    config.hiddenRelativePaths = JStringArrayToVector(env, hiddenRelativePaths);
    config.hiddenPackages = JStringArrayToVector(env, hiddenPackages);
    const auto packages = JStringArrayToVector(env, packageRulePackages);
    const auto rootNamesByPackage = JStringArrayToVector(env, packageRuleHiddenRootEntryNames);
    const auto relativePathsByPackage = JStringArrayToVector(env, packageRuleHiddenRelativePaths);
    for (size_t i = 0; i < packages.size(); ++i) {
        fusehide::PackageHideRule rule;
        rule.packageName = packages[i];
        if (i < rootNamesByPackage.size()) {
            rule.hiddenRootEntryNames = SplitLines(rootNamesByPackage[i]);
        }
        if (i < relativePathsByPackage.size()) {
            rule.hiddenRelativePaths = SplitLines(relativePathsByPackage[i]);
        }
        if (!rule.packageName.empty() &&
            (!rule.hiddenRootEntryNames.empty() || !rule.hiddenRelativePaths.empty())) {
            config.packageRules.emplace_back(std::move(rule));
        }
    }
    fusehide::ApplyHideConfig(std::move(config));
}

// Package add/remove is observed from the injected MediaProvider process and forwarded here so the
// native uid-rule caches track PackageManager-visible changes without a full config reload.
JNIEXPORT void JNICALL
Java_io_github_xiaotong6666_fusehide_config_HideConfigNativeBridge_notifyPackageSetChanged(
    JNIEnv* env, jclass, jstring reason) {
    std::string nativeReason;
    if (env != nullptr && reason != nullptr) {
        const char* chars = env->GetStringUTFChars(reason, nullptr);
        if (chars != nullptr) {
            nativeReason.assign(chars);
            env->ReleaseStringUTFChars(reason, chars);
        }
    }
    if (nativeReason.empty()) {
        nativeReason = "package_set_changed";
    }
    fusehide::NotifyUidRulePackageSetChanged(nativeReason);
}

JNIEXPORT jint JNICALL Java_io_github_xiaotong6666_fusehide_debug_Utils_rmdir(JNIEnv* env,
                                                                              jclass clazz,
                                                                              jstring path) {
    (void)clazz;
    const char* c_path = env->GetStringUTFChars(path, nullptr);

    jint ret = rmdir(c_path);
    if (ret != 0)
        ret = errno;
    else
        ret = 0;
    env->ReleaseStringUTFChars(path, c_path);
    return ret;
}

JNIEXPORT jint JNICALL Java_io_github_xiaotong6666_fusehide_debug_Utils_unlink(JNIEnv* env,
                                                                               jclass clazz,
                                                                               jstring path) {
    (void)clazz;
    const char* c_path = env->GetStringUTFChars(path, nullptr);

    jint ret = unlink(c_path);
    if (ret != 0)
        ret = errno;
    else
        ret = 0;
    env->ReleaseStringUTFChars(path, c_path);
    return ret;
}

JNIEXPORT jint JNICALL Java_io_github_xiaotong6666_fusehide_debug_Utils_mkdir(JNIEnv* env,
                                                                              jclass clazz,
                                                                              jstring path) {
    (void)clazz;
    const char* c_path = env->GetStringUTFChars(path, nullptr);

    jint ret = mkdir(c_path, 0777);
    if (ret != 0)
        ret = errno;
    else
        ret = 0;
    env->ReleaseStringUTFChars(path, c_path);
    return ret;
}

JNIEXPORT jint JNICALL Java_io_github_xiaotong6666_fusehide_debug_Utils_rename(JNIEnv* env,
                                                                               jclass clazz,
                                                                               jstring old_path,
                                                                               jstring new_path) {
    (void)clazz;
    const char* c_old_path = env->GetStringUTFChars(old_path, nullptr);
    const char* c_new_path = env->GetStringUTFChars(new_path, nullptr);

    jint ret = rename(c_old_path, c_new_path);
    if (ret != 0)
        ret = errno;
    else
        ret = 0;
    env->ReleaseStringUTFChars(old_path, c_old_path);
    env->ReleaseStringUTFChars(new_path, c_new_path);
    return ret;
}

JNIEXPORT jint JNICALL Java_io_github_xiaotong6666_fusehide_debug_Utils_create(JNIEnv* env,
                                                                               jclass clazz,
                                                                               jstring path) {
    (void)clazz;
    const char* c_path = env->GetStringUTFChars(path, nullptr);

    const int fd = open(c_path, O_CREAT | O_EXCL | O_CLOEXEC | O_RDWR, 0666);
    jint ret = 0;
    if (fd < 0) {
        ret = errno;
    } else {
        close(fd);
    }
    env->ReleaseStringUTFChars(path, c_path);
    return ret;
}

}  // extern "C"

extern "C" __attribute__((visibility("default"))) void* native_init(void* api) {
    const auto* entries = static_cast<const fusehide::NativeApiEntries*>(api);
    // Legacy Native API v2 has historically allowed hosts without Unhook.
    // Preserve that compatibility; process residency is mandatory either way.
    if (entries == nullptr || entries->version != 2 || entries->hookFunc == nullptr) {
        return nullptr;
    }
    if (!gNativeCallbackGate.TryEnter())
        return nullptr;
    struct InitExit {
        ~InitExit() {
            gNativeCallbackGate.Exit();
        }
    } init_exit;
    if (!BeginNativeModuleInit(NativeModuleInitState::kInitializingV2,
                               NativeModuleInitState::kReadyV2)) {
        const bool sameHost = gNativeModuleInitState.load(std::memory_order_acquire) ==
                                  NativeModuleInitState::kReadyV2 &&
                              fusehide::gHookInstaller == entries->hookFunc &&
                              gNativeOwnerUnhook == entries->unhookFunc;
        if (!sameHost)
            return nullptr;
        __android_log_print(
            ANDROID_LOG_INFO, fusehide::kLogTag,
            "native_init repeated for same v2 host; suppress duplicate callback registration");
        // Vector pushes any non-null returned callback into a process-lifetime
        // list. Returning the same address again would add a duplicate entry.
        return nullptr;
    }
    fusehide::gHookInstaller = entries->hookFunc;
    gNativeOwnerUnhook = entries->unhookFunc;
    fusehide::gStrictHookInstaller = nullptr;
    gNativeModuleInitState.store(NativeModuleInitState::kReadyV2, std::memory_order_release);
    __android_log_print(ANDROID_LOG_INFO, fusehide::kLogTag, "Native API v2 bound");
    return reinterpret_cast<void*>(+PostNativeInit);
}

// Only the owned FuseHide Zygisk loader calls this versioned entry. Vector or
// LSPosed continue using native_init(v2) without reading an unknown tail.
extern "C" __attribute__((visibility("default"))) void* native_init_v3(
    const fusehide::NativeApiEntriesV3* api) {
    if (!api || api->base.version != 3 || api->struct_size < sizeof(fusehide::NativeApiEntriesV3) ||
        api->reserved != 0 || !api->base.hookFunc || !api->base.unhookFunc ||
        !api->hookWithPublication) {
        return nullptr;
    }
    if (!gNativeCallbackGate.TryEnter())
        return nullptr;
    struct InitExit {
        ~InitExit() {
            gNativeCallbackGate.Exit();
        }
    } init_exit;
    if (!BeginNativeModuleInit(NativeModuleInitState::kInitializingV3,
                               NativeModuleInitState::kReadyV3)) {
        const bool sameHost = gNativeModuleInitState.load(std::memory_order_acquire) ==
                                  NativeModuleInitState::kReadyV3 &&
                              fusehide::gHookInstaller == api->base.hookFunc &&
                              gNativeOwnerUnhook == api->base.unhookFunc &&
                              fusehide::gStrictHookInstaller == api->hookWithPublication;
        if (!sameHost)
            return nullptr;
        __android_log_print(
            ANDROID_LOG_INFO, fusehide::kLogTag,
            "native_init repeated for same v3 host; suppress duplicate callback registration");
        return nullptr;
    }
    fusehide::gHookInstaller = api->base.hookFunc;
    gNativeOwnerUnhook = api->base.unhookFunc;
    fusehide::gStrictHookInstaller = api->hookWithPublication;
    gNativeModuleInitState.store(NativeModuleInitState::kReadyV3, std::memory_order_release);
    __android_log_print(ANDROID_LOG_INFO, fusehide::kLogTag,
                        "Native API v3 strict publication enabled");
    return reinterpret_cast<void*>(+PostNativeInit);
}
