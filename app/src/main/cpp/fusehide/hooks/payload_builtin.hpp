#pragma once

#include "fusehide/core/state.hpp"

namespace fusehide {

class ScopedCreateUid final {
   public:
    explicit ScopedCreateUid(uint32_t uid) : previous_(gHookThreadState.activeCreateUid) {
        gHookThreadState.activeCreateUid = uid;
    }

    ~ScopedCreateUid() {
        gHookThreadState.activeCreateUid = previous_;
    }

   private:
    uint32_t previous_;
};

bool BuiltinFuseHidePayloadInit(const FuseHideAnchorApi* anchor, FuseHidePayloadApi* outApi);

extern "C" void BuiltinWrappedPfLookup(fuse_req_t req, uint64_t parent, const char* name);
extern "C" void BuiltinWrappedPfReaddirPostfilter(fuse_req_t req, uint64_t ino, uint32_t error_in,
                                                  off_t off_in, off_t off_out, size_t size_out,
                                                  const void* dirents_in, void* fi);
extern "C" void BuiltinWrappedPfLookupPostfilter(fuse_req_t req, uint64_t parent, uint32_t error_in,
                                                 const char* name, struct fuse_entry_out* feo,
                                                 struct fuse_entry_bpf_out* febo);
extern "C" void BuiltinWrappedPfMkdir(fuse_req_t req, uint64_t parent, const char* name,
                                      uint32_t mode);
extern "C" void BuiltinWrappedPfMknod(fuse_req_t req, uint64_t parent, const char* name,
                                      uint32_t mode, uint64_t rdev);
extern "C" void BuiltinWrappedPfUnlink(fuse_req_t req, uint64_t parent, const char* name);
extern "C" void BuiltinWrappedPfRmdir(fuse_req_t req, uint64_t parent, const char* name);
extern "C" void BuiltinWrappedPfRename(fuse_req_t req, uint64_t parent, const char* name,
                                       uint64_t new_parent, const char* new_name, uint32_t flags);
extern "C" void BuiltinWrappedPfCreate(fuse_req_t req, uint64_t parent, const char* name,
                                       uint32_t mode, void* fi);
extern "C" void BuiltinWrappedPfReaddir(fuse_req_t req, uint64_t ino, size_t size, off_t off,
                                        void* fi);
extern "C" void BuiltinWrappedDoReaddirCommon(fuse_req_t req, uint64_t ino, size_t size, off_t off,
                                              void* fi, bool plus);
extern "C" void BuiltinWrappedPfReaddirplus(fuse_req_t req, uint64_t ino, size_t size, off_t off,
                                            void* fi);
extern "C" void BuiltinWrappedPfAccess(fuse_req_t req, uint64_t ino, int mask);
extern "C" void BuiltinWrappedPfOpen(fuse_req_t req, uint64_t ino, void* fi);
extern "C" void BuiltinWrappedPfOpendir(fuse_req_t req, uint64_t ino, void* fi);
extern "C" int BuiltinWrappedReplyEntry(fuse_req_t req, const struct fuse_entry_param* e);
extern "C" int BuiltinWrappedReplyAttr(fuse_req_t req, const struct stat* attr, double timeout);
extern "C" int BuiltinWrappedReplyBuf(fuse_req_t req, const char* buf, size_t size);
extern "C" int BuiltinWrappedReplyErr(fuse_req_t req, int err);
extern "C" void BuiltinWrappedPfGetattr(fuse_req_t req, uint64_t ino, void* fi);

DirectoryEntries BuiltinWrappedGetDirectoryEntries(void* wrapper, uint32_t uid,
                                                   const std::string& path, DIR* dirp);
void BuiltinWrappedAddDirectoryEntriesFromLowerFs(DIR* dirp, LowerFsDirentFilterFn filter,
                                                  DirectoryEntries* entries);
extern "C" int BuiltinWrappedLstat(const char* path, struct stat* st);
extern "C" int BuiltinWrappedStat(const char* path, struct stat* st);
extern "C" ssize_t BuiltinWrappedGetxattr(const char* path, const char* name, void* value,
                                          size_t size);
extern "C" ssize_t BuiltinWrappedLgetxattr(const char* path, const char* name, void* value,
                                           size_t size);
extern "C" int BuiltinWrappedMkdirLibc(const char* path, mode_t mode);
extern "C" int BuiltinWrappedMknodLibc(const char* path, mode_t mode, dev_t dev);
int BuiltinWrappedOpenLibc(const char* path, int flags, mode_t mode, bool hasMode);
extern "C" int BuiltinWrappedOpen2Libc(const char* path, int flags);
extern "C" bool BuiltinWrappedShouldNotCache(void* fuse, const std::string& path);
bool BuiltinWrappedIsAppAccessiblePath(void* fuse, const std::string& path, uint32_t uid);

}  // namespace fusehide
