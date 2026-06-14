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

#pragma once

#include <android/log.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct fuse_session {};
struct fuse_req {
    struct fuse_session* se;
    uint64_t unique;
};
typedef struct fuse_req* fuse_req_t;

struct fuse_entry_param {
    uint64_t ino;
    uint64_t generation;
    struct stat attr;
    double attr_timeout;
    double entry_timeout;
    uint64_t backing_action;
    uint64_t backing_fd;
    uint64_t bpf_action;
    uint64_t bpf_fd;
};

struct fuse_entry_out;
struct fuse_entry_bpf_out;
struct fuse_dirent {
    uint64_t ino;
    uint64_t off;
    uint32_t namelen;
    uint32_t type;
    char name[];
};
struct fuse_read_out {
    uint64_t offset;
    uint32_t size;
    uint32_t padding;
};

namespace mediaprovider {
namespace fuse {

struct DirectoryEntry {
    DirectoryEntry(const std::string& name, int type) : d_name(name), d_type(type) {
    }
    const std::string d_name;
    const int d_type;
};

}  // namespace fuse
}  // namespace mediaprovider

namespace fusehide {

inline constexpr uint32_t kFuseHidePayloadAbiVersion = 1;
inline constexpr uint32_t kFuseHidePayloadAbiMinSupportedVersion = 1;
inline constexpr uint32_t kFuseHidePayloadAbiMaxSupportedVersion = 1;

inline constexpr const char* kLogTag = "FuseHide";
inline constexpr const char* kTargetLibrary = "libfuse_jni.so";
inline constexpr std::string_view kVisibleStorageRoots[] = {"/storage/emulated/0"};
// Optional stress mode: when enabled, treat every first-level entry under kVisibleStorageRoots
// as hidden for test UIDs. Keep disabled by default to avoid breaking normal app behavior.
inline constexpr bool kEnableHideAllRootEntries = false;
// Entries listed here remain visible even when kEnableHideAllRootEntries is enabled.
// Keeping Android visible avoids breaking /sdcard/Android/data and /sdcard/Android/obb.
inline constexpr std::string_view kHideAllRootEntriesExemptions[] = {
    "Android", "DCIM", "Document", "Download", "Movies", "Pictures",
};
inline constexpr std::string_view kHiddenRootEntryNames[] = {
    "su",
    "daemonsu",
};
inline constexpr std::string_view kHiddenRelativePaths[] = {};
inline constexpr std::string_view kHiddenPackages[] = {
    "com.eltavine.duckdetector",
    "io.github.xiaotong6666.fusehide",
    "io.github.a13e300.fusefixer",
};
inline constexpr std::string_view kFuseHidePackageRulePackages[] = {
    "io.github.xiaotong6666.fusehide",
    "com.eltavine.duckdetector",
};
inline constexpr std::string_view kFuseHidePackageRuleRootEntries0[] = {
    "xinhao",
};
inline constexpr std::string_view kFuseHidePackageRuleRootEntries1[] = {
    "MT2",
    "xinhao",
};

#if defined(NDEBUG)
inline constexpr bool kEnableDebugHooks = false;
#else
inline constexpr bool kEnableDebugHooks = true;
#endif

using UHasBinaryPropertyFn = int8_t (*)(uint32_t codePoint, int32_t which);
extern "C" int8_t u_hasBinaryProperty(uint32_t codePoint, int32_t which);

inline constexpr int32_t kUCHAR_DEFAULT_IGNORABLE_CODE_POINT = 5;

using HookInstaller = int (*)(void* target, void* replacement, void** backup);
using IsAppAccessiblePathFn = bool (*)(void* fuse, const std::string& path, uint32_t uid);
using IsPackageOwnedPathFn = bool (*)(const std::string& lhs, const std::string& rhs);
using IsBpfBackingPathFn = bool (*)(const std::string& path);
using ShouldNotCacheFn = bool (*)(void* fuse, const std::string& path);
using FuseReplyErrFn = int (*)(fuse_req_t, int);
using DirectoryEntries = std::vector<std::shared_ptr<mediaprovider::fuse::DirectoryEntry>>;
using GetDirectoryEntriesFn = DirectoryEntries (*)(void* wrapper, uint32_t uid,
                                                   const std::string& path, DIR* dirp);
using LowerFsDirentFilterFn = bool (*)(const dirent& entry);
using AddDirectoryEntriesFromLowerFsFn = void (*)(DIR* dirp, LowerFsDirentFilterFn filter,
                                                  DirectoryEntries* entries);

struct PackageHideRule {
    std::string packageName;
    std::vector<std::string> hiddenRootEntryNames;
    std::vector<std::string> hiddenRelativePaths;
};

struct HideConfig {
    bool enableHideAllRootEntries = false;
    std::vector<std::string> hideAllRootEntriesExemptions;
    std::vector<std::string> hiddenRootEntryNames;
    std::vector<std::string> hiddenRelativePaths;
    std::vector<std::string> hiddenPackages;
    std::vector<PackageHideRule> packageRules;
};

struct ResolvedHideRule {
    bool enableHideAllRootEntries = false;
    std::vector<std::string> hideAllRootEntriesExemptions;
    std::vector<std::string> hiddenRootEntryNames;
    std::vector<std::string> hiddenRelativePaths;
};

// These RVAs are device-specific addresses from reverse-engineered libfuse_jni.so builds.
// The production device library we analyzed is stripped, so internal helpers such as
// is_app_accessible_path and several pf_* handlers are not always recoverable by name from the
// shipped ELF. Keep these offsets only as a last resort after symbol-based lookup fails, and
// select among known profiles at runtime instead of baking one device's layout globally.
struct DeviceHookProfile {
    const char* name;
    uintptr_t isAppAccessiblePathOffset;
    uintptr_t pfLookupOffset;
    uintptr_t pfLookupPostfilterOffset;
    uintptr_t pfGetattrOffset;
    uintptr_t shouldNotCacheOffset;
    uintptr_t doReaddirCommonOffset;
    uintptr_t getDirectoryEntriesOffset;
    uintptr_t addDirectoryEntriesFromLowerFsOffset;
    uintptr_t addDirectoryEntriesFromLowerFsThunkOffset;
    uintptr_t pfMkdirOffset;
    uintptr_t pfMknodOffset;
    uintptr_t pfUnlinkOffset;
    uintptr_t pfRmdirOffset;
    uintptr_t pfRenameOffset;
    uintptr_t pfCreateOffset;
    uintptr_t pfReaddirOffset;
    uintptr_t pfReaddirPostfilterOffset;
    uintptr_t pfReaddirplusOffset;
};

// Reverse-engineered record: ShouldNotCache @ 0x0017dc64, do_readdir_common @ 0x0018036c,
// GetDirectoryEntries @ 0x0018a3ec, addDirectoryEntriesFromLowerFs @ 0x0018be00,
// thunk @ 0x001fdcf8 in Ghidra with image base 0x00100000.
inline constexpr DeviceHookProfile kDeviceHookProfileLegacy = {
    .name = "legacy_device",
    .isAppAccessiblePathOffset = 0x0007bb5c,
    .pfLookupOffset = 0x00075e48,
    .pfLookupPostfilterOffset = 0x00075f90,
    .pfGetattrOffset = 0x000762bc,
    .shouldNotCacheOffset = 0x0007dc64,
    .doReaddirCommonOffset = 0x0008036c,
    .getDirectoryEntriesOffset = 0x0008a3ec,
    .addDirectoryEntriesFromLowerFsOffset = 0x0008be00,
    .addDirectoryEntriesFromLowerFsThunkOffset = 0x000fdcf8,
    .pfMkdirOffset = 0x00077050,
    .pfMknodOffset = 0x00076ba8,
    .pfUnlinkOffset = 0x00077534,
    .pfRmdirOffset = 0x00077920,
    .pfRenameOffset = 0x00077ef4,
    .pfCreateOffset = 0x0007a7c8,
    .pfReaddirOffset = 0x00079c40,
    .pfReaddirPostfilterOffset = 0x00079cac,
    .pfReaddirplusOffset = 0x0007b320,
};

// Reverse-engineered record: ShouldNotCache @ 0x001eb658, do_readdir_common @ 0x001ee694,
// GetDirectoryEntries @ 0x001f8838, addDirectoryEntriesFromLowerFs @ 0x001fa180,
// thunk @ 0x002073d0 in Ghidra with image base 0x00100000.
inline constexpr DeviceHookProfile kDeviceHookProfileBp2a = {
    .name = "bp2a_device",
    .isAppAccessiblePathOffset = 0x000e9b94,
    .pfLookupOffset = 0x000e3f94,
    .pfLookupPostfilterOffset = 0x000e40e4,
    .pfGetattrOffset = 0x000e4418,
    .shouldNotCacheOffset = 0x000eb658,
    .doReaddirCommonOffset = 0x000ee694,
    .getDirectoryEntriesOffset = 0x000f8838,
    .addDirectoryEntriesFromLowerFsOffset = 0x000fa180,
    .addDirectoryEntriesFromLowerFsThunkOffset = 0x001073d0,
    .pfMkdirOffset = 0x000e5250,
    .pfMknodOffset = 0x000e4d80,
    .pfUnlinkOffset = 0x000e573c,
    .pfRmdirOffset = 0x000e5b64,
    .pfRenameOffset = 0x000e60b0,
    .pfCreateOffset = 0x000e8890,
    .pfReaddirOffset = 0x000e7aa0,
    .pfReaddirPostfilterOffset = 0x000e7b0c,
    .pfReaddirplusOffset = 0x000e93c4,
};

inline constexpr DeviceHookProfile kDeviceHookProfileV3 = {
    .name = "v3_device",
    .isAppAccessiblePathOffset = 0x000e9df8,
    .pfLookupOffset = 0x000e3f18,
    .pfLookupPostfilterOffset = 0x000e4068,
    .pfGetattrOffset = 0x000e43a8,
    .shouldNotCacheOffset = 0x000eb96c,
    .doReaddirCommonOffset = 0x000ee9fc,
    .getDirectoryEntriesOffset = 0x000f8cac,
    .addDirectoryEntriesFromLowerFsOffset = 0x000fa5f4,
    .addDirectoryEntriesFromLowerFsThunkOffset = 0x001077e0,
    .pfMkdirOffset = 0x000e521c,
    .pfMknodOffset = 0x000e4d40,
    .pfUnlinkOffset = 0x000e5714,
    .pfRmdirOffset = 0x000e5b4c,
    .pfRenameOffset = 0x000e60cc,
    .pfCreateOffset = 0x000e8acc,
    .pfReaddirOffset = 0x000e7cb8,
    .pfReaddirPostfilterOffset = 0x000e7d24,
    .pfReaddirplusOffset = 0x000e961c,
};

inline constexpr DeviceHookProfile kDeviceHookProfileV6 = {
    .name = "v6_device",
    .isAppAccessiblePathOffset = 0x000e7620,
    .pfLookupOffset = 0x000e1aec,
    .pfLookupPostfilterOffset = 0x000e1c3c,
    .pfGetattrOffset = 0x000e1f70,
    .shouldNotCacheOffset = 0x000ec724,
    .doReaddirCommonOffset = 0x000ef6f4,
    .getDirectoryEntriesOffset = 0x000f9b98,
    .addDirectoryEntriesFromLowerFsOffset = 0x000fb488,
    .addDirectoryEntriesFromLowerFsThunkOffset = 0x001086f8,
    .pfMkdirOffset = 0x000e2d7c,
    .pfMknodOffset = 0x000e28d8,
    .pfUnlinkOffset = 0x000e323c,
    .pfRmdirOffset = 0x000e3640,
    .pfRenameOffset = 0x000e3b8c,
    .pfCreateOffset = 0x000e635c,
    .pfReaddirOffset = 0x000e557c,
    .pfReaddirPostfilterOffset = 0x000e55e8,
    .pfReaddirplusOffset = 0x000e6e84,
};

inline constexpr DeviceHookProfile kKnownDeviceHookProfiles[] = {
    kDeviceHookProfileLegacy,
    kDeviceHookProfileBp2a,
    kDeviceHookProfileV3,
    kDeviceHookProfileV6,
};
inline constexpr size_t kFuseEntryOutWireSize = 128;

struct NativeApiEntries {
    uint32_t version;
    HookInstaller hookFunc;
    void* unhookFunc;
};

extern HookInstaller gHookInstaller;
extern JavaVM* gJavaVm;
extern UHasBinaryPropertyFn gUHasBinaryProperty;
extern std::once_flag gXzCrcInitOnce;
extern IsPackageOwnedPathFn gOriginalIsPackageOwnedPath;
extern IsBpfBackingPathFn gOriginalIsBpfBackingPath;
extern void* gOriginalStrcasecmp;
extern void* gOriginalEqualsIgnoreCase;

extern std::atomic<int> gAppAccessibleLogCount;
extern std::atomic<int> gPackageOwnedLogCount;
extern std::atomic<int> gBpfBackingLogCount;
extern std::atomic<int> gStrcasecmpLogCount;
extern std::atomic<int> gEqualsIgnoreCaseLogCount;
extern std::atomic<int> gReplyErrFallbackLogCount;
extern std::atomic<int> gErrnoRemapLogCount;
extern std::atomic<int> gSuspiciousDirectLogCount;
extern std::mutex gUidHideCacheMutex;
extern std::unordered_map<uint32_t, std::shared_ptr<const ResolvedHideRule>> gUidHideRuleCache;
extern std::shared_ptr<const HideConfig> gHideConfig;

inline bool ShouldLogLimited(std::atomic<int>& counter, int limit = 8) {
    const int old = counter.fetch_add(1, std::memory_order_relaxed);
    return old < limit;
}

template <typename... Args>
inline void DebugLogPrint(int priority, const char* fmt, Args... args) {
    if constexpr (kEnableDebugHooks) {
        __android_log_print(priority, kLogTag, fmt, args...);
    }
}

std::shared_ptr<const ResolvedHideRule> ResolveHideRuleForUid(uint32_t uid);
std::optional<std::shared_ptr<const ResolvedHideRule>> ResolveHideRuleForUidWithPackageManager(
    uint32_t uid);
HideConfig DefaultHideConfig();
std::shared_ptr<const HideConfig> CurrentHideConfig();
void ApplyHideConfig(HideConfig config);
bool IsHiddenPackageName(std::string_view packageName);
std::shared_ptr<const ResolvedHideRule> RuleForAnyPackage();

class UnicodePolicy final {
   public:
    static std::string EscapeForLog(const uint8_t* data, size_t length);
    static std::string DebugPreview(std::string_view value, size_t limit = 96);
    static void LogInvalidUtf8(const uint8_t* data, size_t dataLen, size_t begin, size_t end);
    static void LogSuspiciousDirectPath(const char* hookName, std::string_view path);
    static bool DecodeUtf8CodePoint(const uint8_t* data, size_t len, size_t index, uint32_t* cp,
                                    size_t* width);
    static size_t InvalidUtf8SpanEnd(const uint8_t* data, size_t len, size_t index);
    static bool NeedsSanitization(const std::string& input);
    static void RewriteString(std::string& input);
    static int CompareCaseFoldIgnoringDefaultIgnorables(const uint8_t* lhsData, size_t lhsLen,
                                                        const uint8_t* rhsData, size_t rhsLen);
};

class RuntimeState final {
   public:
    static uint32_t ReqUid(fuse_req_t req);
    static void RememberFuseSession(fuse_req_t req);
    static void ScheduleHiddenEntryInvalidation();
    static void ScheduleSpecificEntryInvalidation(uint64_t parent, std::string_view name);
    static void ScheduleHiddenInodeInvalidation(uint64_t ino);
};

struct FuseHidePayloadApi {
    uint32_t abiVersion = kFuseHidePayloadAbiVersion;
    uint32_t abiMinSupportedVersion = kFuseHidePayloadAbiMinSupportedVersion;
    uint32_t abiMaxSupportedVersion = kFuseHidePayloadAbiMaxSupportedVersion;
    uint32_t structSize = sizeof(FuseHidePayloadApi);
    // ABI prefix rules:
    // 1. Only append new fields to the tail.
    // 2. Keep existing fields in this prefix stable and ordered.
    // 3. Behavior-bearing callbacks in this prefix are required for a valid payload.
    void (*pfLookup)(fuse_req_t req, uint64_t parent, const char* name) = nullptr;
    void (*pfReaddirPostfilter)(fuse_req_t req, uint64_t ino, uint32_t error_in, off_t off_in,
                                off_t off_out, size_t size_out, const void* dirents_in,
                                void* fi) = nullptr;
    void (*pfLookupPostfilter)(fuse_req_t req, uint64_t parent, uint32_t error_in, const char* name,
                               struct fuse_entry_out* feo,
                               struct fuse_entry_bpf_out* febo) = nullptr;
    void (*pfMkdir)(fuse_req_t req, uint64_t parent, const char* name, uint32_t mode) = nullptr;
    void (*pfMknod)(fuse_req_t req, uint64_t parent, const char* name, uint32_t mode,
                    uint64_t rdev) = nullptr;
    void (*pfUnlink)(fuse_req_t req, uint64_t parent, const char* name) = nullptr;
    void (*pfRmdir)(fuse_req_t req, uint64_t parent, const char* name) = nullptr;
    void (*pfRename)(fuse_req_t req, uint64_t parent, const char* name, uint64_t new_parent,
                     const char* new_name, uint32_t flags) = nullptr;
    void (*pfCreate)(fuse_req_t req, uint64_t parent, const char* name, uint32_t mode,
                     void* fi) = nullptr;
    void (*pfReaddir)(fuse_req_t req, uint64_t ino, size_t size, off_t off, void* fi) = nullptr;
    void (*doReaddirCommon)(fuse_req_t req, uint64_t ino, size_t size, off_t off, void* fi,
                            bool plus) = nullptr;
    void (*pfReaddirplus)(fuse_req_t req, uint64_t ino, size_t size, off_t off, void* fi) = nullptr;
    void (*pfAccess)(fuse_req_t req, uint64_t ino, int mask) = nullptr;
    void (*pfOpen)(fuse_req_t req, uint64_t ino, void* fi) = nullptr;
    void (*pfOpendir)(fuse_req_t req, uint64_t ino, void* fi) = nullptr;
    int (*replyEntry)(fuse_req_t req, const struct fuse_entry_param* e) = nullptr;
    int (*replyAttr)(fuse_req_t req, const struct stat* attr, double timeout) = nullptr;
    int (*replyBuf)(fuse_req_t req, const char* buf, size_t size) = nullptr;
    int (*replyErr)(fuse_req_t req, int err) = nullptr;
    void (*pfGetattr)(fuse_req_t req, uint64_t ino, void* fi) = nullptr;
    DirectoryEntries (*getDirectoryEntries)(void* wrapper, uint32_t uid, const std::string& path,
                                            DIR* dirp) = nullptr;
    void (*addDirectoryEntriesFromLowerFs)(DIR* dirp, LowerFsDirentFilterFn filter,
                                           DirectoryEntries* entries) = nullptr;
    int (*lstat)(const char* path, struct stat* st) = nullptr;
    int (*stat)(const char* path, struct stat* st) = nullptr;
    ssize_t (*getxattr)(const char* path, const char* name, void* value, size_t size) = nullptr;
    ssize_t (*lgetxattr)(const char* path, const char* name, void* value, size_t size) = nullptr;
    int (*mkdirLibc)(const char* path, mode_t mode) = nullptr;
    int (*mknodLibc)(const char* path, mode_t mode, dev_t dev) = nullptr;
    int (*openLibc)(const char* path, int flags, mode_t mode, bool hasMode) = nullptr;
    int (*open2Libc)(const char* path, int flags) = nullptr;
    bool (*shouldNotCache)(void* fuse, const std::string& path) = nullptr;
    bool (*isAppAccessiblePath)(void* fuse, const std::string& path, uint32_t uid) = nullptr;
};

struct FuseHideAnchorApi {
    uint32_t abiVersion = kFuseHidePayloadAbiVersion;
    uint32_t abiMinSupportedVersion = kFuseHidePayloadAbiMinSupportedVersion;
    uint32_t abiMaxSupportedVersion = kFuseHidePayloadAbiMaxSupportedVersion;
    uint32_t structSize = sizeof(FuseHideAnchorApi);
    // Anchor ABI follows the same append-only prefix rule as FuseHidePayloadApi.
    std::shared_ptr<const HideConfig> (*currentHideConfig)() = nullptr;
    void (*applyHideConfig)(HideConfig config) = nullptr;
    uint32_t (*reqUid)(fuse_req_t req) = nullptr;
    void (*rememberFuseSession)(fuse_req_t req) = nullptr;
    void (*scheduleHiddenEntryInvalidation)() = nullptr;
    void (*scheduleSpecificEntryInvalidation)(uint64_t parent, std::string_view name) = nullptr;
    void (*scheduleHiddenInodeInvalidation)(uint64_t ino) = nullptr;
};

using FuseHidePayloadInitFn = bool (*)(const FuseHideAnchorApi* anchor, FuseHidePayloadApi* outApi);

struct NativeGeneration {
    uint64_t id = 0;
    uint64_t versionCode = 0;
    std::string versionHash;
    std::string payloadPath;
    void* payloadHandle = nullptr;
    bool external = false;
    FuseHidePayloadApi api{};
    std::atomic<uint32_t> activeCalls{0};
    std::atomic<bool> draining{false};
};

struct ScheduledAnchorTask {
    std::chrono::steady_clock::time_point dueAt{};
    std::function<void()> task;
};

struct HookInstallState {
    bool installed = false;
    uintptr_t targetModuleBase = 0;
};

struct AnchorState {
    std::mutex installMutex;
    HookInstallState hookInstallState{};

    std::mutex generationMutex;
    std::shared_ptr<NativeGeneration> activeGeneration;
    std::vector<std::shared_ptr<NativeGeneration>> retiredGenerations;
    std::atomic<uint64_t> nextGenerationId{1};

    std::mutex schedulerMutex;
    std::condition_variable schedulerCv;
    std::deque<ScheduledAnchorTask> scheduledTasks;
    std::thread schedulerThread;
    bool schedulerStop = false;
    bool schedulerStarted = false;
};

AnchorState& Anchor();
bool BuiltinFuseHidePayloadInit(const FuseHideAnchorApi* anchor, FuseHidePayloadApi* outApi);
extern "C" bool FuseHideBuiltinPayloadInitV1(const FuseHideAnchorApi* anchor,
                                             FuseHidePayloadApi* outApi);
bool EnsureAnchorGenerationInitialized();
std::shared_ptr<NativeGeneration> AcquireActiveGeneration();
bool SwitchToBuiltinGeneration(uint64_t versionCode, std::string versionHash,
                               uint64_t* outGenerationId = nullptr);
bool SwitchToExternalGeneration(std::string payloadPath, uint64_t versionCode,
                                std::string versionHash, uint64_t* outGenerationId = nullptr);
uint64_t CurrentNativeGenerationId();
void EnqueueAnchorTask(std::chrono::milliseconds delay, std::function<void()> task);
void MaybeCleanupRetiredGenerations();

bool IsTestHiddenUid(uint32_t uid);
bool ShouldHideTestPath(uint32_t uid, std::string_view path);
std::string EscapeForLog(const uint8_t* data, size_t length);
std::string DebugPreview(std::string_view value, size_t limit = 96);
void LogInvalidUtf8(const uint8_t* data, size_t dataLen, size_t begin, size_t end);
void LogSuspiciousDirectPath(const char* hookName, std::string_view path);
bool DecodeUtf8CodePoint(const uint8_t* data, size_t len, size_t index, uint32_t* cp,
                         size_t* width);
size_t InvalidUtf8SpanEnd(const uint8_t* data, size_t len, size_t index);
bool NeedsSanitization(const std::string& input);
void RewriteString(std::string& input);
int CompareCaseFoldIgnoringDefaultIgnorables(const uint8_t* lhsData, size_t lhsLen,
                                             const uint8_t* rhsData, size_t rhsLen);

struct PendingReaddirContext {
    uint32_t uid = 0;
    uint64_t ino = 0;
    std::string path;
};

struct AnchorProcessState {
    IsAppAccessiblePathFn originalIsAppAccessiblePath = nullptr;
    void* originalPfLookup = nullptr;
    void* originalPfLookupPostfilter = nullptr;
    void* originalPfAccess = nullptr;
    void* originalPfOpen = nullptr;
    void* originalPfOpendir = nullptr;
    void* originalPfMknod = nullptr;
    void* originalPfMkdir = nullptr;
    void* originalPfUnlink = nullptr;
    void* originalPfRmdir = nullptr;
    void* originalPfRename = nullptr;
    void* originalPfCreate = nullptr;
    void* originalPfReaddir = nullptr;
    void* originalPfReaddirPostfilter = nullptr;
    void* originalPfReaddirplus = nullptr;
    void* originalDoReaddirCommon = nullptr;
    void* originalPfGetattr = nullptr;
    void* originalOpen = nullptr;
    void* originalOpen2 = nullptr;
    void* originalMkdir = nullptr;
    void* originalMknod = nullptr;
    void* originalLstat = nullptr;
    void* originalStat = nullptr;
    void* originalGetxattr = nullptr;
    void* originalLgetxattr = nullptr;
    void* originalShouldNotCache = nullptr;
    void* originalNotifyInvalEntry = nullptr;
    void* originalNotifyInvalInode = nullptr;
    void* originalReplyAttr = nullptr;
    void* originalReplyEntry = nullptr;
    void* originalReplyBuf = nullptr;
    void* originalReplyErr = nullptr;
    void* originalGetDirectoryEntries = nullptr;
    void* originalAddDirectoryEntriesFromLowerFs = nullptr;
    std::atomic<void*> lastFuseSession{nullptr};
    std::atomic<bool> hiddenEntryInvalidationPending{false};
    std::atomic<uint64_t> hiddenRootParentInode{0};
    std::mutex hiddenSubtreeInodesMutex;
    std::unordered_set<uint64_t> hiddenSubtreeInodes;
    std::mutex inodePathCacheMutex;
    std::unordered_map<uint64_t, std::string> inodePathCache;
    std::mutex pendingReaddirContextsMutex;
    std::unordered_map<uint64_t, PendingReaddirContext> pendingReaddirContexts;
    std::mutex recentHiddenParentPathsMutex;
    std::unordered_map<uint32_t, std::string> recentHiddenParentPaths;
    std::unordered_map<uint32_t, uint32_t> recentHiddenParentPathUids;
    std::string recentHiddenParentPathAnyUid;
    uint32_t recentHiddenParentPathAnyUidOwner = 0;
};

AnchorProcessState& ProcessState();

struct HookThreadState {
    bool inPfLookup = false;
    bool inPfLookupPostfilter = false;
    bool inPfReaddir = false;
    bool inPfReaddirPostfilter = false;
    bool inPfReaddirplus = false;
    bool inPfGetattr = false;
    uint32_t pfGetattrUid = 0;
    uint32_t pfReaddirUid = 0;
    uint64_t pfGetattrIno = 0;
    uint64_t pfReaddirIno = 0;
    uint64_t currentLookupParentInode = 0;
    std::string currentLookupName;
    bool trackRootHiddenLookup = false;
    bool trackHiddenSubtreeLookup = false;
    bool zeroAttrCacheForCurrentGetattr = false;
    fuse_req_t pendingHiddenErrReq = nullptr;
    uint64_t pendingHiddenErrReqUnique = 0;
    int pendingHiddenErrno = 0;
    uint64_t currentReaddirReqUnique = 0;
    uint32_t activeCreateUid = 0;
    uint32_t lastPathPolicyUid = 0;
    std::string lastPathPolicyPath;
};

extern thread_local HookThreadState gHookThreadState;

namespace ReplyErrorBridge {
// Use Original() only when preserving strict "hook backup only" semantics for a wrapper that
// directly proxies fuse_reply_err itself.
// Use Reply() for policy/error short-circuit paths; it resolves via Original() first, then dlsym
// cache, and emits fallback diagnostics when fuse_reply_err cannot be resolved.
FuseReplyErrFn Original();
FuseReplyErrFn Resolve();
std::optional<int> Reply(fuse_req_t req, int err, const char* caller);
}  // namespace ReplyErrorBridge

uint32_t ReqUid(fuse_req_t req);
void RememberFuseSession(fuse_req_t req);
void ScheduleHiddenEntryInvalidation();
void ScheduleHiddenInodeInvalidation(uint64_t ino);
std::string InodePath(uint64_t ino);
bool IsHiddenLookupTarget(uint32_t uid, uint64_t parent, uint32_t error_in, const char* name);
bool IsHiddenLookupCacheTarget(uint32_t uid, uint64_t parent, const char* name);

enum class HiddenNamedTargetKind {
    None,
    Root,
    Descendant,
};

HiddenNamedTargetKind ClassifyHiddenNamedTarget(uint32_t uid, uint64_t parent, const char* name);
bool ReplyHiddenNamedTargetError(fuse_req_t req, const char* opName, HiddenNamedTargetKind kind,
                                 int rootErr, int descendantErr);
void ArmHiddenErrorRemap(fuse_req_t req, int err, const char* opName);
int MaybeRewriteHiddenLeakErrno(fuse_req_t req, int err, const char* caller);
void ArmHiddenCreateLeakRemap(fuse_req_t req, const char* opName);

bool IsTrackedHiddenSubtreeInode(uint64_t ino);
bool TrackHiddenSubtreeInode(uint64_t ino);
bool RemoveTrackedHiddenSubtreeInode(uint64_t ino);
std::optional<std::string> LookupTrackedPathForInode(uint64_t ino);
std::optional<uint64_t> LookupTrackedInodeForPath(std::string_view path);
void RememberTrackedPathForInode(uint64_t ino, std::string_view path);
void NoteHiddenSubtreePathForCache(std::string_view path);
void RememberRecentHiddenParentPath(uint32_t uid, std::string_view path);
std::optional<std::string> LookupRecentHiddenParentPath(uint32_t uid,
                                                        uint32_t* matchedHiddenUid = nullptr);
void ClearRecentHiddenParentPath(uint32_t uid);
DirectoryEntries FilterHiddenDirectoryEntries(uint32_t uid, std::string_view parentPath,
                                              DirectoryEntries entries);
DirectoryEntries WrappedGetDirectoryEntries(void* wrapper, uint32_t uid, const std::string& path,
                                            DIR* dirp);

}  // namespace fusehide
