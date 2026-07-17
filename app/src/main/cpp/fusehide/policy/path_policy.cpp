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

namespace fusehide {

namespace {

void FoldAsciiForMatch(std::string* value) {
    if (value == nullptr) {
        return;
    }
    for (char& ch : *value) {
        const auto byte = static_cast<unsigned char>(ch);
        if (byte < 0x80u) {
            ch = static_cast<char>(std::tolower(byte));
        }
    }
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

std::string CanonicalizeHiddenEntryNameForMatch(std::string_view name) {
    std::string canonical(name);
    UnicodePolicy::RewriteString(canonical);
    FoldAsciiForMatch(&canonical);
    return canonical;
}

std::string CanonicalizeRelativeHiddenPathForMatch(std::string_view path) {
    std::string canonical(path);
    UnicodePolicy::RewriteString(canonical);
    canonical = NormalizeRelativeHiddenPath(canonical);
    if (canonical.empty()) {
        return canonical;
    }
    FoldAsciiForMatch(&canonical);
    return canonical;
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

// Subtree checks are hot enough that the compiled rule keeps both exact and prefix forms ready,
// leaving this helper to normalize only the runtime path being tested.
bool MatchesRelativeHiddenPathList(const CompiledHideRule& rule, std::string_view relativePath,
                                   bool exactOnly) {
    const std::string normalized = CanonicalizeRelativeHiddenPathForMatch(relativePath);
    if (normalized.empty()) {
        return false;
    }
    if (rule.normalizedHiddenRelativePathExactSet.find(normalized) !=
        rule.normalizedHiddenRelativePathExactSet.end()) {
        return true;
    }
    if (!exactOnly) {
        for (const auto& prefix : rule.normalizedHiddenRelativePathPrefixes) {
            if (normalized.size() > prefix.size() &&
                normalized.compare(0, prefix.size(), prefix) == 0) {
                return true;
            }
        }
    }
    return false;
}

bool MatchesRelativeHiddenPathList(std::string_view relativePath, bool exactOnly) {
    const auto rule = RuleForAnyPackage();
    return rule != nullptr && MatchesRelativeHiddenPathList(*rule, relativePath, exactOnly);
}

bool IsWildcardRootEntryCandidate(const CompiledHideRule& rule, std::string_view name) {
    if (name.empty() || name == "." || name == "..") {
        return false;
    }
    if (name.find('/') != std::string_view::npos) {
        return false;
    }
    return rule.hideAllRootEntriesExemptionSet.find(CanonicalizeHiddenEntryNameForMatch(name)) ==
           rule.hideAllRootEntriesExemptionSet.end();
}

bool IsWildcardRootEntryCandidate(std::string_view name) {
    const auto rule = RuleForAnyPackage();
    return rule != nullptr && IsWildcardRootEntryCandidate(*rule, name);
}

bool ShouldHideWildcardRootEntryByParent(uint64_t parent, uint64_t rootParent,
                                         std::string_view name) {
    const auto rule = RuleForAnyPackage();
    return rule != nullptr && rule->enableHideAllRootEntries && rootParent != 0 &&
           parent == rootParent && IsWildcardRootEntryCandidate(*rule, name);
}

namespace {

// Fold runtime names the same way the device's case-insensitive compare path does so root entry
// lookup, stat/open, and descendant filtering agree on the same hidden target.
bool IsConfiguredHiddenRootEntryNameForRule(const CompiledHideRule& rule, std::string_view name) {
    return rule.hiddenRootEntryNameSet.find(CanonicalizeHiddenEntryNameForMatch(name)) !=
           rule.hiddenRootEntryNameSet.end();
}

bool IsHiddenRootEntryNameForRule(const CompiledHideRule& rule, std::string_view name) {
    return IsConfiguredHiddenRootEntryNameForRule(rule, name) ||
           (rule.enableHideAllRootEntries && IsWildcardRootEntryCandidate(rule, name));
}

bool IsAnyHiddenSubtreePathForRule(const CompiledHideRule& rule, std::string_view path) {
    if (const auto relativePath = RelativePathForVisibleRoot(path);
        relativePath.has_value() && MatchesRelativeHiddenPathList(rule, *relativePath, false)) {
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
        if (IsHiddenRootEntryNameForRule(rule, rootEntry)) {
            return true;
        }
    }
    return false;
}

bool IsExactHiddenTargetPathForRule(const CompiledHideRule& rule, std::string_view path) {
    if (const auto relativePath = RelativePathForVisibleRoot(path);
        relativePath.has_value() && MatchesRelativeHiddenPathList(rule, *relativePath, true)) {
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
        if (IsHiddenRootEntryNameForRule(rule, rootEntry)) {
            return true;
        }
    }
    return false;
}

bool IsParentOfExactHiddenTargetPathForRule(const CompiledHideRule& rule, std::string_view path) {
    // Root targets and nested relative targets need different list filtering keys. Root-level
    // targets can be recognized by child name alone under /storage/emulated/0, but nested targets
    // need the exact visible parent path so reply_buf can rebuild parentPath + childName.
    for (const auto& root : kVisibleStorageRoots) {
        if (path == root) {
            return !rule.hiddenRootEntryNames.empty() || rule.enableHideAllRootEntries;
        }
    }

    const auto relativePath = RelativePathForVisibleRoot(path);
    if (!relativePath.has_value()) {
        return false;
    }
    return rule.exactHiddenTargetParentRelativePathSet.find(CanonicalizeRelativeHiddenPathForMatch(
               *relativePath)) != rule.exactHiddenTargetParentRelativePathSet.end();
}

}  // namespace

bool HiddenPathPolicy::IsConfiguredHiddenRootEntryName(std::string_view name) {
    const auto rule = RuleForAnyPackage();
    return rule != nullptr && IsConfiguredHiddenRootEntryNameForRule(*rule, name);
}

bool HiddenPathPolicy::IsConfiguredHiddenRootEntryName(uint32_t uid, std::string_view name) {
    const auto rule = ResolveHideRuleForUid(uid);
    return rule != nullptr && IsConfiguredHiddenRootEntryNameForRule(*rule, name);
}

bool HiddenPathPolicy::IsHiddenRootEntryName(std::string_view name) {
    const auto rule = RuleForAnyPackage();
    return rule != nullptr && IsHiddenRootEntryNameForRule(*rule, name);
}

bool HiddenPathPolicy::IsHiddenRootEntryName(uint32_t uid, std::string_view name) {
    const auto rule = ResolveHideRuleForUid(uid);
    return rule != nullptr && IsHiddenRootEntryNameForRule(*rule, name);
}

bool HiddenPathPolicy::IsAnyHiddenSubtreePath(std::string_view path) {
    const auto rule = RuleForAnyPackage();
    return rule != nullptr && IsAnyHiddenSubtreePathForRule(*rule, path);
}

bool HiddenPathPolicy::IsAnyHiddenSubtreePath(uint32_t uid, std::string_view path) {
    const auto rule = ResolveHideRuleForUid(uid);
    return rule != nullptr && IsAnyHiddenSubtreePathForRule(*rule, path);
}

bool HiddenPathPolicy::IsExactHiddenTargetPath(std::string_view path) {
    const auto rule = RuleForAnyPackage();
    return rule != nullptr && IsExactHiddenTargetPathForRule(*rule, path);
}

bool HiddenPathPolicy::IsExactHiddenTargetPath(uint32_t uid, std::string_view path) {
    const auto rule = ResolveHideRuleForUid(uid);
    return rule != nullptr && IsExactHiddenTargetPathForRule(*rule, path);
}

bool HiddenPathPolicy::IsHiddenRootDirectoryPath(std::string_view path) {
    for (const auto& root : kVisibleStorageRoots) {
        if (path == root) {
            return true;
        }
    }
    return false;
}

bool IsParentOfExactHiddenTargetPath(uint32_t uid, std::string_view path) {
    const auto rule = ResolveHideRuleForUid(uid);
    return rule != nullptr && IsParentOfExactHiddenTargetPathForRule(*rule, path);
}

bool IsParentOfExactHiddenTargetPath(std::string_view path) {
    const auto rule = RuleForAnyPackage();
    return rule != nullptr && IsParentOfExactHiddenTargetPathForRule(*rule, path);
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
    const auto rule = ResolveHideRuleForUid(uid);
    if (rule == nullptr) {
        return false;
    }

    if (const auto parentPath = LookupTrackedPathForInode(ino); parentPath.has_value()) {
        const std::string childPath = JoinPathComponent(*parentPath, name);
        if (IsExactHiddenTargetPathForRule(*rule, childPath)) {
            return true;
        }
    }

    if (!requireParentMatch) {
        // Without a trusted parent inode/path we cannot tell whether this dirent belongs to
        // /storage/emulated/0 or to an exempt subtree such as /storage/emulated/0/Android.
        // Keep the legacy exact-name fallback, but do not apply hide-all wildcard filtering here.
        return IsConfiguredHiddenRootEntryNameForRule(*rule, name);
    }
    if (!IsHiddenRootEntryNameForRule(*rule, name)) {
        return false;
    }
    const uint64_t rootParent = gHiddenRootParentInode.load(std::memory_order_relaxed);
    return rootParent != 0 && ino == rootParent;
}

}  // namespace fusehide
