// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "fusehide/core/state.hpp"

namespace fusehide {

// The FUSE Wrapper checks that this is a visible storage-root path. An empty
// path disables caching; nested directories must never be cached here.
template <typename Entries>
std::optional<Entries> LookupRootSnapshotCache(uint32_t uid, std::string_view canonicalRootPath,
                                               const std::shared_ptr<const CompiledHideRule>& rule);

template <typename Entries>
void MaybeStoreRootSnapshotCache(uint32_t uid, std::string_view canonicalRootPath,
                                 const std::shared_ptr<const CompiledHideRule>& rule,
                                 const Entries& entries);

// Root mutations are invalidated only after a successful FUSE reply.
void BumpRootSnapshotParentGeneration(const char* reason);
void TrackPendingRootSnapshotMutation(fuse_req_t req, const char* opName);
std::optional<const char*> TakePendingRootSnapshotMutation(fuse_req_t req);

}  // namespace fusehide
