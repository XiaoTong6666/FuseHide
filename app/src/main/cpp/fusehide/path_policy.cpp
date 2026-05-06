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

#include "path_policy.hpp"

namespace fusehide {

namespace {

bool HasNonAsciiByte(std::string_view value) {
    for (unsigned char ch : value) {
        if ((ch & 0x80u) != 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

std::string NormalizeRelativeHiddenPath(std::string_view path) {
    size_t begin = 0;
    size_t end = path.size();
    while (begin < end && path[begin] == '/') {
        begin++;
    }
    while (end > begin && path[end - 1] == '/') {
        end--;
    }
    std::string normalized;
    normalized.reserve(end - begin);
    bool previousSlash = false;
    for (size_t i = begin; i < end; ++i) {
        const char ch = path[i];
        if (ch == '/') {
            if (previousSlash) {
                continue;
            }
            previousSlash = true;
        } else {
            previousSlash = false;
        }
        normalized.push_back(ch);
    }
    return normalized;
}

std::optional<std::string> RelativePathForVisibleRoot(std::string_view path) {
    for (const auto& root : kVisibleStorageRoots) {
        if (path == root) {
            return std::string();
        }
        if (path.size() > root.size() && path.compare(0, root.size(), root) == 0 &&
            path[root.size()] == '/') {
            return NormalizeRelativeHiddenPath(path.substr(root.size() + 1));
        }
    }
    return std::nullopt;
}

bool MatchesRelativeHiddenPathList(std::string_view relativePath, bool exactOnly) {
    const std::string normalized = NormalizeRelativeHiddenPath(relativePath);
    if (normalized.empty()) {
        return false;
    }
    const auto config = CurrentHideConfig();
    for (const auto& configuredPath : config->hiddenRelativePaths) {
        const std::string candidate = NormalizeRelativeHiddenPath(configuredPath);
        if (candidate.empty()) {
            continue;
        }
        if (normalized == candidate) {
            return true;
        }
        if (!exactOnly && normalized.size() > candidate.size() &&
            normalized.compare(0, candidate.size(), candidate) == 0 &&
            normalized[candidate.size()] == '/') {
            return true;
        }
    }
    return false;
}

bool IsWildcardRootEntryCandidate(std::string_view name) {
    if (name.empty() || name == "." || name == "..") {
        return false;
    }
    if (name.find('/') != std::string_view::npos) {
        return false;
    }
    const auto config = CurrentHideConfig();
    for (const auto& exemptEntry : config->hideAllRootEntriesExemptions) {
        if (name == exemptEntry) {
            return false;
        }
    }
    return true;
}

bool ShouldHideWildcardRootEntryByParent(uint64_t parent, uint64_t rootParent,
                                         std::string_view name) {
    return CurrentHideConfig()->enableHideAllRootEntries && rootParent != 0 &&
           parent == rootParent && IsWildcardRootEntryCandidate(name);
}

bool HiddenPathPolicy::IsConfiguredHiddenRootEntryName(std::string_view name) {
    const auto config = CurrentHideConfig();
    for (const auto& rootEntryName : config->hiddenRootEntryNames) {
        if (name == rootEntryName) {
            return true;
        }
    }

    if (!HasNonAsciiByte(name)) {
        return false;
    }

    std::string sanitized(name);
    if (!UnicodePolicy::NeedsSanitization(sanitized)) {
        return false;
    }
    UnicodePolicy::RewriteString(sanitized);

    for (const auto& rootEntryName : config->hiddenRootEntryNames) {
        if (sanitized == rootEntryName) {
            return true;
        }
    }
    return false;
}

bool HiddenPathPolicy::IsHiddenRootEntryName(std::string_view name) {
    return IsConfiguredHiddenRootEntryName(name) ||
           (CurrentHideConfig()->enableHideAllRootEntries && IsWildcardRootEntryCandidate(name));
}

bool HiddenPathPolicy::IsAnyHiddenSubtreePath(std::string_view path) {
    if (const auto relativePath = RelativePathForVisibleRoot(path);
        relativePath.has_value() && MatchesRelativeHiddenPathList(*relativePath, false)) {
        return true;
    }
    for (const auto& root : kVisibleStorageRoots) {
        if (path.size() <= root.size() || path.compare(0, root.size(), root) != 0 ||
            path[root.size()] != '/') {
            continue;
        }

        const size_t componentStart = root.size() + 1;
        const size_t slashPos = path.find('/', componentStart);
        const size_t componentEnd = slashPos == std::string_view::npos ? path.size() : slashPos;
        if (componentEnd <= componentStart) {
            continue;
        }

        const std::string_view rootEntry =
            path.substr(componentStart, componentEnd - componentStart);
        if (IsHiddenRootEntryName(rootEntry)) {
            return true;
        }
    }
    return false;
}

bool HiddenPathPolicy::IsExactHiddenTargetPath(std::string_view path) {
    if (const auto relativePath = RelativePathForVisibleRoot(path);
        relativePath.has_value() && MatchesRelativeHiddenPathList(*relativePath, true)) {
        return true;
    }
    for (const auto& root : kVisibleStorageRoots) {
        if (path.size() <= root.size() || path.compare(0, root.size(), root) != 0 ||
            path[root.size()] != '/') {
            continue;
        }

        const size_t componentStart = root.size() + 1;
        const size_t slashPos = path.find('/', componentStart);
        if (slashPos != std::string_view::npos) {
            continue;
        }

        const std::string_view rootEntry = path.substr(componentStart);
        if (IsHiddenRootEntryName(rootEntry)) {
            return true;
        }
    }
    return false;
}

bool HiddenPathPolicy::IsHiddenRootDirectoryPath(std::string_view path) {
    for (const auto& root : kVisibleStorageRoots) {
        if (path == root) {
            return true;
        }
    }
    return false;
}

bool IsParentOfExactHiddenTargetPath(std::string_view path) {
    // Root targets and nested relative targets need different list filtering keys. Root-level
    // targets can be recognized by child name alone under /storage/emulated/0, but nested targets
    // need the exact visible parent path so reply_buf can rebuild parentPath + childName.
    for (const auto& root : kVisibleStorageRoots) {
        if (path == root) {
            const auto config = CurrentHideConfig();
            return !config->hiddenRootEntryNames.empty() || config->enableHideAllRootEntries;
        }
    }

    const auto relativePath = RelativePathForVisibleRoot(path);
    if (!relativePath.has_value()) {
        return false;
    }

    const auto config = CurrentHideConfig();
    for (const auto& hiddenRelativePath : config->hiddenRelativePaths) {
        const std::string normalized = NormalizeRelativeHiddenPath(hiddenRelativePath);
        if (normalized.empty()) {
            continue;
        }
        const size_t slash = normalized.rfind('/');
        if (slash == std::string::npos) {
            continue;
        }
        if (*relativePath == normalized.substr(0, slash)) {
            return true;
        }
    }
    return false;
}

std::string HiddenPathPolicy::JoinPathComponent(std::string_view parent, std::string_view child) {
    std::string joined(parent);
    if (joined.empty() || joined.back() != '/') {
        joined.push_back('/');
    }
    joined.append(child.data(), child.size());
    return joined;
}

bool HiddenPathPolicy::ShouldFilterHiddenRootDirent(uint32_t uid, uint64_t ino,
                                                    std::string_view name,
                                                    bool requireParentMatch) {
    if (!IsTestHiddenUid(uid)) {
        return false;
    }

    if (const auto parentPath = LookupTrackedPathForInode(ino); parentPath.has_value()) {
        const std::string childPath = JoinPathComponent(*parentPath, name);
        if (IsExactHiddenTargetPath(childPath)) {
            return true;
        }
    }

    if (!requireParentMatch) {
        // Without a trusted parent inode/path we cannot tell whether this dirent belongs to
        // /storage/emulated/0 or to an exempt subtree such as /storage/emulated/0/Android.
        // Keep the legacy exact-name fallback, but do not apply hide-all wildcard filtering here.
        return IsConfiguredHiddenRootEntryName(name);
    }
    if (!IsHiddenRootEntryName(name)) {
        return false;
    }
    const uint64_t rootParent = gHiddenRootParentInode.load(std::memory_order_relaxed);
    return rootParent == 0 || ino == rootParent;
}

}  // namespace fusehide
