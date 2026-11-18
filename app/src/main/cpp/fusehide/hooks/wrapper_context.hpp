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

#pragma once

#include "fusehide/core/state.hpp"

namespace fusehide {

// One process-wide definition per thread; all wrapper translation units use
// these same request contexts rather than maintaining per-file TLS copies.
extern thread_local uint32_t gActiveCreateUid;
extern thread_local uint32_t gActiveCreateScopeDepth;
extern thread_local uint32_t gActivePathPolicyUid;
extern thread_local std::string_view gActivePathPolicyPath;
extern thread_local bool gCrossUidSensitiveLookup;

struct RenameTracking {
    fuse_req_t req = nullptr;
    bool success = false;
    bool reconciled = false;
    uint64_t sourceParent = 0;
    uint64_t targetParent = 0;
    std::string sourceName;
    std::string targetName;
    uint32_t flags = 0;
    std::string source;
    std::string destination;
};
extern thread_local RenameTracking* gActiveRenameTracking;

struct ActiveReaddirContext {
    uint64_t reqUnique = 0;
    uint32_t depth = 0;
    bool globalBacked = false;
    PendingReaddirContext pending;
};

extern thread_local ActiveReaddirContext gActiveReaddirContext;

#if defined(__x86_64__)
// A wholly filtered readdirplus page is not EOF. Keep exactly one kernel reply
// per request while advancing the original MediaProvider directory cursor.
struct ReaddirplusContinuation {
    fuse_req_t req = nullptr;
    off_t currentOffset = 0;
    off_t nextOffset = 0;
    bool needsNextPage = false;
    bool replySent = false;
};
extern thread_local ReaddirplusContinuation* gReaddirplusContinuation;
#endif

enum class HiddenPathClassification : uint8_t {
    kNone,
    kHiddenRoot,
    kHiddenDescendant,
};

class ScopedPathPolicyContext final {
   public:
    ScopedPathPolicyContext(uint32_t uid, std::string_view path)
        : previousUid_(gActivePathPolicyUid), previousPath_(gActivePathPolicyPath) {
        gActivePathPolicyUid = uid;
        gActivePathPolicyPath = path;
    }

    ~ScopedPathPolicyContext() {
        gActivePathPolicyUid = previousUid_;
        gActivePathPolicyPath = previousPath_;
    }

   private:
    uint32_t previousUid_;
    std::string_view previousPath_;
};

class ScopedFuseRequestSession final {
   public:
    explicit ScopedFuseRequestSession(fuse_req_t req) : previous_(PushFuseRequestSession(req)) {
    }
    ~ScopedFuseRequestSession() {
        RestoreFuseRequestSession(previous_);
    }

   private:
    SessionToken previous_;
};

bool SnapshotActiveReaddirContext(fuse_req_t req, PendingReaddirContext* out);
void UpdateActiveReaddirPath(uint64_t reqUnique, std::string_view path);
void CompletePendingReaddirContext(fuse_req_t req, uint32_t reqUid);

std::string_view TrimTrailingSlashes(std::string_view path);
bool IsVisibleRootPath(std::string_view path);
bool IsPathUnderVisibleRoot(std::string_view path);
std::string CanonicalRootSnapshotPath(std::string_view path);
bool PathHasVisibleRootParent(std::string_view path);
bool ResolveVisibleParentPath(uint64_t parent, std::string* parentPathOut);
bool IsCrossUidSensitiveLookup(uint64_t parent, const char* name);
std::optional<std::string> InferVisibleRootPathFromLookup(uint64_t parent, std::string_view name);
void RememberVisibleRootParentInode(uint64_t parent, const char* reason);
HiddenPathClassification ClassifyHiddenPath(uint32_t uid, std::string_view path);
bool IsHiddenPathClassification(HiddenPathClassification classification);
bool IsExactHiddenPathClassification(HiddenPathClassification classification);

}  // namespace fusehide
