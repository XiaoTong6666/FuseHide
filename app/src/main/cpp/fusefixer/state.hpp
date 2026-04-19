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
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

namespace fusefixer {

inline constexpr const char* kLogTag = "FuseFixer";
inline constexpr const char* kTargetLibrary = "libfuse_jni.so";
inline constexpr std::string_view kVisibleStorageRoots[] = {"/storage/emulated/0"};
inline constexpr std::string_view kHiddenRootEntryNames[] = {
    "xinhao",
    "MT2",
};
inline constexpr std::string_view kHiddenPackages[] = {
    "com.eltavine.duckdetector",
    "io.github.xiaotong6666.fusefixer",
    "io.github.a13e300.fusefixer",
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

// These RVAs are device-specific addresses from the reverse-engineered libfuse_jni.so.
// The production device library we analyzed is stripped, so internal helpers such as
// is_app_accessible_path and several pf_* handlers are not always recoverable by name from the
// shipped ELF. Keep these offsets only as a last resort for this exact build when symbol-based
// resolution fails.
// Reverse-engineered record: is_app_accessible_path @ 0x0017bb5c.
inline constexpr uintptr_t kDeviceIsAppAccessiblePathOffset = 0x0017bb5c;
// Reverse-engineered record: pf_lookup @ 0x00175e48.
inline constexpr uintptr_t kDevicePfLookupOffset = 0x00175e48;
// Reverse-engineered record: pf_lookup_postfilter @ 0x00175f90.
inline constexpr uintptr_t kDevicePfLookupPostfilterOffset = 0x00175f90;
// Reverse-engineered record: pf_getattr @ 0x001762bc.
inline constexpr uintptr_t kDevicePfGetattrOffset = 0x001762bc;
// Reverse-engineered record: ShouldNotCache @ 0x0017dc64.
inline constexpr uintptr_t kDeviceShouldNotCacheOffset = 0x0017dc64;
// Reverse-engineered record: MediaProviderWrapper::GetDirectoryEntries @ 0x0018a3ec.
inline constexpr uintptr_t kDeviceGetDirectoryEntriesOffset = 0x0018a3ec;
// Reverse-engineered record: pf_mkdir @ 0x00177050.
inline constexpr uintptr_t kDevicePfMkdirOffset = 0x00177050;
// Reverse-engineered record: pf_mknod @ 0x00176ba8.
inline constexpr uintptr_t kDevicePfMknodOffset = 0x00176ba8;
// Reverse-engineered record: pf_unlink @ 0x00177534.
inline constexpr uintptr_t kDevicePfUnlinkOffset = 0x00177534;
// Reverse-engineered record: pf_rmdir @ 0x00177920.
inline constexpr uintptr_t kDevicePfRmdirOffset = 0x00177920;
// Reverse-engineered record: pf_rename @ 0x00177ef4.
inline constexpr uintptr_t kDevicePfRenameOffset = 0x00177ef4;
// Reverse-engineered record: pf_create @ 0x0017a7c8.
inline constexpr uintptr_t kDevicePfCreateOffset = 0x0017a7c8;
// Reverse-engineered record: pf_readdir @ 0x00179c40.
inline constexpr uintptr_t kDevicePfReaddirOffset = 0x00179c40;
// Reverse-engineered record: pf_readdir_postfilter @ 0x00179cac.
inline constexpr uintptr_t kDevicePfReaddirPostfilterOffset = 0x00179cac;
// Reverse-engineered record: pf_readdirplus @ 0x0017b320.
inline constexpr uintptr_t kDevicePfReaddirplusOffset = 0x0017b320;
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
extern IsAppAccessiblePathFn gOriginalIsAppAccessiblePath;
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
extern std::atomic<int> gSuspiciousDirectLogCount;
extern std::mutex gUidHideCacheMutex;
extern std::unordered_map<uint32_t, bool> gUidHideCache;

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

std::optional<bool> ResolveShouldHideUidWithPackageManager(uint32_t uid);

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

class HiddenPathPolicy final {
   public:
    static bool IsTestHiddenUid(uint32_t uid);
    static bool ShouldHideTestPath(uint32_t uid, std::string_view path);
    static bool IsHiddenRootEntryName(std::string_view name);
    static bool IsAnyHiddenSubtreePath(std::string_view path);
    static bool IsExactHiddenTargetPath(std::string_view path);
    static bool IsHiddenRootDirectoryPath(std::string_view path);
    static std::string JoinPathComponent(std::string_view parent, std::string_view child);
    static bool ShouldFilterHiddenRootDirent(uint32_t uid, uint64_t ino, std::string_view name,
                                             bool requireParentMatch);
};

class DirentFilter final {
   public:
    static bool BuildFilteredDirentPayload(const char* data, size_t size, uint32_t uid,
                                           uint64_t ino, std::vector<char>* out,
                                           size_t* removedCount, bool requireParentMatch = true);
    static bool BuildFilteredDirentplusPayload(const char* data, size_t size, uint32_t uid,
                                               uint64_t ino, std::vector<char>* out,
                                               size_t* removedCount,
                                               bool requireParentMatch = true);
};

class RuntimeState final {
   public:
    static uint32_t ReqUid(fuse_req_t req);
    static void RememberFuseSession(fuse_req_t req);
    static void ScheduleHiddenEntryInvalidation();
    static void ScheduleHiddenInodeInvalidation(uint64_t ino);
};

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

extern void* gOriginalPfLookup;
extern void* gOriginalPfLookupPostfilter;
extern void* gOriginalPfAccess;
extern void* gOriginalPfOpen;
extern void* gOriginalPfOpendir;
extern void* gOriginalPfMknod;
extern void* gOriginalPfMkdir;
extern void* gOriginalPfUnlink;
extern void* gOriginalPfRmdir;
extern void* gOriginalPfRename;
extern void* gOriginalPfCreate;
extern void* gOriginalPfReaddir;
extern void* gOriginalPfReaddirPostfilter;
extern void* gOriginalPfReaddirplus;
extern void* gOriginalPfGetattr;
extern void* gOriginalOpen;
extern void* gOriginalOpen2;
extern void* gOriginalMkdir;
extern void* gOriginalMknod;
extern void* gOriginalLstat;
extern void* gOriginalStat;
extern void* gOriginalShouldNotCache;
extern void* gOriginalNotifyInvalEntry;
extern void* gOriginalNotifyInvalInode;
extern void* gOriginalReplyAttr;
extern void* gOriginalReplyEntry;
extern void* gOriginalReplyBuf;
extern void* gOriginalReplyErr;
extern void* gOriginalGetDirectoryEntries;
extern std::atomic<void*> gLastFuseSession;
extern std::atomic<bool> gHiddenEntryInvalidationPending;
extern std::atomic<uint64_t> gHiddenRootParentInode;
extern thread_local bool gInPfLookup;
extern thread_local bool gInPfLookupPostfilter;
extern thread_local bool gInPfReaddir;
extern thread_local bool gInPfReaddirPostfilter;
extern thread_local bool gInPfReaddirplus;
extern thread_local bool gInPfGetattr;
extern thread_local uint32_t gPfGetattrUid;
extern thread_local uint32_t gPfReaddirUid;
extern thread_local uint64_t gPfGetattrIno;
extern thread_local uint64_t gPfReaddirIno;
extern thread_local uint64_t gCurrentLookupParentInode;
extern thread_local bool gTrackRootHiddenLookup;
extern thread_local bool gTrackHiddenSubtreeLookup;
extern thread_local bool gZeroAttrCacheForCurrentGetattr;
extern std::mutex gHiddenSubtreeInodesMutex;
extern std::unordered_set<uint64_t> gHiddenSubtreeInodes;

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

enum class HiddenNamedTargetKind {
    None,
    Root,
    Descendant,
};

HiddenNamedTargetKind ClassifyHiddenNamedTarget(uint32_t uid, uint64_t parent, const char* name);
bool ReplyHiddenNamedTargetError(fuse_req_t req, const char* opName, HiddenNamedTargetKind kind,
                                 int rootErr, int descendantErr);

bool IsTrackedHiddenSubtreeInode(uint64_t ino);
bool TrackHiddenSubtreeInode(uint64_t ino);
bool RemoveTrackedHiddenSubtreeInode(uint64_t ino);
bool IsHiddenRootEntryName(std::string_view name);
bool IsAnyHiddenSubtreePath(std::string_view path);
bool IsExactHiddenTargetPath(std::string_view path);
bool IsHiddenRootDirectoryPath(std::string_view path);
std::string JoinPathComponent(std::string_view parent, std::string_view child);
size_t AlignDirentName(size_t nameLen);
size_t FuseDirentRecordSize(const fuse_dirent* dirent);
size_t FuseDirentplusRecordSize(const fuse_dirent* dirent);
bool ShouldFilterHiddenRootDirent(uint32_t uid, uint64_t ino, std::string_view name,
                                  bool requireParentMatch);
bool BuildFilteredDirentPayload(const char* data, size_t size, uint32_t uid, uint64_t ino,
                                std::vector<char>* out, size_t* removedCount,
                                bool requireParentMatch = true);
bool BuildFilteredDirentplusPayload(const char* data, size_t size, uint32_t uid, uint64_t ino,
                                    std::vector<char>* out, size_t* removedCount,
                                    bool requireParentMatch = true);
void NoteHiddenSubtreePathForCache(std::string_view path);
DirectoryEntries FilterHiddenDirectoryEntries(uint32_t uid, std::string_view parentPath,
                                              DirectoryEntries entries);
DirectoryEntries WrappedGetDirectoryEntries(void* wrapper, uint32_t uid, const std::string& path,
                                            DIR* dirp);

}  // namespace fusefixer
