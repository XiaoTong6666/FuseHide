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
#include "fusehide/policy/path_policy.hpp"

namespace fusehide {
namespace {

class ScopedCreateUid final {
   public:
    explicit ScopedCreateUid(uint32_t uid) : previous_(gActiveCreateUid) {
        ++gActiveCreateScopeDepth;
        gActiveCreateUid = uid;
    }

    ~ScopedCreateUid() {
        gActiveCreateUid = previous_;
        if (gActiveCreateScopeDepth != 0) {
            --gActiveCreateScopeDepth;
        }
    }

   private:
    uint32_t previous_;
};

}  // namespace

// AOSP pf_mkdir only checks parent_path accessibility before it calls mkdir(child_path), so a
// hidden leaf name would still leak existence semantics unless we stop it here.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1184
extern "C" void WrappedPfMkdir(fuse_req_t req, uint64_t parent, const char* name, uint32_t mode) {
    const ScopedFuseRequestSession scopedSession(req);
    const uint32_t uid = RuntimeState::ReqUid(req);
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
    const auto fn = gOriginalPfMkdir.get();
    if (fn) {
        ScopedCreateUid scopedUid(uid);
        fn(req, parent, name, mode);
    }
}

// Some callers create regular files through the mknod op instead of create. AOSP still uses only
// parent_path policy here, so hidden leaf names must be blocked explicitly.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1134
extern "C" void WrappedPfMknod(fuse_req_t req, uint64_t parent, const char* name, uint32_t mode,
                               uint64_t rdev) {
    const ScopedFuseRequestSession scopedSession(req);
    const uint32_t uid = RuntimeState::ReqUid(req);
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
    const auto fn = gOriginalPfMknod.get();
    if (fn) {
        ScopedCreateUid scopedUid(uid);
        fn(req, parent, name, mode, rdev);
    }
}

// AOSP pf_unlink only gates on parent_path before it deletes the final child path, so hidden leaf
// names must return ENOENT here instead of reaching the lower filesystem.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1218
extern "C" void WrappedPfUnlink(fuse_req_t req, uint64_t parent, const char* name) {
    const ScopedFuseRequestSession scopedSession(req);
    const uint32_t uid = RuntimeState::ReqUid(req);
    const HiddenNamedTargetKind kind = ClassifyHiddenNamedTarget(uid, parent, name);
    const auto trackedParentPath = LookupTrackedPathForInode(parent);
    DebugLogPrint(4, "named-target pf_unlink uid=%u parent=%s tracked_parent=%s name=%s kind=%d",
                  static_cast<unsigned>(uid), InodePath(parent).c_str(),
                  trackedParentPath.has_value() ? DebugPreview(*trackedParentPath).c_str() : "",
                  name ? DebugPreview(name).c_str() : "null", static_cast<int>(kind));
    if (ReplyHiddenNamedTargetError(req, "pf_unlink", kind, ENOENT, ENOENT)) {
        return;
    }
    std::string parentPath;
    if (ResolveVisibleParentPath(parent, &parentPath) && IsVisibleRootPath(parentPath)) {
        TrackPendingRootSnapshotMutation(req, "pf_unlink");
    }
    const auto fn = gOriginalPfUnlink.get();
    if (fn) {
        fn(req, parent, name);
    }
}

// AOSP pf_rmdir follows the same parent-only validation pattern as pf_unlink, so hidden child
// names must be rejected before the real directory delete runs.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1248
extern "C" void WrappedPfRmdir(fuse_req_t req, uint64_t parent, const char* name) {
    const ScopedFuseRequestSession scopedSession(req);
    const HiddenNamedTargetKind kind =
        ClassifyHiddenNamedTarget(RuntimeState::ReqUid(req), parent, name);
    if (ReplyHiddenNamedTargetError(req, "pf_rmdir", kind, ENOENT, ENOENT)) {
        return;
    }
    std::string parentPath;
    if (ResolveVisibleParentPath(parent, &parentPath) && IsVisibleRootPath(parentPath)) {
        TrackPendingRootSnapshotMutation(req, "pf_rmdir");
    }
    const auto fn = gOriginalPfRmdir.get();
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
    const ScopedFuseRequestSession scopedSession(req);
    const uint32_t uid = RuntimeState::ReqUid(req);
    const HiddenNamedTargetKind srcKind = ClassifyHiddenNamedTarget(uid, parent, name);
    const HiddenNamedTargetKind dstKind = ClassifyHiddenNamedTarget(uid, new_parent, new_name);
    const auto srcTrackedParentPath = LookupTrackedPathForInode(parent);
    const auto dstTrackedParentPath = LookupTrackedPathForInode(new_parent);
    DebugLogPrint(
        4,
        "named-target pf_rename uid=%u src_parent=%s src_tracked=%s src_name=%s src_kind=%d "
        "dst_parent=%s dst_tracked=%s dst_name=%s dst_kind=%d flags=0x%x",
        static_cast<unsigned>(uid), InodePath(parent).c_str(),
        srcTrackedParentPath.has_value() ? DebugPreview(*srcTrackedParentPath).c_str() : "",
        name ? DebugPreview(name).c_str() : "null", static_cast<int>(srcKind),
        InodePath(new_parent).c_str(),
        dstTrackedParentPath.has_value() ? DebugPreview(*dstTrackedParentPath).c_str() : "",
        new_name ? DebugPreview(new_name).c_str() : "null", static_cast<int>(dstKind), flags);
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
    std::string oldParentPath;
    std::string newParentPath;
    const bool touchesRoot =
        (ResolveVisibleParentPath(parent, &oldParentPath) && IsVisibleRootPath(oldParentPath)) ||
        (ResolveVisibleParentPath(new_parent, &newParentPath) && IsVisibleRootPath(newParentPath));
    if (touchesRoot) {
        TrackPendingRootSnapshotMutation(req, "pf_rename");
    }
    const auto fn = gOriginalPfRename.get();
    if (fn) {
        RenameTracking tracking;
        tracking.req = req;
        tracking.sourceParent = parent;
        tracking.targetParent = new_parent;
        if (name)
            tracking.sourceName = name;
        if (new_name)
            tracking.targetName = new_name;
        tracking.flags = flags;
        if (srcTrackedParentPath && name)
            tracking.source = HiddenPathPolicy::JoinPathComponent(*srcTrackedParentPath, name);
        if (dstTrackedParentPath && new_name)
            tracking.destination =
                HiddenPathPolicy::JoinPathComponent(*dstTrackedParentPath, new_name);
        auto* previous = gActiveRenameTracking;
        gActiveRenameTracking = &tracking;
        fn(req, parent, name, new_parent, new_name, flags);
        gActiveRenameTracking = previous;
        if constexpr (kEnableDebugHooks) {
            DebugLogPrint(4, "hide-diag rename success=%d reconciled=%d source=%s destination=%s",
                          tracking.success ? 1 : 0, tracking.reconciled ? 1 : 0,
                          DebugPreview(tracking.source).c_str(),
                          DebugPreview(tracking.destination).c_str());
        }
    }
}

// AOSP pf_create inserts into MediaProvider and then opens the lower-fs child path. Returning a
// positive entry here would let create leak EEXIST-like behavior for the hidden root entry.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#2121
extern "C" void WrappedPfCreate(fuse_req_t req, uint64_t parent, const char* name, uint32_t mode,
                                void* fi) {
    const ScopedFuseRequestSession scopedSession(req);
    const uint32_t uid = RuntimeState::ReqUid(req);
    const HiddenNamedTargetKind kind = ClassifyHiddenNamedTarget(uid, parent, name);
    const auto trackedParentPath = LookupTrackedPathForInode(parent);
    DebugLogPrint(4,
                  "create-trace pf_create uid=%u parent=%s tracked_parent=%s name=%s mode=%o fi=%p "
                  "hidden_root=%d hidden_desc=%d kind=%d",
                  static_cast<unsigned>(uid), InodePath(parent).c_str(),
                  trackedParentPath.has_value() ? DebugPreview(*trackedParentPath).c_str() : "",
                  name ? DebugPreview(name).c_str() : "null", mode, fi,
                  kind == HiddenNamedTargetKind::Root ? 1 : 0,
                  kind == HiddenNamedTargetKind::Descendant ? 1 : 0, static_cast<int>(kind));
    if (ReplyHiddenNamedTargetError(req, "pf_create", kind, EPERM, ENOENT)) {
        return;
    }
    const auto fn = gOriginalPfCreate.get();
    if (fn) {
        ScopedCreateUid scopedUid(uid);
        fn(req, parent, name, mode, fi);
    }
}

}  // namespace fusehide
