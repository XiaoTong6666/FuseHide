// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <array>

namespace fusehide {

// Required lowlevel operations: lookup, lookup_postfilter, getattr,
// readdir, readdirplus, readdir_postfilter.
inline constexpr size_t kRequiredFUSECallbacks = 6;

constexpr bool HasRequiredFUSECallbackCoverage(size_t installed, size_t required) {
    return required == kRequiredFUSECallbacks && installed == required;
}

// Inline callback replacements alone are insufficient. Reply-path filtering,
// notifications and Session retirement must all be installed before reporting
// a complete ARM64 MediaProvider native hook capability.
struct Arm64HookCoverage {
    std::array<bool, kRequiredFUSECallbacks> callbacks{};
    bool replyAttr = false;
    bool replyEntry = false;
    bool replyBuf = false;
    bool replyErr = false;
    bool notifyEntry = false;
    bool notifyInode = false;
    bool sessionDestroy = false;
};

constexpr size_t CountArm64FUSECallbacks(const Arm64HookCoverage& hooks) {
    size_t installed = 0;
    for (bool callback : hooks.callbacks)
        installed += callback ? 1 : 0;
    return installed;
}

constexpr bool HasCompleteArm64HookCoverage(const Arm64HookCoverage& hooks) {
    return HasRequiredFUSECallbackCoverage(CountArm64FUSECallbacks(hooks),
                                           kRequiredFUSECallbacks) &&
           hooks.replyAttr && hooks.replyEntry && hooks.replyBuf && hooks.replyErr &&
           hooks.notifyEntry && hooks.notifyInode && hooks.sessionDestroy;
}

// Session callback tables provide the six pf_* interceptors without inline
// patches. They still require the same reply, invalidation and retirement
// hooks as the ARM64 inline backend before reporting complete coverage.
constexpr bool HasArm64AuxiliaryHookCoverage(const Arm64HookCoverage& hooks) {
    return hooks.replyAttr && hooks.replyEntry && hooks.replyBuf && hooks.replyErr &&
           hooks.notifyEntry && hooks.notifyInode && hooks.sessionDestroy;
}

}  // namespace fusehide
