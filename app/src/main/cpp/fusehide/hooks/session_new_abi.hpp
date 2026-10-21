// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <string_view>

namespace fusehide {

enum class SessionNewAbi {
    Unsupported,
    FourArgs,
    FiveArgs,
};

inline constexpr std::string_view kSessionNewFourArgSymbol = "fuse_session_new";
inline constexpr std::string_view kSessionNewFiveArgSymbol = "_fuse_session_new_317";

// Select from the importer's actual ELF relocation slots, not its CPU architecture
// or the presence of an unused symbol in .dynsym. Ambiguous or missing imports
// must never be dispatched using a guessed function signature.
constexpr SessionNewAbi ResolveSessionNewAbi(size_t fourArgSlots, size_t fiveArgSlots) {
    if (fourArgSlots == 1 && fiveArgSlots == 0)
        return SessionNewAbi::FourArgs;
    if (fourArgSlots == 0 && fiveArgSlots == 1)
        return SessionNewAbi::FiveArgs;
    return SessionNewAbi::Unsupported;
}

}  // namespace fusehide
