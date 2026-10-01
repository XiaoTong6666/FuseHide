// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include "fusehide/native_hook_api.hpp"

namespace fusehide::zygisk {

int InstallNativeHook(void* target, void* replacement, void** backup);
int InstallNativeHookWithPublication(void* target, void* replacement, void* user_data,
                                     HookBackupPublisher publish);
int UninstallNativeHook(void* target);
const NativeApiEntries& GetNativeHookApi();
const NativeApiEntriesV3& GetStrictNativeHookApi();

}  // namespace fusehide::zygisk
