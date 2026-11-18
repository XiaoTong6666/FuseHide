// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#include "fusehide/core/root_snapshot_cache.hpp"

#include <map>
#include <tuple>

namespace fusehide {
namespace {

// MediaProvider root enumeration is uid-sensitive, so root snapshots stay scoped to uid +
// effective FuseHide rule fingerprint instead of being shared by fingerprint alone.
struct RootSnapshotCacheKey {
    SessionToken session;
    uint32_t uid = 0;
    uint64_t fingerprint = 0;
    std::string path;

    bool operator==(const RootSnapshotCacheKey& other) const {
        return session == other.session && uid == other.uid && fingerprint == other.fingerprint &&
               path == other.path;
    }
};

struct RootSnapshotCacheKeyHash {
    size_t operator()(const RootSnapshotCacheKey& key) const {
        const size_t uidHash = std::hash<uint32_t>{}(key.uid) ^ std::hash<void*>{}(key.session.se) ^
                               std::hash<uint64_t>{}(key.session.generation);
        const size_t fingerprintHash = std::hash<uint64_t>{}(key.fingerprint);
        const size_t pathHash = std::hash<std::string>{}(key.path);
        const size_t combined =
            uidHash ^ (fingerprintHash + 0x9e3779b9 + (uidHash << 6) + (uidHash >> 2));
        return combined ^ (pathHash + 0x9e3779b9 + (combined << 6) + (combined >> 2));
    }
};

struct RootSnapshotCacheEntry {
    uint64_t configGeneration = 0;
    uint64_t packageSetGeneration = 0;
    uint64_t parentGeneration = 0;
    std::shared_ptr<const CompiledHideRule> rule;
    std::shared_ptr<const DirectoryEntries> sharedEntries;
    std::shared_ptr<const CachedValueDirectoryEntries> valueSnapshot;
};

std::mutex gRootSnapshotCacheMutex;
std::unordered_map<RootSnapshotCacheKey, RootSnapshotCacheEntry, RootSnapshotCacheKeyHash>
    gRootSnapshotCache;
std::mutex gPendingRootSnapshotMutationsMutex;
std::map<std::tuple<void*, uint64_t, uint64_t>, const char*> gPendingRootSnapshotMutations;
std::atomic<uint64_t> gRootSnapshotParentGeneration{1};

}  // namespace

namespace {

bool CanCacheRootSnapshotEntries(const DirectoryEntries& entries) {
    if (entries.empty()) {
        return true;
    }
    const auto& first = entries.front();
    return first != nullptr && !first->d_name.empty();
}

bool CanCacheRootSnapshotEntries(const ValueDirectoryEntries& entries) {
    return entries.empty() || !entries.front().d_name.empty();
}

template <typename Entries>
Entries CloneDirectoryEntries(const Entries& entries) {
    return Entries(entries.begin(), entries.end());
}

CachedValueDirectoryEntries SnapshotValueDirectoryEntries(const ValueDirectoryEntries& entries) {
    // Ghidra for the API 37 sample shows do_readdir_common consuming only DirectoryEntry::d_name
    // and DirectoryEntry::d_type from the returned vector. Cache that semantic payload instead of
    // holding a target-facing value-vector object across calls.
    CachedValueDirectoryEntries snapshot;
    snapshot.reserve(entries.size());
    for (const auto& entry : entries) {
        snapshot.push_back(CachedDirectorySnapshotEntry{entry.d_name, entry.d_type});
    }
    return snapshot;
}

ValueDirectoryEntries RebuildValueDirectoryEntries(const CachedValueDirectoryEntries& snapshot) {
    ValueDirectoryEntries entries;
    entries.reserve(snapshot.size());
    for (const auto& entry : snapshot) {
        entries.emplace_back(entry.name, entry.type);
    }
    return entries;
}

}  // namespace

template <typename Entries>
std::optional<Entries> LookupRootSnapshotCache(
    uint32_t uid, std::string_view path, const std::shared_ptr<const CompiledHideRule>& rule) {
    const SessionToken token = CurrentSessionToken();
    const std::string canonicalPath = std::string(path);
    if (!token || canonicalPath.empty()) {
        return std::nullopt;
    }

    const uint64_t fingerprint = EffectiveHideRuleFingerprint(rule);
    const RootSnapshotCacheKey key{token, uid, fingerprint, canonicalPath};
    std::lock_guard<std::mutex> lock(gRootSnapshotCacheMutex);
    const uint64_t configGeneration = gHideConfigGeneration.load(std::memory_order_acquire);
    const uint64_t packageSetGeneration = gUidPackageSetGeneration.load(std::memory_order_acquire);
    const uint64_t parentGeneration = gRootSnapshotParentGeneration.load(std::memory_order_acquire);
    const auto it = gRootSnapshotCache.find(key);
    if (it == gRootSnapshotCache.end()) {
        DebugLogPrint(4,
                      "cache root_snapshot miss uid=%u path=%s fingerprint=%llu reason=empty "
                      "generation=%llu package_generation=%llu parent_generation=%llu",
                      static_cast<unsigned>(uid), DebugPreview(canonicalPath).c_str(),
                      static_cast<unsigned long long>(fingerprint),
                      static_cast<unsigned long long>(configGeneration),
                      static_cast<unsigned long long>(packageSetGeneration),
                      static_cast<unsigned long long>(parentGeneration));
        return std::nullopt;
    }
    const auto snapshot = [&]() {
        if constexpr (std::is_same_v<Entries, DirectoryEntries>) {
            return it->second.sharedEntries;
        } else {
            return it->second.valueSnapshot;
        }
    }();
    if (it->second.configGeneration != configGeneration ||
        it->second.packageSetGeneration != packageSetGeneration ||
        it->second.parentGeneration != parentGeneration || snapshot == nullptr) {
        DebugLogPrint(4,
                      "cache root_snapshot miss uid=%u path=%s fingerprint=%llu reason=stale "
                      "cached_generation=%llu live_generation=%llu "
                      "cached_package_generation=%llu live_package_generation=%llu "
                      "cached_parent_generation=%llu live_parent_generation=%llu",
                      static_cast<unsigned>(uid), DebugPreview(canonicalPath).c_str(),
                      static_cast<unsigned long long>(fingerprint),
                      static_cast<unsigned long long>(it->second.configGeneration),
                      static_cast<unsigned long long>(configGeneration),
                      static_cast<unsigned long long>(it->second.packageSetGeneration),
                      static_cast<unsigned long long>(packageSetGeneration),
                      static_cast<unsigned long long>(it->second.parentGeneration),
                      static_cast<unsigned long long>(parentGeneration));
        gRootSnapshotCache.erase(it);
        return std::nullopt;
    }
    if (!CompiledHideRulesSemanticallyEqual(rule, it->second.rule)) {
        DebugLogPrint(4,
                      "cache root_snapshot miss uid=%u path=%s fingerprint=%llu reason=collision",
                      static_cast<unsigned>(uid), DebugPreview(canonicalPath).c_str(),
                      static_cast<unsigned long long>(fingerprint));
        return std::nullopt;
    }
    DebugLogPrint(4,
                  "cache root_snapshot hit uid=%u path=%s fingerprint=%llu entries=%zu "
                  "generation=%llu package_generation=%llu parent_generation=%llu",
                  static_cast<unsigned>(uid), DebugPreview(canonicalPath).c_str(),
                  static_cast<unsigned long long>(fingerprint), snapshot->size(),
                  static_cast<unsigned long long>(configGeneration),
                  static_cast<unsigned long long>(packageSetGeneration),
                  static_cast<unsigned long long>(parentGeneration));
    if constexpr (std::is_same_v<Entries, DirectoryEntries>) {
        return CloneDirectoryEntries(*snapshot);
    } else {
        // Cache only the semantic (name,type) payload for the value-vector ABI, then rebuild a
        // fresh return object on every hit so the cache never owns a target-facing container
        // instance.
        return RebuildValueDirectoryEntries(*snapshot);
    }
}

template <typename Entries>
void MaybeStoreRootSnapshotCache(uint32_t uid, std::string_view path,
                                 const std::shared_ptr<const CompiledHideRule>& rule,
                                 const Entries& entries) {
    const SessionToken token = CurrentSessionToken();
    const std::string canonicalPath = std::string(path);
    if (!token || canonicalPath.empty() || !CanCacheRootSnapshotEntries(entries)) {
        return;
    }

    const uint64_t fingerprint = EffectiveHideRuleFingerprint(rule);
    const uint64_t configGeneration = gHideConfigGeneration.load(std::memory_order_acquire);
    const uint64_t packageSetGeneration = gUidPackageSetGeneration.load(std::memory_order_acquire);
    const uint64_t parentGeneration = gRootSnapshotParentGeneration.load(std::memory_order_acquire);
    auto snapshot = [&]() {
        if constexpr (std::is_same_v<Entries, DirectoryEntries>) {
            return std::make_shared<const Entries>(CloneDirectoryEntries(entries));
        } else {
            return std::make_shared<const CachedValueDirectoryEntries>(
                SnapshotValueDirectoryEntries(entries));
        }
    }();

    std::lock_guard<std::mutex> lock(gRootSnapshotCacheMutex);
    // Recheck all generations after the expensive directory read/filter work so a concurrent rule
    // or root mutation change becomes a miss instead of storing stale filtered entries.
    if (gHideConfigGeneration.load(std::memory_order_acquire) != configGeneration ||
        gUidPackageSetGeneration.load(std::memory_order_acquire) != packageSetGeneration ||
        gRootSnapshotParentGeneration.load(std::memory_order_acquire) != parentGeneration) {
        return;
    }
    RootSnapshotCacheEntry cacheEntry{
        configGeneration, packageSetGeneration, parentGeneration, rule, nullptr, nullptr};
    if constexpr (std::is_same_v<Entries, DirectoryEntries>) {
        cacheEntry.sharedEntries = std::move(snapshot);
    } else {
        cacheEntry.valueSnapshot = std::move(snapshot);
    }
    gRootSnapshotCache.insert_or_assign(
        RootSnapshotCacheKey{token, uid, fingerprint, canonicalPath}, std::move(cacheEntry));
    DebugLogPrint(4,
                  "cache root_snapshot store uid=%u path=%s fingerprint=%llu entries=%zu "
                  "generation=%llu package_generation=%llu parent_generation=%llu",
                  static_cast<unsigned>(uid), DebugPreview(canonicalPath).c_str(),
                  static_cast<unsigned long long>(fingerprint), entries.size(),
                  static_cast<unsigned long long>(configGeneration),
                  static_cast<unsigned long long>(packageSetGeneration),
                  static_cast<unsigned long long>(parentGeneration));
}

namespace {

void ClearRootSnapshotCacheForSessionImpl(SessionToken token) {
    if (!token)
        return;
    std::lock_guard<std::mutex> lock(gRootSnapshotCacheMutex);
    for (auto it = gRootSnapshotCache.begin(); it != gRootSnapshotCache.end();) {
        if (it->first.session == token)
            it = gRootSnapshotCache.erase(it);
        else
            ++it;
    }
}

}  // namespace

void BumpRootSnapshotParentGeneration(const char* reason) {
    uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(gRootSnapshotCacheMutex);
        generation = gRootSnapshotParentGeneration.fetch_add(1, std::memory_order_acq_rel) + 1;
        gRootSnapshotCache.clear();
    }
    DebugLogPrint(4, "root snapshot invalidated reason=%s generation=%llu", reason,
                  static_cast<unsigned long long>(generation));
}

// FUSE mutation handlers run before the final reply is known. Defer root snapshot invalidation
// until reply_err sees a successful completion so failed operations keep the old snapshot alive.
void TrackPendingRootSnapshotMutation(fuse_req_t req, const char* opName) {
    if (req == nullptr) {
        return;
    }
    const auto token = CurrentSessionToken();
    if (!token || req->se != token.se)
        return;
    std::lock_guard<std::mutex> lock(gPendingRootSnapshotMutationsMutex);
    gPendingRootSnapshotMutations[{token.se, token.generation, req->unique}] = opName;
}

std::optional<const char*> TakePendingRootSnapshotMutation(fuse_req_t req) {
    if (req == nullptr) {
        return std::nullopt;
    }
    const auto token = CurrentSessionToken();
    if (!token || req->se != token.se)
        return std::nullopt;
    std::lock_guard<std::mutex> lock(gPendingRootSnapshotMutationsMutex);
    const auto it = gPendingRootSnapshotMutations.find({token.se, token.generation, req->unique});
    if (it == gPendingRootSnapshotMutations.end()) {
        return std::nullopt;
    }
    const char* opName = it->second;
    gPendingRootSnapshotMutations.erase(it);
    return opName;
}

void ClearRootSnapshotCacheForSession(SessionToken token) {
    ClearRootSnapshotCacheForSessionImpl(token);
}

void ClearPendingRootSnapshotMutations(SessionToken token) {
    if (!token)
        return;
    std::lock_guard<std::mutex> lock(gPendingRootSnapshotMutationsMutex);
    for (auto it = gPendingRootSnapshotMutations.begin();
         it != gPendingRootSnapshotMutations.end();) {
        if (std::get<0>(it->first) == token.se && std::get<1>(it->first) == token.generation) {
            it = gPendingRootSnapshotMutations.erase(it);
        } else {
            ++it;
        }
    }
}

void ClearPendingRootSnapshotMutations() {
    std::lock_guard<std::mutex> lock(gPendingRootSnapshotMutationsMutex);
    gPendingRootSnapshotMutations.clear();
}

void ClearRootSnapshotCache() {
    std::lock_guard<std::mutex> lock(gRootSnapshotCacheMutex);
    gRootSnapshotCache.clear();
}

// Only the two MediaProvider DirectoryEntries ABIs are supported by this cache.
// Keep implementations here so no cache state is exposed to FUSE wrappers.
template std::optional<DirectoryEntries> LookupRootSnapshotCache<DirectoryEntries>(
    uint32_t, std::string_view, const std::shared_ptr<const CompiledHideRule>&);
template std::optional<ValueDirectoryEntries> LookupRootSnapshotCache<ValueDirectoryEntries>(
    uint32_t, std::string_view, const std::shared_ptr<const CompiledHideRule>&);
template void MaybeStoreRootSnapshotCache<DirectoryEntries>(
    uint32_t, std::string_view, const std::shared_ptr<const CompiledHideRule>&,
    const DirectoryEntries&);
template void MaybeStoreRootSnapshotCache<ValueDirectoryEntries>(
    uint32_t, std::string_view, const std::shared_ptr<const CompiledHideRule>&,
    const ValueDirectoryEntries&);

}  // namespace fusehide
