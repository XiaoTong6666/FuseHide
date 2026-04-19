#include "wrappers.hpp"

namespace fusefixer {

// pf_lookup is the earliest reliable place to learn the real root parent inode on this device.
// AOSP reference: jni/FuseDaemon.cpp#851
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#851
extern "C" void WrappedPfLookup(fuse_req_t req, uint64_t parent, const char* name) {
    RuntimeState::RememberFuseSession(req);
    if (name != nullptr && IsHiddenRootEntryName(name) && parent != 0) {
        uint64_t expected = 0;
        if (gHiddenRootParentInode.compare_exchange_strong(expected, parent,
                                                           std::memory_order_relaxed)) {
            DebugLogPrint(4, "record hidden root parent=%s", InodePath(parent).c_str());
        }
    }
    const uint64_t rootParent = gHiddenRootParentInode.load(std::memory_order_relaxed);
    gInPfLookup = true;
    gCurrentLookupParentInode = parent;
    gTrackRootHiddenLookup =
        name != nullptr && IsHiddenRootEntryName(name) && (rootParent == 0 || parent == rootParent);
    gTrackHiddenSubtreeLookup = IsTrackedHiddenSubtreeInode(parent);
    DebugLogPrint(3, "lookup: req=%lu parent=%s name=%s", (unsigned long)req->unique,
                  InodePath(parent).c_str(), name ? DebugPreview(name).c_str() : "null");

    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*)>(gOriginalPfLookup);
    if (fn)
        fn(req, parent, name);
    gCurrentLookupParentInode = 0;
    gInPfLookup = false;
    gTrackHiddenSubtreeLookup = false;
    gTrackRootHiddenLookup = false;
}

// MediaProviderWrapper::GetDirectoryEntries() appends lower-fs directory names after the Java-side
// list is fetched, so root entry hiding must also filter the native vector here.
// AOSP references: jni/MediaProviderWrapper.cpp#373 and jni/FuseDaemon.cpp#1882
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/MediaProviderWrapper.cpp#373
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1882
DirectoryEntries FilterHiddenDirectoryEntries(uint32_t uid, std::string_view parentPath,
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

DirectoryEntries WrappedGetDirectoryEntries(void* wrapper, uint32_t uid, const std::string& path,
                                            DIR* dirp) {
    auto fn = reinterpret_cast<GetDirectoryEntriesFn>(gOriginalGetDirectoryEntries);
    DirectoryEntries entries = fn ? fn(wrapper, uid, path, dirp) : DirectoryEntries();
    return FilterHiddenDirectoryEntries(uid, path, std::move(entries));
}

// AOSP readdir postfilter stats each child path before copying the surviving dirents into a
// fuse_read_out buffer. This context flag lets WrappedReplyBuf preserve that wire layout when the
// device actually goes through pf_readdir_postfilter.
// AOSP reference: jni/FuseDaemon.cpp#1954
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1954
extern "C" void WrappedPfReaddirPostfilter(fuse_req_t req, uint64_t ino, uint32_t error_in,
                                           off_t off_in, off_t off_out, size_t size_out,
                                           const void* dirents_in, void* fi) {
    RuntimeState::RememberFuseSession(req);
    const uint32_t uid = RuntimeState::ReqUid(req);
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, uint32_t, off_t, off_t, size_t,
                                        const void*, void*)>(gOriginalPfReaddirPostfilter);
    if (fn == nullptr) {
        return;
    }
    DebugLogPrint(3, "pf_readdir_postfilter uid=%u ino=%s err=%u off_in=%lld off_out=%lld size=%zu",
                  static_cast<unsigned>(uid), InodePath(ino).c_str(), error_in,
                  static_cast<long long>(off_in), static_cast<long long>(off_out), size_out);

    gInPfReaddirPostfilter = true;
    gPfReaddirUid = uid;
    gPfReaddirIno = ino;
    fn(req, ino, error_in, off_in, off_out, size_out, dirents_in, fi);
    gPfReaddirIno = 0;
    gPfReaddirUid = 0;
    gInPfReaddirPostfilter = false;
}

// pf_lookup_postfilter is the AOSP path-specific ENOENT gate that runs after lookup success but
// before the positive entry reaches the kernel.
// AOSP reference: jni/FuseDaemon.cpp#921
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#921
extern "C" void WrappedPfLookupPostfilter(fuse_req_t req, uint64_t parent, uint32_t error_in,
                                          const char* name, struct fuse_entry_out* feo,
                                          struct fuse_entry_bpf_out* febo) {
    RuntimeState::RememberFuseSession(req);
    const uint32_t uid = RuntimeState::ReqUid(req);
    DebugLogPrint(3, "pf_lookup_postfilter req=%p uid=%u parent=%s name=%s err_in=%u", req,
                  static_cast<unsigned>(uid), InodePath(parent).c_str(),
                  name ? DebugPreview(name).c_str() : "null", error_in);
    if (IsHiddenLookupTarget(uid, parent, error_in, name)) {
        DebugLogPrint(4, "pf_lookup_postfilter hide uid=%u parent=%s name=%s",
                      static_cast<unsigned>(uid), InodePath(parent).c_str(), name);
        RuntimeState::ScheduleHiddenEntryInvalidation();
        if (ReplyErrorBridge::Reply(req, ENOENT, "pf_lookup_postfilter").has_value()) {
            return;
        }
        ArmHiddenErrorRemap(req, ENOENT, "pf_lookup_postfilter");
    }
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, uint32_t, const char*,
                                        struct fuse_entry_out*, struct fuse_entry_bpf_out*)>(
        gOriginalPfLookupPostfilter);
    if (fn) {
        gInPfLookupPostfilter = true;
        fn(req, parent, error_in, name, feo, febo);
        gInPfLookupPostfilter = false;
    }
}

extern "C" void WrappedPfAccess(fuse_req_t req, uint64_t ino, int mask) {
    RuntimeState::RememberFuseSession(req);
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, int)>(gOriginalPfAccess);
    if (fn) {
        fn(req, ino, mask);
    }
}

extern "C" void WrappedPfOpen(fuse_req_t req, uint64_t ino, void* fi) {
    RuntimeState::RememberFuseSession(req);
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, void*)>(gOriginalPfOpen);
    if (fn) {
        fn(req, ino, fi);
    }
}

extern "C" void WrappedPfOpendir(fuse_req_t req, uint64_t ino, void* fi) {
    RuntimeState::RememberFuseSession(req);
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, void*)>(gOriginalPfOpendir);
    if (fn) {
        fn(req, ino, fi);
    }
}

// AOSP pf_mkdir only checks parent_path accessibility before it calls mkdir(child_path), so a
// hidden leaf name would still leak existence semantics unless we stop it here.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1184
extern "C" void WrappedPfMkdir(fuse_req_t req, uint64_t parent, const char* name, uint32_t mode) {
    RuntimeState::RememberFuseSession(req);
    const HiddenNamedTargetKind kind =
        ClassifyHiddenNamedTarget(RuntimeState::ReqUid(req), parent, name);
    if (ReplyHiddenNamedTargetError(req, "pf_mkdir", kind, EACCES, ENOENT)) {
        return;
    }
    auto fn =
        reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*, uint32_t)>(gOriginalPfMkdir);
    if (fn) {
        fn(req, parent, name, mode);
    }
}

// Some callers create regular files through the mknod op instead of create. AOSP still uses only
// parent_path policy here, so hidden leaf names must be blocked explicitly.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1134
extern "C" void WrappedPfMknod(fuse_req_t req, uint64_t parent, const char* name, uint32_t mode,
                               uint64_t rdev) {
    RuntimeState::RememberFuseSession(req);
    const HiddenNamedTargetKind kind =
        ClassifyHiddenNamedTarget(RuntimeState::ReqUid(req), parent, name);
    if (ReplyHiddenNamedTargetError(req, "pf_mknod", kind, EPERM, ENOENT)) {
        return;
    }
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*, uint32_t, uint64_t)>(
        gOriginalPfMknod);
    if (fn) {
        fn(req, parent, name, mode, rdev);
    }
}

// AOSP pf_unlink only gates on parent_path before it deletes the final child path, so hidden leaf
// names must return ENOENT here instead of reaching the lower filesystem.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1218
extern "C" void WrappedPfUnlink(fuse_req_t req, uint64_t parent, const char* name) {
    RuntimeState::RememberFuseSession(req);
    const HiddenNamedTargetKind kind =
        ClassifyHiddenNamedTarget(RuntimeState::ReqUid(req), parent, name);
    if (ReplyHiddenNamedTargetError(req, "pf_unlink", kind, ENOENT, ENOENT)) {
        return;
    }
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*)>(gOriginalPfUnlink);
    if (fn) {
        fn(req, parent, name);
    }
}

// AOSP pf_rmdir follows the same parent-only validation pattern as pf_unlink, so hidden child
// names must be rejected before the real directory delete runs.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1248
extern "C" void WrappedPfRmdir(fuse_req_t req, uint64_t parent, const char* name) {
    RuntimeState::RememberFuseSession(req);
    const HiddenNamedTargetKind kind =
        ClassifyHiddenNamedTarget(RuntimeState::ReqUid(req), parent, name);
    if (ReplyHiddenNamedTargetError(req, "pf_rmdir", kind, ENOENT, ENOENT)) {
        return;
    }
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*)>(gOriginalPfRmdir);
    if (fn) {
        fn(req, parent, name);
    }
}

// AOSP do_rename only validates the old and new parent directories before it passes the final
// child paths into MediaProviderWrapper::Rename, so hidden names must be intercepted here as well.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1299
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1369
extern "C" void WrappedPfRename(fuse_req_t req, uint64_t parent, const char* name,
                                uint64_t new_parent, const char* new_name, uint32_t flags) {
    RuntimeState::RememberFuseSession(req);
    const uint32_t uid = RuntimeState::ReqUid(req);
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
        RuntimeState::ScheduleHiddenEntryInvalidation();
        if (ReplyErrorBridge::Reply(req, ENOENT, "pf_rename").has_value()) {
            return;
        }
        ArmHiddenErrorRemap(req, ENOENT, "pf_rename");
    }
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*, uint64_t, const char*,
                                        uint32_t)>(gOriginalPfRename);
    if (fn) {
        fn(req, parent, name, new_parent, new_name, flags);
    }
}

// AOSP pf_create inserts into MediaProvider and then opens the lower-fs child path. Returning a
// positive entry here would let create leak EEXIST-like behavior for the hidden root entry.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#2121
extern "C" void WrappedPfCreate(fuse_req_t req, uint64_t parent, const char* name, uint32_t mode,
                                void* fi) {
    RuntimeState::RememberFuseSession(req);
    const HiddenNamedTargetKind kind =
        ClassifyHiddenNamedTarget(RuntimeState::ReqUid(req), parent, name);
    if (ReplyHiddenNamedTargetError(req, "pf_create", kind, EPERM, ENOENT)) {
        return;
    }
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, const char*, uint32_t, void*)>(
        gOriginalPfCreate);
    if (fn) {
        fn(req, parent, name, mode, fi);
    }
}

// Plain readdir delegates to do_readdir_common(..., plus=false). Most modern devices keep
// readdirplus enabled, but this hook is still useful as a fallback for alternative FUSE configs.
// AOSP reference: jni/FuseDaemon.cpp#1944
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1944
extern "C" void WrappedPfReaddir(fuse_req_t req, uint64_t ino, size_t size, off_t off, void* fi) {
    RuntimeState::RememberFuseSession(req);
    const uint32_t uid = RuntimeState::ReqUid(req);
    auto fn =
        reinterpret_cast<void (*)(fuse_req_t, uint64_t, size_t, off_t, void*)>(gOriginalPfReaddir);
    if (fn == nullptr) {
        return;
    }
    DebugLogPrint(3, "pf_readdir uid=%u ino=%s size=%zu off=%lld", static_cast<unsigned>(uid),
                  InodePath(ino).c_str(), size, static_cast<long long>(off));
    gInPfReaddir = true;
    gPfReaddirUid = uid;
    gPfReaddirIno = ino;
    fn(req, ino, size, off, fi);
    gPfReaddirIno = 0;
    gPfReaddirUid = 0;
    gInPfReaddir = false;
}

// readdirplus is the common enumeration path on recent Android builds because do_readdir_common()
// emits fuse_direntplus records by first running do_lookup() for each directory entry.
// AOSP references: jni/FuseDaemon.cpp#1904 and #2000
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1904
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#2000
extern "C" void WrappedPfReaddirplus(fuse_req_t req, uint64_t ino, size_t size, off_t off,
                                     void* fi) {
    RuntimeState::RememberFuseSession(req);
    const uint32_t uid = RuntimeState::ReqUid(req);
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, size_t, off_t, void*)>(
        gOriginalPfReaddirplus);
    if (fn == nullptr) {
        return;
    }
    DebugLogPrint(3, "pf_readdirplus uid=%u ino=%s size=%zu off=%lld", static_cast<unsigned>(uid),
                  InodePath(ino).c_str(), size, static_cast<long long>(off));
    gInPfReaddirplus = true;
    gPfReaddirUid = uid;
    gPfReaddirIno = ino;
    fn(req, ino, size, off, fi);
    gPfReaddirIno = 0;
    gPfReaddirUid = 0;
    gInPfReaddirplus = false;
}

extern "C" int WrappedNotifyInvalEntry(void* se, uint64_t parent, const char* name,
                                       size_t namelen) {
    auto fn =
        reinterpret_cast<int (*)(void*, uint64_t, const char*, size_t)>(gOriginalNotifyInvalEntry);
    int ret = fn ? fn(se, parent, name, namelen) : -1;
    DebugLogPrint(3, "notify_inval_entry: ino=0x%lx name=%s ret=%d", (unsigned long)parent,
                  name ? DebugPreview(std::string_view(name, namelen)).c_str() : "null", ret);
    return ret;
}

extern "C" int WrappedNotifyInvalInode(void* se, uint64_t ino, off_t off, off_t len) {
    auto fn = reinterpret_cast<int (*)(void*, uint64_t, off_t, off_t)>(gOriginalNotifyInvalInode);
    int ret = fn ? fn(se, ino, off, len) : -1;
    // Device libfuse_jni routes a fallback invalidation path through notify_inval_inode().
    // The callback receives an inode handle, not a verified node object, so only log the rawvalue
    // here.
    DebugLogPrint(3, "notify_inval_inode: ino=0x%lx name=%s ret=%d", (unsigned long)ino,
                  ino == 1 ? "(ROOT)" : "", ret);
    return ret;
}

// This is the strongest uid-specific hiding point. Once a positive fuse_entry_param escapes here,
// later operations such as getattr, getxattr, or create can still observe existence through the
// resolved inode even if later path-based checks return false.
// AOSP references: jni/FuseDaemon.cpp#912, #1166, and #1211
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#912
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1166
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1211
extern "C" int WrappedReplyEntry(fuse_req_t req, const struct fuse_entry_param* e) {
    auto fn =
        reinterpret_cast<int (*)(fuse_req_t, const struct fuse_entry_param*)>(gOriginalReplyEntry);
    const bool hiddenLookupForUid = HiddenPathPolicy::IsTestHiddenUid(RuntimeState::ReqUid(req)) &&
                                    (gTrackRootHiddenLookup || gTrackHiddenSubtreeLookup);
    if (hiddenLookupForUid) {
        if (gTrackRootHiddenLookup) {
            ArmHiddenCreateLeakRemap(req, "fuse_reply_entry");
        }
        if (auto ret = ReplyErrorBridge::Reply(req, ENOENT, "fuse_reply_entry"); ret.has_value()) {
            DebugLogPrint(4, "hide lookup entry uid=%u req=%lu ino=%s root=%d child=%d ret=%d",
                          static_cast<unsigned>(RuntimeState::ReqUid(req)),
                          req ? (unsigned long)req->unique : 0UL,
                          e != nullptr ? InodePath(e->ino).c_str() : "(null)",
                          gTrackRootHiddenLookup ? 1 : 0, gTrackHiddenSubtreeLookup ? 1 : 0, *ret);
            RuntimeState::ScheduleHiddenEntryInvalidation();
            return *ret;
        }
    }
    fuse_entry_param patchedEntry = {};
    const struct fuse_entry_param* replyEntry = e;
    if (e != nullptr && (gTrackRootHiddenLookup || gTrackHiddenSubtreeLookup)) {
        patchedEntry = *e;
        patchedEntry.entry_timeout = 0.0;
        patchedEntry.attr_timeout = 0.0;
        replyEntry = &patchedEntry;
        DebugLogPrint(4, "disable entry cache req=%lu ino=%s root=%d child=%d",
                      req ? (unsigned long)req->unique : 0UL, InodePath(e->ino).c_str(),
                      gTrackRootHiddenLookup ? 1 : 0, gTrackHiddenSubtreeLookup ? 1 : 0);
        RuntimeState::ScheduleHiddenEntryInvalidation();
        if (TrackHiddenSubtreeInode(e->ino)) {
            RuntimeState::ScheduleHiddenInodeInvalidation(e->ino);
        }
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

// get_entry_timeout() only controls dentry caching; pf_getattr still replies with a separate
// attr timeout. Force both to zero when the request touches the hidden subtree.
// AOSP references: jni/FuseDaemon.cpp#510 and #1002
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#510
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1002
extern "C" int WrappedReplyAttr(fuse_req_t req, const struct stat* attr, double timeout) {
    auto fn = reinterpret_cast<int (*)(fuse_req_t, const struct stat*, double)>(gOriginalReplyAttr);
    const double replyTimeout = gZeroAttrCacheForCurrentGetattr ? 0.0 : timeout;
    if (gZeroAttrCacheForCurrentGetattr) {
        DebugLogPrint(4, "disable attr cache req=%p timeout=%.2le", req, replyTimeout);
    }
    return fn ? fn(req, attr, replyTimeout) : -1;
}

// reply_buf is the last universal filtering point. AOSP emits directory data through plain readdir,
// readdir postfilter, readdirplus, and lookup_postfilter using different wire layouts, so auto-
// detecting dirent and direntplus records here is more reliable than betting on one upstream path.
// AOSP references: jni/FuseDaemon.cpp#946, #1941, and #1997
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#946
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1941
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1997
extern "C" int WrappedReplyBuf(fuse_req_t req, const char* buf, size_t size) {
    auto fn = reinterpret_cast<int (*)(fuse_req_t, const char*, size_t)>(gOriginalReplyBuf);
    const char* replyBuf = buf;
    size_t replySize = size;
    std::vector<char> filteredStorage;
    size_t removedCount = 0;
    const uint32_t reqUid = RuntimeState::ReqUid(req);
    const uint32_t filterUid = gPfReaddirUid != 0 ? gPfReaddirUid : reqUid;
    const uint64_t filterIno = gPfReaddirIno != 0 ? gPfReaddirIno : 0;
    const bool filterPlainReaddir = gInPfReaddir;
    const bool filterPostfilterReaddir = gInPfReaddirPostfilter;
    const bool filterReaddirplus = gInPfReaddirplus;
    const bool requireParentMatch = filterIno != 0;
    const char* filterMode = nullptr;

    if (HiddenPathPolicy::IsTestHiddenUid(filterUid)) {
        if (filterPlainReaddir) {
            if (DirentFilter::BuildFilteredDirentPayload(buf, size, filterUid, filterIno,
                                                         &filteredStorage, &removedCount,
                                                         requireParentMatch)) {
                replyBuf = filteredStorage.data();
                replySize = filteredStorage.size();
                filterMode = "readdir";
            }
        } else if (filterReaddirplus) {
            if (DirentFilter::BuildFilteredDirentplusPayload(buf, size, filterUid, filterIno,
                                                             &filteredStorage, &removedCount,
                                                             requireParentMatch)) {
                replyBuf = filteredStorage.data();
                replySize = filteredStorage.size();
                filterMode = "readdirplus";
            }
        } else if (filterPostfilterReaddir && size >= sizeof(fuse_read_out)) {
            const auto* readOut = reinterpret_cast<const fuse_read_out*>(buf);
            const size_t payloadSize =
                std::min<size_t>(readOut->size, size - sizeof(fuse_read_out));
            std::vector<char> filteredPayload;
            if (DirentFilter::BuildFilteredDirentPayload(buf + sizeof(fuse_read_out), payloadSize,
                                                         filterUid, filterIno, &filteredPayload,
                                                         &removedCount, requireParentMatch)) {
                fuse_read_out patched = *readOut;
                patched.size = static_cast<uint32_t>(filteredPayload.size());
                filteredStorage.resize(sizeof(patched) + filteredPayload.size());
                std::memcpy(filteredStorage.data(), &patched, sizeof(patched));
                std::memcpy(filteredStorage.data() + sizeof(patched), filteredPayload.data(),
                            filteredPayload.size());
                replyBuf = filteredStorage.data();
                replySize = filteredStorage.size();
                filterMode = "readdir_postfilter";
            }
        }

        if (filterMode == nullptr) {
            if (DirentFilter::BuildFilteredDirentplusPayload(
                    buf, size, filterUid, 0, &filteredStorage, &removedCount, false)) {
                replyBuf = filteredStorage.data();
                replySize = filteredStorage.size();
                filterMode = "auto_direntplus";
            } else if (DirentFilter::BuildFilteredDirentPayload(
                           buf, size, filterUid, 0, &filteredStorage, &removedCount, false)) {
                replyBuf = filteredStorage.data();
                replySize = filteredStorage.size();
                filterMode = "auto_dirent";
            } else if (size >= sizeof(fuse_read_out)) {
                const auto* readOut = reinterpret_cast<const fuse_read_out*>(buf);
                const size_t payloadSize =
                    std::min<size_t>(readOut->size, size - sizeof(fuse_read_out));
                std::vector<char> filteredPayload;
                if (DirentFilter::BuildFilteredDirentPayload(
                        buf + sizeof(fuse_read_out), payloadSize, filterUid, 0, &filteredPayload,
                        &removedCount, false)) {
                    fuse_read_out patched = *readOut;
                    patched.size = static_cast<uint32_t>(filteredPayload.size());
                    filteredStorage.resize(sizeof(patched) + filteredPayload.size());
                    std::memcpy(filteredStorage.data(), &patched, sizeof(patched));
                    std::memcpy(filteredStorage.data() + sizeof(patched), filteredPayload.data(),
                                filteredPayload.size());
                    replyBuf = filteredStorage.data();
                    replySize = filteredStorage.size();
                    filterMode = "auto_read_out_dirent";
                }
            }
        }
    }

    int ret = fn ? fn(req, replyBuf, replySize) : -1;
    if (removedCount != 0) {
        DebugLogPrint(4, "filtered readdir reply mode=%s uid=%u ino=%s removed=%zu size=%zu->%zu",
                      filterMode ? filterMode : "unknown", static_cast<unsigned>(filterUid),
                      InodePath(filterIno).c_str(), removedCount, size, replySize);
    }
    if (gInPfLookupPostfilter) {
        DebugLogPrint(3, "pf_lookup_postfilter fuse_reply_buf req=%p", req);
    } else {
        DebugLogPrint(3, "fuse_reply_buf: req=%lu size=%zu ret=%d", (unsigned long)req->unique,
                      replySize, ret);
    }
    return ret;
}

extern "C" int WrappedReplyErr(fuse_req_t req, int err) {
    auto fn = ReplyErrorBridge::Original();
    err = MaybeRewriteHiddenLeakErrno(req, err, "fuse_reply_err");
    int ret = fn ? fn(req, err) : -1;
    if (gInPfLookupPostfilter) {
        DebugLogPrint(3, "pf_lookup_postfilter fuse_reply_err req=%p %d", req, err);
    } else {
        DebugLogPrint(3, "fuse_reply_err: req=%p err=%d ret=%d", req, err, ret);
    }
    return ret;
}

extern "C" void WrappedPfGetattr(fuse_req_t req, uint64_t ino, void* fi) {
    RuntimeState::RememberFuseSession(req);
    const uint32_t uid = RuntimeState::ReqUid(req);
    gZeroAttrCacheForCurrentGetattr = IsTrackedHiddenSubtreeInode(ino);
    if (HiddenPathPolicy::IsTestHiddenUid(uid)) {
        DebugLogPrint(4, "pf_getattr test uid=%u ino=0x%lx", static_cast<unsigned>(uid),
                      (unsigned long)ino);
    }
    auto fn = reinterpret_cast<void (*)(fuse_req_t, uint64_t, void*)>(gOriginalPfGetattr);
    if (fn) {
        gInPfGetattr = true;
        gPfGetattrUid = uid;
        gPfGetattrIno = ino;
        fn(req, ino, fi);
        gPfGetattrIno = 0;
        gPfGetattrUid = 0;
        gInPfGetattr = false;
        gZeroAttrCacheForCurrentGetattr = false;
    }
}

// lstat is the path-based source of truth used by pf_getattr and by some enumeration paths. This is
// where we convert a visible subtree path back into cache invalidation state.
// lstat is the path-based source of truth for pf_getattr and is also consulted by readdir
// postfilter. Recording the root parent inode here avoids assuming a fixed root inode value.
// AOSP references: jni/FuseDaemon.cpp#1002 and #1985
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1002
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1985
extern "C" int WrappedLstat(const char* path, struct stat* st) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    if (gInPfGetattr && gPfGetattrIno != 0 &&
        HiddenPathPolicy::IsHiddenRootDirectoryPath(pathView)) {
        uint64_t expected = 0;
        if (gHiddenRootParentInode.compare_exchange_strong(expected, gPfGetattrIno,
                                                           std::memory_order_relaxed)) {
            DebugLogPrint(4, "record hidden root parent from getattr=%s path=%s",
                          InodePath(gPfGetattrIno).c_str(), DebugPreview(pathView).c_str());
        }
        RemoveTrackedHiddenSubtreeInode(gPfGetattrIno);
    }
    NoteHiddenSubtreePathForCache(pathView);
    if (gInPfGetattr && HiddenPathPolicy::IsTestHiddenUid(gPfGetattrUid)) {
        DebugLogPrint(4, "pf_getattr lstat uid=%u path=%s", static_cast<unsigned>(gPfGetattrUid),
                      DebugPreview(pathView).c_str());
        if (HiddenPathPolicy::ShouldHideTestPath(gPfGetattrUid, pathView)) {
            DebugLogPrint(4, "hide test lstat uid=%u path=%s", static_cast<unsigned>(gPfGetattrUid),
                          DebugPreview(pathView).c_str());
            errno = ENOENT;
            return -1;
        }
    }
    auto fn = reinterpret_cast<int (*)(const char*, struct stat*)>(gOriginalLstat);
    if (fn) {
        return fn(path, st);
    }
    errno = ENOSYS;
    return -1;
}

extern "C" int WrappedStat(const char* path, struct stat* st) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    if (gInPfReaddirPostfilter && HiddenPathPolicy::IsTestHiddenUid(gPfReaddirUid) &&
        HiddenPathPolicy::IsAnyHiddenSubtreePath(pathView)) {
        DebugLogPrint(4, "hide readdir stat uid=%u path=%s", static_cast<unsigned>(gPfReaddirUid),
                      DebugPreview(pathView).c_str());
        errno = ENOENT;
        return -1;
    }
    auto fn = reinterpret_cast<int (*)(const char*, struct stat*)>(gOriginalStat);
    if (fn) {
        return fn(path, st);
    }
    errno = ENOSYS;
    return -1;
}

// Even if the named FUSE wrappers are missed on a device-specific path, lower-fs mkdir/mknod/open
// calls still carry the final child path. These libc hooks are the last fallback for create/mkdir.
extern "C" int WrappedMkdirLibc(const char* path, mode_t mode) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    if (HiddenPathPolicy::IsExactHiddenTargetPath(pathView)) {
        DebugLogPrint(4, "hide mkdir path=%s", DebugPreview(pathView).c_str());
        errno = EACCES;
        return -1;
    }
    auto fn = reinterpret_cast<int (*)(const char*, mode_t)>(gOriginalMkdir);
    if (fn) {
        return fn(path, mode);
    }
    errno = ENOSYS;
    return -1;
}

extern "C" int WrappedMknod(const char* path, mode_t mode, dev_t dev) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    if (HiddenPathPolicy::IsExactHiddenTargetPath(pathView)) {
        DebugLogPrint(4, "hide mknod path=%s", DebugPreview(pathView).c_str());
        errno = EPERM;
        return -1;
    }
    auto fn = reinterpret_cast<int (*)(const char*, mode_t, dev_t)>(gOriginalMknod);
    if (fn) {
        return fn(path, mode, dev);
    }
    errno = ENOSYS;
    return -1;
}

extern "C" int WrappedOpen(const char* path, int flags, ...) {
    mode_t mode = 0;
    if ((flags & O_CREAT) != 0) {
        va_list args;
        va_start(args, flags);
        mode = static_cast<mode_t>(va_arg(args, int));
        va_end(args);
    }
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    if ((flags & O_CREAT) != 0 && HiddenPathPolicy::IsExactHiddenTargetPath(pathView)) {
        DebugLogPrint(4, "hide open create path=%s flags=0x%x", DebugPreview(pathView).c_str(),
                      flags);
        errno = EPERM;
        return -1;
    }
    auto fn = reinterpret_cast<int (*)(const char*, int, ...)>(gOriginalOpen);
    if (fn) {
        if ((flags & O_CREAT) != 0) {
            return fn(path, flags, mode);
        }
        return fn(path, flags);
    }
    errno = ENOSYS;
    return -1;
}

extern "C" int WrappedOpen2(const char* path, int flags) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    if ((flags & O_CREAT) != 0 && HiddenPathPolicy::IsExactHiddenTargetPath(pathView)) {
        DebugLogPrint(4, "hide __open_2 create path=%s flags=0x%x", DebugPreview(pathView).c_str(),
                      flags);
        errno = EPERM;
        return -1;
    }
    auto fn = reinterpret_cast<int (*)(const char*, int)>(gOriginalOpen2);
    if (fn) {
        return fn(path, flags);
    }
    errno = ENOSYS;
    return -1;
}

// Path hook wrappers

bool HiddenPathPolicy::IsTestHiddenUid(uint32_t uid) {
    {
        std::lock_guard<std::mutex> lock(gUidHideCacheMutex);
        const auto it = gUidHideCache.find(uid);
        if (it != gUidHideCache.end()) {
            return it->second;
        }
    }

    const std::optional<bool> resolved = ResolveShouldHideUidWithPackageManager(uid);
    if (!resolved.has_value()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(gUidHideCacheMutex);
        gUidHideCache[uid] = *resolved;
    }
    return *resolved;
}

bool HiddenPathPolicy::ShouldHideTestPath(uint32_t uid, std::string_view path) {
    return IsTestHiddenUid(uid) && IsAnyHiddenSubtreePath(path);
}

// Mirror the original app-accessible gate: sanitize only when needed, then delegate.
bool WrappedIsAppAccessiblePath(void* fuse, const std::string& path, uint32_t uid) {
    if (gOriginalIsAppAccessiblePath == nullptr) {
        return false;
    }
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
        return gOriginalIsAppAccessiblePath(fuse, path, uid);
    }
    std::string sanitized(path);
    UnicodePolicy::RewriteString(sanitized);
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
    return gOriginalIsAppAccessiblePath(fuse, sanitized, uid);
}

// The package-owned helper only sanitizes the first path argument on the device build.
bool WrappedIsPackageOwnedPath(const std::string& lhs, const std::string& rhs) {
    if (gOriginalIsPackageOwnedPath == nullptr) {
        return false;
    }
    if (!UnicodePolicy::NeedsSanitization(lhs)) {
        UnicodePolicy::LogSuspiciousDirectPath("package_owned", lhs);
        if (ShouldLogLimited(gPackageOwnedLogCount)) {
            DebugLogPrint(3, "package_owned direct lhs=%s rhs=%s", DebugPreview(lhs).c_str(),
                          DebugPreview(rhs).c_str());
        }
        return gOriginalIsPackageOwnedPath(lhs, rhs);
    }
    std::string sanitizedLhs(lhs);
    UnicodePolicy::RewriteString(sanitizedLhs);
    if (ShouldLogLimited(gPackageOwnedLogCount)) {
        DebugLogPrint(3, "package_owned rewrite lhs=%s new=%s rhs=%s", DebugPreview(lhs).c_str(),
                      DebugPreview(sanitizedLhs).c_str(), DebugPreview(rhs).c_str());
    }
    return gOriginalIsPackageOwnedPath(sanitizedLhs, rhs);
}

// WrappedIsBpfBackingPath
bool WrappedIsBpfBackingPath(const std::string& path) {
    if (gOriginalIsBpfBackingPath == nullptr) {
        return false;
    }
    if (!UnicodePolicy::NeedsSanitization(path)) {
        UnicodePolicy::LogSuspiciousDirectPath("bpf_backing", path);
        if (ShouldLogLimited(gBpfBackingLogCount)) {
            DebugLogPrint(3, "bpf_backing direct path=%s", DebugPreview(path).c_str());
        }
        return gOriginalIsBpfBackingPath(path);
    }
    std::string sanitized(path);
    UnicodePolicy::RewriteString(sanitized);
    if (ShouldLogLimited(gBpfBackingLogCount)) {
        DebugLogPrint(3, "bpf_backing rewrite old=%s new=%s", DebugPreview(path).c_str(),
                      DebugPreview(sanitized).c_str());
    }
    return gOriginalIsBpfBackingPath(sanitized);
}

// Keep libc strcasecmp behavior aligned with the original case-folding compare.
extern "C" int WrappedStrcasecmp(const char* lhs, const char* rhs) {
    const size_t lhsLen = (lhs != nullptr) ? std::strlen(lhs) : 0;
    const size_t rhsLen = (rhs != nullptr) ? std::strlen(rhs) : 0;
    const int result = UnicodePolicy::CompareCaseFoldIgnoringDefaultIgnorables(
        reinterpret_cast<const uint8_t*>(lhs ? lhs : ""), lhsLen,
        reinterpret_cast<const uint8_t*>(rhs ? rhs : ""), rhsLen);
    if (ShouldLogLimited(gStrcasecmpLogCount)) {
        DebugLogPrint(3, "strcasecmp lhs=%s rhs=%s result=%d",
                      DebugPreview(std::string_view(lhs ? lhs : "", lhsLen)).c_str(),
                      DebugPreview(std::string_view(rhs ? rhs : "", rhsLen)).c_str(), result);
    }
    return result;
}

// ABI adapter for android::base::EqualsIgnoreCase(string_view, string_view).
extern "C" bool WrappedEqualsIgnoreCaseAbi(const char* lhsData, size_t lhsSize, const char* rhsData,
                                           size_t rhsSize) {
    const int result = UnicodePolicy::CompareCaseFoldIgnoringDefaultIgnorables(
        reinterpret_cast<const uint8_t*>(lhsData ? lhsData : ""), lhsSize,
        reinterpret_cast<const uint8_t*>(rhsData ? rhsData : ""), rhsSize);
    if (ShouldLogLimited(gEqualsIgnoreCaseLogCount)) {
        DebugLogPrint(3, "equals_ignore_case lhs=%s rhs=%s result=%d",
                      DebugPreview(std::string_view(lhsData ? lhsData : "", lhsSize)).c_str(),
                      DebugPreview(std::string_view(rhsData ? rhsData : "", rhsSize)).c_str(),
                      result);
    }
    return result == 0;
}

bool IsTestHiddenUid(uint32_t uid) {
    return HiddenPathPolicy::IsTestHiddenUid(uid);
}

bool ShouldHideTestPath(uint32_t uid, std::string_view path) {
    return HiddenPathPolicy::ShouldHideTestPath(uid, path);
}

}  // namespace fusefixer
