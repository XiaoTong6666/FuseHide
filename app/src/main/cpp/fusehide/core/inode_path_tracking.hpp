// Copyright (C) 2026 XiaoTong6666
// Licensed under the Apache License, Version 2.0
#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace fusehide::inode_tracking {

enum class UpdateKind { Ignored, Added, Confirmed, Replaced, ConflictingInference };

struct UpdateResult {
    UpdateKind kind = UpdateKind::Ignored;
    std::string previousPath;
    size_t evicted = 0;
};

struct RenameResult {
    size_t moved = 0;
    size_t replaced = 0;
    bool sourceFound = false;
    bool destinationFound = false;
    bool invalid = false;
};

inline bool IsSameOrChild(std::string_view path, std::string_view prefix) {
    return !prefix.empty() && path.size() >= prefix.size() &&
           path.substr(0, prefix.size()) == prefix &&
           (path.size() == prefix.size() || path[prefix.size()] == '/');
}

// The byPath index contains exactly one (path, uid) pair for every perUid
// entry. Path-order lookup avoids a full UID scan when one parent is retired.
template <class RecentMap, class RecentIndex>
void RememberRecentParent(RecentMap& perUid, RecentIndex& byPath, uint32_t uid,
                          std::string_view path) {
    if (uid == 0 || path.empty())
        return;
    auto [it, inserted] = perUid.try_emplace(uid, path);
    if (!inserted) {
        if (it->second == path)
            return;
        byPath.erase(std::make_pair(it->second, uid));
        it->second.assign(path);
    }
    byPath.emplace(it->second, uid);
}

template <class RecentMap, class RecentIndex>
void EraseRecentParent(RecentMap& perUid, RecentIndex& byPath, uint32_t uid) {
    const auto it = perUid.find(uid);
    if (it == perUid.end())
        return;
    byPath.erase(std::make_pair(it->second, uid));
    perUid.erase(it);
}

template <class RecentMap, class RecentIndex>
size_t PruneRecentParents(RecentMap& perUid, RecentIndex& byPath, std::string& lastPath,
                          uint32_t& lastUid, std::string_view prefix) {
    if (prefix.empty())
        return 0;
    size_t removed = 0;
    // Exact path and descendants form two ordered ranges. Looking up "/foo/"
    // explicitly prevents mistakenly removing the sibling "/foobar".
    const auto eraseRange = [&](std::string_view firstPath, bool descendants) {
        auto it = byPath.lower_bound(std::make_pair(std::string(firstPath), uint32_t{0}));
        while (it != byPath.end() &&
               (descendants ? std::string_view(it->first).starts_with(firstPath)
                            : std::string_view(it->first) == firstPath)) {
            perUid.erase(it->second);
            it = byPath.erase(it);
            ++removed;
        }
    };
    eraseRange(prefix, false);
    std::string descendantPrefix(prefix);
    if (descendantPrefix.back() != '/')
        descendantPrefix += '/';
    eraseRange(descendantPrefix, true);
    if (IsSameOrChild(lastPath, prefix)) {
        lastPath.clear();
        lastUid = 0;
        ++removed;
    }
    return removed;
}

// All callers hold the SessionTracking mutex. No rule resolution or logging is
// performed here. The callback forgets any policy marks attached to a retired
// inode address. Never let an inferred child path overwrite a different path.
template <class Forward, class Reverse, class Forget>
size_t EraseInode(Forward& forward, Reverse& reverse, std::unordered_set<uint64_t>& trusted,
                  uint64_t ino, Forget&& onForget) {
    auto it = forward.find(ino);
    if (it == forward.end()) {
        trusted.erase(ino);
        onForget(ino);
        return 0;
    }
    const auto reverseIt = reverse.find(it->second);
    if (reverseIt != reverse.end() && reverseIt->second == ino)
        reverse.erase(reverseIt);
    forward.erase(it);
    trusted.erase(ino);
    onForget(ino);
    return 1;
}

template <class Forward, class Reverse, class Forget>
size_t EraseSubtree(Forward& forward, Reverse& reverse, std::unordered_set<uint64_t>& trusted,
                    std::string_view prefix, Forget&& onForget) {
    if (prefix.empty())
        return 0;
    size_t removed = 0;
    const auto eraseRange = [&](std::string_view firstPath, bool descendants) {
        auto it = reverse.lower_bound(firstPath);
        while (it != reverse.end() &&
               (descendants ? std::string_view(it->first).starts_with(firstPath)
                            : std::string_view(it->first) == firstPath)) {
            const uint64_t ino = it->second;
            it = reverse.erase(it);
            forward.erase(ino);
            trusted.erase(ino);
            onForget(ino);
            ++removed;
        }
    };
    eraseRange(prefix, false);
    std::string descendantPrefix(prefix);
    if (descendantPrefix.back() != '/')
        descendantPrefix += '/';
    eraseRange(descendantPrefix, true);
    return removed;
}

// Caller holds the session mutex. The source inode identities survive a
// completed rename, so relocate their paths rather than forgetting them and
// temporarily losing hidden-subtree coverage. All destination paths are built
// before mutating either index. Only displaced destination nodes are retired.
template <class Forward, class Reverse, class Retire, class Moved>
RenameResult RenameSubtrees(Forward& forward, Reverse& reverse,
                            std::unordered_set<uint64_t>& trusted, std::string_view source,
                            std::string_view destination, Retire&& onRetire, Moved&& onMoved) {
    RenameResult result;
    if (source.empty() || destination.empty() || source == destination)
        return result;
    if (IsSameOrChild(source, destination) || IsSameOrChild(destination, source)) {
        result.invalid = true;
        return result;
    }

    struct Entry {
        uint64_t ino;
        std::string oldPath;
        std::string newPath;
    };
    const auto collect = [&](std::string_view from, std::string_view to) {
        std::vector<Entry> entries;
        const auto append = [&](auto it) {
            entries.push_back(
                Entry{it->second, it->first, std::string(to) + it->first.substr(from.size())});
        };
        if (auto it = reverse.find(from); it != reverse.end())
            append(it);
        std::string prefix(from);
        if (prefix.back() != '/')
            prefix += '/';
        for (auto it = reverse.lower_bound(prefix);
             it != reverse.end() && std::string_view(it->first).starts_with(prefix); ++it)
            append(it);
        return entries;
    };
    const std::vector<Entry> fromEntries = collect(source, destination);
    result.sourceFound = !fromEntries.empty();
    const auto existingDestination = reverse.lower_bound(destination);
    result.destinationFound = existingDestination != reverse.end() &&
                              IsSameOrChild(existingDestination->first, destination);
    result.replaced = EraseSubtree(forward, reverse, trusted, destination, onRetire);

    const auto removeOriginalPaths = [&](const std::vector<Entry>& entries) {
        for (const auto& entry : entries)
            reverse.erase(entry.oldPath);
    };
    removeOriginalPaths(fromEntries);

    const auto insertMovedPaths = [&](const std::vector<Entry>& entries) {
        for (const auto& entry : entries) {
            auto it = forward.find(entry.ino);
            if (it == forward.end())
                continue;
            it->second = entry.newPath;
            reverse.emplace(entry.newPath, entry.ino);
            onMoved(entry.ino, entry.oldPath, entry.newPath);
            ++result.moved;
        }
    };
    insertMovedPaths(fromEntries);
    return result;
}

// Snapshot only UIDs whose recent parent path is inside a rename's source
// subtree. This must run while holding the tracking mutex, but performs no
// rule resolution or JNI calls.
template <class RecentIndex>
std::unordered_set<uint32_t> RecentRenameUids(const RecentIndex& byPath, std::string_view source) {
    std::unordered_set<uint32_t> uids;
    if (source.empty())
        return uids;
    const auto lower = byPath.lower_bound(std::make_pair(std::string(source), uint32_t{0}));
    for (auto it = lower; it != byPath.end() && it->first == source; ++it)
        uids.insert(it->second);
    std::string prefix(source);
    if (prefix.back() != '/')
        prefix += '/';
    for (auto it = byPath.lower_bound(std::make_pair(prefix, uint32_t{0}));
         it != byPath.end() && std::string_view(it->first).starts_with(prefix); ++it)
        uids.insert(it->second);
    return uids;
}

// Recent reply_buf parent hints are keyed by UID, but their sorted reverse
// index lets a successful rename relocate only affected hints. The callback
// must use PRE-RESOLVED rule snapshots, never resolve a UID under the lock.
template <class RecentMap, class RecentIndex, class ShouldKeep>
size_t RenameRecentParents(RecentMap& perUid, RecentIndex& byPath, std::string& lastPath,
                           uint32_t& lastUid, std::string_view source, std::string_view destination,
                           ShouldKeep&& shouldKeep) {
    if (source.empty() || destination.empty() || source == destination ||
        IsSameOrChild(source, destination) || IsSameOrChild(destination, source))
        return 0;
    struct Entry {
        uint32_t uid;
        std::string path;
    };
    const auto collect = [&](std::string_view from, std::string_view to) {
        std::vector<Entry> entries;
        const auto append = [&](auto it) {
            entries.push_back({it->second, std::string(to) + it->first.substr(from.size())});
        };
        auto it = byPath.lower_bound(std::make_pair(std::string(from), uint32_t{0}));
        while (it != byPath.end() && it->first == from) {
            append(it);
            ++it;
        }
        std::string prefix(from);
        if (prefix.back() != '/')
            prefix += '/';
        it = byPath.lower_bound(std::make_pair(prefix, uint32_t{0}));
        while (it != byPath.end() && std::string_view(it->first).starts_with(prefix)) {
            append(it);
            ++it;
        }
        return entries;
    };
    auto moved = collect(source, destination);
    const std::string oldLastPath = lastPath;
    const uint32_t oldLastUid = lastUid;
    std::string nextLastPath;
    if (IsSameOrChild(oldLastPath, source)) {
        nextLastPath = std::string(destination) + oldLastPath.substr(source.size());
    }
    PruneRecentParents(perUid, byPath, lastPath, lastUid, source);
    PruneRecentParents(perUid, byPath, lastPath, lastUid, destination);
    size_t retained = 0;
    for (const auto& entry : moved) {
        if (shouldKeep(entry.uid, entry.path)) {
            RememberRecentParent(perUid, byPath, entry.uid, entry.path);
            ++retained;
        }
    }
    if (!nextLastPath.empty() && shouldKeep(oldLastUid, nextLastPath)) {
        lastPath = std::move(nextLastPath);
        lastUid = oldLastUid;
    }
    return retained;
}

template <class Forward, class Reverse, class Forget>
UpdateResult Observe(Forward& forward, Reverse& reverse, std::unordered_set<uint64_t>& trusted,
                     uint64_t ino, std::string_view path, bool authoritative, bool allowNew,
                     Forget&& onForget) {
    if (ino == 0 || path.empty())
        return {};
    auto existing = forward.find(ino);
    if (existing == forward.end() && !allowNew) {
        // A verified visible path can still refute an orphaned hidden inode mark.
        if (authoritative)
            onForget(ino);
        return {};
    }
    if (existing != forward.end() && existing->second == path) {
        if (authoritative)
            trusted.insert(ino);
        return {.kind = UpdateKind::Confirmed, .previousPath = {}, .evicted = 0};
    }
    if (existing != forward.end() && !authoritative) {
        // A derived parent/name may be stale (especially after rename). Do not
        // corrupt a verified mapping, and never inherit a hidden mark blindly.
        return {.kind = UpdateKind::ConflictingInference,
                .previousPath = existing->second,
                .evicted = 0};
    }
    // A lookup-derived child path is only a guess until MediaProvider supplies
    // BuildPath/lstat. Never evict another inode's identity based on that guess.
    if (!authoritative) {
        if (const auto collision = reverse.find(std::string(path));
            collision != reverse.end() && collision->second != ino) {
            return {.kind = UpdateKind::ConflictingInference, .previousPath = {}, .evicted = 0};
        }
    }

    UpdateResult result;
    result.kind = existing == forward.end() ? UpdateKind::Added : UpdateKind::Replaced;
    if (existing != forward.end()) {
        result.previousPath = existing->second;
        result.evicted += EraseSubtree(forward, reverse, trusted, result.previousPath, onForget);
    }
    if (const auto collision = reverse.find(std::string(path));
        collision != reverse.end() && collision->second != ino) {
        result.evicted += EraseInode(forward, reverse, trusted, collision->second, onForget);
    }
    forward[ino] = std::string(path);
    reverse[std::string(path)] = ino;
    if (authoritative)
        trusted.insert(ino);
    return result;
}

}  // namespace fusehide::inode_tracking
