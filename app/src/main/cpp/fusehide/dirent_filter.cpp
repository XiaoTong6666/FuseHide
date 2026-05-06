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

#include "dirent_filter.hpp"

namespace fusehide {

size_t AlignDirentName(size_t nameLen) {
    return (nameLen + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1);
}

size_t FuseDirentRecordSize(const fuse_dirent* dirent) {
    return offsetof(fuse_dirent, name) + AlignDirentName(dirent->namelen);
}

size_t FuseDirentplusRecordSize(const fuse_dirent* dirent) {
    return kFuseEntryOutWireSize + offsetof(fuse_dirent, name) + AlignDirentName(dirent->namelen);
}

bool ShouldFilterTrackedHiddenDirentInode(uint32_t uid, uint64_t childIno, std::string_view name) {
    if (!HiddenPathPolicy::IsTestHiddenUid(uid) || childIno == 0) {
        return false;
    }
    if (!IsTrackedHiddenSubtreeInode(childIno)) {
        return false;
    }
    DebugLogPrint(4, "filter dirent by tracked inode uid=%u child=%s name=%s",
                  static_cast<unsigned>(uid), InodePath(childIno).c_str(),
                  DebugPreview(name).c_str());
    return true;
}

bool ShouldFilterDirentForParentPath(uint32_t uid, std::string_view parentPath, uint64_t childIno,
                                     std::string_view name) {
    if (!HiddenPathPolicy::IsTestHiddenUid(uid)) {
        return false;
    }
    // Prefer exact inode matches when a hidden child inode is already known, then fall back to
    // exact path matching for the recovered visible parent directory.
    if (ShouldFilterTrackedHiddenDirentInode(uid, childIno, name)) {
        return true;
    }
    const std::string childPath = HiddenPathPolicy::JoinPathComponent(parentPath, name);
    return HiddenPathPolicy::IsExactHiddenTargetPath(childPath);
}

bool DirentFilter::BuildFilteredDirentPayload(const char* data, size_t size, uint32_t uid,
                                              uint64_t ino, std::vector<char>* out,
                                              size_t* removedCount, bool requireParentMatch) {
    if (data == nullptr || size == 0 || out == nullptr || removedCount == nullptr) {
        return false;
    }

    out->clear();
    out->reserve(size);
    size_t offset = 0;
    size_t removed = 0;
    while (offset + offsetof(fuse_dirent, name) <= size) {
        const auto* dirent = reinterpret_cast<const fuse_dirent*>(data + offset);
        const size_t recordSize = FuseDirentRecordSize(dirent);
        if (recordSize == 0 || offset + recordSize > size) {
            return false;
        }
        const std::string_view name(dirent->name, dirent->namelen);
        if (ShouldFilterTrackedHiddenDirentInode(uid, dirent->ino, name) ||
            ShouldFilterHiddenRootDirent(uid, ino, name, requireParentMatch)) {
            removed++;
        } else {
            out->insert(out->end(), data + offset, data + offset + recordSize);
        }
        offset += recordSize;
    }
    if (offset != size) {
        return false;
    }
    *removedCount = removed;
    return removed != 0;
}

bool BuildFilteredDirentPayloadForParentPath(const char* data, size_t size, uint32_t uid,
                                             std::string_view parentPath, std::vector<char>* out,
                                             size_t* removedCount,
                                             std::vector<FilteredDirentMatch>* removedEntries) {
    if (data == nullptr || size == 0 || out == nullptr || removedCount == nullptr ||
        parentPath.empty()) {
        return false;
    }

    out->clear();
    out->reserve(size);
    size_t offset = 0;
    size_t removed = 0;
    while (offset + offsetof(fuse_dirent, name) <= size) {
        const auto* dirent = reinterpret_cast<const fuse_dirent*>(data + offset);
        const size_t recordSize = FuseDirentRecordSize(dirent);
        if (recordSize == 0 || offset + recordSize > size) {
            return false;
        }
        const std::string_view name(dirent->name, dirent->namelen);
        if (ShouldFilterDirentForParentPath(uid, parentPath, dirent->ino, name)) {
            removed++;
            if (removedEntries != nullptr) {
                removedEntries->push_back(FilteredDirentMatch{std::string(name), dirent->ino});
            }
        } else {
            out->insert(out->end(), data + offset, data + offset + recordSize);
        }
        offset += recordSize;
    }
    if (offset != size) {
        return false;
    }
    *removedCount = removed;
    return removed != 0;
}

bool DirentFilter::BuildFilteredDirentplusPayload(const char* data, size_t size, uint32_t uid,
                                                  uint64_t ino, std::vector<char>* out,
                                                  size_t* removedCount, bool requireParentMatch) {
    if (data == nullptr || size == 0 || out == nullptr || removedCount == nullptr) {
        return false;
    }

    out->clear();
    out->reserve(size);
    size_t offset = 0;
    size_t removed = 0;
    while (offset + kFuseEntryOutWireSize + offsetof(fuse_dirent, name) <= size) {
        const auto* dirent =
            reinterpret_cast<const fuse_dirent*>(data + offset + kFuseEntryOutWireSize);
        const size_t recordSize = FuseDirentplusRecordSize(dirent);
        if (recordSize == 0 || offset + recordSize > size) {
            return false;
        }
        const std::string_view name(dirent->name, dirent->namelen);
        if (ShouldFilterTrackedHiddenDirentInode(uid, dirent->ino, name) ||
            ShouldFilterHiddenRootDirent(uid, ino, name, requireParentMatch)) {
            removed++;
        } else {
            out->insert(out->end(), data + offset, data + offset + recordSize);
        }
        offset += recordSize;
    }
    if (offset != size) {
        return false;
    }
    *removedCount = removed;
    return removed != 0;
}

bool BuildFilteredDirentplusPayloadForParentPath(const char* data, size_t size, uint32_t uid,
                                                 std::string_view parentPath,
                                                 std::vector<char>* out, size_t* removedCount,
                                                 std::vector<FilteredDirentMatch>* removedEntries) {
    if (data == nullptr || size == 0 || out == nullptr || removedCount == nullptr ||
        parentPath.empty()) {
        return false;
    }

    out->clear();
    out->reserve(size);
    size_t offset = 0;
    size_t removed = 0;
    while (offset + kFuseEntryOutWireSize + offsetof(fuse_dirent, name) <= size) {
        const auto* dirent =
            reinterpret_cast<const fuse_dirent*>(data + offset + kFuseEntryOutWireSize);
        const size_t recordSize = FuseDirentplusRecordSize(dirent);
        if (recordSize == 0 || offset + recordSize > size) {
            return false;
        }
        const std::string_view name(dirent->name, dirent->namelen);
        if (ShouldFilterDirentForParentPath(uid, parentPath, dirent->ino, name)) {
            removed++;
            if (removedEntries != nullptr) {
                removedEntries->push_back(FilteredDirentMatch{std::string(name), dirent->ino});
            }
        } else {
            out->insert(out->end(), data + offset, data + offset + recordSize);
        }
        offset += recordSize;
    }
    if (offset != size) {
        return false;
    }
    *removedCount = removed;
    return removed != 0;
}

}  // namespace fusehide
