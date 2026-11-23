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

#include "fusehide/policy/path_policy.hpp"

namespace fusehide {

namespace {

// Keep entry-point diagnostics for device ABI investigations. The runtime
// switch avoids expensive probes in Release without compiling the code out.
void DebugRootCallbackProbe(size_t kind, const char* operation, uint64_t inode,
                            const void* fileInfo) {
    if (DebugLoggingEnabled()) {
        static std::atomic<uint32_t> counts[4]{};
        if (kind < 4 && counts[kind].fetch_add(1, std::memory_order_relaxed) < 12) {
            const SessionToken token = CurrentSessionToken();
            DebugLogPrint(4, "root callback probe op=%s ino=0x%llx fi=%p se=%p generation=%llu",
                          operation, static_cast<unsigned long long>(inode), fileInfo, token.se,
                          static_cast<unsigned long long>(token.generation));
        }
    }
}

class ScopedReaddirContext final {
   public:
    ScopedReaddirContext(fuse_req_t req, uint32_t uid, uint64_t ino) {
        if (req == nullptr || req->unique == 0) {
            return;
        }
        reqUnique_ = req->unique;
        if (gActiveReaddirContext.depth != 0 && gActiveReaddirContext.reqUnique == reqUnique_) {
            ++gActiveReaddirContext.depth;
            nestedSameRequest_ = true;
            return;
        }

        if (gActiveReaddirContext.depth != 0) {
            previousContext_ = gActiveReaddirContext;
        }

        gActiveReaddirContext = ActiveReaddirContext{
            .reqUnique = reqUnique_,
            .depth = 1,
            .globalBacked = uid != 0 && BorrowHideRuleForUid(uid) != nullptr,
            .pending = PendingReaddirContext{uid, ino, {}},
        };
        ownsContext_ = true;
        if (gActiveReaddirContext.globalBacked) {
            PutPendingReaddirContext(reqUnique_, gActiveReaddirContext.pending);
        }
    }

    ~ScopedReaddirContext() {
        if (reqUnique_ == 0) {
            return;
        }
        if (nestedSameRequest_) {
            if (gActiveReaddirContext.reqUnique == reqUnique_ && gActiveReaddirContext.depth != 0) {
                --gActiveReaddirContext.depth;
            }
            return;
        }
        if (!ownsContext_ || gActiveReaddirContext.reqUnique != reqUnique_) {
            return;
        }
        gActiveReaddirContext =
            previousContext_.has_value() ? std::move(*previousContext_) : ActiveReaddirContext{};
    }

   private:
    uint64_t reqUnique_ = 0;
    bool ownsContext_ = false;
    bool nestedSameRequest_ = false;
    std::optional<ActiveReaddirContext> previousContext_;
};

}  // namespace

// pf_lookup is the earliest reliable place to learn the real root parent inode on this device.
// AOSP reference: jni/FuseDaemon.cpp#851
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#851
extern "C" void WrappedPfLookup(fuse_req_t req, uint64_t parent, const char* name) {
    const ScopedFuseRequestSession scopedSession(req);
    DebugRootCallbackProbe(0, "lookup", parent, nullptr);
    const uint32_t uid = RuntimeState::ReqUid(req);
    std::string parentPath;
    if (name != nullptr && parent != 0 && ResolveVisibleParentPath(parent, &parentPath) &&
        IsVisibleRootPath(parentPath) && HiddenPathPolicy::IsHiddenRootEntryName(uid, name)) {
        RememberVisibleRootParentInode(parent, "lookup_parent_path");
    }
    gInPfLookup = true;
    gCurrentLookupUid = uid;
    gCurrentLookupParentInode = parent;
    gCurrentLookupName = name != nullptr ? std::string_view(name) : std::string_view();
    gTrackRootHiddenLookup = IsHiddenLookupCacheTarget(uid, parent, name);
    gTrackHiddenSubtreeLookup = IsTrackedHiddenSubtreeInode(uid, parent);
    if (DebugLoggingEnabled()) {
        if (gTrackHiddenSubtreeLookup) {
            const auto tracked = LookupTrackedPathForInode(parent);
            const bool pathHidden =
                tracked && HiddenPathPolicy::IsAnyHiddenSubtreePath(uid, *tracked);
            static std::atomic<int> sampled{0};
            if (!pathHidden || ShouldDebugLogLimited(sampled, 96)) {
                DebugLogPrint(pathHidden ? 4 : 5,
                              "hide-diag lookup_mark uid=%u parent=0x%llx tracked_path=%s "
                              "name=%s tracked_path_hidden=%d",
                              static_cast<unsigned>(uid), static_cast<unsigned long long>(parent),
                              tracked ? DebugPreview(*tracked).c_str() : "(unknown)",
                              name ? DebugPreview(name).c_str() : "(null)", pathHidden ? 1 : 0);
            }
        }
    }
    gCrossUidSensitiveLookup = IsCrossUidSensitiveLookup(parent, name);
    DebugLogPrint(3, "lookup: req=%lu parent=%s name=%s", (unsigned long)req->unique,
                  InodePath(parent).c_str(), name ? DebugPreview(name).c_str() : "null");

    const auto fn = gOriginalPfLookup.get();
    if (fn)
        fn(req, parent, name);
    gCurrentLookupUid = 0;
    gCurrentLookupParentInode = 0;
    gCurrentLookupName = {};
    gInPfLookup = false;
    gTrackHiddenSubtreeLookup = false;
    gTrackRootHiddenLookup = false;
    gCrossUidSensitiveLookup = false;
}

// FUSE_FORGET decrements the lookup refcount, but other handles may keep the
// MediaProvider node alive. Never evict the identity here.
extern "C" void WrappedPfForget(fuse_req_t req, uint64_t ino, uint64_t nlookup) {
    const ScopedFuseRequestSession scopedSession(req);
    if (DebugLoggingEnabled()) {
        static std::atomic<int> sampled{0};
        if (ShouldDebugLogLimited(sampled, 128)) {
            DebugLogPrint(4,
                          "hide-diag forget_request ino=0x%llx nlookup=%llu "
                          "node_deleted_hook=%d",
                          static_cast<unsigned long long>(ino),
                          static_cast<unsigned long long>(nlookup),
                          gOriginalNodeDeleted.get() != nullptr ? 1 : 0);
        }
    }
    if (const auto fn = gOriginalPfForget.get())
        fn(req, ino, nlookup);
}

extern "C" void WrappedPfForgetMulti(fuse_req_t req, size_t count, FuseForgetData* forgets) {
    const ScopedFuseRequestSession scopedSession(req);
    if (DebugLoggingEnabled()) {
        static std::atomic<int> sampled{0};
        if (ShouldDebugLogLimited(sampled, 128)) {
            DebugLogPrint(4,
                          "hide-diag forget_multi_request count=%zu "
                          "node_deleted_hook=%d",
                          count, gOriginalNodeDeleted.get() != nullptr ? 1 : 0);
        }
    }
    if (const auto fn = gOriginalPfForgetMulti.get())
        fn(req, count, forgets);
}

// Unlike FUSE_FORGET, NodeTracker::NodeDeleted is called by node::~node()
// after the final reference is gone, while the node* address is still owned.
// This is the exact point at which an inode can safely lose its identity.
extern "C" void WrappedNodeDeleted(void* tracker, const void* node) {
    const uint64_t ino = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(node));
    const SessionToken token = CurrentSessionToken();
    if (DebugLoggingEnabled()) {
        static std::atomic<int> sampled{0};
        if (ShouldDebugLogLimited(sampled, 128)) {
            DebugLogPrint(4,
                          "hide-diag node_deleted ino=0x%llx se=%p generation=%llu "
                          "session_known=%d",
                          static_cast<unsigned long long>(ino), token.se,
                          static_cast<unsigned long long>(token.generation), token ? 1 : 0);
        }
    }
    // The original method removes the node from MediaProvider's own tracker.
    // Our state is purged before the address can be reused by its allocator.
    // Destruction is not necessarily caused by a wrapped FUSE request; the
    // pointer identifies one live node process-wide, so on session-less calls
    // clear the matching inode in each active FUSE session.
    if (node != nullptr) {
        if (token) {
            ForgetTrackedInode(token, ino, "node_deleted");
        } else {
            for (const SessionToken session : SnapshotActiveSessions())
                ForgetTrackedInode(session, ino, "node_deleted_without_request");
        }
    }
    if (const auto fn = gOriginalNodeDeleted.get())
        fn(tracker, node);
}

// AOSP readdir postfilter stats each child path before copying the surviving dirents into a
// fuse_read_out buffer. This context flag lets WrappedReplyBuf preserve that wire layout when the
// device actually goes through pf_readdir_postfilter.
// AOSP reference: jni/FuseDaemon.cpp#1954
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1954
extern "C" void WrappedPfReaddirPostfilter(fuse_req_t req, uint64_t ino, uint32_t error_in,
                                           off_t off_in, off_t off_out, size_t size_out,
                                           const void* dirents_in, void* fi) {
    const ScopedFuseRequestSession scopedSession(req);
    DebugRootCallbackProbe(1, "readdir_postfilter", ino, fi);
    const uint32_t uid = RuntimeState::ReqUid(req);
    const auto fn = gOriginalPfReaddirPostfilter.get();
    if (fn == nullptr) {
        return;
    }
    const ScopedReaddirContext scopedReaddir(req, uid, ino);
    DebugLogPrint(3, "pf_readdir_postfilter uid=%u ino=%s err=%u off_in=%lld off_out=%lld size=%zu",
                  static_cast<unsigned>(uid), InodePath(ino).c_str(), error_in,
                  static_cast<long long>(off_in), static_cast<long long>(off_out), size_out);

    gInPfReaddirPostfilter = true;
    gPfReaddirUid = uid;
    gPfReaddirIno = ino;
    gCurrentReaddirReqUnique = req != nullptr ? req->unique : 0;
    fn(req, ino, error_in, off_in, off_out, size_out, dirents_in, fi);
    gCurrentReaddirReqUnique = 0;
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
    const ScopedFuseRequestSession scopedSession(req);
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
    const auto fn = gOriginalPfLookupPostfilter.get();
    if (fn) {
        gInPfLookupPostfilter = true;
        fn(req, parent, error_in, name, feo, febo);
        gInPfLookupPostfilter = false;
    }
}

extern "C" void WrappedPfAccess(fuse_req_t req, uint64_t ino, int mask) {
    const ScopedFuseRequestSession scopedSession(req);
    const auto fn = gOriginalPfAccess.get();
    if (fn) {
        fn(req, ino, mask);
    }
}

extern "C" void WrappedPfOpen(fuse_req_t req, uint64_t ino, void* fi) {
    const ScopedFuseRequestSession scopedSession(req);
    const auto fn = gOriginalPfOpen.get();
    if (fn) {
        fn(req, ino, fi);
    }
}

extern "C" void WrappedPfOpendir(fuse_req_t req, uint64_t ino, void* fi) {
    const ScopedFuseRequestSession scopedSession(req);
    const auto fn = gOriginalPfOpendir.get();
    if (fn) {
        fn(req, ino, fi);
    }
}

// Plain readdir delegates to do_readdir_common(..., plus=false). Most modern devices keep
// readdirplus enabled, but this hook is still useful as a fallback for alternative FUSE configs.
// AOSP reference: jni/FuseDaemon.cpp#1944
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1944
extern "C" void WrappedPfReaddir(fuse_req_t req, uint64_t ino, size_t size, off_t off, void* fi) {
    const ScopedFuseRequestSession scopedSession(req);
    DebugRootCallbackProbe(2, "readdir", ino, fi);
    const uint32_t uid = RuntimeState::ReqUid(req);
    const auto fn = gOriginalPfReaddir.get();
    if (fn == nullptr) {
        return;
    }
    const ScopedReaddirContext scopedReaddir(req, uid, ino);
    DebugLogPrint(3, "pf_readdir uid=%u ino=%s size=%zu off=%lld", static_cast<unsigned>(uid),
                  InodePath(ino).c_str(), size, static_cast<long long>(off));
    gInPfReaddir = true;
    gPfReaddirUid = uid;
    gPfReaddirIno = ino;
    gCurrentReaddirReqUnique = req != nullptr ? req->unique : 0;
    fn(req, ino, size, off, fi);
    gCurrentReaddirReqUnique = 0;
    gPfReaddirIno = 0;
    gPfReaddirUid = 0;
    gInPfReaddir = false;
}

extern "C" void WrappedDoReaddirCommon(fuse_req_t req, uint64_t ino, size_t size, off_t off,
                                       void* fi, bool plus) {
    const ScopedFuseRequestSession scopedSession(req);
    const uint32_t uid = RuntimeState::ReqUid(req);
    const auto fn = gOriginalDoReaddirCommon.get();
    if (fn == nullptr) {
        return;
    }
    const ScopedReaddirContext scopedReaddir(req, uid, ino);
    DebugLogPrint(3, "do_readdir_common uid=%u ino=%s size=%zu off=%lld plus=%d",
                  static_cast<unsigned>(uid), InodePath(ino).c_str(), size,
                  static_cast<long long>(off), plus ? 1 : 0);
    // MediaProvider calls this synchronously from pf_readdir / pf_readdirplus
    // with the same req. Keep the caller's request tag until its wrapper exits.
    const uint64_t previousReqUnique = gCurrentReaddirReqUnique;
    gCurrentReaddirReqUnique = req != nullptr ? req->unique : 0;
    fn(req, ino, size, off, fi, plus);
    gCurrentReaddirReqUnique = previousReqUnique;
}

// readdirplus is the common enumeration path on recent Android builds because do_readdir_common()
// emits fuse_direntplus records by first running do_lookup() for each directory entry.
// AOSP references: jni/FuseDaemon.cpp#1904 and #2000
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1904
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#2000
extern "C" void WrappedPfReaddirplus(fuse_req_t req, uint64_t ino, size_t size, off_t off,
                                     void* fi) {
    const ScopedFuseRequestSession scopedSession(req);
    DebugRootCallbackProbe(3, "readdirplus", ino, fi);
    const uint32_t uid = RuntimeState::ReqUid(req);
    const auto fn = gOriginalPfReaddirplus.get();
    if (fn == nullptr) {
        return;
    }
    const ScopedReaddirContext scopedReaddir(req, uid, ino);
    DebugLogPrint(3, "pf_readdirplus uid=%u ino=%s size=%zu off=%lld", static_cast<unsigned>(uid),
                  InodePath(ino).c_str(), size, static_cast<long long>(off));
    gInPfReaddirplus = true;
    gPfReaddirUid = uid;
    gPfReaddirIno = ino;
    gCurrentReaddirReqUnique = req != nullptr ? req->unique : 0;
#if defined(__x86_64__)
    const auto path = LookupTrackedPathForInode(ino);
    const auto rule = BorrowHideRuleForUid(uid);
    const bool rootHideAll =
        path && IsVisibleRootPath(*path) && rule && HideAllForStorageRoot(*rule, *path);
    if (rootHideAll && fi != nullptr && req != nullptr && size != 0) {
        ReaddirplusContinuation frame{.req = req};
        auto* previousFrame = gReaddirplusContinuation;
        gReaddirplusContinuation = &frame;
        constexpr size_t kMaxEmptyPages = 1024;
        const auto start = std::chrono::steady_clock::now();
        bool finished = false;
        for (size_t page = 0; page < kMaxEmptyPages; ++page) {
            frame.currentOffset = off;
            frame.needsNextPage = false;
            frame.replySent = false;
            fn(req, ino, size, off, fi);
            if (!frame.needsNextPage) {
                finished = true;
                break;
            }
            off = frame.nextOffset;
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5))
                break;
        }
        gReaddirplusContinuation = previousFrame;
        if (!finished) {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                                "readdirplus continuation limit req=%lu ino=%s",
                                static_cast<unsigned long>(req->unique), InodePath(ino).c_str());
            ReplyErrorBridge::Reply(req, EOVERFLOW, "readdirplus pagination");
        }
    } else {
        fn(req, ino, size, off, fi);
    }
#else
    fn(req, ino, size, off, fi);
#endif
    gCurrentReaddirReqUnique = 0;
    gPfReaddirIno = 0;
    gPfReaddirUid = 0;
    gInPfReaddirplus = false;
}

extern "C" void WrappedPfGetattr(fuse_req_t req, uint64_t ino, void* fi) {
    const ScopedFuseRequestSession scopedSession(req);
    const uint32_t uid = RuntimeState::ReqUid(req);
    gZeroAttrCacheForCurrentGetattr = IsTrackedHiddenSubtreeInode(uid, ino);
    if (HiddenPathPolicy::IsTestHiddenUid(uid)) {
        DebugLogPrint(4, "pf_getattr test uid=%u ino=0x%lx", static_cast<unsigned>(uid),
                      (unsigned long)ino);
    }
    const auto fn = gOriginalPfGetattr.get();
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

}  // namespace fusehide
