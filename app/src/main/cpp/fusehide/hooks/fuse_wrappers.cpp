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
#include "fusehide/hooks/payload_builtin.hpp"

#include "fusehide/filters/dirent_filter.hpp"
#include "fusehide/filters/reply_buf_filter.hpp"
#include "fusehide/policy/path_policy.hpp"

namespace fusehide {

namespace {

class ScopedGenerationCall final {
   public:
    explicit ScopedGenerationCall(std::shared_ptr<NativeGeneration> generation)
        : generation_(std::move(generation)) {
    }

    ~ScopedGenerationCall() {
        if (generation_ != nullptr) {
            const uint32_t previous =
                generation_->activeCalls.fetch_sub(1, std::memory_order_acq_rel);
            if (previous == 1 && generation_->draining.load(std::memory_order_acquire)) {
                MaybeCleanupRetiredGenerations();
            }
        }
    }

   private:
    std::shared_ptr<NativeGeneration> generation_;
};

AnchorProcessState& Process() {
    return ProcessState();
}

}  // namespace

extern "C" void WrappedPfReaddirPostfilter(fuse_req_t req, uint64_t ino, uint32_t error_in,
                                           off_t off_in, off_t off_out, size_t size_out,
                                           const void* dirents_in, void* fi) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfReaddirPostfilter != nullptr) {
        generation->api.pfReaddirPostfilter(req, ino, error_in, off_in, off_out, size_out,
                                            dirents_in, fi);
        return;
    }
    BuiltinWrappedPfReaddirPostfilter(req, ino, error_in, off_in, off_out, size_out, dirents_in,
                                      fi);
}

extern "C" void WrappedPfLookupPostfilter(fuse_req_t req, uint64_t parent, uint32_t error_in,
                                          const char* name, struct fuse_entry_out* feo,
                                          struct fuse_entry_bpf_out* febo) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfLookupPostfilter != nullptr) {
        generation->api.pfLookupPostfilter(req, parent, error_in, name, feo, febo);
        return;
    }
    BuiltinWrappedPfLookupPostfilter(req, parent, error_in, name, feo, febo);
}

extern "C" void WrappedPfAccess(fuse_req_t req, uint64_t ino, int mask) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfAccess != nullptr) {
        generation->api.pfAccess(req, ino, mask);
        return;
    }
    BuiltinWrappedPfAccess(req, ino, mask);
}

extern "C" void WrappedPfOpen(fuse_req_t req, uint64_t ino, void* fi) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfOpen != nullptr) {
        generation->api.pfOpen(req, ino, fi);
        return;
    }
    BuiltinWrappedPfOpen(req, ino, fi);
}

extern "C" void WrappedPfOpendir(fuse_req_t req, uint64_t ino, void* fi) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfOpendir != nullptr) {
        generation->api.pfOpendir(req, ino, fi);
        return;
    }
    BuiltinWrappedPfOpendir(req, ino, fi);
}

extern "C" void WrappedPfMkdir(fuse_req_t req, uint64_t parent, const char* name, uint32_t mode) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfMkdir != nullptr) {
        generation->api.pfMkdir(req, parent, name, mode);
        return;
    }
    BuiltinWrappedPfMkdir(req, parent, name, mode);
}

// Some callers create regular files through the mknod op instead of create. AOSP still uses only
// parent_path policy here, so hidden leaf names must be blocked explicitly.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1134
extern "C" void WrappedPfMknod(fuse_req_t req, uint64_t parent, const char* name, uint32_t mode,
                               uint64_t rdev) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfMknod != nullptr) {
        generation->api.pfMknod(req, parent, name, mode, rdev);
        return;
    }
    BuiltinWrappedPfMknod(req, parent, name, mode, rdev);
}

// AOSP pf_unlink only gates on parent_path before it deletes the final child path, so hidden leaf
// names must return ENOENT here instead of reaching the lower filesystem.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1218
extern "C" void WrappedPfUnlink(fuse_req_t req, uint64_t parent, const char* name) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfUnlink != nullptr) {
        generation->api.pfUnlink(req, parent, name);
        return;
    }
    BuiltinWrappedPfUnlink(req, parent, name);
}

// AOSP pf_rmdir follows the same parent-only validation pattern as pf_unlink, so hidden child
// names must be rejected before the real directory delete runs.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1248
extern "C" void WrappedPfRmdir(fuse_req_t req, uint64_t parent, const char* name) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfRmdir != nullptr) {
        generation->api.pfRmdir(req, parent, name);
        return;
    }
    BuiltinWrappedPfRmdir(req, parent, name);
}

// AOSP do_rename only validates the old and new parent directories before it passes the final
// child paths into MediaProviderWrapper::Rename, so hidden names must be intercepted here as well.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1299
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1369
extern "C" void WrappedPfRename(fuse_req_t req, uint64_t parent, const char* name,
                                uint64_t new_parent, const char* new_name, uint32_t flags) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfRename != nullptr) {
        generation->api.pfRename(req, parent, name, new_parent, new_name, flags);
        return;
    }
    BuiltinWrappedPfRename(req, parent, name, new_parent, new_name, flags);
}

// AOSP pf_create inserts into MediaProvider and then opens the lower-fs child path. Returning a
// positive entry here would let create leak EEXIST-like behavior for the hidden root entry.
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#2121
extern "C" void WrappedPfCreate(fuse_req_t req, uint64_t parent, const char* name, uint32_t mode,
                                void* fi) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfCreate != nullptr) {
        generation->api.pfCreate(req, parent, name, mode, fi);
        return;
    }
    BuiltinWrappedPfCreate(req, parent, name, mode, fi);
}

// Plain readdir delegates to do_readdir_common(..., plus=false). Most modern devices keep
// readdirplus enabled, but this hook is still useful as a fallback for alternative FUSE configs.
// AOSP reference: jni/FuseDaemon.cpp#1944
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1944
extern "C" void WrappedPfReaddir(fuse_req_t req, uint64_t ino, size_t size, off_t off, void* fi) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfReaddir != nullptr) {
        generation->api.pfReaddir(req, ino, size, off, fi);
        return;
    }
    BuiltinWrappedPfReaddir(req, ino, size, off, fi);
}

extern "C" void WrappedDoReaddirCommon(fuse_req_t req, uint64_t ino, size_t size, off_t off,
                                       void* fi, bool plus) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.doReaddirCommon != nullptr) {
        generation->api.doReaddirCommon(req, ino, size, off, fi, plus);
        return;
    }
    BuiltinWrappedDoReaddirCommon(req, ino, size, off, fi, plus);
}

// readdirplus is the common enumeration path on recent Android builds because do_readdir_common()
// emits fuse_direntplus records by first running do_lookup() for each directory entry.
// AOSP references: jni/FuseDaemon.cpp#1904 and #2000
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1904
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#2000
extern "C" void WrappedPfReaddirplus(fuse_req_t req, uint64_t ino, size_t size, off_t off,
                                     void* fi) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfReaddirplus != nullptr) {
        generation->api.pfReaddirplus(req, ino, size, off, fi);
        return;
    }
    BuiltinWrappedPfReaddirplus(req, ino, size, off, fi);
}

extern "C" int WrappedNotifyInvalEntry(void* se, uint64_t parent, const char* name,
                                       size_t namelen) {
    auto fn = reinterpret_cast<int (*)(void*, uint64_t, const char*, size_t)>(
        Process().originalNotifyInvalEntry);
    int ret = fn ? fn(se, parent, name, namelen) : -1;
    DebugLogPrint(3, "notify_inval_entry: ino=0x%lx name=%s ret=%d", (unsigned long)parent,
                  name ? DebugPreview(std::string_view(name, namelen)).c_str() : "null", ret);
    return ret;
}

extern "C" int WrappedNotifyInvalInode(void* se, uint64_t ino, off_t off, off_t len) {
    auto fn = reinterpret_cast<int (*)(void*, uint64_t, off_t, off_t)>(
        Process().originalNotifyInvalInode);
    int ret = fn ? fn(se, ino, off, len) : -1;
    // Device libfuse_jni routes a fallback invalidation path through notify_inval_inode().
    // The callback receives an inode handle, not a verified node object, so only log the rawvalue
    // here.
    DebugLogPrint(3, "notify_inval_inode: ino=0x%lx name=%s ret=%d", (unsigned long)ino,
                  ino == 1 ? "(ROOT)" : "", ret);
    return ret;
}

extern "C" int WrappedReplyErr(fuse_req_t req, int err) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.replyErr != nullptr) {
        return generation->api.replyErr(req, err);
    }
    return BuiltinWrappedReplyErr(req, err);
}

extern "C" void WrappedPfGetattr(fuse_req_t req, uint64_t ino, void* fi) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfGetattr != nullptr) {
        generation->api.pfGetattr(req, ino, fi);
        return;
    }
    BuiltinWrappedPfGetattr(req, ino, fi);
}

extern "C" int WrappedOpen(const char* path, int flags, ...) {
    mode_t mode = 0;
    if ((flags & O_CREAT) != 0) {
        va_list args;
        va_start(args, flags);
        mode = static_cast<mode_t>(va_arg(args, int));
        va_end(args);
    }
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.openLibc != nullptr) {
        return generation->api.openLibc(path, flags, mode, (flags & O_CREAT) != 0);
    }
    return BuiltinWrappedOpenLibc(path, flags, mode, (flags & O_CREAT) != 0);
}

extern "C" int WrappedOpen2(const char* path, int flags) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.open2Libc != nullptr) {
        return generation->api.open2Libc(path, flags);
    }
    return BuiltinWrappedOpen2Libc(path, flags);
}

// Path hook wrappers

bool HiddenPathPolicy::IsTestHiddenUid(uint32_t uid) {
    return ResolveHideRuleForUid(uid) != nullptr;
}

bool HiddenPathPolicy::ShouldHideTestPath(uint32_t uid, std::string_view path) {
    if (!IsTestHiddenUid(uid)) {
        return false;
    }
    if (IsHiddenRootDirectoryPath(path)) {
        return false;
    }
    return IsAnyHiddenSubtreePath(uid, path);
}

bool WrappedIsAppAccessiblePath(void* fuse, const std::string& path, uint32_t uid) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.isAppAccessiblePath != nullptr) {
        return generation->api.isAppAccessiblePath(fuse, path, uid);
    }
    return BuiltinWrappedIsAppAccessiblePath(fuse, path, uid);
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

// These stable hook entry points keep MediaProvider hooked once, then dispatch each call through
// the active native generation. The actual hiding logic lives in the payload implementation.
extern "C" void WrappedPfLookup(fuse_req_t req, uint64_t parent, const char* name) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.pfLookup != nullptr) {
        generation->api.pfLookup(req, parent, name);
        return;
    }
    BuiltinWrappedPfLookup(req, parent, name);
}

extern "C" bool WrappedShouldNotCache(void* fuse, const std::string& path) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.shouldNotCache != nullptr) {
        return generation->api.shouldNotCache(fuse, path);
    }
    return BuiltinWrappedShouldNotCache(fuse, path);
}

extern "C" int WrappedReplyEntry(fuse_req_t req, const struct fuse_entry_param* e) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.replyEntry != nullptr) {
        return generation->api.replyEntry(req, e);
    }
    return BuiltinWrappedReplyEntry(req, e);
}

extern "C" int WrappedReplyAttr(fuse_req_t req, const struct stat* attr, double timeout) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.replyAttr != nullptr) {
        return generation->api.replyAttr(req, attr, timeout);
    }
    return BuiltinWrappedReplyAttr(req, attr, timeout);
}

extern "C" int WrappedReplyBuf(fuse_req_t req, const char* buf, size_t size) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.replyBuf != nullptr) {
        return generation->api.replyBuf(req, buf, size);
    }
    return BuiltinWrappedReplyBuf(req, buf, size);
}

DirectoryEntries WrappedGetDirectoryEntries(void* wrapper, uint32_t uid, const std::string& path,
                                            DIR* dirp) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.getDirectoryEntries != nullptr) {
        return generation->api.getDirectoryEntries(wrapper, uid, path, dirp);
    }
    return BuiltinWrappedGetDirectoryEntries(wrapper, uid, path, dirp);
}

void WrappedAddDirectoryEntriesFromLowerFs(DIR* dirp, LowerFsDirentFilterFn filter,
                                           DirectoryEntries* entries) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.addDirectoryEntriesFromLowerFs != nullptr) {
        generation->api.addDirectoryEntriesFromLowerFs(dirp, filter, entries);
        return;
    }
    BuiltinWrappedAddDirectoryEntriesFromLowerFs(dirp, filter, entries);
}

extern "C" int WrappedLstat(const char* path, struct stat* st) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.lstat != nullptr) {
        return generation->api.lstat(path, st);
    }
    return BuiltinWrappedLstat(path, st);
}

extern "C" int WrappedStat(const char* path, struct stat* st) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.stat != nullptr) {
        return generation->api.stat(path, st);
    }
    return BuiltinWrappedStat(path, st);
}

extern "C" ssize_t WrappedGetxattr(const char* path, const char* name, void* value, size_t size) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.getxattr != nullptr) {
        return generation->api.getxattr(path, name, value, size);
    }
    return BuiltinWrappedGetxattr(path, name, value, size);
}

extern "C" ssize_t WrappedLgetxattr(const char* path, const char* name, void* value, size_t size) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.lgetxattr != nullptr) {
        return generation->api.lgetxattr(path, name, value, size);
    }
    return BuiltinWrappedLgetxattr(path, name, value, size);
}

extern "C" int WrappedMkdirLibc(const char* path, mode_t mode) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.mkdirLibc != nullptr) {
        return generation->api.mkdirLibc(path, mode);
    }
    return BuiltinWrappedMkdirLibc(path, mode);
}

extern "C" int WrappedMknod(const char* path, mode_t mode, dev_t dev) {
    const auto generation = AcquireActiveGeneration();
    ScopedGenerationCall scopedGeneration(generation);
    if (generation != nullptr && generation->api.mknodLibc != nullptr) {
        return generation->api.mknodLibc(path, mode, dev);
    }
    return BuiltinWrappedMknodLibc(path, mode, dev);
}

}  // namespace fusehide
