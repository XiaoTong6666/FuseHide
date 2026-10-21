// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace fusehide {

// The directory root of an emulated-storage FUSE Session is inode 1, while
// the visible /storage/emulated/0 directory is its child. Extract that
// child's *kernel nodeid* only from a verified READDIRPLUS page for inode 1.
// The nodeid is NOT the backing stat.st_ino (and never FUSE_ROOT_ID itself).
inline std::optional<uint64_t> FindEmulatedZeroDirentplusNodeId(std::span<const char> input) {
    constexpr size_t kEntryOut = 128;
    constexpr size_t kDirentHeader = 24;
    constexpr size_t kHeader = kEntryOut + kDirentHeader;
    if (input.empty())
        return std::nullopt;
    size_t at = 0;
    std::optional<uint64_t> found;
    while (at < input.size()) {
        const size_t remaining = input.size() - at;
        if (remaining < kHeader)
            return std::nullopt;
        uint32_t len = 0;
        std::memcpy(&len, input.data() + at + kEntryOut + 16, sizeof(len));
        if (len == 0 || len > 255 || len > remaining - kHeader)
            return std::nullopt;
        const size_t padded = (static_cast<size_t>(len) + 7) & ~size_t{7};
        if (padded > remaining - kHeader)
            return std::nullopt;
        const std::string_view name(input.data() + at + kHeader, len);
        if (name.find('/') != std::string_view::npos || name.find('\0') != std::string_view::npos)
            return std::nullopt;
        if (name == "0") {
            uint64_t nodeid = 0;
            std::memcpy(&nodeid, input.data() + at, sizeof(nodeid));
            if (nodeid <= 1 || found.has_value())
                return std::nullopt;
            found = nodeid;
        }
        at += kHeader + padded;
    }
    return at == input.size() ? found : std::nullopt;
}

// FUSE UAPI: fuse_direntplus = fuse_entry_out (128 bytes) + fuse_dirent.
// entry_valid/attr_valid and their nsec portions begin at offsets 16/24/32/36.
// Called only for known READDIRPLUS buffers; never try to classify arbitrary
// reply_buf bytes as direntplus. First validate the entire payload.
template <typename MatchesName>
bool CopyUncachedSensitiveDirentplus(std::span<const char> input, std::vector<char>* output,
                                     size_t* changed, MatchesName&& matchesName) {
    constexpr size_t kEntryOut = 128;
    constexpr size_t kDirentHeader = 24;
    constexpr size_t kHeader = kEntryOut + kDirentHeader;
    constexpr size_t kValidityOffset = 16;
    constexpr size_t kValiditySize = 24;
    static_assert(kValidityOffset + kValiditySize <= kEntryOut);
    if (input.empty() || output == nullptr || changed == nullptr)
        return false;

    std::vector<size_t> offsets;
    size_t at = 0;
    while (at < input.size()) {
        const size_t remaining = input.size() - at;
        if (remaining < kHeader)
            return false;
        uint32_t len = 0;
        std::memcpy(&len, input.data() + at + kEntryOut + 16, sizeof(len));
        if (len == 0 || len > 255 || len > remaining - kHeader)
            return false;
        const size_t padded = (static_cast<size_t>(len) + 7) & ~size_t{7};
        if (padded > remaining - kHeader)
            return false;
        const std::string_view name(input.data() + at + kHeader, len);
        if (name.find('/') != std::string_view::npos || name.find('\0') != std::string_view::npos)
            return false;
        if (matchesName(name))
            offsets.push_back(at);
        at += kHeader + padded;
    }
    if (at != input.size() || offsets.empty())
        return false;

    output->assign(input.begin(), input.end());
    for (const size_t record : offsets)
        std::memset(output->data() + record + kValidityOffset, 0, kValiditySize);
    *changed = offsets.size();
    return true;
}

}  // namespace fusehide
