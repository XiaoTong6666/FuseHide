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
#include "fusehide/core/invalidation_admission.hpp"
#include "fusehide/hooks/callback_coverage.hpp"
#include "fusehide/filters/sensitive_direntplus_cache.hpp"

#include <condition_variable>
#include <map>
#include <unordered_set>

namespace fusehide {

// Hook state for libfuse_jni.so running inside the MediaProvider process.
HookOriginal<PfLookupFn> gOriginalPfLookup;
HookOriginal<PfLookupPostfilterFn> gOriginalPfLookupPostfilter;
HookOriginal<PfAccessFn> gOriginalPfAccess;
HookOriginal<PfFileInfoFn> gOriginalPfOpen;
HookOriginal<PfFileInfoFn> gOriginalPfOpendir;
HookOriginal<PfMknodFn> gOriginalPfMknod;
HookOriginal<PfMkdirFn> gOriginalPfMkdir;
HookOriginal<PfRemoveFn> gOriginalPfUnlink;
HookOriginal<PfRemoveFn> gOriginalPfRmdir;
HookOriginal<PfRenameFn> gOriginalPfRename;
HookOriginal<PfCreateFn> gOriginalPfCreate;
HookOriginal<PfReaddirFn> gOriginalPfReaddir;
HookOriginal<PfReaddirPostfilterFn> gOriginalPfReaddirPostfilter;
HookOriginal<PfReaddirFn> gOriginalPfReaddirplus;
HookOriginal<DoReaddirCommonFn> gOriginalDoReaddirCommon;
HookOriginal<PfFileInfoFn> gOriginalPfGetattr;
HookOriginal<LibcOpenFn> gOriginalOpen;
HookOriginal<LibcOpen2Fn> gOriginalOpen2;
HookOriginal<FuseReqCtxFn> gFuseReqCtx;
HookOriginal<LibcMkdirFn> gOriginalMkdir;
HookOriginal<LibcMknodFn> gOriginalMknod;
HookOriginal<LibcStatFn> gOriginalLstat;
HookOriginal<LibcStatFn> gOriginalStat;
HookOriginal<LibcXattrFn> gOriginalGetxattr;
HookOriginal<LibcXattrFn> gOriginalLgetxattr;
HookOriginal<ShouldNotCacheFn> gOriginalShouldNotCache;
HookOriginal<NotifyInvalEntryFn> gOriginalNotifyInvalEntry;
HookOriginal<NotifyInvalInodeFn> gOriginalNotifyInvalInode;
HookOriginal<FuseSessionDestroyFn> gOriginalFuseSessionDestroy;
HookOriginal<FuseReplyAttrFn> gOriginalReplyAttr;
HookOriginal<FuseReplyEntryFn> gOriginalReplyEntry;
HookOriginal<FuseReplyBufFn> gOriginalReplyBuf;
HookOriginal<FuseReplyErrFn> gOriginalReplyErr;
DynamicAbiHookOriginal gOriginalGetDirectoryEntries;
DynamicAbiHookOriginal gOriginalAddDirectoryEntriesFromLowerFs;
std::atomic<DirectoryEntriesAbi> gDirectoryEntriesAbi{DirectoryEntriesAbi::kUnknown};
std::atomic<void*> gLastFuseSession{nullptr};
thread_local bool gInPfLookup = false;
thread_local bool gInPfLookupPostfilter = false;
thread_local bool gInPfReaddir = false;
thread_local bool gInPfReaddirPostfilter = false;
thread_local bool gInPfReaddirplus = false;
thread_local bool gInPfGetattr = false;
thread_local uint32_t gPfGetattrUid = 0;
thread_local uint32_t gPfReaddirUid = 0;
thread_local uint32_t gCurrentLookupUid = 0;
thread_local uint64_t gPfGetattrIno = 0;
thread_local uint64_t gPfReaddirIno = 0;
thread_local uint64_t gCurrentLookupParentInode = 0;
thread_local std::string_view gCurrentLookupName;
thread_local bool gTrackRootHiddenLookup = false;
thread_local bool gTrackHiddenSubtreeLookup = false;
thread_local bool gZeroAttrCacheForCurrentGetattr = false;
thread_local fuse_req_t gPendingHiddenErrReq = nullptr;
thread_local uint64_t gPendingHiddenErrReqUnique = 0;
thread_local SessionToken gPendingHiddenErrToken;
thread_local int gPendingHiddenErrno = 0;
thread_local uint64_t gCurrentReaddirReqUnique = 0;

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
    return gOriginalReplyErr.get();
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

struct UidErrRemapState {
    int baselineErr = 0;
    std::chrono::steady_clock::time_point expiresAt{};
    uint32_t pendingCount = 0;
};

namespace {

constexpr size_t kMaxTrackedPathEntries = 16384;

struct TransparentStringHash {
    using is_transparent = void;

    size_t operator()(std::string_view value) const noexcept {
        return std::hash<std::string_view>{}(value);
    }

    size_t operator()(const std::string& value) const noexcept {
        return (*this)(std::string_view(value));
    }
};

struct TransparentStringEqual {
    using is_transparent = void;

    bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
        return lhs == rhs;
    }
};

struct SessionTracking {
    std::mutex mutex;
    uint64_t rootIno = 0;
    std::string rootPath;
    bool rootRefreshNeeded = false;
    std::unordered_map<uint64_t, std::unordered_set<std::string>> deferredEntries;
    std::unordered_set<uint64_t> deferredInodes;
    size_t deferredEntryCount = 0;
    std::unordered_map<uint64_t, std::string> inodeToPath;
    std::unordered_map<std::string, uint64_t, TransparentStringHash, TransparentStringEqual>
        pathToInode;
    std::unordered_map<uint64_t, std::vector<std::shared_ptr<const CompiledHideRule>>>
        hiddenSubtreeRules;
    std::unordered_map<uint64_t, PendingReaddirContext> pendingReaddir;
    std::unordered_map<uint32_t, std::string> recentParent;
    std::string recentAnyParent;
    uint32_t recentAnyUid = 0;
    std::unordered_map<uint32_t, UidErrRemapState> uidErrRemap;
};

thread_local SessionToken gActiveFuseRequestToken;

enum class InvalidationKind { Root, Parent, Entry, Inode };

struct InvalidationTask {
    InvalidationKind kind;
    void* session;
    uint64_t generation;
    uint64_t ino;
    std::string name;
    std::string logicalPath;
    bool sweepAll = false;
    uint64_t configEpoch = 0;
};

struct InvalidationKey {
    void* session;
    uint64_t generation;
    InvalidationKind kind;
    uint64_t ino;
    std::string name;
    uint64_t configEpoch = 0;

    bool operator==(const InvalidationKey&) const = default;
};

struct InvalidationKeyHash {
    size_t operator()(const InvalidationKey& key) const {
        size_t hash = std::hash<void*>{}(key.session);
        const auto combine = [&](size_t part) {
            hash ^= part + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        };
        combine(std::hash<uint64_t>{}(key.generation));
        combine(std::hash<int>{}(static_cast<int>(key.kind)));
        combine(std::hash<uint64_t>{}(key.ino));
        combine(std::hash<std::string>{}(key.name));
        combine(std::hash<uint64_t>{}(key.configEpoch));
        return hash;
    }
};

InvalidationKey TaskKey(const InvalidationTask& task) {
    return {task.session, task.generation, task.kind, task.ino, task.name, task.configEpoch};
}

struct InvalidationSession {
    uint64_t generation;
    bool retiring = false;
    void* fuseUserdata = nullptr;
    size_t rootProbeReaders = 0;
    bool callbackCoverageKnown = false;
    bool callbackCoverageComplete = false;
    bool callbackTableVerified = false;
    bool callbackRepairAttempted = false;
    bool rootPending = false;
    bool rootNeedsReplay = false;
    uint64_t rootReplayEpoch = 0;
    std::shared_ptr<SessionTracking> tracking = std::make_shared<SessionTracking>();
};

using VerifiedRootLookupFn = const void* (*)(const void*, const std::string&);
std::atomic<VerifiedRootLookupFn> gVerifiedPrimaryRootLookup{nullptr};

// Process-resident like the native hooks. Never destroy the mutex while the worker is alive.
struct InvalidationQueue {
    std::mutex mutex;
    std::condition_variable cv;
    std::multimap<std::chrono::steady_clock::time_point, InvalidationTask> pending;
    std::unordered_set<InvalidationKey, InvalidationKeyHash> pendingKeys;
    // Overflow uses bounded parent sweeps, not one allocation per hidden child.
    std::unordered_map<InvalidationKey, InvalidationTask, InvalidationKeyHash> overflowParents;
    // Exact queued inode work, not an unsafe parent-entry approximation.
    std::unordered_map<InvalidationKey, InvalidationTask, InvalidationKeyHash> overflowInodes;
    std::unordered_map<void*, InvalidationSession> sessions;
    void* runningSession = nullptr;
    std::optional<InvalidationKey> runningKey;
    uint64_t nextGeneration = 0;
    bool workerStarted = false;
    uint64_t notifyFailures = 0;
    uint64_t currentConfigEpoch = 0;
    InvalidationBatchCounters batch;
};

InvalidationQueue& Invalidations() {
    static auto* queue = new InvalidationQueue();
    return *queue;
}

std::shared_ptr<SessionTracking> TrackingForToken(SessionToken token) {
    if (!token) {
        return {};
    }
    auto& queue = Invalidations();
    std::lock_guard<std::mutex> lock(queue.mutex);
    const auto it = queue.sessions.find(token.se);
    if (it == queue.sessions.end() || it->second.retiring ||
        it->second.generation != token.generation) {
        return {};
    }
    return it->second.tracking;
}

std::shared_ptr<SessionTracking> CurrentTracking() {
    return TrackingForToken(gActiveFuseRequestToken);
}

bool IsProtectedMountTarget(std::string_view logicalPath) {
    const auto equalsIgnoreCase = [](std::string_view lhs, std::string_view rhs) {
        if (lhs.size() != rhs.size())
            return false;
        for (size_t index = 0; index < lhs.size(); ++index) {
            const auto fold = [](char c) {
                return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
            };
            if (fold(lhs[index]) != fold(rhs[index]))
                return false;
        }
        return true;
    };
    // Exact AOSP FuseUtils::containsMount path semantics, not a subtree-wide denylist.
    const auto root = VisibleStorageRootForPath(logicalPath);
    if (!root || logicalPath.size() <= root->size())
        return false;
    const std::string_view suffix = logicalPath.substr(root->size());
    return equalsIgnoreCase(suffix, "/Android") || equalsIgnoreCase(suffix, "/Android/data") ||
           equalsIgnoreCase(suffix, "/Android/obb");
}

bool RunInvalidation(const InvalidationTask& task) {
    if (task.kind != InvalidationKind::Root &&
        (task.logicalPath.empty() || IsProtectedMountTarget(task.logicalPath))) {
        DebugLogPrint(4, "skip unknown or mount-protected invalidation se=%p ino=0x%lx",
                      task.session, static_cast<unsigned long>(task.ino));
        // Missing paths are not successful notifications. Protected mount
        // targets are exempt and must remain untouched.
        return !task.logicalPath.empty() && IsProtectedMountTarget(task.logicalPath);
    }
    if (task.kind == InvalidationKind::Inode) {
        const auto notify = gOriginalNotifyInvalInode.get();
        if (notify != nullptr) {
            const int ret = notify(task.session, task.ino, 0, 0);
            DebugLogPrint(4, "scheduled hidden inode invalidation ino=0x%lx ret=%d",
                          (unsigned long)task.ino, ret);
            return ret == 0 || ret == -ENOENT;
        }
        return false;
    }

    const auto notify = gOriginalNotifyInvalEntry.get();
    if (notify == nullptr) {
        return false;
    }
    if (task.kind == InvalidationKind::Entry) {
        const int ret = notify(task.session, task.ino, task.name.c_str(), task.name.size());
        DebugLogPrint(4, "scheduled specific entry invalidation parent=%s name=%s ret=%d",
                      InodePath(task.ino).c_str(), DebugPreview(task.name).c_str(), ret);
        return ret == 0 || ret == -ENOENT;
    }

    const auto rule = RuleForAnyPackage();
    std::unordered_set<std::string> names;
    bool scan = task.kind == InvalidationKind::Parent || task.sweepAll;
    bool complete = true;
    const auto send = [&](std::string_view name) {
        if (IsProtectedMountTarget(HiddenPathPolicy::JoinPathComponent(task.logicalPath, name)))
            return;
        const int ret = notify(task.session, task.ino, name.data(), name.size());
        if (ret != 0 && ret != -ENOENT)
            complete = false;
        DebugLogPrint(4, "scheduled hidden entry invalidation parent=0x%lx name=%s ret=%d",
                      static_cast<unsigned long>(task.ino), DebugPreview(name).c_str(), ret);
    };
    if (rule != nullptr && task.kind == InvalidationKind::Root) {
        names.insert(rule->hiddenRootEntryNames.begin(), rule->hiddenRootEntryNames.end());
        scan = scan || HideAllForStorageRoot(*rule, task.logicalPath);
    }
    for (const auto& name : names)
        send(name);
    if (scan) {
        DIR* dir = opendir(task.logicalPath.c_str());
        if (dir == nullptr) {
            DebugLogPrint(2, "invalidation sweep failed path=%s errno=%d",
                          DebugPreview(task.logicalPath).c_str(), errno);
            return false;
        }
        while (dirent* entry = readdir(dir)) {
            if (entry->d_name[0] == '.' &&
                (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
                continue;
            if ((task.kind == InvalidationKind::Parent || task.sweepAll ||
                 IsWildcardRootEntryCandidate(entry->d_name, task.logicalPath)) &&
                !IsProtectedMountTarget(
                    HiddenPathPolicy::JoinPathComponent(task.logicalPath, entry->d_name))) {
                if (!names.contains(entry->d_name))
                    send(entry->d_name);
            }
        }
        closedir(dir);
    }
    return complete;
}

void RecordInvalidationCompletion(InvalidationQueue& queue, const InvalidationTask& task,
                                  bool success, bool compensated) {
    if (!success)
        ++queue.notifyFailures;
    if (task.configEpoch == 0 || task.configEpoch != queue.batch.epoch)
        return;
    queue.batch.finish(success, compensated);
}

void RecordInvalidationAccepted(InvalidationQueue& queue, const InvalidationTask& task) {
    if (task.configEpoch && task.configEpoch == queue.batch.epoch) {
        queue.batch.accept();
    }
}

void InvalidationWorker() {
    auto& queue = Invalidations();
    std::unique_lock<std::mutex> lock(queue.mutex);
    for (;;) {
        if (queue.pending.empty() && queue.overflowParents.empty() &&
            queue.overflowInodes.empty()) {
            queue.cv.wait(lock, [&] {
                return !queue.pending.empty() || !queue.overflowParents.empty() ||
                       !queue.overflowInodes.empty();
            });
        }
        InvalidationTask task{};
        bool compensated = false;
        // Already-due work has priority over overflow compensation. Otherwise
        // a continuous producer can starve the ordinary queue indefinitely.
        // The config planner inserts changed paths before still-hidden paths.
        if (!queue.pending.empty() &&
            queue.pending.begin()->first <= std::chrono::steady_clock::now()) {
            auto next = queue.pending.begin();
            task = std::move(next->second);
            queue.pendingKeys.erase(TaskKey(task));
            queue.pending.erase(next);
        } else if (!queue.overflowInodes.empty()) {
            auto next = queue.overflowInodes.begin();
            task = std::move(next->second);
            queue.overflowInodes.erase(next);
            compensated = true;
        } else if (!queue.overflowParents.empty()) {
            auto next = queue.overflowParents.begin();
            task = std::move(next->second);
            queue.overflowParents.erase(next);
            compensated = true;
        } else {
            queue.cv.wait_until(lock, queue.pending.begin()->first);
            continue;
        }
        const auto state = queue.sessions.find(task.session);
        if (state == queue.sessions.end() || state->second.retiring ||
            state->second.generation != task.generation) {
            // Destroyed FUSE sessions cannot retain the old dentry or inode cache.
            RecordInvalidationCompletion(queue, task, true, true);
            queue.cv.notify_all();
            continue;
        }
        queue.runningSession = task.session;
        queue.runningKey = TaskKey(task);
        queue.cv.notify_all();
        lock.unlock();
        const bool notified = RunInvalidation(task);
        lock.lock();
        queue.runningSession = nullptr;
        queue.runningKey.reset();
        RecordInvalidationCompletion(queue, task, notified, compensated);
        const auto active = queue.sessions.find(task.session);
        if (task.kind == InvalidationKind::Root && active != queue.sessions.end() &&
            active->second.generation == task.generation) {
            active->second.rootPending = false;
            if (active->second.rootNeedsReplay && !active->second.retiring) {
                active->second.rootNeedsReplay = false;
                active->second.rootPending = true;
                InvalidationTask replay{InvalidationKind::Root,
                                        task.session,
                                        task.generation,
                                        task.ino,
                                        {},
                                        task.logicalPath,
                                        true,
                                        active->second.rootReplayEpoch};
                active->second.rootReplayEpoch = 0;
                RecordInvalidationAccepted(queue, replay);
                queue.pending.emplace(
                    std::chrono::steady_clock::now() + std::chrono::milliseconds(50), replay);
                queue.pendingKeys.insert(TaskKey(replay));
            }
        }
        queue.cv.notify_all();
    }
}

bool QueueInvalidation(InvalidationKind kind, SessionToken token, uint64_t ino,
                       std::string_view name, std::string logicalPath,
                       std::chrono::milliseconds delay, bool configReload = false) {
    if (!token || gOriginalFuseSessionDestroy.get() == nullptr) {
        return false;
    }
    auto& queue = Invalidations();
    std::unique_lock<std::mutex> lock(queue.mutex);
    const auto state = queue.sessions.find(token.se);
    if (state == queue.sessions.end() || state->second.retiring ||
        state->second.generation != token.generation) {
        return false;
    }
    if (kind == InvalidationKind::Root && state->second.rootPending) {
        if (configReload) {
            state->second.rootNeedsReplay = true;
            state->second.rootReplayEpoch = queue.currentConfigEpoch;
        }
        return true;
    }
    const uint64_t epoch = configReload ? queue.currentConfigEpoch : 0;
    InvalidationKey key{token.se, token.generation, kind, ino, std::string(name), epoch};
    if (queue.pendingKeys.contains(key) || queue.overflowInodes.contains(key) ||
        (queue.runningKey && *queue.runningKey == key)) {
        return true;
    }
    const bool critical = kind != InvalidationKind::Inode;
    const size_t slash = logicalPath.rfind('/');
    const bool hasParentPath =
        kind == InvalidationKind::Entry && slash != std::string::npos && slash > 0;
    InvalidationKey parentKey{token.se, token.generation, InvalidationKind::Parent, ino, {}, epoch};
    // Never evict an exact inode notification when its bounded compensation
    // map cannot retain it. Parent invalidation is not an equivalent fallback.
    const auto evict = critical && queue.pending.size() >= kInvalidationQueueLimit &&
                               CanEvictInodeIntoCompensation(queue.overflowInodes.size())
                           ? std::find_if(queue.pending.begin(), queue.pending.end(),
                                          [&](const auto& pending) {
                                              return pending.second.session == token.se &&
                                                     pending.second.kind == InvalidationKind::Inode;
                                          })
                           : queue.pending.end();
    const auto admission = DecideInvalidationAdmission(
        queue.pending.size(), queue.overflowParents.size(), critical,
        kind == InvalidationKind::Entry, hasParentPath, queue.overflowParents.contains(parentKey),
        evict != queue.pending.end(), configReload, queue.overflowInodes.size());
    if (admission == InvalidationAdmission::EvictInodeThenQueue) {
        // An inode callback cannot be replaced with a parent entry sweep.
        // Preserve exact inode identity until the native notification finishes.
        const auto evictedKey = TaskKey(evict->second);
        queue.overflowInodes.emplace(evictedKey, evict->second);
        queue.pendingKeys.erase(TaskKey(evict->second));
        queue.pending.erase(evict);
    } else if (admission == InvalidationAdmission::CoalescedParent) {
        return true;
    } else if (admission == InvalidationAdmission::NewParentSweep) {
        InvalidationTask sweep{InvalidationKind::Parent,
                               token.se,
                               token.generation,
                               ino,
                               {},
                               logicalPath.substr(0, slash),
                               true,
                               epoch};
        queue.overflowParents.emplace(std::move(parentKey), sweep);
        RecordInvalidationAccepted(queue, sweep);
        if (!queue.workerStarted) {
            queue.workerStarted = true;
            std::thread(InvalidationWorker).detach();
        }
        queue.cv.notify_one();
        return true;
    } else if (admission == InvalidationAdmission::ExactInodeCompensation) {
        InvalidationTask compensation{
            kind,  token.se, token.generation, ino, std::string(name), std::move(logicalPath),
            false, epoch};
        queue.overflowInodes.emplace(std::move(key), compensation);
        RecordInvalidationAccepted(queue, compensation);
        if (!queue.workerStarted) {
            queue.workerStarted = true;
            std::thread(InvalidationWorker).detach();
        }
        queue.cv.notify_all();
        return true;
    } else if (admission == InvalidationAdmission::Reject) {
        if (kind == InvalidationKind::Inode && configReload) {
            // This is a configuration-update producer, not a FUSE request.
            // Use bounded exact-inode compensation and backpressure rather
            // than discard thousands of notifications on a warm device.
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            bool sessionAlive = true;
            while (queue.overflowInodes.size() >= kInvalidationInodeCompensationLimit) {
                if (queue.cv.wait_until(lock, deadline) == std::cv_status::timeout)
                    break;
                const auto live = queue.sessions.find(token.se);
                if (live == queue.sessions.end() || live->second.retiring ||
                    live->second.generation != token.generation) {
                    sessionAlive = false;
                    break;
                }
            }
            if (sessionAlive && queue.overflowInodes.size() < kInvalidationInodeCompensationLimit) {
                InvalidationTask compensation{kind,
                                              token.se,
                                              token.generation,
                                              ino,
                                              std::string(name),
                                              std::move(logicalPath),
                                              false,
                                              epoch};
                queue.overflowInodes.emplace(key, compensation);
                RecordInvalidationAccepted(queue, compensation);
                if (!queue.workerStarted) {
                    queue.workerStarted = true;
                    std::thread(InvalidationWorker).detach();
                }
                queue.cv.notify_all();
                return true;
            }
        }
        ++queue.notifyFailures;
        if (epoch != 0 && epoch == queue.batch.epoch)
            queue.batch.reject();
        static std::atomic<uint64_t> rejected{0};
        const uint64_t count = rejected.fetch_add(1, std::memory_order_relaxed) + 1;
        if (count <= 4 || count % 1024 == 0) {
            __android_log_print(ANDROID_LOG_WARN, kLogTag, "invalidation queue full; rejected=%llu",
                                static_cast<unsigned long long>(count));
        }
        return false;
    }
    InvalidationTask admitted{
        kind,         token.se, token.generation, ino, std::string(name), std::move(logicalPath),
        configReload, epoch};
    RecordInvalidationAccepted(queue, admitted);
    queue.pending.emplace(std::chrono::steady_clock::now() + delay, std::move(admitted));
    queue.pendingKeys.insert(std::move(key));
    if (kind == InvalidationKind::Root) {
        state->second.rootPending = true;
    }
    if (!queue.workerStarted) {
        queue.workerStarted = true;
        std::thread(InvalidationWorker).detach();
    }
    queue.cv.notify_one();
    return true;
}

// A raw FUSE inode is only meaningful within its session generation.
// No tracking lock is held while acquiring the scheduler lock.
bool PathEqualsForFuseLookup(std::string_view lhs, std::string_view rhs) {
    return UnicodePolicy::CompareCaseFoldIgnoringDefaultIgnorables(
               reinterpret_cast<const uint8_t*>(lhs.data()), lhs.size(),
               reinterpret_cast<const uint8_t*>(rhs.data()), rhs.size()) == 0;
}

bool IsVisibleStorageRootPath(std::string_view path) {
    const auto root = VisibleStorageRootForPath(path);
    return root && path == *root;
}

bool MatchesCanonicalCandidate(std::string_view name,
                               const std::unordered_set<std::string>& canonicalNames) {
    for (const auto& canonical : canonicalNames) {
        if (PathEqualsForFuseLookup(name, canonical)) {
            return true;
        }
    }
    return false;
}

bool MayParticipateInHidePolicy(std::string_view path, const CompiledHideRule* rule) {
    if (path.empty()) {
        return false;
    }
    if (IsVisibleStorageRootPath(path)) {
        return true;
    }
    if (rule == nullptr) {
        return false;
    }

    if (const auto rootPath = VisibleStorageRootForPath(path);
        rootPath && path.size() > rootPath->size()) {
        const auto root = *rootPath;

        if (HideAllForStorageRoot(*rule, root)) {
            return true;
        }

        const size_t componentStart = root.size() + 1;
        const size_t slash = path.find('/', componentStart);
        const size_t componentEnd = slash == std::string_view::npos ? path.size() : slash;
        if (componentEnd <= componentStart) {
            return true;
        }
        const std::string_view firstComponent =
            path.substr(componentStart, componentEnd - componentStart);
        return MatchesCanonicalCandidate(firstComponent, rule->hiddenRootEntryNameSet) ||
               MatchesCanonicalCandidate(firstComponent, rule->hiddenRelativePathFirstComponentSet);
    }

    // Unknown MediaProvider path forms stay on the conservative path. This filter is only a
    // performance precheck and must never become an authorization boundary.
    return true;
}

bool MayParticipateInHidePolicy(std::string_view path) {
    const auto rule = RuleForAnyPackage();
    return MayParticipateInHidePolicy(path, rule.get());
}

bool IsVisibleRootChildLookupPath(std::string_view path, std::string_view name) {
    if (path.empty() || name.empty()) {
        return false;
    }
    if (const auto rootPath = VisibleStorageRootForPath(path);
        rootPath && path.size() > rootPath->size()) {
        const auto root = *rootPath;
        const std::string_view childName = path.substr(root.size() + 1);
        if (childName.find('/') == std::string_view::npos &&
            PathEqualsForFuseLookup(childName, name)) {
            return true;
        }
    }
    return false;
}

bool IsProtectedTrackedPath(std::string_view path) {
    if (HiddenPathPolicy::IsAnyHiddenSubtreePath(path) || IsParentOfExactHiddenTargetPath(path)) {
        return true;
    }

    const auto rule = RuleForAnyPackage();
    if (rule == nullptr) {
        return false;
    }

    if (const auto root = VisibleStorageRootForPath(path); root && path == *root)
        return !rule->hiddenRootEntryNames.empty() || HideAllForStorageRoot(*rule, *root) ||
               !rule->normalizedHiddenRelativePathExactSet.empty();

    const auto relativePath = RelativePathForVisibleRoot(path);
    if (!relativePath.has_value() || relativePath->empty()) {
        return false;
    }
    const std::string canonicalRelativePath = CanonicalizeRelativeHiddenPathForMatch(*relativePath);
    if (canonicalRelativePath.empty()) {
        return false;
    }
    for (const auto& hiddenPath : rule->normalizedHiddenRelativePathExactSet) {
        if (hiddenPath.size() > canonicalRelativePath.size() &&
            hiddenPath.compare(0, canonicalRelativePath.size(), canonicalRelativePath) == 0 &&
            hiddenPath[canonicalRelativePath.size()] == '/') {
            return true;
        }
    }
    return false;
}

}  // namespace

uint32_t RuntimeState::ReqUid(fuse_req_t req) {
    if (req == nullptr) {
        return 0;
    }
    if (const auto fn = gFuseReqCtx.get(); fn != nullptr) {
        const fuse_ctx* context = fn(req);
        return context != nullptr ? context->uid : 0;
    }
    return 0;
}

SessionToken CurrentSessionToken() {
    return gActiveFuseRequestToken;
}

bool IsProtectedMountInvalidationPath(std::string_view logicalPath) {
    return IsProtectedMountTarget(logicalPath);
}

#if defined(__aarch64__)
Arm64HookCoverage CurrentArm64HookCoverage() {
    Arm64HookCoverage hooks;
    hooks.callbacks = {
        gOriginalPfLookup.get() != nullptr,      gOriginalPfLookupPostfilter.get() != nullptr,
        gOriginalPfGetattr.get() != nullptr,     gOriginalPfReaddir.get() != nullptr,
        gOriginalPfReaddirplus.get() != nullptr, gOriginalPfReaddirPostfilter.get() != nullptr,
    };
    hooks.replyAttr = gOriginalReplyAttr.get() != nullptr;
    hooks.replyEntry = gOriginalReplyEntry.get() != nullptr;
    hooks.replyBuf = gOriginalReplyBuf.get() != nullptr;
    hooks.replyErr = gOriginalReplyErr.get() != nullptr;
    hooks.notifyEntry = gOriginalNotifyInvalEntry.get() != nullptr;
    hooks.notifyInode = gOriginalNotifyInvalInode.get() != nullptr;
    hooks.sessionDestroy = gOriginalFuseSessionDestroy.get() != nullptr;
    return hooks;
}
#endif

SessionToken RegisterSession(void* session) {
    if (session == nullptr)
        return {};
    auto& queue = Invalidations();
    std::lock_guard<std::mutex> lock(queue.mutex);
    auto it = queue.sessions.find(session);
    if (it == queue.sessions.end()) {
        it = queue.sessions.emplace(session, InvalidationSession{++queue.nextGeneration}).first;
#if defined(__x86_64__)
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "new FUSE session observed se=%p; callbacks not verified yet", session);
#elif defined(__aarch64__)
        // ARM64 inline hooks are process-wide, but their backup pointers are
        // only populated after a successful Dobby installation. Do not infer
        // callback coverage merely from the existence of a FUSE request.
        const auto hooks = CurrentArm64HookCoverage();
        const size_t installed = CountArm64FUSECallbacks(hooks);
        it->second.callbackCoverageKnown = true;
        it->second.callbackCoverageComplete = HasCompleteArm64HookCoverage(hooks);
        __android_log_print(
            it->second.callbackCoverageComplete ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, kLogTag,
            "FUSE callback coverage se=%p replaced=%zu required=%zu complete=%d "
            "architecture=arm64",
            session, installed, kRequiredFUSECallbacks,
            it->second.callbackCoverageComplete ? 1 : 0);
#endif
    }
    if (it->second.retiring)
        return {};
    gLastFuseSession.store(session, std::memory_order_release);  // Diagnostic only.
    return {session, it->second.generation};
}

void SetVerifiedPrimaryRootNodeLookup(void* lookupFunction) {
    gVerifiedPrimaryRootLookup.store(reinterpret_cast<VerifiedRootLookupFn>(lookupFunction),
                                     std::memory_order_release);
}

void BindSessionFUSEUserdata(SessionToken token, void* userdata) {
    if (!token || userdata == nullptr)
        return;
    auto& queue = Invalidations();
    std::lock_guard<std::mutex> lock(queue.mutex);
    const auto found = queue.sessions.find(token.se);
    if (found != queue.sessions.end() && found->second.generation == token.generation &&
        !found->second.retiring)
        found->second.fuseUserdata = userdata;
}

void ResolveUnknownPrimaryRootNodes() {
#if defined(__aarch64__)
    static std::atomic<uint32_t> diagnosticCount{0};
    const auto lookup = gVerifiedPrimaryRootLookup.load(std::memory_order_acquire);
    if (!lookup)
        return;
    struct Probe {
        SessionToken token;
        void* userdata;
    };
    std::vector<Probe> probes;
    auto& queue = Invalidations();
    {
        std::lock_guard<std::mutex> lock(queue.mutex);
        for (auto& [se, session] : queue.sessions) {
            if (session.retiring || session.fuseUserdata == nullptr)
                continue;
            std::lock_guard<std::mutex> trackingLock(session.tracking->mutex);
            if (session.tracking->rootPath != "/storage/emulated/0" ||
                session.tracking->rootIno != 0)
                continue;
            ++session.rootProbeReaders;
            probes.push_back({{se, session.generation}, session.fuseUserdata});
        }
    }
    bool logProbe = false;
    if constexpr (kEnableDebugHooks) {
        logProbe = diagnosticCount.fetch_add(1, std::memory_order_relaxed) < 8;
        if (logProbe)
            DebugLogPrint(4, "primary root tree probe candidates=%zu sessions=%zu", probes.size(),
                          SnapshotActiveSessions().size());
    }
    // AOSP FuseDaemon.cpp::fuse::FromInode(1) reads fuse->root at +0x70.
    // LookupAbsolutePath takes the node tree lock and creates no nodes.
    const std::string absoluteRoot = "/storage/emulated/0";
    for (const auto& probe : probes) {
        const void* rootNode = nullptr;
        std::memcpy(&rootNode, static_cast<const char*>(probe.userdata) + kFuseRootOffset,
                    sizeof(rootNode));
        const void* child = rootNode ? lookup(rootNode, absoluteRoot) : nullptr;
        if (logProbe) {
            const void* self = rootNode ? lookup(rootNode, "/storage/emulated") : nullptr;
            DebugLogPrint(4,
                          "primary root tree lookup se=%p userdata=%p root=%p self=%p "
                          "child0=%p",
                          probe.token.se, probe.userdata, rootNode, self, child);
        }
        // AOSP node::LookupAbsolutePath locks the node tree and only finds
        // instantiated children. If the verified root resolves to itself but
        // /storage/emulated/0 is not instantiated, this Session has no kernel
        // dentry for that child to invalidate. An empty tracking table proves
        // there are no previously observed descendants either. Skip only this
        // Session's impossible root refresh, never one with a real child.
        if (rootNode != nullptr && child == nullptr &&
            lookup(rootNode, "/storage/emulated") == rootNode) {
            if (const auto state = TrackingForToken(probe.token)) {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (state->rootPath == absoluteRoot && state->rootIno == 0 &&
                    CanSkipVerifiedEmptyPrimaryRoot(
                        true, true, state->inodeToPath.size(), state->pathToInode.size(),
                        state->deferredEntryCount, state->deferredInodes.size(),
                        state->hiddenSubtreeRules.size())) {
                    if (state->rootRefreshNeeded) {
                        state->rootRefreshNeeded = false;
                        DebugLogPrint(4,
                                      "verified empty primary root Session se=%p "
                                      "generation=%llu; no cached child0 to invalidate",
                                      probe.token.se,
                                      static_cast<unsigned long long>(probe.token.generation));
                    }
                }
            }
        }
        if (child != nullptr && child != rootNode && reinterpret_cast<uintptr_t>(child) > 4096 &&
            (reinterpret_cast<uintptr_t>(child) & (alignof(void*) - 1)) == 0) {
            DebugLogPrint(4, "learned primary FUSE root from node tree se=%p nodeid=0x%llx",
                          probe.token.se,
                          static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(child)));
            RememberTrackedPathForInode(probe.token,
                                        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(child)),
                                        absoluteRoot);
        }
        {
            std::lock_guard<std::mutex> lock(queue.mutex);
            const auto found = queue.sessions.find(probe.token.se);
            if (found != queue.sessions.end() &&
                found->second.generation == probe.token.generation &&
                found->second.rootProbeReaders > 0)
                --found->second.rootProbeReaders;
            queue.cv.notify_all();
        }
    }
#endif
}

void RegisterSessionRootPath(SessionToken token, std::string_view sourcePath) {
    const auto state = TrackingForToken(token);
    if (!state || sourcePath.empty())
        return;
    std::string rootPath;
    if (sourcePath == "/storage/emulated" || sourcePath == "/storage/emulated/0" ||
        sourcePath == "/data/media/0" || sourcePath == "/mnt/user/0/primary" ||
        sourcePath == "/mnt/user/0/emulated/0") {
        rootPath = "/storage/emulated/0";
    } else if (sourcePath.starts_with("/mnt/media_rw/")) {
        rootPath = "/storage/";
        rootPath.append(sourcePath.substr(sizeof("/mnt/media_rw/") - 1));
    } else if (sourcePath.starts_with("/storage/")) {
        rootPath = sourcePath;
    }
    if (rootPath.empty()) {
        DebugLogPrint(2, "unrecognized FUSE root source se=%p path=%s", token.se,
                      DebugPreview(sourcePath).c_str());
        return;
    }
    const bool publicRoot =
        VisibleStorageRootForPath(rootPath).has_value() && rootPath != kVisibleStorageRoots[0];
    bool refresh = false;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->rootPath.empty() && state->rootPath != rootPath) {
            DebugLogPrint(2, "FUSE root path mismatch se=%p", token.se);
            return;
        }
        state->rootPath = std::move(rootPath);
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "FUSE root source se=%p source=%s root=%s",
                            token.se, DebugPreview(sourcePath).c_str(), state->rootPath.c_str());
        if (publicRoot && state->rootIno == 0) {
            // Public volumes expose their visible root directly as FUSE_ROOT_ID.
            state->rootIno = 1;
            state->inodeToPath.emplace(1, state->rootPath);
            state->pathToInode.emplace(state->rootPath, 1);
        }
        refresh = std::exchange(state->rootRefreshNeeded, false);
    }
    // FUSE_ROOT_ID (1) is the parent of "0" on emulated storage, not its inode.
    // WrappedReplyEntry learns the real child inode from lookup(1, "0").
    if (refresh)
        ScheduleHiddenEntryInvalidation(token, true);
}

void RememberEmulatedZeroFromRootDirentplus(SessionToken token, uint64_t parent, const char* data,
                                            size_t size) {
    if (!token || parent != 1 || data == nullptr || size == 0)
        return;
    const auto state = TrackingForToken(token);
    if (!state)
        return;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        // Only the primary emulated-volume session has the child named "0".
        // Public removable volumes already use FUSE_ROOT_ID as their root.
        if (state->rootPath != "/storage/emulated/0" || state->rootIno != 0)
            return;
    }
    const auto nodeid = FindEmulatedZeroDirentplusNodeId(std::span<const char>(data, size));
    if (!nodeid)
        return;
    DebugLogPrint(4, "learned primary FUSE root from readdirplus se=%p nodeid=0x%llx", token.se,
                  static_cast<unsigned long long>(*nodeid));
    RememberTrackedPathForInode(token, *nodeid, "/storage/emulated/0");
}

void MarkSessionLowlevelCallbacks(SessionToken token, size_t replaced, size_t required) {
    if (!token)
        return;
    auto& queue = Invalidations();
    std::lock_guard<std::mutex> lock(queue.mutex);
    const auto found = queue.sessions.find(token.se);
    if (found == queue.sessions.end() || found->second.generation != token.generation)
        return;
    found->second.callbackCoverageKnown = true;
    found->second.callbackTableVerified = HasRequiredFUSECallbackCoverage(replaced, required);
#if defined(__aarch64__)
    // The callbacks can be published before the separate reply/notify GOT hooks
    // are installed. Do not certify invalidation readiness during that window.
    found->second.callbackCoverageComplete =
        found->second.callbackTableVerified &&
        HasArm64AuxiliaryHookCoverage(CurrentArm64HookCoverage());
#else
    found->second.callbackCoverageComplete = found->second.callbackTableVerified;
#endif
    __android_log_print(
        found->second.callbackCoverageComplete ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, kLogTag,
        "FUSE callback coverage se=%p replaced=%zu required=%zu complete=%d", token.se, replaced,
        required, found->second.callbackCoverageComplete ? 1 : 0);
}

void RequireSessionRootRefresh(SessionToken token) {
    if (const auto state = TrackingForToken(token)) {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->rootRefreshNeeded = true;
    }
}

void EnsureSessionLowlevelCallbacks(SessionToken token) {
#if defined(__x86_64__)
    if (!token)
        return;
    auto& queue = Invalidations();
    {
        std::lock_guard<std::mutex> lock(queue.mutex);
        const auto it = queue.sessions.find(token.se);
        if (it == queue.sessions.end() || it->second.generation != token.generation ||
            it->second.retiring || it->second.callbackCoverageComplete ||
            it->second.callbackRepairAttempted)
            return;
        it->second.callbackRepairAttempted = true;
    }
    if (BackfillSessionLowlevelCallbacks(token.se)) {
        MarkSessionLowlevelCallbacks(token, 6, 6);
        RequireSessionRootRefresh(token);
        ScheduleHiddenEntryInvalidation(token, true);
    } else {
        MarkSessionLowlevelCallbacks(token, 0, 6);
    }
#else
    (void)token;
#endif
}

bool AllActiveSessionsHaveLowlevelCallbacks() {
    // An unseen pre-hook Session is not in sessions. Compare against the real
    // MediaProvider FUSE mounts so one observed Session cannot certify two volumes.
    std::unordered_set<std::string> expectedRoots;
    FILE* mounts = fopen("/proc/self/mountinfo", "re");
    if (mounts == nullptr)
        return false;
    char line[2048];
    while (fgets(line, sizeof(line), mounts) != nullptr) {
        if (std::strstr(line, " - fuse ") == nullptr)
            continue;
        char mountpoint[1024];
        if (std::sscanf(line, "%*u %*u %*s %*s %1023s", mountpoint) != 1)
            continue;
        const std::string_view path(mountpoint);
        if (path == "/storage/emulated" || path == "/mnt/user/0/emulated") {
            expectedRoots.emplace("/storage/emulated/0");
        } else if (const auto root = VisibleStorageRootForPath(path); root && *root == path) {
            expectedRoots.emplace(*root);
        } else if (path.starts_with("/mnt/user/0/")) {
            const std::string_view publicName = path.substr(sizeof("/mnt/user/0/") - 1);
            const std::string storageRoot = std::string("/storage/") + std::string(publicName);
            if (const auto root = VisibleStorageRootForPath(storageRoot);
                root && *root == storageRoot)
                expectedRoots.emplace(storageRoot);
        }
    }
    fclose(mounts);
    if (expectedRoots.empty())
        return false;
    auto& queue = Invalidations();
    std::lock_guard<std::mutex> lock(queue.mutex);
    if (queue.sessions.empty())
        return false;
#if defined(__aarch64__)
    const auto hooks = CurrentArm64HookCoverage();
    const size_t installed = CountArm64FUSECallbacks(hooks);
    const bool covered = HasCompleteArm64HookCoverage(hooks);
    const bool auxiliariesCovered = HasArm64AuxiliaryHookCoverage(hooks);
#endif
    std::unordered_set<std::string> coveredRoots;
    for (auto& [se, state] : queue.sessions) {
        if (state.retiring)
            continue;
#if defined(__aarch64__)
        // A Session whose table was verified at creation must not be
        // reclassified by the inline-only backup-pointer metric. The six
        // callbacks are installed for this Session, not as global code patches.
        // Reply/notify/retirement hooks remain separately mandatory.
        if (state.callbackTableVerified) {
            if (!auxiliariesCovered)
                return false;
            if (!state.callbackCoverageComplete) {
                state.callbackCoverageComplete = true;
                __android_log_print(ANDROID_LOG_INFO, kLogTag,
                                    "FUSE callback coverage se=%p replaced=%zu required=%zu "
                                    "complete=1 architecture=arm64 session_table=1 rechecked=1",
                                    se, kRequiredFUSECallbacks, kRequiredFUSECallbacks);
            }
        } else if (!state.callbackCoverageKnown || state.callbackCoverageComplete != covered) {
            state.callbackCoverageKnown = true;
            state.callbackCoverageComplete = covered;
            __android_log_print(covered ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, kLogTag,
                                "FUSE callback coverage se=%p replaced=%zu required=%zu "
                                "complete=%d architecture=arm64 rechecked=1",
                                se, installed, kRequiredFUSECallbacks, covered ? 1 : 0);
        }
#endif
        if (!state.callbackCoverageComplete)
            return false;
        std::lock_guard<std::mutex> trackingLock(state.tracking->mutex);
        if (!state.tracking->rootPath.empty())
            coveredRoots.insert(state.tracking->rootPath);
    }
    for (const auto& root : expectedRoots)
        if (!coveredRoots.contains(root))
            return false;
    return true;
}

SessionToken PushFuseRequestSession(fuse_req_t req) {
    const SessionToken previous = gActiveFuseRequestToken;
    RuntimeState::RememberFuseSession(req);
    return previous;
}

void RestoreFuseRequestSession(SessionToken previous) {
    gActiveFuseRequestToken = previous;
}

void RuntimeState::RememberFuseSession(fuse_req_t req) {
    gActiveFuseRequestToken = req != nullptr ? RegisterSession(req->se) : SessionToken{};
    EnsureSessionLowlevelCallbacks(gActiveFuseRequestToken);
}

std::vector<SessionToken> SnapshotActiveSessions() {
    auto& queue = Invalidations();
    std::lock_guard<std::mutex> lock(queue.mutex);
    std::vector<SessionToken> tokens;
    tokens.reserve(queue.sessions.size());
    for (const auto& [se, state] : queue.sessions) {
        if (!state.retiring)
            tokens.push_back({se, state.generation});
    }
    return tokens;
}

void RuntimeState::RetireFuseSession(void* session) {
    if (session == nullptr)
        return;
    auto& queue = Invalidations();
    std::unique_lock<std::mutex> lock(queue.mutex);
    const auto state = queue.sessions.find(session);
    if (state != queue.sessions.end()) {
        state->second.retiring = true;
        for (auto it = queue.pending.begin(); it != queue.pending.end();) {
            if (it->second.session == session) {
                // Destroying the Session invalidates its kernel inode cache.
                // Complete the exact batch task so outstanding is not leaked.
                RecordInvalidationCompletion(queue, it->second, true, true);
                queue.pendingKeys.erase(TaskKey(it->second));
                it = queue.pending.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = queue.overflowParents.begin(); it != queue.overflowParents.end();) {
            if (it->second.session == session) {
                RecordInvalidationCompletion(queue, it->second, true, false);
                it = queue.overflowParents.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = queue.overflowInodes.begin(); it != queue.overflowInodes.end();) {
            if (it->second.session == session) {
                RecordInvalidationCompletion(queue, it->second, true, false);
                it = queue.overflowInodes.erase(it);
            } else {
                ++it;
            }
        }
        state->second.rootPending = false;
        state->second.rootNeedsReplay = false;
        state->second.rootReplayEpoch = 0;
        queue.cv.notify_all();
    }
    const auto started = std::chrono::steady_clock::now();
    while (queue.runningSession == session ||
           (state != queue.sessions.end() && state->second.rootProbeReaders != 0)) {
        if (queue.cv.wait_for(lock, std::chrono::seconds(1)) == std::cv_status::timeout) {
            __android_log_print(
                ANDROID_LOG_WARN, kLogTag,
                "waiting for FUSE invalidation to exit se=%p waited_ms=%lld", session,
                static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - started)
                                           .count()));
        }
    }
}

void FinishFuseSessionDestroy(SessionToken token) {
    if (!token)
        return;
    auto& queue = Invalidations();
    std::lock_guard<std::mutex> lock(queue.mutex);
    const auto it = queue.sessions.find(token.se);
    if (it != queue.sessions.end() && it->second.generation == token.generation) {
        queue.sessions.erase(it);
    }
    if (gLastFuseSession.load(std::memory_order_relaxed) == token.se) {
        gLastFuseSession.store(nullptr, std::memory_order_release);
    }
}

extern "C" void WrappedFuseSessionDestroy(void* session) {
    SessionToken token{};
    {
        auto& queue = Invalidations();
        std::lock_guard<std::mutex> lock(queue.mutex);
        const auto it = queue.sessions.find(session);
        if (it != queue.sessions.end())
            token = {session, it->second.generation};
    }
    RuntimeState::RetireFuseSession(session);
    ClearPendingRootSnapshotMutations(token);
    ClearRootSnapshotCacheForSession(token);
    if (const auto original = gOriginalFuseSessionDestroy.get(); original != nullptr) {
        original(session);
    }
    FinishFuseSessionDestroy(token);
}

// Shared dentry cache is not scoped per uid. Once another app resolves the hidden entry, the
// target uid can reuse that positive cache unless we actively invalidate the root dentry.
void ScheduleHiddenEntryInvalidation(SessionToken token, bool configReload) {
    if (!token || gOriginalNotifyInvalEntry.get() == nullptr)
        return;
    const auto state = TrackingForToken(token);
    if (!state)
        return;
    uint64_t root = 0;
    std::string rootPath;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        root = state->rootIno;
        rootPath = state->rootPath;
        if (root == 0 || rootPath.empty()) {
            if (CanSkipUntrackedSessionRootInvalidation(
                    !rootPath.empty(), state->inodeToPath.size(), state->deferredEntryCount,
                    state->deferredInodes.size())) {
                // A rootless auxiliary Session cannot own cached visible
                // storage entries. Avoid an impossible refresh requirement.
                // Missing *real* storage roots still fail the mount check.
                state->rootRefreshNeeded = false;
                return;
            }
            state->rootRefreshNeeded = true;
            return;
        }
    }
    if (root != 0) {
        QueueInvalidation(InvalidationKind::Root, token, root, {}, std::move(rootPath),
                          std::chrono::milliseconds(50), configReload);
    }
}

void BeginHideInvalidationBatch() {
    auto& queue = Invalidations();
    std::lock_guard<std::mutex> lock(queue.mutex);
    // Old failures stay in notifyFailures for diagnostics but never count as a
    // failure in a newly published configuration generation.
    queue.batch = {};
    queue.batch.epoch = ++queue.currentConfigEpoch;
    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "invalidation batch begin epoch=%llu historical_failures=%llu",
                        static_cast<unsigned long long>(queue.batch.epoch),
                        static_cast<unsigned long long>(queue.notifyFailures));
}

bool WaitForHideInvalidation(std::chrono::milliseconds timeout) {
    auto& queue = Invalidations();
    uint64_t awaitedEpoch = 0;
    {
        std::lock_guard<std::mutex> lock(queue.mutex);
        awaitedEpoch = queue.currentConfigEpoch;
    }
    if (!awaitedEpoch)
        return false;
    if (!AllActiveSessionsHaveLowlevelCallbacks())
        return false;
    if (gOriginalFuseSessionDestroy.get() == nullptr ||
        gOriginalNotifyInvalEntry.get() == nullptr || gOriginalNotifyInvalInode.get() == nullptr)
        return false;
    ResolveUnknownPrimaryRootNodes();
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::vector<std::shared_ptr<SessionTracking>> trackers;
    {
        std::unique_lock<std::mutex> lock(queue.mutex);
        const bool idle = queue.cv.wait_until(lock, deadline, [&] {
            if (!queue.pending.empty() || !queue.overflowParents.empty() ||
                !queue.overflowInodes.empty() || queue.runningSession != nullptr)
                return false;
            for (const auto& [se, session] : queue.sessions) {
                if (!session.retiring && (session.rootPending || session.rootNeedsReplay))
                    return false;
            }
            return true;
        });
        const bool batchOk = queue.batch.epoch == awaitedEpoch && queue.batch.complete();
        if (!idle || !batchOk || queue.sessions.empty()) {
            __android_log_print(
                ANDROID_LOG_WARN, kLogTag,
                "invalidation completion blocked idle=%d pending=%zu overflow=%zu "
                "running=%p failures=%llu sessions=%zu "
                "epoch=%llu submitted=%llu completed=%llu compensated=%llu "
                "rejected=%llu failed=%llu outstanding=%llu inode_overflow=%zu",
                idle ? 1 : 0, queue.pending.size(), queue.overflowParents.size(),
                queue.runningSession, static_cast<unsigned long long>(queue.notifyFailures),
                queue.sessions.size(), static_cast<unsigned long long>(queue.batch.epoch),
                static_cast<unsigned long long>(queue.batch.submitted),
                static_cast<unsigned long long>(queue.batch.completed),
                static_cast<unsigned long long>(queue.batch.compensated),
                static_cast<unsigned long long>(queue.batch.rejected),
                static_cast<unsigned long long>(queue.batch.failed),
                static_cast<unsigned long long>(queue.batch.outstanding),
                queue.overflowInodes.size());
            return false;
        }
        for (const auto& [se, session] : queue.sessions) {
            if (session.retiring)
                continue;
            if (!session.callbackCoverageComplete)
                return false;
            trackers.push_back(session.tracking);
        }
    }
    for (const auto& tracking : trackers) {
        std::lock_guard<std::mutex> lock(tracking->mutex);
        if (tracking->rootRefreshNeeded || tracking->deferredEntryCount != 0 ||
            !tracking->deferredInodes.empty()) {
            __android_log_print(ANDROID_LOG_WARN, kLogTag,
                                "invalidation tracking incomplete root_refresh=%d "
                                "deferred_entries=%zu deferred_inodes=%zu root_ino=%llu path=%s",
                                tracking->rootRefreshNeeded ? 1 : 0, tracking->deferredEntryCount,
                                tracking->deferredInodes.size(),
                                static_cast<unsigned long long>(tracking->rootIno),
                                tracking->rootPath.c_str());
            return false;
        }
    }
    // A second config publication while checking per-Session tracking makes
    // the old acknowledgement obsolete even if its own notifications drained.
    {
        std::lock_guard<std::mutex> lock(queue.mutex);
        if (queue.currentConfigEpoch != awaitedEpoch || !queue.batch.complete() ||
            !queue.pending.empty() || !queue.overflowParents.empty() ||
            !queue.overflowInodes.empty() || queue.runningSession)
            return false;
        __android_log_print(ANDROID_LOG_INFO, kLogTag,
                            "invalidation batch complete epoch=%llu submitted=%llu completed=%llu "
                            "compensated=%llu rejected=%llu failed=%llu historical_failures=%llu",
                            static_cast<unsigned long long>(queue.batch.epoch),
                            static_cast<unsigned long long>(queue.batch.submitted),
                            static_cast<unsigned long long>(queue.batch.completed),
                            static_cast<unsigned long long>(queue.batch.compensated),
                            static_cast<unsigned long long>(queue.batch.rejected),
                            static_cast<unsigned long long>(queue.batch.failed),
                            static_cast<unsigned long long>(queue.notifyFailures));
    }
    return true;
}

void RuntimeState::ScheduleHiddenEntryInvalidation() {
    ::fusehide::ScheduleHiddenEntryInvalidation(CurrentSessionToken());
}

void RuntimeState::ScheduleSpecificEntryInvalidation(uint64_t parent, std::string_view name) {
    const auto token = CurrentSessionToken();
    if (!token || gOriginalNotifyInvalEntry.get() == nullptr || parent == 0 || name.empty())
        return;
    const auto parentPath = LookupTrackedPathForInode(token, parent);
    if (!parentPath) {
        if (const auto state = TrackingForToken(token)) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->rootRefreshNeeded = true;
            constexpr size_t kMaxDeferredEntries = 4096;
            auto it = state->deferredEntries.find(parent);
            if (it == state->deferredEntries.end() &&
                state->deferredEntryCount < kMaxDeferredEntries)
                it =
                    state->deferredEntries.emplace(parent, std::unordered_set<std::string>{}).first;
            if (it != state->deferredEntries.end() &&
                (it->second.contains(std::string(name)) ||
                 state->deferredEntryCount < kMaxDeferredEntries)) {
                state->deferredEntryCount += it->second.emplace(name).second ? 1 : 0;
            } else {
                DebugLogPrint(2, "deferred entry invalidation capacity reached se=%p", token.se);
            }
        }
        return;
    }
    const auto root = VisibleStorageRootForPath(*parentPath);
    if (root && *root == *parentPath) {
        const auto rule = RuleForAnyPackage();
        if (rule && HideAllForStorageRoot(*rule, *root)) {
            ::fusehide::ScheduleHiddenEntryInvalidation(token);
            return;
        }
    }
    const std::string path = HiddenPathPolicy::JoinPathComponent(*parentPath, name);
    if (IsProtectedMountTarget(path))
        return;
    QueueInvalidation(InvalidationKind::Entry, token, parent, name, path,
                      std::chrono::milliseconds(25));
}

void ScheduleHiddenInodeInvalidation(SessionToken token, uint64_t ino, bool configReload) {
    if (!token || gOriginalNotifyInvalInode.get() == nullptr || ino == 0)
        return;
    const auto path = LookupTrackedPathForInode(token, ino);
    if (!path) {
        if (const auto state = TrackingForToken(token)) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->rootRefreshNeeded = true;
            if (state->deferredInodes.size() < 4096 || state->deferredInodes.contains(ino)) {
                if (state->deferredInodes.insert(ino).second && state->deferredInodes.size() <= 4) {
                    __android_log_print(ANDROID_LOG_WARN, kLogTag,
                                        "deferred unknown inode se=%p ino=0x%llx lookup=%d "
                                        "root_lookup=%d getattr=%d root=%s",
                                        token.se, static_cast<unsigned long long>(ino),
                                        gInPfLookup ? 1 : 0, gTrackRootHiddenLookup ? 1 : 0,
                                        gInPfGetattr ? 1 : 0, state->rootPath.c_str());
                }
            } else
                DebugLogPrint(2, "deferred inode invalidation capacity reached se=%p", token.se);
        }
        return;
    }
    if (IsProtectedMountTarget(*path))
        return;
    if (const auto root = VisibleStorageRootForPath(*path);
        root && path->size() > root->size() &&
        path->find('/', root->size() + 1) == std::string::npos) {
        const auto rule = RuleForAnyPackage();
        if (rule && HideAllForStorageRoot(*rule, *root)) {
            ScheduleHiddenEntryInvalidation(token, configReload);
            return;
        }
    }
    QueueInvalidation(InvalidationKind::Inode, token, ino, {}, *path, std::chrono::milliseconds(50),
                      configReload);
}

void RuntimeState::ScheduleHiddenInodeInvalidation(uint64_t ino) {
    ::fusehide::ScheduleHiddenInodeInvalidation(CurrentSessionToken(), ino);
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
    (void)error_in;
    if (!IsTestHiddenUid(uid) || name == nullptr) {
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
    if (!IsTestHiddenUid(uid)) {
        return false;
    }
    const uint64_t rootParent = CurrentRootParentInode();
    return rootParent != 0 && parent == rootParent &&
           HiddenPathPolicy::IsHiddenRootEntryName(uid, name);
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
    const uint64_t rootParent = CurrentRootParentInode();
    if (parent != 0 && parent != rootParent && IsTrackedHiddenSubtreeInode(uid, parent)) {
        return HiddenNamedTargetKind::Descendant;
    }
    if (rootParent != 0 && parent == rootParent &&
        HiddenPathPolicy::IsHiddenRootEntryName(uid, name)) {
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
    gPendingHiddenErrReq = req;
    gPendingHiddenErrReqUnique = req->unique;
    gPendingHiddenErrToken = CurrentSessionToken();
    gPendingHiddenErrno = err;
    const uint32_t uid = RuntimeState::ReqUid(req);
    if (uid != 0) {
        if (const auto tracking = CurrentTracking()) {
            std::lock_guard<std::mutex> lock(tracking->mutex);
            UidErrRemapState& state = tracking->uidErrRemap[uid];
            state.baselineErr = err;
            state.expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            state.pendingCount = std::min<uint32_t>(state.pendingCount + 1, 8);
        }
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
    if (const auto tracking = CurrentTracking()) {
        std::lock_guard<std::mutex> lock(tracking->mutex);
        UidErrRemapState& state = tracking->uidErrRemap[uid];
        state.baselineErr = EPERM;
        state.expiresAt = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        state.pendingCount = std::min<uint32_t>(state.pendingCount + 1, 8);
    }
    DebugLogPrint(4, "%s arm create leak remap uid=%u baseline=%d", opName,
                  static_cast<unsigned>(uid), EPERM);
}

int MaybeRewriteHiddenLeakErrno(fuse_req_t req, int err, const char* caller) {
    if (req != nullptr && gPendingHiddenErrReq == req && gPendingHiddenErrToken &&
        gPendingHiddenErrToken == CurrentSessionToken() &&
        gPendingHiddenErrReqUnique == req->unique && gPendingHiddenErrno > 0 && err > 0 &&
        IsExistenceLeakErrno(err)) {
        const int baselineErr = gPendingHiddenErrno;
        gPendingHiddenErrReq = nullptr;
        gPendingHiddenErrReqUnique = 0;
        gPendingHiddenErrToken = {};
        gPendingHiddenErrno = 0;

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
        const auto tracking = CurrentTracking();
        if (tracking) {
            std::lock_guard<std::mutex> lock(tracking->mutex);
            const auto it = tracking->uidErrRemap.find(uid);
            if (it != tracking->uidErrRemap.end()) {
                if (it->second.expiresAt >= std::chrono::steady_clock::now() &&
                    it->second.baselineErr > 0 && it->second.pendingCount > 0) {
                    uidBaselineErr = it->second.baselineErr;
                    it->second.pendingCount--;
                }
                if (it->second.pendingCount == 0 ||
                    it->second.expiresAt < std::chrono::steady_clock::now()) {
                    tracking->uidErrRemap.erase(it);
                }
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

// Device reverse engineering shows make_node_entry() and create_handle_for_node() both consult
// fuse->ShouldNotCache(path). Matching that behavior is what keeps positive dentries and file-cache
// state from being reused across UIDs.
// AOSP references: jni/FuseDaemon.cpp#347, #510, and #1428
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#347
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#510
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1428
extern "C" bool WrappedShouldNotCache(void* fuse, AbiStringParam pathArg) {
    const std::string_view path = AbiStringView(pathArg);
    if (gInPfLookup && gCurrentLookupParentInode != 0 && !gCurrentLookupName.empty() &&
        IsVisibleRootChildLookupPath(path, gCurrentLookupName)) {
        // This is an actual path built by MediaProvider for this very lookup.
        // Learn the parent for *all* request UIDs: waiting until a restricted
        // UID looks up a hidden name can fail when the dentry is already cached.
        RememberRootParentInode(gCurrentLookupParentInode, "should_not_cache_path");
        if (gCurrentLookupUid != 0 &&
            HiddenPathPolicy::IsExactHiddenTargetPath(gCurrentLookupUid, path))
            gTrackRootHiddenLookup = true;
    }
    const auto anyPackageRule = RuleForAnyPackage();
    if (anyPackageRule != nullptr && MayParticipateInHidePolicy(path, anyPackageRule.get()) &&
        HiddenPathPolicy::IsAnyHiddenSubtreePath(*anyPackageRule, path)) {
        DebugLogPrint(4, "force uncached subtree path=%s", DebugPreview(path).c_str());
        return true;
    }
    const auto fn = gOriginalShouldNotCache.get();
    return fn ? fn(fuse, pathArg) : false;
}

uint64_t CurrentRootParentInode() {
    const auto state = CurrentTracking();
    if (!state)
        return 0;
    std::lock_guard<std::mutex> lock(state->mutex);
    return state->rootIno;
}

void RememberRootParentInode(uint64_t ino, const char* reason) {
    if (ino == 0)
        return;
    const auto state = CurrentTracking();
    if (!state)
        return;
    bool flush = false;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->rootIno == 0) {
            state->rootIno = ino;
            DebugLogPrint(4, "record root parent se=%p ino=%s reason=%s", CurrentSessionToken().se,
                          InodePath(ino).c_str(), reason);
        } else if (state->rootIno != ino) {
            DebugLogPrint(2, "root parent mismatch se=%p old=%s new=%s reason=%s",
                          CurrentSessionToken().se, InodePath(state->rootIno).c_str(),
                          InodePath(ino).c_str(), reason);
        }
        if (state->rootIno == ino && !state->rootPath.empty() && state->rootRefreshNeeded) {
            state->rootRefreshNeeded = false;
            flush = true;
        }
    }
    if (flush)
        ScheduleHiddenEntryInvalidation(CurrentSessionToken(), true);
}

bool IsTrackedHiddenSubtreeInode(uint32_t uid, uint64_t ino) {
    const auto& rule = BorrowHideRuleForUid(uid);
    const auto state = CurrentTracking();
    if (rule == nullptr || ino == 0 || !state)
        return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    const auto it = state->hiddenSubtreeRules.find(ino);
    if (it == state->hiddenSubtreeRules.end())
        return false;
    return std::any_of(it->second.begin(), it->second.end(), [&](const auto& trackedRule) {
        return CompiledHideRulesSemanticallyEqual(rule, trackedRule);
    });
}

bool TrackHiddenSubtreeInode(uint32_t uid, uint64_t ino) {
    const auto& rule = BorrowHideRuleForUid(uid);
    const auto state = CurrentTracking();
    if (rule == nullptr || ino == 0 || !state)
        return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    auto& rules = state->hiddenSubtreeRules[ino];
    const bool alreadyTracked =
        std::any_of(rules.begin(), rules.end(), [&](const auto& trackedRule) {
            return CompiledHideRulesSemanticallyEqual(rule, trackedRule);
        });
    if (alreadyTracked)
        return false;
    rules.push_back(rule);
    return true;
}

bool RemoveTrackedHiddenSubtreeInode(uint32_t uid, uint64_t ino) {
    const auto& rule = BorrowHideRuleForUid(uid);
    const auto state = CurrentTracking();
    if (rule == nullptr || ino == 0 || !state)
        return false;
    std::lock_guard<std::mutex> lock(state->mutex);
    const auto it = state->hiddenSubtreeRules.find(ino);
    if (it == state->hiddenSubtreeRules.end())
        return false;
    auto& rules = it->second;
    const size_t oldSize = rules.size();
    rules.erase(std::remove_if(rules.begin(), rules.end(),
                               [&](const auto& trackedRule) {
                                   return CompiledHideRulesSemanticallyEqual(rule, trackedRule);
                               }),
                rules.end());
    const bool changed = rules.size() != oldSize;
    if (rules.empty())
        state->hiddenSubtreeRules.erase(it);
    return changed;
}

std::optional<std::string> LookupTrackedPathForInode(SessionToken token, uint64_t ino) {
    if (ino == 0)
        return std::nullopt;
    const auto state = TrackingForToken(token);
    if (!state)
        return std::nullopt;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (ino == state->rootIno && !state->rootPath.empty())
        return state->rootPath;
    const auto it = state->inodeToPath.find(ino);
    return it != state->inodeToPath.end() ? std::optional<std::string>(it->second) : std::nullopt;
}

std::optional<std::string> LookupTrackedPathForInode(uint64_t ino) {
    return LookupTrackedPathForInode(CurrentSessionToken(), ino);
}

std::optional<uint64_t> LookupTrackedInodeForPath(SessionToken token, std::string_view path) {
    if (path.empty())
        return std::nullopt;
    const auto state = TrackingForToken(token);
    if (!state)
        return std::nullopt;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (path == state->rootPath && state->rootIno != 0)
        return state->rootIno;
    const auto it = state->pathToInode.find(path);
    return it != state->pathToInode.end() ? std::optional<uint64_t>(it->second) : std::nullopt;
}

std::optional<uint64_t> LookupTrackedInodeForPath(std::string_view path) {
    return LookupTrackedInodeForPath(CurrentSessionToken(), path);
}

void RememberTrackedPathForInode(SessionToken token, uint64_t ino, std::string_view path) {
    if (ino == 0 || path.empty())
        return;
    const bool visibleRoot = IsVisibleStorageRootPath(path);
    if (!visibleRoot && (!MayParticipateInHidePolicy(path) || !IsProtectedTrackedPath(path)))
        return;
    const auto state = TrackingForToken(token);
    if (!state)
        return;
    bool refresh = false;
    std::unordered_set<std::string> deferredNames;
    bool deferredInode = false;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (visibleRoot) {
            // Session creation supplies the FUSE source, but lookup(1, "0") supplies the
            // actual visible root inode/path. The observed lookup takes precedence.
            if (state->rootIno == 0 || state->rootIno == ino) {
                state->rootIno = ino;
                state->rootPath.assign(path);
                refresh = std::exchange(state->rootRefreshNeeded, false);
            } else {
                DebugLogPrint(2, "visible root inode mismatch se=%p old=%s new=%s", token.se,
                              InodePath(state->rootIno).c_str(), InodePath(ino).c_str());
            }
        }
        const auto existing = state->inodeToPath.find(ino);
        const bool newEntry = existing == state->inodeToPath.end();
        if (newEntry && state->inodeToPath.size() >= kMaxTrackedPathEntries)
            return;
        if (!newEntry && existing->second != path) {
            const auto oldReverse = state->pathToInode.find(existing->second);
            if (oldReverse != state->pathToInode.end() && oldReverse->second == ino) {
                state->pathToInode.erase(oldReverse);
            }
        }
        if (newEntry) {
            state->inodeToPath.emplace(ino, std::string(path));
        } else if (existing->second != path) {
            existing->second.assign(path.data(), path.size());
        }
        state->pathToInode[std::string(path)] = ino;
        if (auto pending = state->deferredEntries.find(ino);
            pending != state->deferredEntries.end()) {
            deferredNames = std::move(pending->second);
            state->deferredEntryCount -= deferredNames.size();
            state->deferredEntries.erase(pending);
        }
        deferredInode = state->deferredInodes.erase(ino) != 0;
    }
    for (const auto& name : deferredNames) {
        const std::string child = HiddenPathPolicy::JoinPathComponent(path, name);
        if (!IsProtectedMountTarget(child))
            QueueInvalidation(InvalidationKind::Entry, token, ino, name, child,
                              std::chrono::milliseconds(25));
    }
    if (deferredInode && !IsProtectedMountTarget(path))
        QueueInvalidation(InvalidationKind::Inode, token, ino, {}, std::string(path),
                          std::chrono::milliseconds(50));
    if (refresh)
        ScheduleHiddenEntryInvalidation(token, true);
}

void RememberTrackedPathForInode(uint64_t ino, std::string_view path) {
    RememberTrackedPathForInode(CurrentSessionToken(), ino, path);
}

std::vector<std::pair<uint64_t, std::string>> SnapshotTrackedPaths(SessionToken token) {
    const auto state = TrackingForToken(token);
    if (!state)
        return {};
    std::lock_guard<std::mutex> lock(state->mutex);
    return {state->inodeToPath.begin(), state->inodeToPath.end()};
}

void ResetSessionRuleTracking(SessionToken token) {
    const auto state = TrackingForToken(token);
    if (!state)
        return;
    std::lock_guard<std::mutex> lock(state->mutex);
    state->hiddenSubtreeRules.clear();
    state->recentParent.clear();
    state->recentAnyParent.clear();
    state->recentAnyUid = 0;
    state->uidErrRemap.clear();
}

void PutPendingReaddirContext(uint64_t unique, const PendingReaddirContext& context) {
    const auto state = CurrentTracking();
    if (!state || unique == 0)
        return;
    std::lock_guard<std::mutex> lock(state->mutex);
    state->pendingReaddir[unique] = context;
}

std::optional<PendingReaddirContext> GetPendingReaddirContext(uint64_t unique) {
    const auto state = CurrentTracking();
    if (!state || unique == 0)
        return std::nullopt;
    std::lock_guard<std::mutex> lock(state->mutex);
    const auto it = state->pendingReaddir.find(unique);
    return it == state->pendingReaddir.end() ? std::nullopt
                                             : std::optional<PendingReaddirContext>(it->second);
}

void SetPendingReaddirPath(uint64_t unique, std::string_view path) {
    const auto state = CurrentTracking();
    if (!state || unique == 0)
        return;
    std::lock_guard<std::mutex> lock(state->mutex);
    const auto it = state->pendingReaddir.find(unique);
    if (it != state->pendingReaddir.end())
        it->second.path.assign(path.data(), path.size());
}

void RemovePendingReaddirContext(uint64_t unique) {
    const auto state = CurrentTracking();
    if (!state || unique == 0)
        return;
    std::lock_guard<std::mutex> lock(state->mutex);
    state->pendingReaddir.erase(unique);
}

void RememberRecentHiddenParentPath(uint32_t uid, std::string_view path) {
    const auto state = CurrentTracking();
    if (!state || path.empty())
        return;
    std::lock_guard<std::mutex> lock(state->mutex);
    state->recentAnyParent.assign(path.data(), path.size());
    state->recentAnyUid = uid;
    if (uid != 0)
        state->recentParent[uid] = state->recentAnyParent;
}

std::optional<std::string> LookupRecentHiddenParentPath(uint32_t uid, uint32_t* matchedHiddenUid) {
    const auto state = CurrentTracking();
    if (!state)
        return std::nullopt;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (uid != 0) {
        const auto it = state->recentParent.find(uid);
        if (it == state->recentParent.end())
            return std::nullopt;
        if (matchedHiddenUid)
            *matchedHiddenUid = uid;
        return it->second;
    }
    if (state->recentAnyParent.empty())
        return std::nullopt;
    if (matchedHiddenUid)
        *matchedHiddenUid = state->recentAnyUid;
    return state->recentAnyParent;
}

void ClearRecentHiddenParentPath(uint32_t uid) {
    const auto state = CurrentTracking();
    if (!state)
        return;
    std::lock_guard<std::mutex> lock(state->mutex);
    if (uid != 0)
        state->recentParent.erase(uid);
    if (uid == 0 || uid == state->recentAnyUid) {
        state->recentAnyParent.clear();
        state->recentAnyUid = 0;
    }
}

// AOSP only decides dentry caching from the resolved path, not from uid policy.
// Once the daemon sees any path inside the hidden subtree, force cache invalidation globally for
// that subtree so positive dentries from other apps stop leaking into the target uid.
void NoteHiddenSubtreePathForCache(uint32_t uid, std::string_view path) {
    if (!HiddenPathPolicy::IsAnyHiddenSubtreePath(uid, path)) {
        return;
    }

    // AOSP get_entry_timeout()/pf_getattr cache decisions are path-based rather than uid-based.
    // Once this subtree is observed anywhere in the daemon, proactively invalidate the root dentry
    // so a positive lookup seeded by another uid does not stay shared in kernel/VFS cache.
    RuntimeState::ScheduleHiddenEntryInvalidation();

    // pf_getattr/lstat can intentionally deny a hidden first-level child
    // before its inode path is recorded. Root dentry invalidation already
    // covers the child; avoid an inode notification that would be deferred
    // forever with no opportunity to learn that path.
    bool coveredByRoot = false;
    if (const auto root = VisibleStorageRootForPath(path); root) {
        const auto& rule = BorrowHideRuleForUid(uid);
        coveredByRoot = rule != nullptr && RootSweepCoversUnknownChildInode(
                                               path, *root, HideAllForStorageRoot(*rule, *root));
    }

    if (gInPfLookup && gCurrentLookupParentInode != 0) {
        const uint64_t rootParent = CurrentRootParentInode();
        if (HiddenPathPolicy::IsExactHiddenTargetPath(uid, path) &&
            gCurrentLookupParentInode == rootParent) {
            RemoveTrackedHiddenSubtreeInode(uid, gCurrentLookupParentInode);
            return;
        }
        gTrackHiddenSubtreeLookup = true;
        if (TrackHiddenSubtreeInode(uid, gCurrentLookupParentInode)) {
            DebugLogPrint(4, "track hidden lookup parent=%s path=%s",
                          InodePath(gCurrentLookupParentInode).c_str(), DebugPreview(path).c_str());
            if (!coveredByRoot)
                RuntimeState::ScheduleHiddenInodeInvalidation(gCurrentLookupParentInode);
        }
    }

    if (gInPfGetattr && gPfGetattrIno != 0) {
        gZeroAttrCacheForCurrentGetattr = true;
        if (TrackHiddenSubtreeInode(uid, gPfGetattrIno)) {
            DebugLogPrint(4, "track hidden getattr ino=%s path=%s",
                          InodePath(gPfGetattrIno).c_str(), DebugPreview(path).c_str());
            if (!coveredByRoot)
                RuntimeState::ScheduleHiddenInodeInvalidation(gPfGetattrIno);
        }
    }
}

uint32_t ReqUid(fuse_req_t req) {
    return RuntimeState::ReqUid(req);
}

void RememberFuseSession(fuse_req_t req) {
    RuntimeState::RememberFuseSession(req);
}

void ClearActiveFuseRequestSession() {
    gActiveFuseRequestToken = {};
}

void ScheduleHiddenEntryInvalidation() {
    RuntimeState::ScheduleHiddenEntryInvalidation();
}

void ScheduleHiddenInodeInvalidation(uint64_t ino) {
    RuntimeState::ScheduleHiddenInodeInvalidation(ino);
}

}  // namespace fusehide
