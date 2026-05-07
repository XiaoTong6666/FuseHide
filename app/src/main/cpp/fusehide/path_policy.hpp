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

#include "state.hpp"

namespace fusehide {

class HiddenPathPolicy final {
   public:
    static bool IsTestHiddenUid(uint32_t uid);
    static bool ShouldHideTestPath(uint32_t uid, std::string_view path);
    static bool IsConfiguredHiddenRootEntryName(std::string_view name);
    static bool IsHiddenRootEntryName(std::string_view name);
    static bool IsAnyHiddenSubtreePath(std::string_view path);
    static bool IsExactHiddenTargetPath(std::string_view path);
    static bool IsHiddenRootDirectoryPath(std::string_view path);
    static std::string JoinPathComponent(std::string_view parent, std::string_view child);
    static bool ShouldFilterHiddenRootDirent(uint32_t uid, uint64_t ino, std::string_view name,
                                             bool requireParentMatch);
};

std::string NormalizeRelativeHiddenPath(std::string_view path);
std::optional<std::string> RelativePathForVisibleRoot(std::string_view path);
bool MatchesRelativeHiddenPathList(std::string_view relativePath, bool exactOnly);
bool IsWildcardRootEntryCandidate(std::string_view name);
bool ShouldHideWildcardRootEntryByParent(uint64_t parent, uint64_t rootParent,
                                         std::string_view name);
bool IsParentOfExactHiddenTargetPath(std::string_view path);

}  // namespace fusehide
