#include "fusehide/hooks/payload_builtin.hpp"

#include "fusehide/filters/dirent_filter.hpp"
#include "fusehide/filters/reply_buf_filter.hpp"
#include "fusehide/policy/path_policy.hpp"

namespace fusehide {

namespace {

FuseHideAnchorApi gAnchorApi;

AnchorProcessState& Process() {
    return ProcessState();
}

uint32_t AnchorReqUid(fuse_req_t req) {
    return gAnchorApi.reqUid != nullptr ? gAnchorApi.reqUid(req) : RuntimeState::ReqUid(req);
}

void AnchorRememberFuseSession(fuse_req_t req) {
    if (gAnchorApi.rememberFuseSession != nullptr) {
        gAnchorApi.rememberFuseSession(req);
        return;
    }
    RuntimeState::RememberFuseSession(req);
}

void AnchorScheduleHiddenEntryInvalidation() {
    if (gAnchorApi.scheduleHiddenEntryInvalidation != nullptr) {
        gAnchorApi.scheduleHiddenEntryInvalidation();
        return;
    }
    RuntimeState::ScheduleHiddenEntryInvalidation();
}

void AnchorScheduleSpecificEntryInvalidation(uint64_t parent, std::string_view name) {
    if (gAnchorApi.scheduleSpecificEntryInvalidation != nullptr) {
        gAnchorApi.scheduleSpecificEntryInvalidation(parent, name);
        return;
    }
    RuntimeState::ScheduleSpecificEntryInvalidation(parent, name);
}

void AnchorScheduleHiddenInodeInvalidation(uint64_t ino) {
    if (gAnchorApi.scheduleHiddenInodeInvalidation != nullptr) {
        gAnchorApi.scheduleHiddenInodeInvalidation(ino);
        return;
    }
    RuntimeState::ScheduleHiddenInodeInvalidation(ino);
}

bool ShouldHideLowerFsCreatePath(std::string_view pathView) {
    const uint32_t uid = gHookThreadState.activeCreateUid != 0 ? gHookThreadState.activeCreateUid
                                                               : gHookThreadState.lastPathPolicyUid;
    return uid != 0 && HiddenPathPolicy::IsTestHiddenUid(uid) &&
           HiddenPathPolicy::IsExactHiddenTargetPath(uid, pathView);
}

bool ShouldHideLowerFsPath(std::string_view pathView) {
    const uint32_t uid = gHookThreadState.activeCreateUid != 0 ? gHookThreadState.activeCreateUid
                                                               : gHookThreadState.lastPathPolicyUid;
    return uid != 0 && HiddenPathPolicy::ShouldHideTestPath(uid, pathView);
}

std::string ReadDirectoryPathFromDir(DIR* dirp) {
    if (dirp == nullptr) {
        return {};
    }
    const int fd = dirfd(dirp);
    if (fd < 0) {
        return {};
    }
    char procPath[64];
    std::snprintf(procPath, sizeof(procPath), "/proc/self/fd/%d", fd);
    char resolved[PATH_MAX];
    const ssize_t len = readlink(procPath, resolved, sizeof(resolved) - 1);
    if (len <= 0) {
        return {};
    }
    resolved[len] = '\0';
    return std::string(resolved, static_cast<size_t>(len));
}

bool IsFirstComponentOfHiddenRelativePath(uint32_t uid, std::string_view name) {
    if (name.empty() || name.find('/') != std::string_view::npos) {
        return false;
    }
    const auto rule = ResolveHideRuleForUid(uid);
    if (rule == nullptr) {
        return false;
    }
    for (const auto& hiddenRelativePath : rule->hiddenRelativePaths) {
        std::string normalized = hiddenRelativePath;
        while (!normalized.empty() && normalized.front() == '/') {
            normalized.erase(normalized.begin());
        }
        while (!normalized.empty() && normalized.back() == '/') {
            normalized.pop_back();
        }
        if (normalized.empty()) {
            continue;
        }
        const size_t slash = normalized.find('/');
        const std::string_view first = slash == std::string::npos
                                           ? std::string_view(normalized)
                                           : std::string_view(normalized).substr(0, slash);
        if (first == name) {
            return true;
        }
    }
    return false;
}

void InvalidateFilteredParentChildren(std::string_view parentPath,
                                      const std::vector<FilteredDirentMatch>& removedEntries) {
    if (parentPath.empty() || removedEntries.empty()) {
        return;
    }
    const auto parentIno = LookupTrackedInodeForPath(parentPath);
    if (!parentIno.has_value()) {
        DebugLogPrint(4,
                      "skip filtered child invalidation parent=%s names=%zu reason=no_parent_ino",
                      DebugPreview(parentPath).c_str(), removedEntries.size());
        return;
    }
    for (const auto& entry : removedEntries) {
        AnchorScheduleSpecificEntryInvalidation(*parentIno, entry.name);
        if (entry.ino != 0) {
            const std::string childPath =
                HiddenPathPolicy::JoinPathComponent(parentPath, entry.name);
            RememberTrackedPathForInode(entry.ino, childPath);
            if (TrackHiddenSubtreeInode(entry.ino)) {
                DebugLogPrint(4, "track filtered child inode parent=%s child=%s ino=%s",
                              DebugPreview(parentPath).c_str(), DebugPreview(entry.name).c_str(),
                              InodePath(entry.ino).c_str());
            }
            AnchorScheduleHiddenInodeInvalidation(entry.ino);
        }
    }
    DebugLogPrint(4, "invalidate filtered children parent=%s ino=%s names=%zu",
                  DebugPreview(parentPath).c_str(), InodePath(*parentIno).c_str(),
                  removedEntries.size());
}

DirectoryEntries FilterHiddenDirectoryEntriesLocal(uint32_t uid, std::string_view parentPath,
                                                   DirectoryEntries entries) {
    if (!HiddenPathPolicy::IsTestHiddenUid(uid) || entries.empty()) {
        return entries;
    }

    const size_t before = entries.size();
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [&](const auto& entry) {
                                     if (!entry) {
                                         return false;
                                     }
                                     const std::string& name = entry->d_name;
                                     if (name.empty() || name[0] == '/') {
                                         return false;
                                     }
                                     return HiddenPathPolicy::ShouldHideTestPath(
                                         uid,
                                         HiddenPathPolicy::JoinPathComponent(parentPath, name));
                                 }),
                  entries.end());

    if (entries.size() != before) {
        DebugLogPrint(4, "filter dir entries uid=%u parent=%s removed=%zu remaining=%zu",
                      static_cast<unsigned>(uid), DebugPreview(parentPath).c_str(),
                      before - entries.size(), entries.size());
    }
    return entries;
}

}  // namespace

// pf_lookup is the earliest reliable place to learn the real root parent inode on this device.
// AOSP reference: jni/FuseDaemon.cpp#851
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#851
extern "C" void BuiltinWrappedPfLookup(fuse_req_t req, uint64_t parent, const char* name) {
    AnchorRememberFuseSession(req);
    const uint32_t uid = AnchorReqUid(req);
    if (name != nullptr && HiddenPathPolicy::IsConfiguredHiddenRootEntryName(uid, name) &&
        parent != 0) {
        uint64_t expected = 0;
        if (Process().hiddenRootParentInode.compare_exchange_strong(expected, parent,
                                                                    std::memory_order_relaxed)) {
            DebugLogPrint(4, "record hidden root parent=%s", InodePath(parent).c_str());
        }
    }
    gHookThreadState.inPfLookup = true;
    gHookThreadState.currentLookupParentInode = parent;
    gHookThreadState.currentLookupName = name != nullptr ? std::string(name) : std::string();
    gHookThreadState.trackRootHiddenLookup = IsHiddenLookupCacheTarget(uid, parent, name);
    gHookThreadState.trackHiddenSubtreeLookup = IsTrackedHiddenSubtreeInode(parent);
    DebugLogPrint(3, "lookup: req=%lu parent=%s name=%s", (unsigned long)req->unique,
                  InodePath(parent).c_str(), name ? DebugPreview(name).c_str() : "null");

    auto fn =
        reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*)>(Process().originalPfLookup);
    if (fn)
        fn(req, parent, name);
    gHookThreadState.currentLookupParentInode = 0;
    gHookThreadState.currentLookupName.clear();
    gHookThreadState.inPfLookup = false;
    gHookThreadState.trackHiddenSubtreeLookup = false;
    gHookThreadState.trackRootHiddenLookup = false;
}

// MediaProviderWrapper::GetDirectoryEntries() appends lower-fs directory names after the Java-side
// list is fetched, so root entry hiding must also filter the native vector here.
// AOSP references: jni/MediaProviderWrapper.cpp#373 and jni/FuseDaemon.cpp#1882
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/MediaProviderWrapper.cpp#373
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1882
DirectoryEntries BuiltinWrappedGetDirectoryEntries(void* wrapper, uint32_t uid,
                                                   const std::string& path, DIR* dirp) {
    auto fn = reinterpret_cast<GetDirectoryEntriesFn>(Process().originalGetDirectoryEntries);
    DirectoryEntries entries = fn ? fn(wrapper, uid, path, dirp) : DirectoryEntries();
    if (gHookThreadState.currentReaddirReqUnique != 0) {
        std::lock_guard<std::mutex> lock(Process().pendingReaddirContextsMutex);
        auto it = Process().pendingReaddirContexts.find(gHookThreadState.currentReaddirReqUnique);
        if (it != Process().pendingReaddirContexts.end()) {
            it->second.path = path;
            DebugLogPrint(4, "record readdir path req=%lu ino=%s path=%s",
                          (unsigned long)gHookThreadState.currentReaddirReqUnique,
                          InodePath(it->second.ino).c_str(), DebugPreview(path).c_str());
        }
    }
    return FilterHiddenDirectoryEntriesLocal(uid, path, std::move(entries));
}

void BuiltinWrappedAddDirectoryEntriesFromLowerFs(DIR* dirp, LowerFsDirentFilterFn filter,
                                                  DirectoryEntries* entries) {
    auto fn = reinterpret_cast<AddDirectoryEntriesFromLowerFsFn>(
        Process().originalAddDirectoryEntriesFromLowerFs);
    if (fn == nullptr) {
        return;
    }
    fn(dirp, filter, entries);
    if (entries == nullptr || entries->empty()) {
        return;
    }

    const uint32_t uid = gHookThreadState.lastPathPolicyUid;
    if (!HiddenPathPolicy::IsTestHiddenUid(uid)) {
        return;
    }

    std::string parentPath = ReadDirectoryPathFromDir(dirp);
    if (parentPath.empty()) {
        parentPath = gHookThreadState.lastPathPolicyPath;
    }
    if (parentPath.empty()) {
        return;
    }

    const size_t before = entries->size();
    *entries = FilterHiddenDirectoryEntriesLocal(uid, parentPath, std::move(*entries));
    if (entries->size() != before) {
        DebugLogPrint(4, "filter lower-fs dir entries uid=%u parent=%s removed=%zu remaining=%zu",
                      static_cast<unsigned>(uid), DebugPreview(parentPath).c_str(),
                      before - entries->size(), entries->size());
    }
}

// AOSP readdir postfilter stats each child path before copying the surviving dirents into a
// fuse_read_out buffer. This context flag lets reply_buf preserve that wire layout when the
// device actually goes through pf_readdir_postfilter.
// AOSP reference: jni/FuseDaemon.cpp#1954
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1954
extern "C" void BuiltinWrappedPfReaddirPostfilter(fuse_req_t req, uint64_t ino, uint32_t error_in,
                                                  off_t off_in, off_t off_out, size_t size_out,
                                                  const void* dirents_in, void* fi) {
    AnchorRememberFuseSession(req);
    const uint32_t uid = AnchorReqUid(req);
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, uint32_t, off_t, off_t, size_t,
                                        const void*, void*)>(Process().originalPfReaddirPostfilter);
    if (fn == nullptr) {
        return;
    }
    DebugLogPrint(3, "pf_readdir_postfilter uid=%u ino=%s err=%u off_in=%lld off_out=%lld size=%zu",
                  static_cast<unsigned>(uid), InodePath(ino).c_str(), error_in,
                  static_cast<long long>(off_in), static_cast<long long>(off_out), size_out);

    gHookThreadState.inPfReaddirPostfilter = true;
    gHookThreadState.pfReaddirUid = uid;
    gHookThreadState.pfReaddirIno = ino;
    gHookThreadState.currentReaddirReqUnique = req != nullptr ? req->unique : 0;
    fn(req, ino, error_in, off_in, off_out, size_out, dirents_in, fi);
    gHookThreadState.currentReaddirReqUnique = 0;
    gHookThreadState.pfReaddirIno = 0;
    gHookThreadState.pfReaddirUid = 0;
    gHookThreadState.inPfReaddirPostfilter = false;
}

// pf_lookup_postfilter is the AOSP path-specific ENOENT gate that runs after lookup success but
// before the positive entry reaches the kernel.
// AOSP reference: jni/FuseDaemon.cpp#921
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#921
extern "C" void BuiltinWrappedPfLookupPostfilter(fuse_req_t req, uint64_t parent, uint32_t error_in,
                                                 const char* name, struct fuse_entry_out* feo,
                                                 struct fuse_entry_bpf_out* febo) {
    AnchorRememberFuseSession(req);
    const uint32_t uid = AnchorReqUid(req);
    DebugLogPrint(3, "pf_lookup_postfilter req=%p uid=%u parent=%s name=%s err_in=%u", req,
                  static_cast<unsigned>(uid), InodePath(parent).c_str(),
                  name ? DebugPreview(name).c_str() : "null", error_in);
    if (IsHiddenLookupTarget(uid, parent, error_in, name)) {
        DebugLogPrint(4, "pf_lookup_postfilter hide uid=%u parent=%s name=%s",
                      static_cast<unsigned>(uid), InodePath(parent).c_str(), name);
        AnchorScheduleHiddenEntryInvalidation();
        if (ReplyErrorBridge::Reply(req, ENOENT, "pf_lookup_postfilter").has_value()) {
            return;
        }
        ArmHiddenErrorRemap(req, ENOENT, "pf_lookup_postfilter");
    }
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, uint32_t, const char*,
                                        struct fuse_entry_out*, struct fuse_entry_bpf_out*)>(
        Process().originalPfLookupPostfilter);
    if (fn) {
        gHookThreadState.inPfLookupPostfilter = true;
        fn(req, parent, error_in, name, feo, febo);
        gHookThreadState.inPfLookupPostfilter = false;
    }
}

// AOSP pf_mkdir only checks parent_path accessibility before it calls mkdir(child_path), so a
// hidden leaf name would still leak existence semantics unless we stop it here.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1184
extern "C" void BuiltinWrappedPfMkdir(fuse_req_t req, uint64_t parent, const char* name,
                                      uint32_t mode) {
    AnchorRememberFuseSession(req);
    const uint32_t uid = AnchorReqUid(req);
    const HiddenNamedTargetKind kind = ClassifyHiddenNamedTarget(uid, parent, name);
    DebugLogPrint(4,
                  "create-trace pf_mkdir uid=%u parent=%s name=%s mode=%o hidden_root=%d "
                  "hidden_desc=%d",
                  static_cast<unsigned>(uid), InodePath(parent).c_str(),
                  name ? DebugPreview(name).c_str() : "null", mode,
                  kind == HiddenNamedTargetKind::Root ? 1 : 0,
                  kind == HiddenNamedTargetKind::Descendant ? 1 : 0);
    if (ReplyHiddenNamedTargetError(req, "pf_mkdir", kind, EACCES, ENOENT)) {
        return;
    }
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*, uint32_t)>(
        Process().originalPfMkdir);
    if (fn) {
        ScopedCreateUid scopedUid(uid);
        fn(req, parent, name, mode);
    }
}

// Some callers create regular files through the mknod op instead of create. AOSP still uses only
// parent_path policy here, so hidden leaf names must be blocked explicitly.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1134
extern "C" void BuiltinWrappedPfMknod(fuse_req_t req, uint64_t parent, const char* name,
                                      uint32_t mode, uint64_t rdev) {
    AnchorRememberFuseSession(req);
    const uint32_t uid = AnchorReqUid(req);
    const HiddenNamedTargetKind kind = ClassifyHiddenNamedTarget(uid, parent, name);
    DebugLogPrint(4,
                  "create-trace pf_mknod uid=%u parent=%s name=%s mode=%o rdev=%llu hidden_root=%d "
                  "hidden_desc=%d",
                  static_cast<unsigned>(uid), InodePath(parent).c_str(),
                  name ? DebugPreview(name).c_str() : "null", mode,
                  static_cast<unsigned long long>(rdev),
                  kind == HiddenNamedTargetKind::Root ? 1 : 0,
                  kind == HiddenNamedTargetKind::Descendant ? 1 : 0);
    if (ReplyHiddenNamedTargetError(req, "pf_mknod", kind, EPERM, ENOENT)) {
        return;
    }
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*, uint32_t, uint64_t)>(
        Process().originalPfMknod);
    if (fn) {
        ScopedCreateUid scopedUid(uid);
        fn(req, parent, name, mode, rdev);
    }
}

// AOSP pf_unlink only gates on parent_path before it deletes the final child path, so hidden leaf
// names must return ENOENT here instead of reaching the lower filesystem.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1218
extern "C" void BuiltinWrappedPfUnlink(fuse_req_t req, uint64_t parent, const char* name) {
    AnchorRememberFuseSession(req);
    const HiddenNamedTargetKind kind = ClassifyHiddenNamedTarget(AnchorReqUid(req), parent, name);
    if (ReplyHiddenNamedTargetError(req, "pf_unlink", kind, ENOENT, ENOENT)) {
        return;
    }
    auto fn =
        reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*)>(Process().originalPfUnlink);
    if (fn) {
        fn(req, parent, name);
    }
}

// AOSP pf_rmdir follows the same parent-only validation pattern as pf_unlink, so hidden child
// names must be rejected before the real directory delete runs.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1248
extern "C" void BuiltinWrappedPfRmdir(fuse_req_t req, uint64_t parent, const char* name) {
    AnchorRememberFuseSession(req);
    const HiddenNamedTargetKind kind = ClassifyHiddenNamedTarget(AnchorReqUid(req), parent, name);
    if (ReplyHiddenNamedTargetError(req, "pf_rmdir", kind, ENOENT, ENOENT)) {
        return;
    }
    auto fn =
        reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*)>(Process().originalPfRmdir);
    if (fn) {
        fn(req, parent, name);
    }
}

// AOSP do_rename only validates the old and new parent directories before it passes the final
// child paths into MediaProviderWrapper::Rename, so hidden names must be intercepted here as well.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1299
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1369
extern "C" void BuiltinWrappedPfRename(fuse_req_t req, uint64_t parent, const char* name,
                                       uint64_t new_parent, const char* new_name, uint32_t flags) {
    AnchorRememberFuseSession(req);
    const uint32_t uid = AnchorReqUid(req);
    const HiddenNamedTargetKind srcKind = ClassifyHiddenNamedTarget(uid, parent, name);
    const HiddenNamedTargetKind dstKind = ClassifyHiddenNamedTarget(uid, new_parent, new_name);
    if (srcKind != HiddenNamedTargetKind::None || dstKind != HiddenNamedTargetKind::None) {
        DebugLogPrint(4,
                      "pf_rename hide named target src_root=%d src_desc=%d dst_root=%d dst_desc=%d "
                      "flags=0x%x",
                      srcKind == HiddenNamedTargetKind::Root ? 1 : 0,
                      srcKind == HiddenNamedTargetKind::Descendant ? 1 : 0,
                      dstKind == HiddenNamedTargetKind::Root ? 1 : 0,
                      dstKind == HiddenNamedTargetKind::Descendant ? 1 : 0, flags);
        AnchorScheduleHiddenEntryInvalidation();
        if (ReplyErrorBridge::Reply(req, ENOENT, "pf_rename").has_value()) {
            return;
        }
        ArmHiddenErrorRemap(req, ENOENT, "pf_rename");
    }
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*, uint64_t, const char*,
                                        uint32_t)>(Process().originalPfRename);
    if (fn) {
        fn(req, parent, name, new_parent, new_name, flags);
    }
}

// AOSP pf_create inserts into MediaProvider and then opens the lower-fs child path. Returning a
// positive entry here would let create leak EEXIST-like behavior for the hidden root entry.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#2121
extern "C" void BuiltinWrappedPfCreate(fuse_req_t req, uint64_t parent, const char* name,
                                       uint32_t mode, void* fi) {
    AnchorRememberFuseSession(req);
    const uint32_t uid = AnchorReqUid(req);
    const HiddenNamedTargetKind kind = ClassifyHiddenNamedTarget(uid, parent, name);
    DebugLogPrint(4,
                  "create-trace pf_create uid=%u parent=%s name=%s mode=%o fi=%p hidden_root=%d "
                  "hidden_desc=%d",
                  static_cast<unsigned>(uid), InodePath(parent).c_str(),
                  name ? DebugPreview(name).c_str() : "null", mode, fi,
                  kind == HiddenNamedTargetKind::Root ? 1 : 0,
                  kind == HiddenNamedTargetKind::Descendant ? 1 : 0);
    if (ReplyHiddenNamedTargetError(req, "pf_create", kind, EPERM, ENOENT)) {
        return;
    }
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*, uint32_t, void*)>(
        Process().originalPfCreate);
    if (fn) {
        ScopedCreateUid scopedUid(uid);
        fn(req, parent, name, mode, fi);
    }
}

// Plain readdir delegates to do_readdir_common(..., plus=false). Most modern devices keep
// readdirplus enabled, but this hook is still useful as a fallback for alternative FUSE configs.
// AOSP reference: jni/FuseDaemon.cpp#1944
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1944
extern "C" void BuiltinWrappedPfReaddir(fuse_req_t req, uint64_t ino, size_t size, off_t off,
                                        void* fi) {
    AnchorRememberFuseSession(req);
    const uint32_t uid = AnchorReqUid(req);
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, size_t, off_t, void*)>(
        Process().originalPfReaddir);
    if (fn == nullptr) {
        return;
    }
    DebugLogPrint(3, "pf_readdir uid=%u ino=%s size=%zu off=%lld", static_cast<unsigned>(uid),
                  InodePath(ino).c_str(), size, static_cast<long long>(off));
    if (req != nullptr) {
        std::lock_guard<std::mutex> lock(Process().pendingReaddirContextsMutex);
        Process().pendingReaddirContexts[req->unique] = PendingReaddirContext{uid, ino, {}};
    }
    gHookThreadState.inPfReaddir = true;
    gHookThreadState.pfReaddirUid = uid;
    gHookThreadState.pfReaddirIno = ino;
    gHookThreadState.currentReaddirReqUnique = req != nullptr ? req->unique : 0;
    fn(req, ino, size, off, fi);
    gHookThreadState.currentReaddirReqUnique = 0;
    gHookThreadState.pfReaddirIno = 0;
    gHookThreadState.pfReaddirUid = 0;
    gHookThreadState.inPfReaddir = false;
}

extern "C" void BuiltinWrappedDoReaddirCommon(fuse_req_t req, uint64_t ino, size_t size, off_t off,
                                              void* fi, bool plus) {
    AnchorRememberFuseSession(req);
    const uint32_t uid = AnchorReqUid(req);
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, size_t, off_t, void*, bool)>(
        Process().originalDoReaddirCommon);
    if (fn == nullptr) {
        return;
    }
    DebugLogPrint(3, "do_readdir_common uid=%u ino=%s size=%zu off=%lld plus=%d",
                  static_cast<unsigned>(uid), InodePath(ino).c_str(), size,
                  static_cast<long long>(off), plus ? 1 : 0);
    if (req != nullptr) {
        std::lock_guard<std::mutex> lock(Process().pendingReaddirContextsMutex);
        Process().pendingReaddirContexts[req->unique] = PendingReaddirContext{uid, ino, {}};
    }
    gHookThreadState.currentReaddirReqUnique = req != nullptr ? req->unique : 0;
    fn(req, ino, size, off, fi, plus);
    gHookThreadState.currentReaddirReqUnique = 0;
}

// readdirplus is the common enumeration path on recent Android builds because do_readdir_common()
// emits fuse_direntplus records by first running do_lookup() for each directory entry.
// AOSP references: jni/FuseDaemon.cpp#1904 and #2000
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1904
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#2000
extern "C" void BuiltinWrappedPfReaddirplus(fuse_req_t req, uint64_t ino, size_t size, off_t off,
                                            void* fi) {
    AnchorRememberFuseSession(req);
    const uint32_t uid = AnchorReqUid(req);
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, size_t, off_t, void*)>(
        Process().originalPfReaddirplus);
    if (fn == nullptr) {
        return;
    }
    DebugLogPrint(3, "pf_readdirplus uid=%u ino=%s size=%zu off=%lld", static_cast<unsigned>(uid),
                  InodePath(ino).c_str(), size, static_cast<long long>(off));
    if (req != nullptr) {
        std::lock_guard<std::mutex> lock(Process().pendingReaddirContextsMutex);
        Process().pendingReaddirContexts[req->unique] = PendingReaddirContext{uid, ino, {}};
    }
    gHookThreadState.inPfReaddirplus = true;
    gHookThreadState.pfReaddirUid = uid;
    gHookThreadState.pfReaddirIno = ino;
    gHookThreadState.currentReaddirReqUnique = req != nullptr ? req->unique : 0;
    fn(req, ino, size, off, fi);
    gHookThreadState.currentReaddirReqUnique = 0;
    gHookThreadState.pfReaddirIno = 0;
    gHookThreadState.pfReaddirUid = 0;
    gHookThreadState.inPfReaddirplus = false;
}

extern "C" void BuiltinWrappedPfAccess(fuse_req_t req, uint64_t ino, int mask) {
    AnchorRememberFuseSession(req);
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, int)>(Process().originalPfAccess);
    if (fn) {
        fn(req, ino, mask);
    }
}

extern "C" void BuiltinWrappedPfOpen(fuse_req_t req, uint64_t ino, void* fi) {
    AnchorRememberFuseSession(req);
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, void*)>(Process().originalPfOpen);
    if (fn) {
        fn(req, ino, fi);
    }
}

extern "C" void BuiltinWrappedPfOpendir(fuse_req_t req, uint64_t ino, void* fi) {
    AnchorRememberFuseSession(req);
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, void*)>(Process().originalPfOpendir);
    if (fn) {
        fn(req, ino, fi);
    }
}

extern "C" int BuiltinWrappedReplyErr(fuse_req_t req, int err) {
    auto fn = ReplyErrorBridge::Original();
    if (req != nullptr) {
        std::lock_guard<std::mutex> lock(Process().pendingReaddirContextsMutex);
        Process().pendingReaddirContexts.erase(req->unique);
    }
    err = MaybeRewriteHiddenLeakErrno(req, err, "fuse_reply_err");
    int ret = fn ? fn(req, err) : -1;
    if (gHookThreadState.inPfLookupPostfilter) {
        DebugLogPrint(3, "pf_lookup_postfilter fuse_reply_err req=%p %d", req, err);
    } else {
        DebugLogPrint(3, "fuse_reply_err: req=%p err=%d ret=%d", req, err, ret);
    }
    return ret;
}

// Device reverse engineering shows make_node_entry() and create_handle_for_node() both consult
// fuse->ShouldNotCache(path). Matching that behavior is what keeps positive dentries and file-cache
// state from being reused across UIDs.
// AOSP references: jni/FuseDaemon.cpp#347, #510, and #1428
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#347
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#510
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1428
extern "C" bool BuiltinWrappedShouldNotCache(void* fuse, const std::string& path) {
    if (HiddenPathPolicy::IsAnyHiddenSubtreePath(path)) {
        DebugLogPrint(4, "force uncached subtree path=%s", DebugPreview(path).c_str());
        return true;
    }
    auto fn = reinterpret_cast<ShouldNotCacheFn>(Process().originalShouldNotCache);
    return fn ? fn(fuse, path) : false;
}

extern "C" int BuiltinWrappedReplyAttr(fuse_req_t req, const struct stat* attr, double timeout) {
    auto fn = reinterpret_cast<int (*)(fuse_req_t, const struct stat*, double)>(
        Process().originalReplyAttr);

    // get_entry_timeout() only controls dentry caching; pf_getattr still replies with a separate
    // attr timeout. Force both to zero when the request touches the hidden subtree.
    // AOSP references: jni/FuseDaemon.cpp#510 and #1002
    // https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#510
    // https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1002
    const double replyTimeout = gHookThreadState.zeroAttrCacheForCurrentGetattr ? 0.0 : timeout;
    if (gHookThreadState.zeroAttrCacheForCurrentGetattr) {
        DebugLogPrint(4, "disable attr cache req=%p timeout=%.2le", req, replyTimeout);
    }
    return fn ? fn(req, attr, replyTimeout) : -1;
}

extern "C" void BuiltinWrappedPfGetattr(fuse_req_t req, uint64_t ino, void* fi) {
    AnchorRememberFuseSession(req);
    const uint32_t uid = AnchorReqUid(req);
    gHookThreadState.zeroAttrCacheForCurrentGetattr = IsTrackedHiddenSubtreeInode(ino);
    if (HiddenPathPolicy::IsTestHiddenUid(uid)) {
        DebugLogPrint(4, "pf_getattr test uid=%u ino=0x%lx", static_cast<unsigned>(uid),
                      (unsigned long)ino);
    }
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, void*)>(Process().originalPfGetattr);
    if (fn) {
        gHookThreadState.inPfGetattr = true;
        gHookThreadState.pfGetattrUid = uid;
        gHookThreadState.pfGetattrIno = ino;
        fn(req, ino, fi);
        gHookThreadState.pfGetattrIno = 0;
        gHookThreadState.pfGetattrUid = 0;
        gHookThreadState.inPfGetattr = false;
        gHookThreadState.zeroAttrCacheForCurrentGetattr = false;
    }
}

// This is the strongest uid-specific hiding point. Once a positive fuse_entry_param escapes here,
// later operations such as getattr, getxattr, or create can still observe existence through the
// resolved inode even if later path-based checks return false.
// AOSP references: jni/FuseDaemon.cpp#912, #1166, and #1211
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#912
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1166
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1211
extern "C" int BuiltinWrappedReplyEntry(fuse_req_t req, const struct fuse_entry_param* e) {
    auto fn = reinterpret_cast<int (*)(fuse_req_t, const struct fuse_entry_param*)>(
        Process().originalReplyEntry);
    const bool hiddenLookupForUid =
        HiddenPathPolicy::IsTestHiddenUid(AnchorReqUid(req)) &&
        (gHookThreadState.trackRootHiddenLookup || gHookThreadState.trackHiddenSubtreeLookup);
    if (hiddenLookupForUid) {
        if (gHookThreadState.trackRootHiddenLookup || gHookThreadState.trackHiddenSubtreeLookup) {
            ArmHiddenCreateLeakRemap(req, "fuse_reply_entry");
        }
        if (gHookThreadState.trackHiddenSubtreeLookup) {
            if (const auto parentPath =
                    LookupTrackedPathForInode(gHookThreadState.currentLookupParentInode);
                parentPath.has_value()) {
                RememberTrackedPathForInode(gHookThreadState.currentLookupParentInode, *parentPath);
                RememberRecentHiddenParentPath(AnchorReqUid(req), *parentPath);
                DebugLogPrint(4, "refresh recent hidden parent from hidden lookup uid=%u path=%s",
                              static_cast<unsigned>(AnchorReqUid(req)),
                              DebugPreview(*parentPath).c_str());
                AnchorScheduleHiddenInodeInvalidation(gHookThreadState.currentLookupParentInode);
            }
        }
        if (auto ret = ReplyErrorBridge::Reply(req, ENOENT, "fuse_reply_entry"); ret.has_value()) {
            DebugLogPrint(4, "hide lookup entry uid=%u req=%lu ino=%s root=%d child=%d ret=%d",
                          static_cast<unsigned>(AnchorReqUid(req)),
                          req ? (unsigned long)req->unique : 0UL,
                          e != nullptr ? InodePath(e->ino).c_str() : "(null)",
                          gHookThreadState.trackRootHiddenLookup ? 1 : 0,
                          gHookThreadState.trackHiddenSubtreeLookup ? 1 : 0, *ret);
            AnchorScheduleHiddenEntryInvalidation();
            if (gHookThreadState.currentLookupParentInode != 0 &&
                !gHookThreadState.currentLookupName.empty()) {
                AnchorScheduleSpecificEntryInvalidation(gHookThreadState.currentLookupParentInode,
                                                        gHookThreadState.currentLookupName);
            }
            if (e != nullptr && e->ino != 0) {
                AnchorScheduleHiddenInodeInvalidation(e->ino);
            }
            return *ret;
        }
    }
    fuse_entry_param patchedEntry = {};
    const struct fuse_entry_param* replyEntry = e;
    bool forceUncachedReplyEntry =
        gHookThreadState.trackRootHiddenLookup || gHookThreadState.trackHiddenSubtreeLookup;
    bool exactHiddenTargetReplyEntry = false;
    if (e != nullptr &&
        (gHookThreadState.trackRootHiddenLookup || gHookThreadState.trackHiddenSubtreeLookup)) {
        patchedEntry = *e;
        patchedEntry.entry_timeout = 0.0;
        patchedEntry.attr_timeout = 0.0;
        replyEntry = &patchedEntry;
        DebugLogPrint(4, "disable entry cache req=%lu ino=%s root=%d child=%d",
                      req ? (unsigned long)req->unique : 0UL, InodePath(e->ino).c_str(),
                      gHookThreadState.trackRootHiddenLookup ? 1 : 0,
                      gHookThreadState.trackHiddenSubtreeLookup ? 1 : 0);
        AnchorScheduleHiddenEntryInvalidation();
        if (TrackHiddenSubtreeInode(e->ino)) {
            AnchorScheduleHiddenInodeInvalidation(e->ino);
        }
    }

    // The device build sometimes delivers directory data through reply_buf without ever hitting our
    // readdir-family wrappers, but lookup success for the visible parent still flows through
    // fuse_reply_entry. Record the parent path here so reply_buf can later reconstruct an exact
    // parentPath + childName match for nested hidden targets.
    // AOSP references: jni/FuseDaemon.cpp#889, #912, and #1909
    // https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#889
    // https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#912
    // https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1909
    if (e != nullptr && gHookThreadState.currentLookupParentInode != 0 &&
        !gHookThreadState.currentLookupName.empty()) {
        std::optional<std::string> childPath;
        if (const auto parentPath =
                LookupTrackedPathForInode(gHookThreadState.currentLookupParentInode);
            parentPath.has_value()) {
            childPath = HiddenPathPolicy::JoinPathComponent(*parentPath,
                                                            gHookThreadState.currentLookupName);
        } else if (IsFirstComponentOfHiddenRelativePath(AnchorReqUid(req),
                                                        gHookThreadState.currentLookupName)) {
            childPath = HiddenPathPolicy::JoinPathComponent(kVisibleStorageRoots[0],
                                                            gHookThreadState.currentLookupName);
            DebugLogPrint(4, "infer root child path ino=%s name=%s path=%s",
                          InodePath(gHookThreadState.currentLookupParentInode).c_str(),
                          DebugPreview(gHookThreadState.currentLookupName).c_str(),
                          DebugPreview(*childPath).c_str());
        }
        if (childPath.has_value()) {
            const uint32_t uid = AnchorReqUid(req);
            RememberTrackedPathForInode(e->ino, *childPath);
            const bool hiddenSubtreePath =
                HiddenPathPolicy::IsAnyHiddenSubtreePath(uid, *childPath);
            exactHiddenTargetReplyEntry =
                HiddenPathPolicy::IsExactHiddenTargetPath(uid, *childPath);
            if (hiddenSubtreePath) {
                TrackHiddenSubtreeInode(e->ino);
                forceUncachedReplyEntry = true;
            }
            if (exactHiddenTargetReplyEntry) {
                forceUncachedReplyEntry = true;
                AnchorScheduleSpecificEntryInvalidation(gHookThreadState.currentLookupParentInode,
                                                        gHookThreadState.currentLookupName);
                AnchorScheduleHiddenInodeInvalidation(e->ino);
                DebugLogPrint(4, "force uncached exact hidden target uid=%u path=%s ino=%s",
                              static_cast<unsigned>(uid), DebugPreview(*childPath).c_str(),
                              InodePath(e->ino).c_str());
            }
            if (fusehide::IsParentOfExactHiddenTargetPath(uid, *childPath)) {
                RememberRecentHiddenParentPath(uid, *childPath);
                DebugLogPrint(4, "remember recent hidden parent uid=%u path=%s",
                              static_cast<unsigned>(uid), DebugPreview(*childPath).c_str());
            }
        }
    }
    if (e != nullptr && forceUncachedReplyEntry && replyEntry == e) {
        patchedEntry = *e;
        patchedEntry.entry_timeout = 0.0;
        patchedEntry.attr_timeout = 0.0;
        replyEntry = &patchedEntry;
        DebugLogPrint(4, "disable entry cache req=%lu ino=%s root=%d child=%d exact=%d",
                      req ? (unsigned long)req->unique : 0UL, InodePath(e->ino).c_str(),
                      gHookThreadState.trackRootHiddenLookup ? 1 : 0,
                      gHookThreadState.trackHiddenSubtreeLookup ? 1 : 0,
                      exactHiddenTargetReplyEntry ? 1 : 0);
    }
    int ret = fn ? fn(req, replyEntry) : -1;
    DebugLogPrint(3,
                  "fuse_reply_entry: req=%lu ino=%s timeout=%.2le attr_timeout=%.2le bpf_fd=%lu "
                  "bpf_action=%lu backing_action=%lu backing_fd=%lu ret=%d",
                  (unsigned long)req->unique, InodePath(replyEntry->ino).c_str(),
                  replyEntry->entry_timeout, replyEntry->attr_timeout,
                  (unsigned long)replyEntry->bpf_fd, (unsigned long)replyEntry->bpf_action,
                  (unsigned long)replyEntry->backing_action, (unsigned long)replyEntry->backing_fd,
                  ret);
    return ret;
}

// reply_buf is the last universal filtering point. AOSP emits directory data through plain readdir,
// readdir postfilter, readdirplus, and lookup_postfilter using different wire layouts, so auto-
// detecting dirent and direntplus records here is more reliable than betting on one upstream path.
// AOSP references: jni/FuseDaemon.cpp#946, #1941, and #1997
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#946
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1941
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1997
extern "C" int BuiltinWrappedReplyBuf(fuse_req_t req, const char* buf, size_t size) {
    auto fn =
        reinterpret_cast<int (*)(fuse_req_t, const char*, size_t)>(Process().originalReplyBuf);
    const char* replyBuf = buf;
    size_t replySize = size;
    std::vector<char> filteredStorage;
    size_t removedCount = 0;
    std::vector<FilteredDirentMatch> removedEntries;
    PendingReaddirContext pendingContext{};
    bool hasPendingContext = false;
    if (req != nullptr) {
        std::lock_guard<std::mutex> lock(Process().pendingReaddirContextsMutex);
        const auto it = Process().pendingReaddirContexts.find(req->unique);
        if (it != Process().pendingReaddirContexts.end()) {
            pendingContext = it->second;
            hasPendingContext = true;
        }
    }
    const uint32_t reqUid = AnchorReqUid(req);
    const uint32_t requestFilterUid =
        gHookThreadState.pfReaddirUid != 0
            ? gHookThreadState.pfReaddirUid
            : (hasPendingContext && pendingContext.uid != 0 ? pendingContext.uid : reqUid);
    const uint64_t filterIno = gHookThreadState.pfReaddirIno != 0
                                   ? gHookThreadState.pfReaddirIno
                                   : (hasPendingContext ? pendingContext.ino : 0);
    const bool filterPlainReaddir = gHookThreadState.inPfReaddir;
    const bool filterPostfilterReaddir = gHookThreadState.inPfReaddirPostfilter;
    const bool filterReaddirplus = gHookThreadState.inPfReaddirplus;
    const bool requireParentMatch = filterIno != 0;
    const char* filterMode = nullptr;
    uint32_t fallbackHiddenUid = 0;
    const std::optional<std::string> fallbackParentPath =
        filterIno == 0 ? LookupRecentHiddenParentPath(requestFilterUid, &fallbackHiddenUid)
                       : std::nullopt;

    // Some device reply_buf paths lose the caller uid entirely, but we must not reuse the last
    // hidden uid for an explicit non-hidden app. Only borrow fallback uid when the request uid is
    // absent (0); otherwise recent parent state would bleed into unrelated apps.
    const bool canBorrowFallbackUid =
        requestFilterUid == 0 && HiddenPathPolicy::IsTestHiddenUid(fallbackHiddenUid);
    const uint32_t filterUid = HiddenPathPolicy::IsTestHiddenUid(requestFilterUid)
                                   ? requestFilterUid
                                   : (canBorrowFallbackUid ? fallbackHiddenUid : 0);

    if (filterIno != 0 && !pendingContext.path.empty()) {
        RememberTrackedPathForInode(filterIno, pendingContext.path);
    }

    if (HiddenPathPolicy::IsTestHiddenUid(filterUid)) {
        const ReplyBufFilterResult primaryResult =
            FilterReplyBufPayload(buf, size,
                                  ReplyBufFilterContext{
                                      .filterUid = filterUid,
                                      .filterIno = filterIno,
                                      .filterPlainReaddir = filterPlainReaddir,
                                      .filterPostfilterReaddir = filterPostfilterReaddir,
                                      .filterReaddirplus = filterReaddirplus,
                                      .requireParentMatch = requireParentMatch,
                                      .enableAutoFallback = false,
                                  },
                                  &filteredStorage);
        if (primaryResult.mode != nullptr) {
            replyBuf = primaryResult.data;
            replySize = primaryResult.size;
            filterMode = primaryResult.mode;
            removedCount = primaryResult.removedCount;
        }

        // When the active device path bypasses our readdir wrappers, reply_buf still sees the final
        // dirent payload but loses the parent inode/path context. Fall back to the last visible
        // parent path learned from fuse_reply_entry so nested targets like Download/Download can be
        // filtered by exact path instead of by name alone.
        if (filterMode == nullptr) {
            if (fallbackParentPath.has_value() &&
                BuildFilteredDirentplusPayloadForParentPath(buf, size, filterUid,
                                                            *fallbackParentPath, &filteredStorage,
                                                            &removedCount, &removedEntries)) {
                replyBuf = filteredStorage.data();
                replySize = filteredStorage.size();
                filterMode = "fallback_parent_direntplus";
                InvalidateFilteredParentChildren(*fallbackParentPath, removedEntries);
                ClearRecentHiddenParentPath(filterUid);
            } else if (fallbackParentPath.has_value() &&
                       BuildFilteredDirentPayloadForParentPath(
                           buf, size, filterUid, *fallbackParentPath, &filteredStorage,
                           &removedCount, &removedEntries)) {
                replyBuf = filteredStorage.data();
                replySize = filteredStorage.size();
                filterMode = "fallback_parent_dirent";
                InvalidateFilteredParentChildren(*fallbackParentPath, removedEntries);
                ClearRecentHiddenParentPath(filterUid);
            } else if (size >= sizeof(fuse_read_out) && fallbackParentPath.has_value()) {
                const auto* readOut = reinterpret_cast<const fuse_read_out*>(buf);
                const size_t payloadSize =
                    std::min<size_t>(readOut->size, size - sizeof(fuse_read_out));
                std::vector<char> filteredPayload;
                if (BuildFilteredDirentPayloadForParentPath(
                        buf + sizeof(fuse_read_out), payloadSize, filterUid, *fallbackParentPath,
                        &filteredPayload, &removedCount, &removedEntries)) {
                    fuse_read_out patched = *readOut;
                    patched.size = static_cast<uint32_t>(filteredPayload.size());
                    filteredStorage.resize(sizeof(patched) + filteredPayload.size());
                    std::memcpy(filteredStorage.data(), &patched, sizeof(patched));
                    std::memcpy(filteredStorage.data() + sizeof(patched), filteredPayload.data(),
                                filteredPayload.size());
                    replyBuf = filteredStorage.data();
                    replySize = filteredStorage.size();
                    filterMode = "fallback_parent_read_out_dirent";
                    InvalidateFilteredParentChildren(*fallbackParentPath, removedEntries);
                    ClearRecentHiddenParentPath(filterUid);
                }
            }
        }

        if (filterMode == nullptr) {
            const ReplyBufFilterResult autoResult =
                FilterReplyBufPayload(buf, size,
                                      ReplyBufFilterContext{
                                          .filterUid = filterUid,
                                          .requireParentMatch = false,
                                          .enableAutoFallback = true,
                                      },
                                      &filteredStorage);
            if (autoResult.mode != nullptr) {
                replyBuf = autoResult.data;
                replySize = autoResult.size;
                filterMode = autoResult.mode;
                removedCount = autoResult.removedCount;
            }
        }
    }

    int ret = fn ? fn(req, replyBuf, replySize) : -1;
    if (hasPendingContext && req != nullptr) {
        std::lock_guard<std::mutex> lock(Process().pendingReaddirContextsMutex);
        Process().pendingReaddirContexts.erase(req->unique);
    }
    if (removedCount != 0) {
        DebugLogPrint(4, "filtered readdir reply mode=%s uid=%u ino=%s removed=%zu size=%zu->%zu",
                      filterMode ? filterMode : "unknown", static_cast<unsigned>(filterUid),
                      InodePath(filterIno).c_str(), removedCount, size, replySize);
    }
    if (gHookThreadState.inPfLookupPostfilter) {
        DebugLogPrint(3, "pf_lookup_postfilter fuse_reply_buf req=%p", req);
    } else {
        DebugLogPrint(3, "fuse_reply_buf: req=%lu size=%zu ret=%d", (unsigned long)req->unique,
                      replySize, ret);
    }
    return ret;
}

extern "C" int BuiltinWrappedStat(const char* path, struct stat* st) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    if (gHookThreadState.inPfReaddirPostfilter &&
        HiddenPathPolicy::IsTestHiddenUid(gHookThreadState.pfReaddirUid) &&
        HiddenPathPolicy::IsAnyHiddenSubtreePath(gHookThreadState.pfReaddirUid, pathView)) {
        DebugLogPrint(4, "hide readdir stat uid=%u path=%s",
                      static_cast<unsigned>(gHookThreadState.pfReaddirUid),
                      DebugPreview(pathView).c_str());
        errno = ENOENT;
        return -1;
    }
    auto fn = reinterpret_cast<int (*)(const char*, struct stat*)>(Process().originalStat);
    if (fn) {
        return fn(path, st);
    }
    errno = ENOSYS;
    return -1;
}

// lstat is the path-based source of truth used by pf_getattr and by some enumeration paths. This is
// where we convert a visible subtree path back into cache invalidation state.
// lstat is the path-based source of truth for pf_getattr and is also consulted by readdir
// postfilter. Recording the root parent inode here avoids assuming a fixed root inode value.
// AOSP references: jni/FuseDaemon.cpp#1002 and #1985
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1002
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1985
extern "C" int BuiltinWrappedLstat(const char* path, struct stat* st) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    if (gHookThreadState.inPfGetattr && gHookThreadState.pfGetattrIno != 0 &&
        HiddenPathPolicy::IsHiddenRootDirectoryPath(pathView)) {
        uint64_t expected = 0;
        const bool recorded = Process().hiddenRootParentInode.compare_exchange_strong(
            expected, gHookThreadState.pfGetattrIno, std::memory_order_relaxed);
        if (recorded) {
            DebugLogPrint(4, "record hidden root parent from getattr=%s path=%s",
                          InodePath(gHookThreadState.pfGetattrIno).c_str(),
                          DebugPreview(pathView).c_str());
        }
        RemoveTrackedHiddenSubtreeInode(gHookThreadState.pfGetattrIno);
        if (const auto rule = RuleForAnyPackage();
            recorded && rule != nullptr && rule->enableHideAllRootEntries) {
            RuntimeState::ScheduleHiddenEntryInvalidation();
        }
    }
    NoteHiddenSubtreePathForCache(pathView);
    if (gHookThreadState.inPfGetattr &&
        HiddenPathPolicy::IsTestHiddenUid(gHookThreadState.pfGetattrUid)) {
        DebugLogPrint(4, "pf_getattr lstat uid=%u path=%s",
                      static_cast<unsigned>(gHookThreadState.pfGetattrUid),
                      DebugPreview(pathView).c_str());
        if (HiddenPathPolicy::ShouldHideTestPath(gHookThreadState.pfGetattrUid, pathView)) {
            DebugLogPrint(4, "hide test lstat uid=%u path=%s",
                          static_cast<unsigned>(gHookThreadState.pfGetattrUid),
                          DebugPreview(pathView).c_str());
            errno = ENOENT;
            return -1;
        }
    }
    auto fn = reinterpret_cast<int (*)(const char*, struct stat*)>(Process().originalLstat);
    if (fn) {
        const int ret = fn(path, st);
        if (ret == 0 && gHookThreadState.inPfGetattr && gHookThreadState.pfGetattrIno != 0) {
            RememberTrackedPathForInode(gHookThreadState.pfGetattrIno, pathView);
            if (HiddenPathPolicy::IsTestHiddenUid(gHookThreadState.pfGetattrUid) &&
                fusehide::IsParentOfExactHiddenTargetPath(gHookThreadState.pfGetattrUid,
                                                          pathView)) {
                // Some device builds enumerate the parent directory after only touching it through
                // pf_getattr/lstat, without a visible parent lookup that would reach reply_entry.
                // Seed the fallback parent path here so reply_buf can still filter nested children.
                RememberRecentHiddenParentPath(gHookThreadState.pfGetattrUid, pathView);
                DebugLogPrint(4, "remember recent hidden parent from getattr uid=%u path=%s",
                              static_cast<unsigned>(gHookThreadState.pfGetattrUid),
                              DebugPreview(pathView).c_str());
            }
            if (HiddenPathPolicy::IsAnyHiddenSubtreePath(gHookThreadState.pfGetattrUid, pathView)) {
                TrackHiddenSubtreeInode(gHookThreadState.pfGetattrIno);
            }
        }
        return ret;
    }
    errno = ENOSYS;
    return -1;
}

extern "C" ssize_t BuiltinWrappedGetxattr(const char* path, const char* name, void* value,
                                          size_t size) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    if (ShouldHideLowerFsPath(pathView)) {
        DebugLogPrint(4, "hide getxattr path=%s name=%s", DebugPreview(pathView).c_str(),
                      name != nullptr ? DebugPreview(name).c_str() : "null");
        errno = ENOENT;
        return -1;
    }
    auto fn = reinterpret_cast<ssize_t (*)(const char*, const char*, void*, size_t)>(
        Process().originalGetxattr);
    if (fn) {
        return fn(path, name, value, size);
    }
    errno = ENOSYS;
    return -1;
}

extern "C" ssize_t BuiltinWrappedLgetxattr(const char* path, const char* name, void* value,
                                           size_t size) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    if (ShouldHideLowerFsPath(pathView)) {
        DebugLogPrint(4, "hide lgetxattr path=%s name=%s", DebugPreview(pathView).c_str(),
                      name != nullptr ? DebugPreview(name).c_str() : "null");
        errno = ENOENT;
        return -1;
    }
    auto fn = reinterpret_cast<ssize_t (*)(const char*, const char*, void*, size_t)>(
        Process().originalLgetxattr);
    if (fn) {
        return fn(path, name, value, size);
    }
    errno = ENOSYS;
    return -1;
}

// Even if the named FUSE wrappers are missed on a device-specific path, lower-fs mkdir/mknod/open
// calls still carry the final child path. These libc hooks are the last fallback for create/mkdir.
extern "C" int BuiltinWrappedMkdirLibc(const char* path, mode_t mode) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    const uint32_t uid = gHookThreadState.activeCreateUid;
    const bool hidden = ShouldHideLowerFsCreatePath(pathView);
    DebugLogPrint(4, "create-trace lower_mkdir uid=%u path=%s mode=%o hidden=%d",
                  static_cast<unsigned>(uid), DebugPreview(pathView).c_str(), mode, hidden ? 1 : 0);
    if (hidden) {
        DebugLogPrint(4, "hide mkdir path=%s", DebugPreview(pathView).c_str());
        errno = EACCES;
        return -1;
    }
    auto fn = reinterpret_cast<int (*)(const char*, mode_t)>(Process().originalMkdir);
    if (fn) {
        return fn(path, mode);
    }
    errno = ENOSYS;
    return -1;
}

extern "C" int BuiltinWrappedMknodLibc(const char* path, mode_t mode, dev_t dev) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    const uint32_t uid = gHookThreadState.activeCreateUid;
    const bool hidden = ShouldHideLowerFsCreatePath(pathView);
    DebugLogPrint(4, "create-trace lower_mknod uid=%u path=%s mode=%o dev=%llu hidden=%d",
                  static_cast<unsigned>(uid), DebugPreview(pathView).c_str(), mode,
                  static_cast<unsigned long long>(dev), hidden ? 1 : 0);
    if (hidden) {
        DebugLogPrint(4, "hide mknod path=%s", DebugPreview(pathView).c_str());
        errno = EPERM;
        return -1;
    }
    auto fn = reinterpret_cast<int (*)(const char*, mode_t, dev_t)>(Process().originalMknod);
    if (fn) {
        return fn(path, mode, dev);
    }
    errno = ENOSYS;
    return -1;
}

// The libc open hook is the last fallback for create flows that bypass the named pf_create path.
int BuiltinWrappedOpenLibc(const char* path, int flags, mode_t mode, bool hasMode) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    const uint32_t uid = gHookThreadState.activeCreateUid;
    const bool hidden = hasMode && (flags & O_CREAT) != 0 && ShouldHideLowerFsCreatePath(pathView);
    if ((flags & O_CREAT) != 0) {
        DebugLogPrint(4, "create-trace lower_open uid=%u path=%s flags=0x%x mode=%o hidden=%d",
                      static_cast<unsigned>(uid), DebugPreview(pathView).c_str(), flags, mode,
                      hidden ? 1 : 0);
    }
    if (hidden) {
        DebugLogPrint(4, "hide open create path=%s flags=0x%x", DebugPreview(pathView).c_str(),
                      flags);
        errno = EPERM;
        return -1;
    }
    auto fn = reinterpret_cast<int (*)(const char*, int, ...)>(Process().originalOpen);
    if (fn) {
        if (hasMode && (flags & O_CREAT) != 0) {
            return fn(path, flags, mode);
        }
        return fn(path, flags);
    }
    errno = ENOSYS;
    return -1;
}

// __open_2 appears on some device paths instead of open(...), so keep the same create fallback
// behavior here as the plain libc open hook.
extern "C" int BuiltinWrappedOpen2Libc(const char* path, int flags) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    const uint32_t uid = gHookThreadState.activeCreateUid;
    const bool hidden = (flags & O_CREAT) != 0 && ShouldHideLowerFsCreatePath(pathView);
    if ((flags & O_CREAT) != 0) {
        DebugLogPrint(4, "create-trace lower_open2 uid=%u path=%s flags=0x%x hidden=%d",
                      static_cast<unsigned>(uid), DebugPreview(pathView).c_str(), flags,
                      hidden ? 1 : 0);
    }
    if (hidden) {
        DebugLogPrint(4, "hide __open_2 create path=%s flags=0x%x", DebugPreview(pathView).c_str(),
                      flags);
        errno = EPERM;
        return -1;
    }
    auto fn = reinterpret_cast<int (*)(const char*, int)>(Process().originalOpen2);
    if (fn) {
        return fn(path, flags);
    }
    errno = ENOSYS;
    return -1;
}

// Mirror the original app-accessible gate: sanitize only when needed, then delegate.
bool BuiltinWrappedIsAppAccessiblePath(void* fuse, const std::string& path, uint32_t uid) {
    auto fn = Process().originalIsAppAccessiblePath;
    if (fn == nullptr) {
        return false;
    }
    gHookThreadState.lastPathPolicyUid = uid;
    gHookThreadState.lastPathPolicyPath = path;
    if (!UnicodePolicy::NeedsSanitization(path)) {
        UnicodePolicy::LogSuspiciousDirectPath("app_accessible", path);
        if (ShouldLogLimited(gAppAccessibleLogCount)) {
            DebugLogPrint(3, "app_accessible direct uid=%u path=%s", uid,
                          DebugPreview(path).c_str());
        }
        NoteHiddenSubtreePathForCache(path);
        if (HiddenPathPolicy::ShouldHideTestPath(uid, path)) {
            DebugLogPrint(4, "hide test path uid=%u path=%s", static_cast<unsigned>(uid),
                          DebugPreview(path).c_str());
            return false;
        }
        return fn(fuse, path, uid);
    }
    std::string sanitized(path);
    UnicodePolicy::RewriteString(sanitized);
    gHookThreadState.lastPathPolicyPath = sanitized;
    if (ShouldLogLimited(gAppAccessibleLogCount)) {
        DebugLogPrint(3, "app_accessible rewrite uid=%u old=%s new=%s", uid,
                      DebugPreview(path).c_str(), DebugPreview(sanitized).c_str());
    }
    NoteHiddenSubtreePathForCache(sanitized);
    if (HiddenPathPolicy::ShouldHideTestPath(uid, sanitized)) {
        DebugLogPrint(4, "hide test path uid=%u path=%s src=%s", static_cast<unsigned>(uid),
                      DebugPreview(sanitized).c_str(), DebugPreview(path).c_str());
        return false;
    }
    return fn(fuse, sanitized, uid);
}

bool BuiltinFuseHidePayloadInit(const FuseHideAnchorApi* anchor, FuseHidePayloadApi* outApi) {
    if (outApi == nullptr) {
        return false;
    }
    if (anchor != nullptr &&
        (anchor->structSize <
             offsetof(FuseHideAnchorApi, structSize) + sizeof(FuseHideAnchorApi::structSize) ||
         anchor->abiMinSupportedVersion > kFuseHidePayloadAbiMaxSupportedVersion ||
         anchor->abiMaxSupportedVersion < kFuseHidePayloadAbiMinSupportedVersion)) {
        DebugLogPrint(
            6,
            "reject payload init due to anchor ABI mismatch anchor_abi=%u "
            "anchor_range=%u-%u anchor_size=%u supported_range=%u-%u required_prefix=%zu",
            anchor->abiVersion, anchor->abiMinSupportedVersion, anchor->abiMaxSupportedVersion,
            anchor->structSize, kFuseHidePayloadAbiMinSupportedVersion,
            kFuseHidePayloadAbiMaxSupportedVersion,
            offsetof(FuseHideAnchorApi, structSize) + sizeof(FuseHideAnchorApi::structSize));
        return false;
    }
    gAnchorApi = anchor != nullptr ? *anchor : FuseHideAnchorApi{};
    *outApi = FuseHidePayloadApi{
        .abiVersion = kFuseHidePayloadAbiVersion,
        .abiMinSupportedVersion = kFuseHidePayloadAbiMinSupportedVersion,
        .abiMaxSupportedVersion = kFuseHidePayloadAbiMaxSupportedVersion,
        .structSize = sizeof(FuseHidePayloadApi),
        .pfLookup = BuiltinWrappedPfLookup,
        .pfReaddirPostfilter = BuiltinWrappedPfReaddirPostfilter,
        .pfLookupPostfilter = BuiltinWrappedPfLookupPostfilter,
        .pfMkdir = BuiltinWrappedPfMkdir,
        .pfMknod = BuiltinWrappedPfMknod,
        .pfUnlink = BuiltinWrappedPfUnlink,
        .pfRmdir = BuiltinWrappedPfRmdir,
        .pfRename = BuiltinWrappedPfRename,
        .pfCreate = BuiltinWrappedPfCreate,
        .pfReaddir = BuiltinWrappedPfReaddir,
        .doReaddirCommon = BuiltinWrappedDoReaddirCommon,
        .pfReaddirplus = BuiltinWrappedPfReaddirplus,
        .replyEntry = BuiltinWrappedReplyEntry,
        .replyAttr = BuiltinWrappedReplyAttr,
        .replyBuf = BuiltinWrappedReplyBuf,
        .replyErr = BuiltinWrappedReplyErr,
        .pfGetattr = BuiltinWrappedPfGetattr,
        .getDirectoryEntries = BuiltinWrappedGetDirectoryEntries,
        .addDirectoryEntriesFromLowerFs = BuiltinWrappedAddDirectoryEntriesFromLowerFs,
        .lstat = BuiltinWrappedLstat,
        .stat = BuiltinWrappedStat,
        .getxattr = BuiltinWrappedGetxattr,
        .lgetxattr = BuiltinWrappedLgetxattr,
        .mkdirLibc = BuiltinWrappedMkdirLibc,
        .mknodLibc = BuiltinWrappedMknodLibc,
        .openLibc = BuiltinWrappedOpenLibc,
        .open2Libc = BuiltinWrappedOpen2Libc,
        .shouldNotCache = BuiltinWrappedShouldNotCache,
        .isAppAccessiblePath = BuiltinWrappedIsAppAccessiblePath,
    };
    return true;
}

extern "C" __attribute__((visibility("default"))) bool FuseHideBuiltinPayloadInitV1(
    const FuseHideAnchorApi* anchor, FuseHidePayloadApi* outApi) {
    return BuiltinFuseHidePayloadInit(anchor, outApi);
}

extern "C" __attribute__((visibility("default"))) bool FuseHidePayloadInitV1(
    const FuseHideAnchorApi* anchor, FuseHidePayloadApi* outApi) {
    return BuiltinFuseHidePayloadInit(anchor, outApi);
}

}  // namespace fusehide
