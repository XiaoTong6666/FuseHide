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

#include "fusehide/hooks/wrapper_context.hpp"
#include "fusehide/hooks/wrappers.hpp"

#include "fusehide/core/root_snapshot_cache.hpp"
#include "fusehide/filters/dirent_filter.hpp"
#include "fusehide/filters/readdir_cookie.hpp"
#include "fusehide/filters/reply_buf_filter.hpp"
#include "fusehide/policy/path_policy.hpp"

namespace fusehide {
namespace {

bool IsFirstComponentOfHiddenRelativePath(uint32_t uid, std::string_view name) {
    if (name.empty() || name.find('/') != std::string_view::npos) {
        return false;
    }
    const auto& rule = BorrowHideRuleForUid(uid);
    if (rule == nullptr) {
        return false;
    }
    return rule->hiddenRelativePathFirstComponentSet.find(CanonicalizeHiddenEntryNameForMatch(
               name)) != rule->hiddenRelativePathFirstComponentSet.end();
}

void InvalidateFilteredParentChildren(uint32_t uid, std::string_view parentPath,
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
    const auto& rule = BorrowHideRuleForUid(uid);
    const bool wildcardRoot = IsVisibleRootPath(parentPath) && rule != nullptr &&
                              HideAllForStorageRoot(*rule, parentPath);
    if (wildcardRoot) {
        // A session root sweep covers the filtered names without queuing two tasks per entry.
        RuntimeState::ScheduleHiddenEntryInvalidation();
    }
    for (const auto& entry : removedEntries) {
        if (!wildcardRoot)
            RuntimeState::ScheduleSpecificEntryInvalidation(*parentIno, entry.name);
        if (entry.ino != 0) {
            const std::string childPath =
                HiddenPathPolicy::JoinPathComponent(parentPath, entry.name);
            RememberTrackedPathForInode(entry.ino, childPath);
            if (TrackHiddenSubtreeInode(uid, entry.ino)) {
                DebugLogPrint(4, "track filtered child inode parent=%s child=%s ino=%s",
                              DebugPreview(parentPath).c_str(), DebugPreview(entry.name).c_str(),
                              InodePath(entry.ino).c_str());
            }
            if (!wildcardRoot)
                RuntimeState::ScheduleHiddenInodeInvalidation(entry.ino);
        }
    }
    DebugLogPrint(4, "invalidate filtered children parent=%s ino=%s names=%zu",
                  DebugPreview(parentPath).c_str(), InodePath(*parentIno).c_str(),
                  removedEntries.size());
}

}  // namespace

extern "C" int WrappedNotifyInvalEntry(void* se, uint64_t parent, const char* name,
                                       size_t namelen) {
    const auto fn = gOriginalNotifyInvalEntry.get();
    int ret = fn ? fn(se, parent, name, namelen) : -1;
    DebugLogPrint(3, "notify_inval_entry: ino=0x%lx name=%s ret=%d", (unsigned long)parent,
                  name ? DebugPreview(std::string_view(name, namelen)).c_str() : "null", ret);
    return ret;
}

extern "C" int WrappedNotifyInvalInode(void* se, uint64_t ino, off_t off, off_t len) {
    const auto fn = gOriginalNotifyInvalInode.get();
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
    const ScopedFuseRequestSession scopedSession(req);
    const auto fn = gOriginalReplyEntry.get();
    const uint32_t lookupUid = RuntimeState::ReqUid(req);
    // A positive MediaProvider lookup is direct evidence of the returned
    // child's current parent/name, *provided that the parent itself has a
    // verified path*. This closes the rename fallback gap on devices whose
    // FUSE ABI does not permit installing the pf_rename wrapper (x86_64 AVD).
    // Do it before consulting historical hidden inode marks, so an old
    // hidden directory moved to a visible path cannot poison its children.
    if (e != nullptr && e->ino != 0 && gInPfLookup && gCurrentLookupParentInode != 0 &&
        !gCurrentLookupName.empty()) {
        if (const auto verifiedParent =
                LookupVerifiedTrackedPathForInode(gCurrentLookupParentInode)) {
            RememberTrackedPathForInode(
                e->ino, HiddenPathPolicy::JoinPathComponent(*verifiedParent, gCurrentLookupName),
                true);
        }
    }
    // A concurrent authoritative readdir/lstat may have corrected this parent
    // since WrappedPfLookup captured its old hidden mark. Revalidate before
    // sending an irreversible ENOENT to the kernel.
    if (gTrackHiddenSubtreeLookup &&
        !IsTrackedHiddenSubtreeInode(lookupUid, gCurrentLookupParentInode)) {
        DebugLogPrint(4, "hide-diag drop_stale_lookup uid=%u parent=0x%llx name=%s",
                      static_cast<unsigned>(lookupUid),
                      static_cast<unsigned long long>(gCurrentLookupParentInode),
                      DebugPreview(gCurrentLookupName).c_str());
        gTrackHiddenSubtreeLookup = false;
    }
    if (gTrackRootHiddenLookup &&
        !IsHiddenLookupCacheTarget(
            lookupUid, gCurrentLookupParentInode,
            gCurrentLookupName.empty() ? nullptr : gCurrentLookupName.data())) {
        DebugLogPrint(4, "hide-diag drop_stale_root_lookup uid=%u parent=0x%llx",
                      static_cast<unsigned>(lookupUid),
                      static_cast<unsigned long long>(gCurrentLookupParentInode));
        gTrackRootHiddenLookup = false;
    }
    const bool hiddenLookupForUid = HiddenPathPolicy::IsTestHiddenUid(lookupUid) &&
                                    (gTrackRootHiddenLookup || gTrackHiddenSubtreeLookup);
    // An ENOENT root lookup cannot learn e->ino through a successful reply.
    // The root sweep already invalidates this first-level dentry in hide-all mode.
    // Avoid scheduling a forever-unresolvable inode invalidation for that child.
    bool rootSweepCoversLookup = false;
    if (gTrackRootHiddenLookup && gCurrentLookupParentInode != 0 && !gCurrentLookupName.empty()) {
        const auto parentPath = LookupTrackedPathForInode(gCurrentLookupParentInode);
        const auto& rule = BorrowHideRuleForUid(lookupUid);
        rootSweepCoversLookup = parentPath && IsVisibleRootPath(*parentPath) && rule &&
                                HideAllForStorageRoot(*rule, *parentPath) &&
                                IsWildcardRootEntryCandidate(gCurrentLookupName, *parentPath);
    }
    if (hiddenLookupForUid) {
        if (gTrackRootHiddenLookup || gTrackHiddenSubtreeLookup) {
            ArmHiddenCreateLeakRemap(req, "fuse_reply_entry");
        }
        // A successful *underlying* lookup already supplies the real inode.
        // Even though we suppress its positive reply for this UID, remember
        // the verified parent/name relationship before scheduling an exact
        // inode invalidation. Otherwise the notification is deferred forever
        // (and a later config reload correctly remains ready=false).
        if (e != nullptr && e->ino != 0 && gCurrentLookupParentInode != 0 &&
            !gCurrentLookupName.empty()) {
            if (const auto parentPath = LookupTrackedPathForInode(gCurrentLookupParentInode);
                parentPath.has_value()) {
                RememberTrackedPathForInode(
                    e->ino, HiddenPathPolicy::JoinPathComponent(*parentPath, gCurrentLookupName));
            }
        }
        if (gTrackHiddenSubtreeLookup) {
            if (const auto parentPath = LookupTrackedPathForInode(gCurrentLookupParentInode);
                parentPath.has_value()) {
                RememberTrackedPathForInode(gCurrentLookupParentInode, *parentPath);
                RememberRecentHiddenParentPath(RuntimeState::ReqUid(req), *parentPath);
                DebugLogPrint(4, "refresh recent hidden parent from hidden lookup uid=%u path=%s",
                              static_cast<unsigned>(RuntimeState::ReqUid(req)),
                              DebugPreview(*parentPath).c_str());
                RuntimeState::ScheduleHiddenInodeInvalidation(gCurrentLookupParentInode);
            }
        }
        if (auto ret = ReplyErrorBridge::Reply(req, ENOENT, "fuse_reply_entry"); ret.has_value()) {
            DebugLogPrint(4, "hide lookup entry uid=%u req=%lu ino=%s root=%d child=%d ret=%d",
                          static_cast<unsigned>(RuntimeState::ReqUid(req)),
                          req ? (unsigned long)req->unique : 0UL,
                          e != nullptr ? InodePath(e->ino).c_str() : "(null)",
                          gTrackRootHiddenLookup ? 1 : 0, gTrackHiddenSubtreeLookup ? 1 : 0, *ret);
            RuntimeState::ScheduleHiddenEntryInvalidation();
            if (gCurrentLookupParentInode != 0 && !gCurrentLookupName.empty()) {
                RuntimeState::ScheduleSpecificEntryInvalidation(gCurrentLookupParentInode,
                                                                gCurrentLookupName);
            }
            if (e != nullptr && e->ino != 0 && TrackHiddenSubtreeInode(lookupUid, e->ino) &&
                !rootSweepCoversLookup) {
                RuntimeState::ScheduleHiddenInodeInvalidation(e->ino);
            }
            return *ret;
        }
    }
    fuse_entry_param patchedEntry = {};
    const struct fuse_entry_param* replyEntry = e;
    // Even a successful lookup by an unrestricted UID must not create a
    // positive dentry that a restricted UID can later stat/open without FUSE.
    bool forceUncachedReplyEntry =
        gTrackRootHiddenLookup || gTrackHiddenSubtreeLookup || gCrossUidSensitiveLookup;
    bool exactHiddenTargetReplyEntry = false;
    if (e != nullptr && (gTrackRootHiddenLookup || gTrackHiddenSubtreeLookup)) {
        patchedEntry = *e;
        patchedEntry.entry_timeout = 0.0;
        patchedEntry.attr_timeout = 0.0;
        replyEntry = &patchedEntry;
        DebugLogPrint(4, "disable entry cache req=%lu ino=%s root=%d child=%d",
                      req ? (unsigned long)req->unique : 0UL, InodePath(e->ino).c_str(),
                      gTrackRootHiddenLookup ? 1 : 0, gTrackHiddenSubtreeLookup ? 1 : 0);
        RuntimeState::ScheduleHiddenEntryInvalidation();
        if (TrackHiddenSubtreeInode(lookupUid, e->ino) && !rootSweepCoversLookup) {
            RuntimeState::ScheduleHiddenInodeInvalidation(e->ino);
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
    if (e != nullptr && gCurrentLookupParentInode != 0 && !gCurrentLookupName.empty()) {
        std::optional<std::string> childPath;
        if (const auto parentPath = LookupTrackedPathForInode(gCurrentLookupParentInode);
            parentPath.has_value()) {
            if (IsVisibleRootPath(*parentPath)) {
                RememberVisibleRootParentInode(gCurrentLookupParentInode,
                                               "reply_entry_parent_path");
            }
            childPath = HiddenPathPolicy::JoinPathComponent(*parentPath, gCurrentLookupName);
        } else if (const auto visibleRootPath = InferVisibleRootPathFromLookup(
                       gCurrentLookupParentInode, gCurrentLookupName);
                   visibleRootPath.has_value()) {
            childPath = *visibleRootPath;
        } else if (IsFirstComponentOfHiddenRelativePath(RuntimeState::ReqUid(req),
                                                        gCurrentLookupName)) {
            const auto rootPath = LookupTrackedPathForInode(CurrentRootParentInode());
            if (rootPath && IsVisibleRootPath(*rootPath))
                childPath = HiddenPathPolicy::JoinPathComponent(*rootPath, gCurrentLookupName);
            DebugLogPrint(4, "infer root child path ino=%s name=%s path=%s",
                          InodePath(gCurrentLookupParentInode).c_str(),
                          DebugPreview(gCurrentLookupName).c_str(),
                          DebugPreview(*childPath).c_str());
        }
        if (childPath.has_value()) {
            const uint32_t uid = RuntimeState::ReqUid(req);
            RememberTrackedPathForInode(e->ino, *childPath);
            if (IsVisibleRootPath(*childPath)) {
                RememberVisibleRootParentInode(e->ino, "reply_entry_visible_root");
            }
            const bool hiddenSubtreePath =
                HiddenPathPolicy::IsAnyHiddenSubtreePath(uid, *childPath);
            exactHiddenTargetReplyEntry =
                HiddenPathPolicy::IsExactHiddenTargetPath(uid, *childPath);
            if (hiddenSubtreePath) {
                TrackHiddenSubtreeInode(uid, e->ino);
                forceUncachedReplyEntry = true;
            }
            if (exactHiddenTargetReplyEntry) {
                forceUncachedReplyEntry = true;
                RuntimeState::ScheduleSpecificEntryInvalidation(gCurrentLookupParentInode,
                                                                gCurrentLookupName);
                RuntimeState::ScheduleHiddenInodeInvalidation(e->ino);
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
                      gTrackRootHiddenLookup ? 1 : 0, gTrackHiddenSubtreeLookup ? 1 : 0,
                      exactHiddenTargetReplyEntry ? 1 : 0);
    }
    int ret = fn ? fn(req, replyEntry) : -1;
    const std::string replyIno = replyEntry != nullptr ? InodePath(replyEntry->ino) : "(null)";
    const double replyEntryTimeout = replyEntry != nullptr ? replyEntry->entry_timeout : 0.0;
    const double replyAttrTimeout = replyEntry != nullptr ? replyEntry->attr_timeout : 0.0;
    const unsigned long replyBpfFd =
        replyEntry != nullptr ? (unsigned long)replyEntry->bpf_fd : 0UL;
    const unsigned long replyBpfAction =
        replyEntry != nullptr ? (unsigned long)replyEntry->bpf_action : 0UL;
    const unsigned long replyBackingAction =
        replyEntry != nullptr ? (unsigned long)replyEntry->backing_action : 0UL;
    const unsigned long replyBackingFd =
        replyEntry != nullptr ? (unsigned long)replyEntry->backing_fd : 0UL;
    DebugLogPrint(3,
                  "fuse_reply_entry: req=%lu ino=%s timeout=%.2le attr_timeout=%.2le bpf_fd=%lu "
                  "bpf_action=%lu backing_action=%lu backing_fd=%lu ret=%d",
                  req ? (unsigned long)req->unique : 0UL, replyIno.c_str(), replyEntryTimeout,
                  replyAttrTimeout, replyBpfFd, replyBpfAction, replyBackingAction, replyBackingFd,
                  ret);
    return ret;
}

// get_entry_timeout() only controls dentry caching; pf_getattr still replies with a separate
// attr timeout. Force both to zero when the request touches the hidden subtree.
// AOSP references: jni/FuseDaemon.cpp#510 and #1002
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#510
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1002
extern "C" int WrappedReplyAttr(fuse_req_t req, const struct stat* attr, double timeout) {
    const ScopedFuseRequestSession scopedSession(req);
    const auto fn = gOriginalReplyAttr.get();
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
    const ScopedFuseRequestSession scopedSession(req);
    const auto fn = gOriginalReplyBuf.get();
    const char* replyBuf = buf;
    size_t replySize = size;
    std::vector<char> filteredStorage;
    size_t removedCount = 0;
    std::vector<FilteredDirentMatch> removedEntries;
    PendingReaddirContext pendingContext{};
    const uint32_t reqUid = RuntimeState::ReqUid(req);
    bool hasPendingContext = SnapshotActiveReaddirContext(req, &pendingContext);
    if (!hasPendingContext && req != nullptr &&
        (reqUid == 0 || BorrowHideRuleForUid(reqUid) != nullptr)) {
        if (const auto stored = GetPendingReaddirContext(req->unique)) {
            pendingContext = *stored;
            hasPendingContext = true;
        }
    }
    const uint32_t requestFilterUid =
        gPfReaddirUid != 0
            ? gPfReaddirUid
            : (hasPendingContext && pendingContext.uid != 0 ? pendingContext.uid : reqUid);
    const uint64_t filterIno =
        gPfReaddirIno != 0 ? gPfReaddirIno : (hasPendingContext ? pendingContext.ino : 0);
    const bool filterPlainReaddir = gInPfReaddir;
    const bool filterPostfilterReaddir = gInPfReaddirPostfilter;
    const bool filterReaddirplus = gInPfReaddirplus;
    const bool requireParentMatch = filterIno != 0;
    if (filterReaddirplus && filterIno == 1)
        RememberEmulatedZeroFromRootDirentplus(CurrentSessionToken(), filterIno, buf, size);
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
        RememberTrackedPathForInode(filterIno, pendingContext.path, true);
    }

    if (HiddenPathPolicy::IsTestHiddenUid(filterUid)) {
        DebugLogPrint(
            4,
            "reply_buf filter ctx req=%lu req_uid=%u uid=%u ino=%s pending=%d pending_path=%s "
            "fallback_parent=%s fallback_uid=%u mode_flags=%d/%d/%d",
            req ? (unsigned long)req->unique : 0UL, static_cast<unsigned>(requestFilterUid),
            static_cast<unsigned>(filterUid), InodePath(filterIno).c_str(),
            hasPendingContext ? 1 : 0,
            pendingContext.path.empty() ? "" : DebugPreview(pendingContext.path).c_str(),
            fallbackParentPath.has_value() ? DebugPreview(*fallbackParentPath).c_str() : "",
            static_cast<unsigned>(fallbackHiddenUid), filterPlainReaddir ? 1 : 0,
            filterReaddirplus ? 1 : 0, filterPostfilterReaddir ? 1 : 0);
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
                InvalidateFilteredParentChildren(filterUid, *fallbackParentPath, removedEntries);
                ClearRecentHiddenParentPath(filterUid);
            } else if (fallbackParentPath.has_value() &&
                       BuildFilteredDirentPayloadForParentPath(
                           buf, size, filterUid, *fallbackParentPath, &filteredStorage,
                           &removedCount, &removedEntries)) {
                replyBuf = filteredStorage.data();
                replySize = filteredStorage.size();
                filterMode = "fallback_parent_dirent";
                InvalidateFilteredParentChildren(filterUid, *fallbackParentPath, removedEntries);
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
                    InvalidateFilteredParentChildren(filterUid, *fallbackParentPath,
                                                     removedEntries);
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

    // AOSP readdirplus calls fuse_add_direntry_plus(), not fuse_reply_entry().
    // Otherwise an unrestricted UID can populate a long-lived positive dentry
    // for a target hidden from another UID. Only touch verified plus payloads
    // for a tracked parent; preserve names, cookies and inode attributes.
    if (filterReaddirplus && filterIno != 0 && replyBuf != nullptr && replySize != 0) {
        const auto parentPath = LookupTrackedPathForInode(filterIno);
        if (parentPath.has_value()) {
            std::vector<char> uncached;
            size_t uncachedCount = 0;
            if (BuildUncachedSensitiveDirentplusPayload(replyBuf, replySize, *parentPath, &uncached,
                                                        &uncachedCount)) {
                filteredStorage = std::move(uncached);
                replyBuf = filteredStorage.data();
                replySize = filteredStorage.size();
                DebugLogPrint(4, "readdirplus cross-uid uncached entries=%zu uid=%u parent=%s",
                              uncachedCount, static_cast<unsigned>(reqUid),
                              DebugPreview(*parentPath).c_str());
            }
        }
    }

#if defined(__x86_64__)
    if (filterReaddirplus && filterIno != 0 && size != 0 && replySize == 0 && removedCount != 0 &&
        filterMode != nullptr && gReaddirplusContinuation != nullptr &&
        gReaddirplusContinuation->req == req && !gReaddirplusContinuation->replySent) {
        const auto next =
            LastReaddirplusCookie(std::span<const char>(buf, size),
                                  static_cast<int64_t>(gReaddirplusContinuation->currentOffset));
        if (next) {
            gReaddirplusContinuation->nextOffset = static_cast<off_t>(*next);
            gReaddirplusContinuation->needsNextPage = true;
            return 0;  // Do not send an EOF reply while the directory has more entries.
        }
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "readdirplus invalid continuation cookie req=%lu",
                            req ? static_cast<unsigned long>(req->unique) : 0UL);
        gReaddirplusContinuation->replySent = true;
        return ReplyErrorBridge::Reply(req, EIO, "readdirplus cookie").value_or(-EIO);
    }
    if (gReaddirplusContinuation != nullptr && gReaddirplusContinuation->req == req)
        gReaddirplusContinuation->replySent = true;
#endif
    int ret = fn ? fn(req, replyBuf, replySize) : -1;
    if (DebugLoggingEnabled()) {
        if (filterPlainReaddir || filterReaddirplus || filterPostfilterReaddir ||
            (hasPendingContext && filterIno != 0)) {
            DebugLogPrint(4,
                          "hide-diag readdir_reply req=%llu req_uid=%u filter_uid=%u ino=0x%llx "
                          "mode=%d/%d/%d input_bytes=%zu output_bytes=%zu removed=%zu "
                          "filter=%s ret=%d",
                          req ? static_cast<unsigned long long>(req->unique) : 0ULL,
                          static_cast<unsigned>(reqUid), static_cast<unsigned>(filterUid),
                          static_cast<unsigned long long>(filterIno), filterPlainReaddir ? 1 : 0,
                          filterReaddirplus ? 1 : 0, filterPostfilterReaddir ? 1 : 0, size,
                          replySize, removedCount, filterMode ? filterMode : "none", ret);
        }
    }
    if (hasPendingContext) {
        CompletePendingReaddirContext(req, reqUid);
    }
    if (removedCount != 0) {
        DebugLogPrint(4, "filtered readdir reply mode=%s uid=%u ino=%s removed=%zu size=%zu->%zu",
                      filterMode ? filterMode : "unknown", static_cast<unsigned>(filterUid),
                      InodePath(filterIno).c_str(), removedCount, size, replySize);
    }
    if (gInPfLookupPostfilter) {
        DebugLogPrint(3, "pf_lookup_postfilter fuse_reply_buf req=%p", req);
    } else {
        DebugLogPrint(3, "fuse_reply_buf: req=%lu size=%zu ret=%d",
                      req ? (unsigned long)req->unique : 0UL, replySize, ret);
    }
    return ret;
}

extern "C" int WrappedReplyErr(fuse_req_t req, int err) {
    const ScopedFuseRequestSession scopedSession(req);
#if defined(__x86_64__)
    if (gReaddirplusContinuation != nullptr && gReaddirplusContinuation->req == req)
        gReaddirplusContinuation->replySent = true;
#endif
    auto fn = ReplyErrorBridge::Original();
    [[maybe_unused]] const int originalErr = err;
    // Root mutations are tracked when the request is issued, but the snapshot must only be bumped
    // after the kernel-visible reply succeeds.
    const auto pendingRootMutation = TakePendingRootSnapshotMutation(req);
    CompletePendingReaddirContext(req, RuntimeState::ReqUid(req));
    err = MaybeRewriteHiddenLeakErrno(req, err, "fuse_reply_err");
    if (DebugLoggingEnabled()) {
        if (err == ENOENT) {
            // The error may come from MediaProvider itself, not necessarily our
            // filtering. Capture the enclosing lookup context to distinguish them.
            const uint32_t uid = RuntimeState::ReqUid(req);
            const auto parentPath = gInPfLookup && gCurrentLookupParentInode != 0
                                        ? LookupTrackedPathForInode(gCurrentLookupParentInode)
                                        : std::nullopt;
            const bool hasLookupName = gInPfLookup && !gCurrentLookupName.empty();
            const std::string childPath =
                hasLookupName && parentPath
                    ? HiddenPathPolicy::JoinPathComponent(*parentPath, gCurrentLookupName)
                    : "";
            const bool policyHidden =
                !childPath.empty() && HiddenPathPolicy::ShouldHideTestPath(uid, childPath);
            const bool trackedParent =
                gInPfLookup && IsTrackedHiddenSubtreeInode(uid, gCurrentLookupParentInode);
            const bool mismatch = trackedParent && parentPath && !policyHidden;
            static std::atomic<int> sampled{0};
            if (mismatch || ShouldDebugLogLimited(sampled, 192)) {
                DebugLogPrint(mismatch ? 5 : 4,
                              "hide-diag reply_err uid=%u req=%lu original_err=%d err=%d "
                              "lookup=%d parent=0x%llx parent_path=%s name=%s policy_hidden=%d "
                              "tracked_parent=%d mismatch=%d",
                              static_cast<unsigned>(uid),
                              req ? static_cast<unsigned long>(req->unique) : 0UL, originalErr, err,
                              gInPfLookup ? 1 : 0,
                              static_cast<unsigned long long>(gCurrentLookupParentInode),
                              parentPath ? DebugPreview(*parentPath).c_str() : "(unknown)",
                              hasLookupName ? DebugPreview(gCurrentLookupName).c_str() : "(none)",
                              policyHidden ? 1 : 0, trackedParent ? 1 : 0, mismatch ? 1 : 0);
            }
        }
    }
    if (pendingRootMutation.has_value() && err == 0) {
        BumpRootSnapshotParentGeneration(*pendingRootMutation);
    }
    if (err == 0 && gActiveRenameTracking != nullptr && gActiveRenameTracking->req == req &&
        !gActiveRenameTracking->reconciled) {
        // The MediaProvider rename already succeeded but its FUSE response is
        // not yet published. Atomically migrate the tracked live identities
        // before the kernel can issue a new lookup for the destination.
        auto& rename = *gActiveRenameTracking;
        rename.reconciled = true;
        bool reconciled = false;
        // Without a verified destruction callback, an old node* address may
        // still carry a stale path. Never promote it across a rename.
        if (gOriginalNodeDeleted.get() != nullptr && rename.flags == 0) {
            reconciled =
                ReconcileTrackedRename(CurrentSessionToken(), rename.sourceParent,
                                       rename.sourceName, rename.targetParent, rename.targetName);
        } else if (rename.flags != 0) {
            DebugLogPrint(5, "hide-diag rename_migrate skipped=unsupported_flags flags=0x%x",
                          rename.flags);
        } else if (DebugLoggingEnabled()) {
            DebugLogPrint(5, "hide-diag rename_migrate skipped=no_node_deleted_hook req=%lu",
                          req ? static_cast<unsigned long>(req->unique) : 0UL);
        }
        if (!reconciled && rename.source != rename.destination) {
            // Unverified parents or unrecognized rename semantics: never
            // promote an inferred path into a trusted inode identity.
            ForgetTrackedPathSubtree(CurrentSessionToken(), rename.source,
                                     "rename_unverified_source");
            ForgetTrackedPathSubtree(CurrentSessionToken(), rename.destination,
                                     "rename_unverified_destination");
        }
        if (!rename.sourceName.empty())
            RuntimeState::ScheduleSpecificEntryInvalidation(rename.sourceParent, rename.sourceName);
        if (!rename.targetName.empty())
            RuntimeState::ScheduleSpecificEntryInvalidation(rename.targetParent, rename.targetName);
    }
    int ret = fn ? fn(req, err) : -1;
    if (gActiveRenameTracking != nullptr && gActiveRenameTracking->req == req && err == 0 &&
        ret == 0)
        gActiveRenameTracking->success = true;
    if (gInPfLookupPostfilter) {
        DebugLogPrint(3, "pf_lookup_postfilter fuse_reply_err req=%p %d", req, err);
    } else {
        DebugLogPrint(3, "fuse_reply_err: req=%p err=%d ret=%d", req, err, ret);
    }
    return ret;
}

}  // namespace fusehide
