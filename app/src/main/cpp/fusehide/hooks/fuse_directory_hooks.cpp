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
#include "fusehide/hooks/wrappers.hpp"

#include "fusehide/core/root_snapshot_cache.hpp"
#include "fusehide/filters/dirent_filter.hpp"
#include "fusehide/policy/path_policy.hpp"

namespace fusehide {
namespace {

std::string ReadDirectoryPathFromDir(DIR* dirp) {
    if (dirp == nullptr) {
        return {};
    }
    const int fd = dirfd(dirp);
    if (fd < 0) {
        return {};
    }
    char procPath[64];
    std::snprintf(procPath, sizeof(procPath), "/proc/self/fd/%d", fd);
    char resolved[PATH_MAX];
    const ssize_t len = readlink(procPath, resolved, sizeof(resolved) - 1);
    if (len <= 0) {
        return {};
    }
    resolved[len] = '\0';
    return std::string(resolved, static_cast<size_t>(len));
}

}  // namespace

bool ShouldFilterDirectoryEntry(uint32_t uid, std::string_view parentPath, std::string_view name) {
    return !name.empty() && name[0] != '/' &&
           HiddenPathPolicy::ShouldHideTestPath(
               uid, HiddenPathPolicy::JoinPathComponent(parentPath, name));
}

// MediaProviderWrapper::GetDirectoryEntries() appends lower-fs directory names after the Java-side
// list is fetched, so root entry hiding must also filter the native vector here.
// AOSP references: jni/MediaProviderWrapper.cpp#373 and jni/FuseDaemon.cpp#1882
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/MediaProviderWrapper.cpp#373
// https://android.googlesource.com/platform/packages/providers/MediaProvider/+/refs/heads/android14-release/jni/FuseDaemon.cpp#1882
//
// Supplied API 37 DEV sample Ghidra evidence (arm64 image base 0x00100000):
// GetDirectoryEntries @ 0x001749e0 and addDirectoryEntriesFromLowerFs @ 0x00176620 operate on
// vector<DirectoryEntry>, while do_readdir_common @ 0x0016a9d0 advances elements by 0x20 and reads
// d_type at +0x18. The legacy AOSP path above advances vector<shared_ptr<DirectoryEntry>> elements,
// so retain separate overloads rather than interpreting one representation as the other.
DirectoryEntries FilterHiddenDirectoryEntries(uint32_t uid, std::string_view parentPath,
                                              DirectoryEntries entries) {
    if (!HiddenPathPolicy::IsTestHiddenUid(uid) || entries.empty()) {
        return entries;
    }

    const size_t before = entries.size();
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [&](const auto& entry) {
                                     if (!entry) {
                                         return false;
                                     }
                                     const std::string& name = entry->d_name;
                                     return ShouldFilterDirectoryEntry(uid, parentPath, name);
                                 }),
                  entries.end());

    if (entries.size() != before) {
        DebugLogPrint(4, "filter dir entries uid=%u parent=%s removed=%zu remaining=%zu",
                      static_cast<unsigned>(uid), DebugPreview(parentPath).c_str(),
                      before - entries.size(), entries.size());
    }
    return entries;
}

ValueDirectoryEntries FilterHiddenDirectoryEntries(uint32_t uid, std::string_view parentPath,
                                                   ValueDirectoryEntries entries) {
    if (!HiddenPathPolicy::IsTestHiddenUid(uid) || entries.empty()) {
        return entries;
    }

    const size_t before = entries.size();
    size_t firstHidden = 0;
    while (firstHidden < before &&
           !ShouldFilterDirectoryEntry(uid, parentPath, entries[firstHidden].d_name)) {
        ++firstHidden;
    }
    if (firstHidden == before) {
        return entries;
    }

    // Build a new value vector instead of applying the legacy shared_ptr erase path. The returned
    // object is handed back across the platform libc++ boundary and must retain 0x20-byte elements.
    ValueDirectoryEntries filtered;
    filtered.reserve(before - 1);
    for (size_t index = 0; index < firstHidden; ++index) {
        const auto& entry = entries[index];
        filtered.emplace_back(entry.d_name, entry.d_type);
    }
    for (size_t index = firstHidden + 1; index < before; ++index) {
        const auto& entry = entries[index];
        if (!ShouldFilterDirectoryEntry(uid, parentPath, entry.d_name)) {
            filtered.emplace_back(entry.d_name, entry.d_type);
        }
    }
    if (filtered.size() != before) {
        DebugLogPrint(4, "filter value dir entries uid=%u parent=%s removed=%zu remaining=%zu",
                      static_cast<unsigned>(uid), DebugPreview(parentPath).c_str(),
                      before - filtered.size(), filtered.size());
    }
    return filtered;
}

template <typename Entries, typename OriginalFn>
Entries WrappedGetDirectoryEntriesForAbi(OriginalFn fn, void* wrapper, uint32_t uid,
                                         AbiStringParam pathArg, DIR* dirp) {
    const std::string_view path = AbiStringView(pathArg);
    // Cache only the already-filtered visible root listing. Nested directories still rely on the
    // normal tracked-path invalidation path because their visibility is not just a root child set.
    const std::shared_ptr<const CompiledHideRule> rule =
        IsVisibleRootPath(path) ? ResolveHideRuleForUid(uid) : nullptr;
    if (gCurrentReaddirReqUnique != 0) {
        UpdateActiveReaddirPath(gCurrentReaddirReqUnique, path);
        if (gActiveReaddirContext.depth != 0 &&
            gActiveReaddirContext.reqUnique == gCurrentReaddirReqUnique) {
            DebugLogPrint(4, "record readdir path req=%lu ino=%s path=%s",
                          (unsigned long)gCurrentReaddirReqUnique,
                          InodePath(gActiveReaddirContext.pending.ino).c_str(),
                          DebugPreview(path).c_str());
        }
    }

    if (dirp != nullptr) {
        if (auto cachedEntries =
                LookupRootSnapshotCache<Entries>(uid, CanonicalRootSnapshotPath(path), rule);
            cachedEntries.has_value()) {
            if constexpr (kEnableDebugHooks) {
                if (IsVisibleRootPath(path)) {
                    DebugLogPrint(4,
                                  "hide-diag get_dir req=%llu uid=%u source=cache entries=%zu "
                                  "path=%s",
                                  static_cast<unsigned long long>(gCurrentReaddirReqUnique),
                                  static_cast<unsigned>(uid), cachedEntries->size(),
                                  DebugPreview(path).c_str());
                }
            }
            return std::move(*cachedEntries);
        }
    }

    Entries entries;
    {
        const ScopedPathPolicyContext scopedPath(uid, path);
        entries = fn ? fn(wrapper, uid, pathArg, dirp) : Entries();
    }
    const size_t rawCount = entries.size();
    entries = FilterHiddenDirectoryEntries(uid, path, std::move(entries));
    if constexpr (kEnableDebugHooks) {
        if (IsVisibleRootPath(path) || (rawCount != entries.size())) {
            DebugLogPrint(4,
                          "hide-diag get_dir req=%llu uid=%u source=original dirp=%d fn=%d "
                          "raw=%zu filtered=%zu path=%s",
                          static_cast<unsigned long long>(gCurrentReaddirReqUnique),
                          static_cast<unsigned>(uid), dirp != nullptr ? 1 : 0, fn ? 1 : 0, rawCount,
                          entries.size(), DebugPreview(path).c_str());
        }
    }
    if (dirp != nullptr && fn != nullptr) {
        MaybeStoreRootSnapshotCache(uid, CanonicalRootSnapshotPath(path), rule, entries);
    }
    return entries;
}

DirectoryEntries WrappedGetDirectoryEntriesShared(void* wrapper, uint32_t uid,
                                                  AbiStringParam pathArg, DIR* dirp) {
    const auto fn = gOriginalGetDirectoryEntries.getAs<GetDirectoryEntriesFn>();
    return WrappedGetDirectoryEntriesForAbi<DirectoryEntries>(fn, wrapper, uid, pathArg, dirp);
}

ValueDirectoryEntries WrappedGetDirectoryEntriesValue(void* wrapper, uint32_t uid,
                                                      AbiStringParam pathArg, DIR* dirp) {
    const auto fn = gOriginalGetDirectoryEntries.getAs<GetValueDirectoryEntriesFn>();
    return WrappedGetDirectoryEntriesForAbi<ValueDirectoryEntries>(fn, wrapper, uid, pathArg, dirp);
}

template <typename Entries, typename OriginalFn>
void WrappedAddDirectoryEntriesFromLowerFsForAbi(OriginalFn fn, DIR* dirp,
                                                 LowerFsDirentFilterFn filter, Entries* entries) {
    if (fn == nullptr) {
        return;
    }
    fn(dirp, filter, entries);
    if (entries == nullptr || entries->empty()) {
        return;
    }

    const uint32_t uid = gActivePathPolicyUid;
    if (!HiddenPathPolicy::IsTestHiddenUid(uid)) {
        return;
    }

    std::string parentPath = ReadDirectoryPathFromDir(dirp);
    if (parentPath.empty()) {
        parentPath = gActivePathPolicyPath;
    }
    if (parentPath.empty()) {
        return;
    }

    const size_t before = entries->size();
    *entries = FilterHiddenDirectoryEntries(uid, parentPath, std::move(*entries));
    if (entries->size() != before) {
        DebugLogPrint(4, "filter lower-fs dir entries uid=%u parent=%s removed=%zu remaining=%zu",
                      static_cast<unsigned>(uid), DebugPreview(parentPath).c_str(),
                      before - entries->size(), entries->size());
    }
}

void WrappedAddDirectoryEntriesFromLowerFsShared(DIR* dirp, LowerFsDirentFilterFn filter,
                                                 DirectoryEntries* entries) {
    const auto fn =
        gOriginalAddDirectoryEntriesFromLowerFs.getAs<AddDirectoryEntriesFromLowerFsFn>();
    WrappedAddDirectoryEntriesFromLowerFsForAbi(fn, dirp, filter, entries);
}

void WrappedAddDirectoryEntriesFromLowerFsValue(DIR* dirp, LowerFsDirentFilterFn filter,
                                                ValueDirectoryEntries* entries) {
    const auto fn =
        gOriginalAddDirectoryEntriesFromLowerFs.getAs<AddValueDirectoryEntriesFromLowerFsFn>();
    WrappedAddDirectoryEntriesFromLowerFsForAbi(fn, dirp, filter, entries);
}

}  // namespace fusehide
