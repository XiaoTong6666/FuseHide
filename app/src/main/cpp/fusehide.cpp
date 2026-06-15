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

#include "fusehide/core/state.hpp"
#include "fusehide/policy/path_policy.hpp"

namespace fusehide {

UHasBinaryPropertyFn gUHasBinaryProperty = u_hasBinaryProperty;
HookInstaller gHookInstaller = nullptr;
JavaVM* gJavaVm = nullptr;
std::once_flag gXzCrcInitOnce;
IsAppAccessiblePathFn gOriginalIsAppAccessiblePath = nullptr;
IsPackageOwnedPathFn gOriginalIsPackageOwnedPath = nullptr;
IsBpfBackingPathFn gOriginalIsBpfBackingPath = nullptr;
void* gOriginalStrcasecmp = nullptr;
void* gOriginalEqualsIgnoreCase = nullptr;

std::atomic<int> gAppAccessibleLogCount{0};
std::atomic<int> gPackageOwnedLogCount{0};
std::atomic<int> gBpfBackingLogCount{0};
std::atomic<int> gStrcasecmpLogCount{0};
std::atomic<int> gEqualsIgnoreCaseLogCount{0};
std::atomic<int> gReplyErrFallbackLogCount{0};
std::atomic<int> gErrnoRemapLogCount{0};
std::atomic<int> gSuspiciousDirectLogCount{0};
std::mutex gUidHideCacheMutex;
std::unordered_map<uint32_t, std::shared_ptr<const ResolvedHideRule>> gUidHideRuleCache;
std::shared_ptr<const HideConfig> gHideConfig = std::make_shared<HideConfig>(DefaultHideConfig());

namespace {

void AppendUnique(std::vector<std::string>* out, const std::vector<std::string>& values) {
    for (const auto& value : values) {
        if (std::find(out->begin(), out->end(), value) == out->end()) {
            out->push_back(value);
        }
    }
}

void MergeGlobalRule(const HideConfig& config, ResolvedHideRule* out) {
    out->enableHideAllRootEntries =
        out->enableHideAllRootEntries || config.enableHideAllRootEntries;
    AppendUnique(&out->hideAllRootEntriesExemptions, config.hideAllRootEntriesExemptions);
    AppendUnique(&out->hiddenRootEntryNames, config.hiddenRootEntryNames);
    AppendUnique(&out->hiddenRelativePaths, config.hiddenRelativePaths);
}

void MergePackageRule(const PackageHideRule& rule, ResolvedHideRule* out) {
    AppendUnique(&out->hiddenRootEntryNames, rule.hiddenRootEntryNames);
    AppendUnique(&out->hiddenRelativePaths, rule.hiddenRelativePaths);
}

bool RuleHasTargets(const ResolvedHideRule& rule) {
    return rule.enableHideAllRootEntries || !rule.hiddenRootEntryNames.empty() ||
           !rule.hiddenRelativePaths.empty();
}

void InvalidateTrackedHideTargetsForCurrentConfig() {
    ScheduleHiddenEntryInvalidation();

    std::vector<uint64_t> inodesToInvalidate;
    {
        std::lock_guard<std::mutex> lock(gInodePathCacheMutex);
        inodesToInvalidate.reserve(gInodePathCache.size());
        for (const auto& [ino, trackedPath] : gInodePathCache) {
            if (HiddenPathPolicy::IsAnyHiddenSubtreePath(trackedPath)) {
                inodesToInvalidate.push_back(ino);
            }
        }
    }
    for (const uint64_t ino : inodesToInvalidate) {
        RuntimeState::ScheduleHiddenInodeInvalidation(ino);
    }

    {
        std::lock_guard<std::mutex> lock(gHiddenSubtreeInodesMutex);
        gHiddenSubtreeInodes.clear();
    }
    {
        std::lock_guard<std::mutex> lock(gRecentHiddenParentPathsMutex);
        gRecentHiddenParentPaths.clear();
        gRecentHiddenParentPathUids.clear();
        gRecentHiddenParentPathAnyUid.clear();
        gRecentHiddenParentPathAnyUidOwner = 0;
    }
}

}  // namespace

HideConfig DefaultHideConfig() {
    HideConfig config;
    config.enableHideAllRootEntries = kEnableHideAllRootEntries;
    for (const auto& value : kHideAllRootEntriesExemptions) {
        config.hideAllRootEntriesExemptions.emplace_back(value);
    }
    for (const auto& value : kHiddenRootEntryNames) {
        config.hiddenRootEntryNames.emplace_back(value);
    }
    for (const auto& value : kHiddenRelativePaths) {
        config.hiddenRelativePaths.emplace_back(value);
    }
    for (const auto& value : kHiddenPackages) {
        config.hiddenPackages.emplace_back(value);
    }
    PackageHideRule fuseHideRule;
    fuseHideRule.packageName = std::string(kFuseHidePackageRulePackages[0]);
    for (const auto& value : kFuseHidePackageRuleRootEntries0) {
        fuseHideRule.hiddenRootEntryNames.emplace_back(value);
    }
    config.packageRules.emplace_back(std::move(fuseHideRule));

    PackageHideRule duckDetectorRule;
    duckDetectorRule.packageName = std::string(kFuseHidePackageRulePackages[1]);
    for (const auto& value : kFuseHidePackageRuleRootEntries1) {
        duckDetectorRule.hiddenRootEntryNames.emplace_back(value);
    }
    config.packageRules.emplace_back(std::move(duckDetectorRule));
    return config;
}

std::shared_ptr<const HideConfig> CurrentHideConfig() {
    return std::atomic_load_explicit(&gHideConfig, std::memory_order_acquire);
}

void ApplyHideConfig(HideConfig config) {
    auto next = std::make_shared<const HideConfig>(std::move(config));
    std::atomic_store_explicit(&gHideConfig, std::move(next), std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(gUidHideCacheMutex);
        gUidHideRuleCache.clear();
    }
    InvalidateTrackedHideTargetsForCurrentConfig();
    DebugLogPrint(4,
                  "applied hide config hide_all=%d exemptions=%zu roots=%zu packages=%zu "
                  "package_rules=%zu",
                  CurrentHideConfig()->enableHideAllRootEntries ? 1 : 0,
                  CurrentHideConfig()->hideAllRootEntriesExemptions.size(),
                  CurrentHideConfig()->hiddenRootEntryNames.size(),
                  CurrentHideConfig()->hiddenPackages.size(),
                  CurrentHideConfig()->packageRules.size());
}

bool IsHiddenPackageName(std::string_view packageName) {
    const auto config = CurrentHideConfig();
    for (const auto& hiddenPackage : config->hiddenPackages) {
        if (packageName == hiddenPackage) {
            return true;
        }
    }
    return false;
}

std::shared_ptr<const ResolvedHideRule> RuleForAnyPackage() {
    static std::mutex cacheMutex;
    static std::shared_ptr<const HideConfig> cachedConfig;
    static std::shared_ptr<const ResolvedHideRule> cachedRule;

    const auto config = CurrentHideConfig();
    std::lock_guard<std::mutex> lock(cacheMutex);
    if (cachedConfig == config) {
        return cachedRule;
    }

    ResolvedHideRule merged;
    MergeGlobalRule(*config, &merged);
    for (const auto& packageRule : config->packageRules) {
        MergePackageRule(packageRule, &merged);
    }
    cachedConfig = config;
    cachedRule = RuleHasTargets(merged)
                     ? std::make_shared<const ResolvedHideRule>(std::move(merged))
                     : nullptr;
    return cachedRule;
}

namespace {

// Resolve uid -> packages inside the already-hooked MediaProvider process.
// We intentionally stay inside the current process and ask PackageManager instead of adding
// a separate framework hook in system_server. The injection entry point is Entry.java, which only
// loads this library into MediaProvider, so currentApplication() is available here.
// AOSP reference for the uid flowing into FUSE handlers: jni/FuseDaemon.cpp#1134 and #2121
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1134
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#2121
JNIEnv* GetJniEnv(bool* didAttach) {
    if (didAttach != nullptr) {
        *didAttach = false;
    }
    if (gJavaVm == nullptr) {
        return nullptr;
    }
    JNIEnv* env = nullptr;
    const jint status = gJavaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_OK) {
        return env;
    }
    if (status != JNI_EDETACHED) {
        return nullptr;
    }
    if (gJavaVm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
        return nullptr;
    }
    if (didAttach != nullptr) {
        *didAttach = true;
    }
    return env;
}

}  // namespace

std::shared_ptr<const ResolvedHideRule> ResolveHideRuleForUid(uint32_t uid) {
    {
        std::lock_guard<std::mutex> lock(gUidHideCacheMutex);
        const auto it = gUidHideRuleCache.find(uid);
        if (it != gUidHideRuleCache.end()) {
            return it->second;
        }
    }

    auto resolved = ResolveHideRuleForUidWithPackageManager(uid);
    if (!resolved.has_value()) {
        return nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(gUidHideCacheMutex);
        gUidHideRuleCache[uid] = *resolved;
    }
    return *resolved;
}

// Query PackageManager once per uid and cache the merged package rule for hot FUSE paths.
std::optional<std::shared_ptr<const ResolvedHideRule>> ResolveHideRuleForUidWithPackageManager(
    uint32_t uid) {
    bool didAttach = false;
    JNIEnv* env = GetJniEnv(&didAttach);
    if (env == nullptr) {
        return std::nullopt;
    }

    auto finish = [&](std::optional<std::shared_ptr<const ResolvedHideRule>> value) {
        if (didAttach) {
            gJavaVm->DetachCurrentThread();
        }
        return value;
    };

    jclass activityThreadClass = env->FindClass("android/app/ActivityThread");
    if (activityThreadClass == nullptr || env->ExceptionCheck()) {
        env->ExceptionClear();
        return finish(std::nullopt);
    }
    jmethodID currentApplication = env->GetStaticMethodID(activityThreadClass, "currentApplication",
                                                          "()Landroid/app/Application;");
    if (currentApplication == nullptr || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(activityThreadClass);
        return finish(std::nullopt);
    }
    jobject application = env->CallStaticObjectMethod(activityThreadClass, currentApplication);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(activityThreadClass);
        return finish(std::nullopt);
    }
    env->DeleteLocalRef(activityThreadClass);
    if (application == nullptr) {
        return finish(std::nullopt);
    }

    jclass applicationClass = env->GetObjectClass(application);
    jmethodID getPackageManager = env->GetMethodID(applicationClass, "getPackageManager",
                                                   "()Landroid/content/pm/PackageManager;");
    if (getPackageManager == nullptr || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(applicationClass);
        env->DeleteLocalRef(application);
        return finish(std::nullopt);
    }
    jobject packageManager = env->CallObjectMethod(application, getPackageManager);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(applicationClass);
        env->DeleteLocalRef(application);
        return finish(std::nullopt);
    }
    env->DeleteLocalRef(applicationClass);
    env->DeleteLocalRef(application);
    if (packageManager == nullptr) {
        return finish(std::nullopt);
    }

    jclass packageManagerClass = env->FindClass("android/content/pm/PackageManager");
    jmethodID getPackagesForUid =
        env->GetMethodID(packageManagerClass, "getPackagesForUid", "(I)[Ljava/lang/String;");
    if (getPackagesForUid == nullptr || env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(packageManagerClass);
        env->DeleteLocalRef(packageManager);
        return finish(std::nullopt);
    }

    jobjectArray packages = static_cast<jobjectArray>(
        env->CallObjectMethod(packageManager, getPackagesForUid, (jint)uid));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(packageManagerClass);
        env->DeleteLocalRef(packageManager);
        return finish(std::nullopt);
    }
    env->DeleteLocalRef(packageManagerClass);
    env->DeleteLocalRef(packageManager);

    auto merged = std::make_unique<ResolvedHideRule>();
    const auto config = CurrentHideConfig();
    if (packages != nullptr) {
        const jsize count = env->GetArrayLength(packages);
        for (jsize i = 0; i < count; ++i) {
            jstring packageName = static_cast<jstring>(env->GetObjectArrayElement(packages, i));
            if (packageName == nullptr) {
                continue;
            }
            const char* packageNameChars = env->GetStringUTFChars(packageName, nullptr);
            if (packageNameChars != nullptr) {
                const std::string_view packageNameView(packageNameChars);
                if (IsHiddenPackageName(packageNameView)) {
                    MergeGlobalRule(*config, merged.get());
                }
                for (const auto& packageRule : config->packageRules) {
                    if (packageNameView == packageRule.packageName) {
                        MergePackageRule(packageRule, merged.get());
                    }
                }
                env->ReleaseStringUTFChars(packageName, packageNameChars);
            }
            env->DeleteLocalRef(packageName);
        }
        env->DeleteLocalRef(packages);
    }
    if (!RuleHasTargets(*merged)) {
        DebugLogPrint(4, "resolved uid=%u hide=0", static_cast<unsigned>(uid));
        return finish(std::shared_ptr<const ResolvedHideRule>(nullptr));
    }
    DebugLogPrint(4, "resolved uid=%u hide=1 roots=%zu rel=%zu hide_all=%d",
                  static_cast<unsigned>(uid), merged->hiddenRootEntryNames.size(),
                  merged->hiddenRelativePaths.size(), merged->enableHideAllRootEntries ? 1 : 0);
    return finish(std::shared_ptr<const ResolvedHideRule>(merged.release()));
}

}  // namespace fusehide
