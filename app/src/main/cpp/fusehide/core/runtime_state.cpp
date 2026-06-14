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

#include "fusehide/policy/path_policy.hpp"

namespace fusehide {

thread_local HookThreadState gHookThreadState;

AnchorProcessState& ProcessState() {
    static AnchorProcessState state;
    return state;
}

namespace ReplyErrorBridge {

namespace {

void LogFallbackFailure(const char* caller) {
    if (ShouldLogLimited(gReplyErrFallbackLogCount, 8)) {
        __android_log_print(6, kLogTag,
                            "%s could not resolve fuse_reply_err; delegating to the original path",
                            caller);
    }
}

}  // namespace

FuseReplyErrFn Original() {
    return reinterpret_cast<FuseReplyErrFn>(ProcessState().originalReplyErr);
}

FuseReplyErrFn Resolve() {
    if (auto replyErr = Original(); replyErr != nullptr) {
        return replyErr;
    }

    static std::atomic<void*> sResolvedReplyErr{nullptr};
    void* cached = sResolvedReplyErr.load(std::memory_order_acquire);
    if (cached != nullptr) {
        return reinterpret_cast<FuseReplyErrFn>(cached);
    }

    void* resolved = dlsym(RTLD_DEFAULT, "fuse_reply_err");
    if (resolved == nullptr) {
        return nullptr;
    }

    sResolvedReplyErr.store(resolved, std::memory_order_release);
    return reinterpret_cast<FuseReplyErrFn>(resolved);
}

std::optional<int> Reply(fuse_req_t req, int err, const char* caller) {
    auto replyErr = Resolve();
    if (replyErr == nullptr) {
        LogFallbackFailure(caller);
        return std::nullopt;
    }
    return replyErr(req, err);
}

}  // namespace ReplyErrorBridge

std::mutex gUidErrRemapMutex;

struct UidErrRemapState {
    int baselineErr = 0;
    std::chrono::steady_clock::time_point expiresAt{};
    uint32_t pendingCount = 0;
};

std::unordered_map<uint32_t, UidErrRemapState> gUidErrRemapStates;

namespace {}  // namespace

uint32_t RuntimeState::ReqUid(fuse_req_t req) {
    if (req == nullptr) {
        return 0;
    }
    // The reverse-engineered device build reads req->ctx.uid from fuse_req + 0x3c in pf_getattr()
    // and related handlers. AOSP accesses req->ctx.uid directly in C++, but our low-level hooks
    // only receive the opaque request pointer, so this mirrors the verified device layout. AOSP
    // reference: jni/FuseDaemon.cpp#2134 and #2145
    // https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#2134
    return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(req) + 0x3c);
}

void RuntimeState::RememberFuseSession(fuse_req_t req) {
    if (req != nullptr && req->se != nullptr) {
        ProcessState().lastFuseSession.store(req->se, std::memory_order_relaxed);
    }
}

// Shared dentry cache is not scoped per uid. Once another app resolves the hidden entry, the
// target uid can reuse that positive cache unless we actively invalidate the root dentry.
void RuntimeState::ScheduleHiddenEntryInvalidation() {
    auto& process = ProcessState();
    auto notifyEntry = reinterpret_cast<int (*)(void*, uint64_t, const char*, size_t)>(
        process.originalNotifyInvalEntry);
    void* session = process.lastFuseSession.load(std::memory_order_relaxed);
    if (notifyEntry == nullptr || session == nullptr) {
        return;
    }
    if (process.hiddenEntryInvalidationPending.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    const uint64_t parent = process.hiddenRootParentInode.load(std::memory_order_relaxed);
    if (parent == 0) {
        process.hiddenEntryInvalidationPending.store(false, std::memory_order_release);
        return;
    }
    EnqueueAnchorTask(std::chrono::milliseconds(50), [notifyEntry, session]() {
        auto& processState = ProcessState();
        const uint64_t rootParent =
            processState.hiddenRootParentInode.load(std::memory_order_relaxed);
        const auto rule = RuleForAnyPackage();
        std::unordered_set<std::string> namesToInvalidate;
        if (rule != nullptr) {
            for (const auto& rootEntryName : rule->hiddenRootEntryNames) {
                namesToInvalidate.emplace(rootEntryName);
            }
        }

        if (rule != nullptr && rule->enableHideAllRootEntries) {
            for (const auto& rootPath : kVisibleStorageRoots) {
                DIR* dir = opendir(std::string(rootPath).c_str());
                if (dir == nullptr) {
                    continue;
                }
                while (dirent* entry = readdir(dir)) {
                    const std::string_view name(entry->d_name);
                    if (IsWildcardRootEntryCandidate(name)) {
                        namesToInvalidate.emplace(name);
                    }
                }
                closedir(dir);
            }
        }

        for (const auto& name : namesToInvalidate) {
            const int ret = notifyEntry(session, rootParent, name.c_str(), name.size());
            DebugLogPrint(4, "scheduled hidden entry invalidation parent=0x%lx name=%s ret=%d",
                          (unsigned long)rootParent, DebugPreview(name).c_str(), ret);
        }
        processState.hiddenEntryInvalidationPending.store(false, std::memory_order_release);
    });
}

void RuntimeState::ScheduleSpecificEntryInvalidation(uint64_t parent, std::string_view name) {
    auto& process = ProcessState();
    auto notifyEntry = reinterpret_cast<int (*)(void*, uint64_t, const char*, size_t)>(
        process.originalNotifyInvalEntry);
    void* session = process.lastFuseSession.load(std::memory_order_relaxed);
    if (notifyEntry == nullptr || session == nullptr || parent == 0 || name.empty()) {
        return;
    }
    const std::string ownedName(name);
    EnqueueAnchorTask(std::chrono::milliseconds(25), [notifyEntry, session, parent, ownedName]() {
        const int ret = notifyEntry(session, parent, ownedName.c_str(), ownedName.size());
        DebugLogPrint(4, "scheduled specific entry invalidation parent=%s name=%s ret=%d",
                      InodePath(parent).c_str(), DebugPreview(ownedName).c_str(), ret);
    });
}

// Track subtree inodes so later getattr/readdir replies can also be forced uncached.
void RuntimeState::ScheduleHiddenInodeInvalidation(uint64_t ino) {
    auto& process = ProcessState();
    auto notifyInode =
        reinterpret_cast<int (*)(void*, uint64_t, off_t, off_t)>(process.originalNotifyInvalInode);
    void* session = process.lastFuseSession.load(std::memory_order_relaxed);
    if (notifyInode == nullptr || session == nullptr || ino == 0) {
        return;
    }
    EnqueueAnchorTask(std::chrono::milliseconds(50), [notifyInode, session, ino]() {
        const int ret = notifyInode(session, ino, 0, 0);
        DebugLogPrint(4, "scheduled hidden inode invalidation ino=0x%lx ret=%d", (unsigned long)ino,
                      ret);
    });
}

std::string InodePath(uint64_t ino) {
    if (ino == 1)
        return "(ROOT)";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "(%p)", (void*)ino);
    // Keep inode values opaque in debug output.
    // On the analyzed device build, node::BuildPath() is a C++ member function with an
    // out-parameter return ABI and internal locking, so this logging helper must not assume
    // that an inode value can be converted into a valid node object or path string.
    return std::string(buf);
}

bool IsHiddenLookupTarget(uint32_t uid, uint64_t parent, uint32_t error_in, const char* name) {
    if (!IsTestHiddenUid(uid) || error_in != 0 || name == nullptr) {
        return false;
    }
    if (IsHiddenLookupCacheTarget(uid, parent, name)) {
        return true;
    }
    const auto kind = ClassifyHiddenNamedTarget(uid, parent, name);
    return kind == HiddenNamedTargetKind::Root || kind == HiddenNamedTargetKind::Descendant;
}

bool IsHiddenLookupCacheTarget(uint32_t uid, uint64_t parent, const char* name) {
    if (name == nullptr) {
        return false;
    }
    const auto rule = ResolveHideRuleForUid(uid);
    if (rule == nullptr) {
        return false;
    }
    const uint64_t rootParent =
        ProcessState().hiddenRootParentInode.load(std::memory_order_relaxed);
    if (rootParent != 0 && parent == rootParent && rule->enableHideAllRootEntries &&
        IsWildcardRootEntryCandidate(name)) {
        return true;
    }
    return HiddenPathPolicy::IsConfiguredHiddenRootEntryName(uid, name) &&
           (rootParent == 0 || parent == rootParent);
}

std::optional<HiddenNamedTargetKind> ClassifyHiddenNamedTargetByTrackedPath(uint32_t uid,
                                                                            uint64_t parent,
                                                                            const char* name) {
    if (name == nullptr) {
        return std::nullopt;
    }
    const auto parentPath = LookupTrackedPathForInode(parent);
    if (!parentPath.has_value()) {
        return std::nullopt;
    }
    const std::string childPath = HiddenPathPolicy::JoinPathComponent(*parentPath, name);
    if (HiddenPathPolicy::IsExactHiddenTargetPath(uid, childPath)) {
        return HiddenNamedTargetKind::Root;
    }
    if (HiddenPathPolicy::IsAnyHiddenSubtreePath(uid, childPath)) {
        return HiddenNamedTargetKind::Descendant;
    }
    return std::nullopt;
}

// Classify the current name-based operation as either the hidden root entry itself or a descendant
// below a previously learned hidden subtree inode.
HiddenNamedTargetKind ClassifyHiddenNamedTarget(uint32_t uid, uint64_t parent, const char* name) {
    if (!IsTestHiddenUid(uid) || name == nullptr) {
        return HiddenNamedTargetKind::None;
    }
    const uint64_t rootParent =
        ProcessState().hiddenRootParentInode.load(std::memory_order_relaxed);
    if (parent != 0 && parent != rootParent && IsTrackedHiddenSubtreeInode(parent)) {
        return HiddenNamedTargetKind::Descendant;
    }
    const auto rule = ResolveHideRuleForUid(uid);
    if (rule == nullptr) {
        return HiddenNamedTargetKind::None;
    }
    if (parent == rootParent && rule->enableHideAllRootEntries &&
        IsWildcardRootEntryCandidate(name)) {
        return HiddenNamedTargetKind::Root;
    }
    if (HiddenPathPolicy::IsConfiguredHiddenRootEntryName(uid, name) &&
        (rootParent == 0 || parent == rootParent)) {
        return HiddenNamedTargetKind::Root;
    }
    if (const auto trackedPathKind = ClassifyHiddenNamedTargetByTrackedPath(uid, parent, name);
        trackedPathKind.has_value()) {
        return *trackedPathKind;
    }
    return HiddenNamedTargetKind::None;
}

bool ReplyHiddenNamedTargetError(fuse_req_t req, const char* opName, HiddenNamedTargetKind kind,
                                 int rootErr, int descendantErr) {
    if (kind == HiddenNamedTargetKind::None) {
        return false;
    }
    const int err = kind == HiddenNamedTargetKind::Root ? rootErr : descendantErr;
    DebugLogPrint(4, "%s hide named target err=%d root=%d", opName, err,
                  kind == HiddenNamedTargetKind::Root ? 1 : 0);
    if (ReplyErrorBridge::Reply(req, err, opName).has_value()) {
        return true;
    }
    ArmHiddenErrorRemap(req, err, opName);
    return false;
}

namespace {

bool IsExistenceLeakErrno(int err) {
    switch (err) {
        case EEXIST:
        case EISDIR:
        case ENOTEMPTY:
        case ENOTDIR:
            return true;
        default:
            return false;
    }
}

void LogErrnoRemapEvent(const char* source, fuse_req_t req, uint32_t uid, int fromErr, int toErr) {
    if (!ShouldLogLimited(gErrnoRemapLogCount, 24)) {
        return;
    }
    __android_log_print(4, kLogTag, "errno remap source=%s req=%p unique=%lu uid=%u from=%d to=%d",
                        source, req, req ? (unsigned long)req->unique : 0UL,
                        static_cast<unsigned>(uid), fromErr, toErr);
}

}  // namespace

void ArmHiddenErrorRemap(fuse_req_t req, int err, const char* opName) {
    if (req == nullptr || err <= 0) {
        return;
    }
    gHookThreadState.pendingHiddenErrReq = req;
    gHookThreadState.pendingHiddenErrReqUnique = req->unique;
    gHookThreadState.pendingHiddenErrno = err;
    const uint32_t uid = RuntimeState::ReqUid(req);
    if (uid != 0) {
        std::lock_guard<std::mutex> lock(gUidErrRemapMutex);
        UidErrRemapState& state = gUidErrRemapStates[uid];
        state.baselineErr = err;
        state.expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        state.pendingCount = std::min<uint32_t>(state.pendingCount + 1, 8);
    }
    DebugLogPrint(4, "%s arm hidden errno remap req=%p unique=%lu baseline=%d", opName, req,
                  req ? (unsigned long)req->unique : 0UL, err);
}

void ArmHiddenCreateLeakRemap(fuse_req_t req, const char* opName) {
    if (req == nullptr) {
        return;
    }
    const uint32_t uid = RuntimeState::ReqUid(req);
    if (uid == 0 || !HiddenPathPolicy::IsTestHiddenUid(uid)) {
        return;
    }
    std::lock_guard<std::mutex> lock(gUidErrRemapMutex);
    UidErrRemapState& state = gUidErrRemapStates[uid];
    state.baselineErr = EPERM;
    state.expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    state.pendingCount = std::min<uint32_t>(state.pendingCount + 1, 8);
    DebugLogPrint(4, "%s arm create leak remap uid=%u baseline=%d", opName,
                  static_cast<unsigned>(uid), EPERM);
}

int MaybeRewriteHiddenLeakErrno(fuse_req_t req, int err, const char* caller) {
    if (req != nullptr && gHookThreadState.pendingHiddenErrReq == req &&
        gHookThreadState.pendingHiddenErrReqUnique == req->unique &&
        gHookThreadState.pendingHiddenErrno > 0 && err > 0 && IsExistenceLeakErrno(err)) {
        const int baselineErr = gHookThreadState.pendingHiddenErrno;
        gHookThreadState.pendingHiddenErrReq = nullptr;
        gHookThreadState.pendingHiddenErrReqUnique = 0;
        gHookThreadState.pendingHiddenErrno = 0;

        if (err != baselineErr) {
            DebugLogPrint(4, "%s remap leaked errno req=%p unique=%lu from=%d to=%d", caller, req,
                          (unsigned long)req->unique, err, baselineErr);
            LogErrnoRemapEvent("req", req, RuntimeState::ReqUid(req), err, baselineErr);
            return baselineErr;
        }
        return err;
    }

    if (req == nullptr || err <= 0 || !IsExistenceLeakErrno(err)) {
        return err;
    }

    const uint32_t uid = RuntimeState::ReqUid(req);
    if (uid == 0) {
        return err;
    }

    int uidBaselineErr = 0;
    {
        std::lock_guard<std::mutex> lock(gUidErrRemapMutex);
        const auto it = gUidErrRemapStates.find(uid);
        if (it != gUidErrRemapStates.end()) {
            if (it->second.expiresAt >= std::chrono::steady_clock::now() &&
                it->second.baselineErr > 0 && it->second.pendingCount > 0) {
                uidBaselineErr = it->second.baselineErr;
                it->second.pendingCount--;
            }
            if (it->second.pendingCount == 0 ||
                it->second.expiresAt < std::chrono::steady_clock::now()) {
                gUidErrRemapStates.erase(it);
            }
        }
    }

    if (uidBaselineErr > 0 && uidBaselineErr != err) {
        DebugLogPrint(4, "%s remap leaked errno by uid=%u from=%d to=%d", caller,
                      static_cast<unsigned>(uid), err, uidBaselineErr);
        LogErrnoRemapEvent("uid", req, uid, err, uidBaselineErr);
        return uidBaselineErr;
    }
    return err;
}

bool IsTrackedHiddenSubtreeInode(uint64_t ino) {
    auto& process = ProcessState();
    std::lock_guard<std::mutex> lock(process.hiddenSubtreeInodesMutex);
    return process.hiddenSubtreeInodes.find(ino) != process.hiddenSubtreeInodes.end();
}

bool TrackHiddenSubtreeInode(uint64_t ino) {
    if (ino == 0) {
        return false;
    }
    auto& process = ProcessState();
    std::lock_guard<std::mutex> lock(process.hiddenSubtreeInodesMutex);
    return process.hiddenSubtreeInodes.insert(ino).second;
}

bool RemoveTrackedHiddenSubtreeInode(uint64_t ino) {
    if (ino == 0) {
        return false;
    }
    auto& process = ProcessState();
    std::lock_guard<std::mutex> lock(process.hiddenSubtreeInodesMutex);
    return process.hiddenSubtreeInodes.erase(ino) != 0;
}

std::optional<std::string> LookupTrackedPathForInode(uint64_t ino) {
    if (ino == 0) {
        return std::nullopt;
    }
    auto& process = ProcessState();
    const uint64_t rootParent = process.hiddenRootParentInode.load(std::memory_order_relaxed);
    if (rootParent != 0 && ino == rootParent) {
        return std::string(kVisibleStorageRoots[0]);
    }
    std::lock_guard<std::mutex> lock(process.inodePathCacheMutex);
    const auto it = process.inodePathCache.find(ino);
    if (it == process.inodePathCache.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<uint64_t> LookupTrackedInodeForPath(std::string_view path) {
    if (path.empty()) {
        return std::nullopt;
    }
    auto& process = ProcessState();
    const uint64_t rootParent = process.hiddenRootParentInode.load(std::memory_order_relaxed);
    if (rootParent != 0 && path == kVisibleStorageRoots[0]) {
        return rootParent;
    }
    std::lock_guard<std::mutex> lock(process.inodePathCacheMutex);
    for (const auto& [ino, trackedPath] : process.inodePathCache) {
        if (trackedPath == path) {
            return ino;
        }
    }
    return std::nullopt;
}

void RememberTrackedPathForInode(uint64_t ino, std::string_view path) {
    if (ino == 0 || path.empty()) {
        return;
    }
    auto& process = ProcessState();
    std::lock_guard<std::mutex> lock(process.inodePathCacheMutex);
    process.inodePathCache[ino] = std::string(path);
}

void RememberRecentHiddenParentPath(uint32_t uid, std::string_view path) {
    if (path.empty()) {
        return;
    }
    auto& process = ProcessState();
    std::lock_guard<std::mutex> lock(process.recentHiddenParentPathsMutex);
    process.recentHiddenParentPathAnyUid = std::string(path);
    process.recentHiddenParentPathAnyUidOwner = uid;
    if (uid != 0) {
        process.recentHiddenParentPaths[uid] = process.recentHiddenParentPathAnyUid;
        process.recentHiddenParentPathUids[uid] = uid;
    }
}

std::optional<std::string> LookupRecentHiddenParentPath(uint32_t uid, uint32_t* matchedHiddenUid) {
    auto& process = ProcessState();
    std::lock_guard<std::mutex> lock(process.recentHiddenParentPathsMutex);
    if (uid != 0) {
        const auto it = process.recentHiddenParentPaths.find(uid);
        if (it != process.recentHiddenParentPaths.end()) {
            if (matchedHiddenUid != nullptr) {
                const auto uidIt = process.recentHiddenParentPathUids.find(uid);
                *matchedHiddenUid =
                    uidIt != process.recentHiddenParentPathUids.end() ? uidIt->second : uid;
            }
            return it->second;
        }
    }
    if (process.recentHiddenParentPathAnyUid.empty()) {
        return std::nullopt;
    }
    if (matchedHiddenUid != nullptr) {
        *matchedHiddenUid = process.recentHiddenParentPathAnyUidOwner;
    }
    return process.recentHiddenParentPathAnyUid;
}

void ClearRecentHiddenParentPath(uint32_t uid) {
    auto& process = ProcessState();
    std::lock_guard<std::mutex> lock(process.recentHiddenParentPathsMutex);
    if (uid != 0) {
        process.recentHiddenParentPaths.erase(uid);
        process.recentHiddenParentPathUids.erase(uid);
    }
    if (uid == 0 || uid == process.recentHiddenParentPathAnyUidOwner) {
        process.recentHiddenParentPathAnyUid.clear();
        process.recentHiddenParentPathAnyUidOwner = 0;
    }
}

// AOSP only decides dentry caching from the resolved path, not from uid policy.
// Once the daemon sees any path inside the hidden subtree, force cache invalidation globally for
// that subtree so positive dentries from other apps stop leaking into the target uid.
void NoteHiddenSubtreePathForCache(std::string_view path) {
    if (!HiddenPathPolicy::IsAnyHiddenSubtreePath(path)) {
        return;
    }

    // AOSP get_entry_timeout()/pf_getattr cache decisions are path-based rather than uid-based.
    // Once this subtree is observed anywhere in the daemon, proactively invalidate the root dentry
    // so a positive lookup seeded by another uid does not stay shared in kernel/VFS cache.
    RuntimeState::ScheduleHiddenEntryInvalidation();

    if (gHookThreadState.inPfLookup && gHookThreadState.currentLookupParentInode != 0) {
        const uint64_t rootParent =
            ProcessState().hiddenRootParentInode.load(std::memory_order_relaxed);
        if (HiddenPathPolicy::IsExactHiddenTargetPath(path) &&
            gHookThreadState.currentLookupParentInode == rootParent) {
            RemoveTrackedHiddenSubtreeInode(gHookThreadState.currentLookupParentInode);
            return;
        }
        gHookThreadState.trackHiddenSubtreeLookup = true;
        if (TrackHiddenSubtreeInode(gHookThreadState.currentLookupParentInode)) {
            DebugLogPrint(4, "track hidden lookup parent=%s path=%s",
                          InodePath(gHookThreadState.currentLookupParentInode).c_str(),
                          DebugPreview(path).c_str());
            RuntimeState::ScheduleHiddenInodeInvalidation(
                gHookThreadState.currentLookupParentInode);
        }
    }

    if (gHookThreadState.inPfGetattr && gHookThreadState.pfGetattrIno != 0) {
        gHookThreadState.zeroAttrCacheForCurrentGetattr = true;
        if (TrackHiddenSubtreeInode(gHookThreadState.pfGetattrIno)) {
            DebugLogPrint(4, "track hidden getattr ino=%s path=%s",
                          InodePath(gHookThreadState.pfGetattrIno).c_str(),
                          DebugPreview(path).c_str());
            RuntimeState::ScheduleHiddenInodeInvalidation(gHookThreadState.pfGetattrIno);
        }
    }
}

uint32_t ReqUid(fuse_req_t req) {
    return RuntimeState::ReqUid(req);
}

void RememberFuseSession(fuse_req_t req) {
    RuntimeState::RememberFuseSession(req);
}

void ScheduleHiddenEntryInvalidation() {
    RuntimeState::ScheduleHiddenEntryInvalidation();
}

void ScheduleHiddenInodeInvalidation(uint64_t ino) {
    RuntimeState::ScheduleHiddenInodeInvalidation(ino);
}

}  // namespace fusehide
