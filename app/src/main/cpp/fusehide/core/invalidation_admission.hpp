// Copyright (C) 2026 XiaoTong6666
// Licensed under the Apache License, Version 2.0.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace fusehide {
inline constexpr size_t kInvalidationQueueLimit = 4096;
inline constexpr size_t kInvalidationCriticalReserve = 64;
inline constexpr size_t kInvalidationParentSweepsLimit = 64;
// Exact inode notifications, never approximated by parent dentry invalidation.
inline constexpr size_t kInvalidationInodeCompensationLimit = 512;

// An evicted inode must have an exact, bounded home before the queue slot is
// reassigned. A parent sweep cannot compensate for an inode notification.
constexpr bool CanEvictInodeIntoCompensation(size_t overflowInodes) {
    return overflowInodes < kInvalidationInodeCompensationLimit;
}

// Only the active config epoch can determine an acknowledgement. Historical
// errors remain in the process-wide diagnostic counter.
struct InvalidationBatchCounters {
    uint64_t epoch = 0;
    uint64_t submitted = 0;
    uint64_t completed = 0;
    uint64_t compensated = 0;
    uint64_t rejected = 0;
    uint64_t failed = 0;
    uint64_t outstanding = 0;

    constexpr void accept() {
        ++submitted;
        ++outstanding;
    }
    constexpr void finish(bool success, bool compensation) {
        if (outstanding == 0) {
            ++failed;
            return;
        }
        --outstanding;
        if (success) {
            ++completed;
            if (compensation)
                ++compensated;
        } else {
            ++failed;
        }
    }
    constexpr void reject() {
        ++rejected;
    }

    constexpr bool complete() const {
        return epoch != 0 && outstanding == 0 && rejected == 0 && failed == 0 &&
               completed == submitted;
    }
};

enum class InvalidationAdmission {
    Queue,
    EvictInodeThenQueue,
    ExactInodeCompensation,
    CoalescedParent,
    NewParentSweep,
    Reject,
};

// Pure decision shared by production and host tests; session/generation identity
// remains the caller's responsibility.
constexpr InvalidationAdmission DecideInvalidationAdmission(
    size_t queued, size_t parents, bool critical, bool isEntry, bool hasParentPath,
    bool parentAlreadyScheduled, bool canEvictInode, bool configReload = false,
    size_t overflowInodes = 0) {
    if (queued < kInvalidationQueueLimit - (critical ? 0 : kInvalidationCriticalReserve))
        return InvalidationAdmission::Queue;
    if (critical && queued >= kInvalidationQueueLimit && canEvictInode)
        return InvalidationAdmission::EvictInodeThenQueue;
    // Exact cached-inode references cannot be invalidated with a parent dentry
    // sweep. A config publisher may overflow into a bounded worklist while
    // retaining the original Session generation and inode identity.
    if (!critical && configReload && overflowInodes < kInvalidationInodeCompensationLimit)
        return InvalidationAdmission::ExactInodeCompensation;
    if (isEntry && hasParentPath) {
        if (parentAlreadyScheduled)
            return InvalidationAdmission::CoalescedParent;
        if (parents < kInvalidationParentSweepsLimit)
            return InvalidationAdmission::NewParentSweep;
    }
    return InvalidationAdmission::Reject;
}

// A hidden first-level child has its dentry removed by the already queued
// root sweep. Do not add an unresolved inode to deferredInodes when lstat()
// deliberately returned ENOENT before learning its inode->path mapping.
constexpr bool RootSweepCoversUnknownChildInode(std::string_view path, std::string_view rootPath,
                                                bool hideAllForRoot) {
    return hideAllForRoot && !rootPath.empty() && path.size() > rootPath.size() + 1 &&
           path.substr(0, rootPath.size()) == rootPath && path[rootPath.size()] == '/' &&
           path.find('/', rootPath.size() + 1) == std::string_view::npos;
}

// Some Android MediaProvider builds create auxiliary FUSE sessions which
// never expose a visible storage root or any inode/path. There can be no
// cached target belonging to that session. Actual mounted storage roots are
// still checked separately by AllActiveSessionsHaveLowlevelCallbacks().
constexpr bool CanSkipUntrackedSessionRootInvalidation(bool hasKnownRoot, size_t trackedPaths,
                                                       size_t deferredEntries,
                                                       size_t deferredInodes) {
    return !hasKnownRoot && trackedPaths == 0 && deferredEntries == 0 && deferredInodes == 0;
}

// A root path can be known even when that Session has never instantiated its
// visible child. The verified MediaProvider node tree, not a guessed inode,
// establishes whether there is any pre-existing /storage/emulated/0 dentry to
// invalidate. A new lookup after config publication sees the new rules.
constexpr bool CanSkipVerifiedEmptyPrimaryRoot(bool verifiedRoot, bool childAbsent,
                                               size_t trackedPaths, size_t reversePaths,
                                               size_t deferredEntries, size_t deferredInodes,
                                               size_t trackedSubtrees) {
    return verifiedRoot && childAbsent && trackedPaths == 0 && reversePaths == 0 &&
           deferredEntries == 0 && deferredInodes == 0 && trackedSubtrees == 0;
}
}  // namespace fusehide
