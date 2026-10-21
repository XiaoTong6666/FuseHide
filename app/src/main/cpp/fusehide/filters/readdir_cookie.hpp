// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>

namespace fusehide {

// Extract the next FUSE offset after a fully filtered readdirplus page.
// The Android MediaProvider fuse_entry_out wire layout is 128 bytes and
// fuse_dirent (before name) is 24 bytes. Never trust a partial wire record.
inline std::optional<int64_t> LastReaddirplusCookie(std::span<const char> page,
                                                    int64_t previousOffset) {
    constexpr size_t kEntryOut = 128;
    constexpr size_t kDirentHeader = 24;
    constexpr size_t kHeader = kEntryOut + kDirentHeader;
    if (previousOffset < 0 || page.size() < kHeader)
        return std::nullopt;
    size_t cursor = 0;
    uint64_t last = static_cast<uint64_t>(previousOffset);
    while (cursor < page.size()) {
        const size_t remaining = page.size() - cursor;
        if (remaining < kHeader)
            return std::nullopt;
        uint64_t offset = 0;
        uint32_t nameLen = 0;
        std::memcpy(&offset, page.data() + cursor + kEntryOut + 8, sizeof(offset));
        std::memcpy(&nameLen, page.data() + cursor + kEntryOut + 16, sizeof(nameLen));
        if (nameLen == 0 || offset <= last ||
            offset > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
            return std::nullopt;
        const size_t alignedName = (static_cast<size_t>(nameLen) + 7) & ~size_t(7);
        if (nameLen > remaining - kHeader || alignedName > remaining - kHeader)
            return std::nullopt;
        cursor += kHeader + alignedName;
        last = offset;
    }
    return static_cast<int64_t>(last);
}
}  // namespace fusehide
