/*
 * Copyright (C) 2026 XiaoTong6666
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

namespace fusehide::testing {

// Reject unrecognized callback layouts without relying on a private fuse_session offset.
// This only validates a candidate; callers must separately establish memory lifetime and
// concurrency safety before modifying any callback pointer.
template <size_t MaxCallbacks>
std::optional<std::array<size_t, MaxCallbacks>> LocateCallbackTable(
    std::span<const std::byte> allocation, const std::array<void*, MaxCallbacks>& originals,
    const std::array<void*, MaxCallbacks>& replacements, size_t required, size_t maxTableBytes) {
    if (required == 0 || required > MaxCallbacks || maxTableBytes == 0 ||
        allocation.size() < maxTableBytes || allocation.size() % sizeof(void*) != 0 ||
        maxTableBytes % sizeof(void*) != 0)
        return std::nullopt;

    std::array<size_t, MaxCallbacks> positions{};
    positions.fill(SIZE_MAX);
    for (size_t slot = 0; slot < allocation.size() / sizeof(void*); ++slot) {
        void* value = nullptr;
        std::memcpy(&value, allocation.data() + slot * sizeof(void*), sizeof(value));
        for (size_t n = 0; n < required; ++n) {
            if (!originals[n] || !replacements[n] || originals[n] == replacements[n])
                return std::nullopt;
            if (value != originals[n] && value != replacements[n])
                continue;
            if (positions[n] != SIZE_MAX)
                return std::nullopt;
            positions[n] = slot;
        }
    }
    size_t first = SIZE_MAX;
    size_t last = 0;
    for (size_t n = 0; n < required; ++n) {
        if (positions[n] == SIZE_MAX)
            return std::nullopt;
        first = first < positions[n] ? first : positions[n];
        last = last > positions[n] ? last : positions[n];
    }
    if (last - first >= maxTableBytes / sizeof(void*))
        return std::nullopt;
    return positions;
}
}  // namespace fusehide::testing
