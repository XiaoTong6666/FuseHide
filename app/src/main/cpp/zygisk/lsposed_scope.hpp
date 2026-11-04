// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace fusehide::zygisk {

enum class LsposedScope : int8_t { kUnknown = -1, kDisabled = 0, kEnabled = 1 };

LsposedScope QueryLsposedScope(const char* sqliteLibrary, const char* database, int userId,
                               bool googleMediaProvider);

}  // namespace fusehide::zygisk
