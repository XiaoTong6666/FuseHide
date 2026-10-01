// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace fusehide {

using HookInstaller = int (*)(void* target, void* replacement, void** backup);
using HookBackupPublisher = void (*)(void* user_data, void* original);
using StrictHookInstaller = int (*)(void* target, void* replacement, void* user_data,
                                    HookBackupPublisher publish);

// Legacy Native API v2 ABI: no extra fields and no changed offsets.
struct NativeApiEntries {
    uint32_t version;
    HookInstaller hookFunc;
    int (*unhookFunc)(void*);
};

// Only native_init_v3 receives this extension. Its first member is an
// unchanged v2-layout prefix; old native_init never reads past that prefix.
struct NativeApiEntriesV3 {
    NativeApiEntries base;
    uint32_t struct_size;
    uint32_t reserved;
    StrictHookInstaller hookWithPublication;
};
static_assert(std::is_standard_layout_v<NativeApiEntriesV3>);
static_assert(offsetof(NativeApiEntriesV3, base) == 0);
static_assert(sizeof(NativeApiEntriesV3::base) == sizeof(NativeApiEntries));

}  // namespace fusehide
