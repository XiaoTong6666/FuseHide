// Copyright (C) 2026 XiaoTong6666
// Licensed under the Apache License, Version 2.0.
#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fusehide {

struct InodeInvalidationCandidate {
    uint64_t ino;
    std::string path;
    bool changed;
};

// Preserve exact inode identities: a parent-entry notification does not
// invalidate an already-referenced child inode. Within one Session, dedupe
// inode identities and schedule policy transitions before still-hidden nodes.
// The latter still need notification because a visible client may have
// populated positive cache entries between configuration publications.
template <typename OldHidden, typename NewHidden>
std::vector<InodeInvalidationCandidate> PlanInodeInvalidations(
    const std::vector<std::pair<uint64_t, std::string>>& paths, OldHidden oldHidden,
    NewHidden newHidden) {
    std::vector<InodeInvalidationCandidate> selected;
    selected.reserve(paths.size());
    std::unordered_set<uint64_t> seen;
    seen.reserve(paths.size());
    for (const auto& [ino, path] : paths) {
        if (ino == 0 || !seen.insert(ino).second)
            continue;
        const bool before = oldHidden(path);
        const bool after = newHidden(path);
        if (!before && !after)
            continue;
        selected.push_back({ino, path, before != after});
    }
    std::stable_partition(selected.begin(), selected.end(),
                          [](const auto& item) { return item.changed; });
    return selected;
}

}  // namespace fusehide
