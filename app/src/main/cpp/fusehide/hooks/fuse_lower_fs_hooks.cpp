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

bool ShouldHideLowerFsCreatePath(std::string_view pathView) {
    const uint32_t uid = gActiveCreateUid != 0 ? gActiveCreateUid : gActivePathPolicyUid;
    return uid != 0 && IsExactHiddenPathClassification(ClassifyHiddenPath(uid, pathView));
}

bool ShouldHideLowerFsPath(std::string_view pathView) {
    const uint32_t uid = gActiveCreateUid != 0 ? gActiveCreateUid : gActivePathPolicyUid;
    return uid != 0 && IsHiddenPathClassification(ClassifyHiddenPath(uid, pathView));
}

}  // namespace

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
        // Recover the volume identity for Sessions first observed after creation.
        RegisterSessionRootPath(CurrentSessionToken(), pathView);
        const bool recorded = CurrentRootParentInode() == 0;
        RememberRootParentInode(gPfGetattrIno, "getattr_lstat");
        RemoveTrackedHiddenSubtreeInode(gPfGetattrUid, gPfGetattrIno);
        if (const auto rule = RuleForAnyPackage();
            recorded && rule != nullptr && HideAllForStorageRoot(*rule, pathView)) {
            RuntimeState::ScheduleHiddenEntryInvalidation();
        }
    }
    NoteHiddenSubtreePathForCache(gPfGetattrUid, pathView);
    if (gInPfGetattr && HiddenPathPolicy::IsTestHiddenUid(gPfGetattrUid)) {
        DebugLogPrint(4, "pf_getattr lstat uid=%u path=%s", static_cast<unsigned>(gPfGetattrUid),
                      DebugPreview(pathView).c_str());
        if (IsHiddenPathClassification(ClassifyHiddenPath(gPfGetattrUid, pathView))) {
            DebugLogPrint(4, "hide test lstat uid=%u path=%s", static_cast<unsigned>(gPfGetattrUid),
                          DebugPreview(pathView).c_str());
            errno = ENOENT;
            return -1;
        }
    }
    const auto fn = gOriginalLstat.get();
    if (fn) {
        const int ret = fn(path, st);
        if (DebugLoggingEnabled()) {
            if (ret == -1 && errno == ENOENT && gInPfGetattr) {
                // Logging must not change the errno observed by MediaProvider.
                const int savedErrno = errno;
                static std::atomic<int> sampled{0};
                if (ShouldDebugLogLimited(sampled, 192)) {
                    DebugLogPrint(
                        4,
                        "hide-diag lower_lstat uid=%u ino=0x%llx path=%s "
                        "original_enoent=1 policy_hidden=%d",
                        static_cast<unsigned>(gPfGetattrUid),
                        static_cast<unsigned long long>(gPfGetattrIno),
                        DebugPreview(pathView).c_str(),
                        HiddenPathPolicy::ShouldHideTestPath(gPfGetattrUid, pathView) ? 1 : 0);
                }
                errno = savedErrno;
            }
        }
        if (ret == 0 && gInPfGetattr && gPfGetattrIno != 0) {
            RememberTrackedPathForInode(gPfGetattrIno, pathView, true);
            if (HiddenPathPolicy::IsTestHiddenUid(gPfGetattrUid) &&
                fusehide::IsParentOfExactHiddenTargetPath(gPfGetattrUid, pathView)) {
                // Some device builds enumerate the parent directory after only touching it through
                // pf_getattr/lstat, without a visible parent lookup that would reach reply_entry.
                // Seed the fallback parent path here so reply_buf can still filter nested children.
                RememberRecentHiddenParentPath(gPfGetattrUid, pathView);
                DebugLogPrint(4, "remember recent hidden parent from getattr uid=%u path=%s",
                              static_cast<unsigned>(gPfGetattrUid), DebugPreview(pathView).c_str());
            }
            if (HiddenPathPolicy::IsAnyHiddenSubtreePath(gPfGetattrUid, pathView)) {
                TrackHiddenSubtreeInode(gPfGetattrUid, gPfGetattrIno);
            }
        }
        return ret;
    }
    errno = ENOSYS;
    return -1;
}

extern "C" int WrappedStat(const char* path, struct stat* st) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    if (gInPfReaddirPostfilter &&
        IsHiddenPathClassification(ClassifyHiddenPath(gPfReaddirUid, pathView))) {
        DebugLogPrint(4, "hide readdir stat uid=%u path=%s", static_cast<unsigned>(gPfReaddirUid),
                      DebugPreview(pathView).c_str());
        errno = ENOENT;
        return -1;
    }
    const auto fn = gOriginalStat.get();
    if (fn) {
        const int ret = fn(path, st);
        if (DebugLoggingEnabled()) {
            if (ret == -1 && errno == ENOENT && gInPfReaddirPostfilter) {
                const int savedErrno = errno;
                static std::atomic<int> sampled{0};
                if (ShouldDebugLogLimited(sampled, 192)) {
                    DebugLogPrint(
                        4,
                        "hide-diag lower_stat uid=%u path=%s original_enoent=1 "
                        "policy_hidden=%d",
                        static_cast<unsigned>(gPfReaddirUid), DebugPreview(pathView).c_str(),
                        HiddenPathPolicy::ShouldHideTestPath(gPfReaddirUid, pathView) ? 1 : 0);
                }
                errno = savedErrno;
            }
        }
        return ret;
    }
    errno = ENOSYS;
    return -1;
}

extern "C" ssize_t WrappedGetxattr(const char* path, const char* name, void* value, size_t size) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    const bool hidden = ShouldHideLowerFsPath(pathView);
    DebugLogPrint(4, "xattr-trace getxattr path=%s name=%s size=%zu hidden=%d",
                  DebugPreview(pathView).c_str(),
                  name != nullptr ? DebugPreview(name).c_str() : "null", size, hidden ? 1 : 0);
    if (hidden) {
        DebugLogPrint(4, "hide getxattr path=%s name=%s", DebugPreview(pathView).c_str(),
                      name != nullptr ? DebugPreview(name).c_str() : "null");
        errno = ENOENT;
        return -1;
    }
    const auto fn = gOriginalGetxattr.get();
    if (fn) {
        return fn(path, name, value, size);
    }
    errno = ENOSYS;
    return -1;
}

extern "C" ssize_t WrappedLgetxattr(const char* path, const char* name, void* value, size_t size) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    const bool hidden = ShouldHideLowerFsPath(pathView);
    DebugLogPrint(4, "xattr-trace lgetxattr path=%s name=%s size=%zu hidden=%d",
                  DebugPreview(pathView).c_str(),
                  name != nullptr ? DebugPreview(name).c_str() : "null", size, hidden ? 1 : 0);
    if (hidden) {
        DebugLogPrint(4, "hide lgetxattr path=%s name=%s", DebugPreview(pathView).c_str(),
                      name != nullptr ? DebugPreview(name).c_str() : "null");
        errno = ENOENT;
        return -1;
    }
    const auto fn = gOriginalLgetxattr.get();
    if (fn) {
        return fn(path, name, value, size);
    }
    errno = ENOSYS;
    return -1;
}

// Even if the named FUSE wrappers are missed on a device-specific path, lower-fs mkdir/mknod/open
// calls still carry the final child path. These libc hooks are the last fallback for create/mkdir.
extern "C" int WrappedMkdirLibc(const char* path, mode_t mode) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    const uint32_t uid = gActiveCreateUid;
    const bool hidden = ShouldHideLowerFsCreatePath(pathView);
    DebugLogPrint(4, "create-trace lower_mkdir uid=%u path=%s mode=%o hidden=%d",
                  static_cast<unsigned>(uid), DebugPreview(pathView).c_str(), mode, hidden ? 1 : 0);
    if (hidden) {
        DebugLogPrint(4, "hide mkdir path=%s", DebugPreview(pathView).c_str());
        errno = EACCES;
        return -1;
    }
    const auto fn = gOriginalMkdir.get();
    if (fn) {
        const int ret = fn(path, mode);
        if (ret == 0 && gActiveCreateScopeDepth != 0 && PathHasVisibleRootParent(pathView)) {
            BumpRootSnapshotParentGeneration("lower_mkdir");
        }
        return ret;
    }
    errno = ENOSYS;
    return -1;
}

extern "C" int WrappedMknod(const char* path, mode_t mode, dev_t dev) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    const uint32_t uid = gActiveCreateUid;
    const bool hidden = ShouldHideLowerFsCreatePath(pathView);
    DebugLogPrint(4, "create-trace lower_mknod uid=%u path=%s mode=%o dev=%llu hidden=%d",
                  static_cast<unsigned>(uid), DebugPreview(pathView).c_str(), mode,
                  static_cast<unsigned long long>(dev), hidden ? 1 : 0);
    if (hidden) {
        DebugLogPrint(4, "hide mknod path=%s", DebugPreview(pathView).c_str());
        errno = EPERM;
        return -1;
    }
    const auto fn = gOriginalMknod.get();
    if (fn) {
        const int ret = fn(path, mode, dev);
        if (ret == 0 && gActiveCreateScopeDepth != 0 && PathHasVisibleRootParent(pathView)) {
            BumpRootSnapshotParentGeneration("lower_mknod");
        }
        return ret;
    }
    errno = ENOSYS;
    return -1;
}

extern "C" int WrappedOpen(const char* path, int flags, ...) {
    // Match bionic open(): both O_CREAT and the complete O_TMPFILE mask require a mode argument.
    // https://android.googlesource.com/platform/bionic/+/731631f300090436d7f5df80d50b6275c8c60a93/libc/bionic/open.cpp#47
    const bool needsMode = (flags & O_CREAT) != 0 || (flags & O_TMPFILE) == O_TMPFILE;
    mode_t mode = 0;
    if (needsMode) {
        va_list args;
        va_start(args, flags);
        mode = static_cast<mode_t>(va_arg(args, int));
        va_end(args);
    }
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    const uint32_t uid = gActiveCreateUid;
    const bool hidden = (flags & O_CREAT) != 0 && ShouldHideLowerFsCreatePath(pathView);
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
    const auto fn = gOriginalOpen.get();
    if (fn) {
        if (needsMode) {
            const int ret = fn(path, flags, mode);
            if (ret >= 0 && gActiveCreateScopeDepth != 0 && PathHasVisibleRootParent(pathView)) {
                BumpRootSnapshotParentGeneration("lower_open_create");
            }
            return ret;
        }
        return fn(path, flags);
    }
    errno = ENOSYS;
    return -1;
}

extern "C" int WrappedOpen2(const char* path, int flags) {
    const std::string_view pathView = path != nullptr ? std::string_view(path) : std::string_view();
    const uint32_t uid = gActiveCreateUid;
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
    const auto fn = gOriginalOpen2.get();
    if (fn) {
        const int ret = fn(path, flags);
        if (ret >= 0 && (flags & O_CREAT) != 0 && gActiveCreateScopeDepth != 0 &&
            PathHasVisibleRootParent(pathView)) {
            BumpRootSnapshotParentGeneration("lower_open2_create");
        }
        return ret;
    }
    errno = ENOSYS;
    return -1;
}

}  // namespace fusehide
