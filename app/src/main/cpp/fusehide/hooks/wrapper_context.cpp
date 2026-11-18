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

#include "fusehide/policy/path_policy.hpp"

namespace fusehide {

thread_local uint32_t gActiveCreateUid = 0;
thread_local uint32_t gActiveCreateScopeDepth = 0;
thread_local uint32_t gActivePathPolicyUid = 0;
thread_local std::string_view gActivePathPolicyPath;
thread_local bool gCrossUidSensitiveLookup = false;
thread_local RenameTracking* gActiveRenameTracking = nullptr;
thread_local ActiveReaddirContext gActiveReaddirContext;
#if defined(__x86_64__)
thread_local ReaddirplusContinuation* gReaddirplusContinuation = nullptr;
#endif

namespace {

struct HiddenPathClassificationCacheKey {
    uint32_t uid = 0;
    std::string path;

    bool operator==(const HiddenPathClassificationCacheKey& other) const {
        return uid == other.uid && path == other.path;
    }
};

struct HiddenPathClassificationCacheKeyHash {
    size_t operator()(const HiddenPathClassificationCacheKey& key) const {
        const size_t uidHash = std::hash<uint32_t>{}(key.uid);
        const size_t pathHash = std::hash<std::string>{}(key.path);
        return uidHash ^ (pathHash + 0x9e3779b9 + (uidHash << 6) + (uidHash >> 2));
    }
};

struct HiddenPathClassificationCacheEntry {
    uint64_t configGeneration = 0;
    uint64_t packageSetGeneration = 0;
    HiddenPathClassification classification = HiddenPathClassification::kNone;
};

constexpr size_t kMaxHiddenPathClassificationCacheEntries = 4096;

std::mutex gHiddenPathClassificationCacheMutex;
std::unordered_map<HiddenPathClassificationCacheKey, HiddenPathClassificationCacheEntry,
                   HiddenPathClassificationCacheKeyHash>
    gHiddenPathClassificationCache;
}  // namespace

bool SnapshotActiveReaddirContext(fuse_req_t req, PendingReaddirContext* out) {
    if (req == nullptr || out == nullptr || gActiveReaddirContext.depth == 0 ||
        gActiveReaddirContext.reqUnique != req->unique) {
        return false;
    }
    *out = gActiveReaddirContext.pending;
    return true;
}

void UpdateActiveReaddirPath(uint64_t reqUnique, std::string_view path) {
    if (reqUnique == 0 || gActiveReaddirContext.depth == 0 ||
        gActiveReaddirContext.reqUnique != reqUnique) {
        return;
    }

    const uint64_t ino = gActiveReaddirContext.pending.ino;
    if (ino != 0) {
        RememberTrackedPathForInode(ino, path, true);
        if (IsVisibleRootPath(path)) {
            RememberVisibleRootParentInode(ino, "readdir_path");
        }
    }

    if (!gActiveReaddirContext.globalBacked) {
        return;
    }
    gActiveReaddirContext.pending.path.assign(path.data(), path.size());
    SetPendingReaddirPath(reqUnique, gActiveReaddirContext.pending.path);
}

void CompletePendingReaddirContext(fuse_req_t req, uint32_t reqUid) {
    if (req == nullptr) {
        return;
    }
    if (gActiveReaddirContext.depth != 0 && gActiveReaddirContext.reqUnique == req->unique) {
        if (gActiveReaddirContext.globalBacked) {
            RemovePendingReaddirContext(req->unique);
            gActiveReaddirContext.globalBacked = false;
        }
        return;
    }

    // A global context exists only for target UIDs. Keep uid==0 as a conservative fallback for
    // device paths that lose the request uid before reply_buf/reply_err.
    if (reqUid != 0 && BorrowHideRuleForUid(reqUid) == nullptr) {
        return;
    }
    RemovePendingReaddirContext(req->unique);
}

std::string_view TrimTrailingSlashes(std::string_view path) {
    while (path.size() > 1 && path.back() == '/') {
        path.remove_suffix(1);
    }
    return path;
}

bool IsVisibleRootPath(std::string_view path) {
    path = TrimTrailingSlashes(path);
    const auto root = VisibleStorageRootForPath(path);
    return root.has_value() && path == *root;
}

bool IsPathUnderVisibleRoot(std::string_view path) {
    path = TrimTrailingSlashes(path);
    return VisibleStorageRootForPath(path).has_value();
}

std::string CanonicalRootSnapshotPath(std::string_view path) {
    return IsVisibleRootPath(path) ? std::string(path) : std::string();
}

bool PathHasVisibleRootParent(std::string_view path) {
    path = TrimTrailingSlashes(path);
    const size_t slash = path.rfind('/');
    if (slash == std::string_view::npos || slash == 0) {
        return false;
    }
    return IsVisibleRootPath(path.substr(0, slash));
}

bool ResolveVisibleParentPath(uint64_t parent, std::string* parentPathOut) {
    if (parentPathOut == nullptr) {
        return false;
    }
    const auto trackedParentPath = LookupTrackedPathForInode(parent);
    if (!trackedParentPath.has_value()) {
        return false;
    }
    *parentPathOut = *trackedParentPath;
    return true;
}

// The kernel's positive FUSE dentries are shared across request UIDs. If an
// unrestricted UID looks up a name that *another* UID hides, the normal AOSP
// entry_timeout (DBL_MAX) lets the restricted UID bypass our lookup callback
// via dcache. Keep only names covered by some effective rule uncached.
bool IsCrossUidSensitiveLookup(uint64_t parent, const char* name) {
    if (parent == 0 || name == nullptr || name[0] == '\0') {
        return false;
    }
    const auto anyRule = RuleForAnyPackage();
    if (!anyRule) {
        return false;
    }
    if (const auto parentPath = LookupTrackedPathForInode(parent); parentPath.has_value()) {
        return HiddenPathPolicy::IsAnyHiddenSubtreePath(
            *anyRule, HiddenPathPolicy::JoinPathComponent(*parentPath, name));
    }
    // Root parent tracking may lag its first lookup. Do not let that window
    // create a long-lived positive dentry for a configured hidden root entry.
    return parent == CurrentRootParentInode() && HiddenPathPolicy::IsHiddenRootEntryName(name);
}

std::optional<std::string> InferVisibleRootPathFromLookup(uint64_t parent, std::string_view name) {
    if (parent != 1 || name.empty()) {
        return std::nullopt;
    }
    const std::string canonicalName = CanonicalizeHiddenEntryNameForMatch(name);
    for (const auto& root : kVisibleStorageRoots) {
        const size_t slash = root.rfind('/');
        const std::string_view baseName =
            slash == std::string_view::npos ? root : root.substr(slash + 1);
        if (CanonicalizeHiddenEntryNameForMatch(baseName) == canonicalName) {
            return std::string(root);
        }
    }
    return std::nullopt;
}

void RememberVisibleRootParentInode(uint64_t parent, const char* reason) {
    if (parent == 0) {
        return;
    }
    RememberRootParentInode(parent, reason);
}

HiddenPathClassification ComputeHiddenPathClassification(uint32_t uid, std::string_view path) {
    if (uid == 0 || HiddenPathPolicy::IsHiddenRootDirectoryPath(path)) {
        return HiddenPathClassification::kNone;
    }
    if (HiddenPathPolicy::IsExactHiddenTargetPath(uid, path)) {
        return HiddenPathClassification::kHiddenRoot;
    }
    if (HiddenPathPolicy::IsAnyHiddenSubtreePath(uid, path)) {
        return HiddenPathClassification::kHiddenDescendant;
    }
    return HiddenPathClassification::kNone;
}

const char* HiddenPathClassificationName(HiddenPathClassification classification) {
    switch (classification) {
        case HiddenPathClassification::kNone:
            return "none";
        case HiddenPathClassification::kHiddenRoot:
            return "hidden_root";
        case HiddenPathClassification::kHiddenDescendant:
            return "hidden_descendant";
    }
    return "unknown";
}

// lstat/stat/getxattr and the lower-fs fallbacks can repeatedly ask the same question during one
// request burst. Cache the final classification behind config + package-set generations.
HiddenPathClassification ClassifyHiddenPath(uint32_t uid, std::string_view path) {
    if (uid == 0 || path.empty()) {
        return HiddenPathClassification::kNone;
    }

    if (BorrowHideRuleForUid(uid) == nullptr) {
        return HiddenPathClassification::kNone;
    }

    HiddenPathClassificationCacheKey key{uid, std::string(path)};
    uint64_t generation = 0;
    uint64_t packageSetGeneration = 0;
    bool staleEntry = false;
    {
        std::lock_guard<std::mutex> lock(gHiddenPathClassificationCacheMutex);
        generation = gHideConfigGeneration.load(std::memory_order_acquire);
        packageSetGeneration = gUidPackageSetGeneration.load(std::memory_order_acquire);
        const auto it = gHiddenPathClassificationCache.find(key);
        if (it != gHiddenPathClassificationCache.end()) {
            if (it->second.configGeneration == generation &&
                it->second.packageSetGeneration == packageSetGeneration) {
                if (IsPathUnderVisibleRoot(path)) {
                    DebugLogPrint(4,
                                  "cache hidden_path hit uid=%u path=%s class=%s generation=%llu "
                                  "package_generation=%llu",
                                  static_cast<unsigned>(uid), DebugPreview(path).c_str(),
                                  HiddenPathClassificationName(it->second.classification),
                                  static_cast<unsigned long long>(generation),
                                  static_cast<unsigned long long>(packageSetGeneration));
                }
                return it->second.classification;
            }
            staleEntry = true;
            gHiddenPathClassificationCache.erase(it);
        }
    }
    if (IsPathUnderVisibleRoot(path)) {
        DebugLogPrint(4,
                      "cache hidden_path miss uid=%u path=%s reason=%s generation=%llu "
                      "package_generation=%llu",
                      static_cast<unsigned>(uid), DebugPreview(path).c_str(),
                      staleEntry ? "stale" : "empty", static_cast<unsigned long long>(generation),
                      static_cast<unsigned long long>(packageSetGeneration));
    }

    const HiddenPathClassification classification = ComputeHiddenPathClassification(uid, path);
    {
        std::lock_guard<std::mutex> lock(gHiddenPathClassificationCacheMutex);
        if (gHideConfigGeneration.load(std::memory_order_acquire) == generation &&
            gUidPackageSetGeneration.load(std::memory_order_acquire) == packageSetGeneration) {
            HiddenPathClassificationCacheEntry entry{generation, packageSetGeneration,
                                                     classification};
            const auto existing = gHiddenPathClassificationCache.find(key);
            if (existing != gHiddenPathClassificationCache.end()) {
                existing->second = entry;
            } else if (gHiddenPathClassificationCache.size() >=
                       kMaxHiddenPathClassificationCacheEntries) {
                // The cache is deliberately approximate. Recycle one existing unordered-map node
                // instead of clearing all 4096 entries at once: this keeps capacity bounded while
                // avoiding the allocator/free burst caused by a whole-table clear during a large
                // target-app scan.
                auto recycled =
                    gHiddenPathClassificationCache.extract(gHiddenPathClassificationCache.begin());
                recycled.key() = std::move(key);
                recycled.mapped() = entry;
                gHiddenPathClassificationCache.insert(std::move(recycled));
            } else {
                gHiddenPathClassificationCache.emplace(std::move(key), entry);
            }
            if (IsPathUnderVisibleRoot(path)) {
                DebugLogPrint(4,
                              "cache hidden_path store uid=%u path=%s class=%s generation=%llu "
                              "package_generation=%llu",
                              static_cast<unsigned>(uid), DebugPreview(path).c_str(),
                              HiddenPathClassificationName(classification),
                              static_cast<unsigned long long>(generation),
                              static_cast<unsigned long long>(packageSetGeneration));
            }
        }
    }
    return classification;
}

bool IsHiddenPathClassification(HiddenPathClassification classification) {
    return classification != HiddenPathClassification::kNone;
}

bool IsExactHiddenPathClassification(HiddenPathClassification classification) {
    return classification == HiddenPathClassification::kHiddenRoot;
}

void ClearHiddenPathClassificationCache() {
    std::lock_guard<std::mutex> lock(gHiddenPathClassificationCacheMutex);
    gHiddenPathClassificationCache.clear();
}

}  // namespace fusehide
