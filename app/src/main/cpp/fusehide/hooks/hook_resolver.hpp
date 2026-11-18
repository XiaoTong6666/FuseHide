// Copyright (C) 2026 XiaoTong6666
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "fusehide/elf/elf_utils.hpp"

namespace fusehide {

enum class HookResolutionSource {
    kUnavailable,
    kProfileFallback,
    kResolvedSymbol,
    kResolvedContains,
    kDerivedLayout,
};

#if defined(__aarch64__) || defined(__arm__)
inline constexpr bool kSupportsDeviceProfileOffsets = true;
#else
inline constexpr bool kSupportsDeviceProfileOffsets = false;
#endif

struct CriticalHookTargetPlan {
    uintptr_t offset = 0;
    HookResolutionSource source = HookResolutionSource::kProfileFallback;
};

struct DeviceHookInstallPlan {
    DeviceHookProfile profile;
    bool profileTrusted = false;
    // An exact string-bearing symbol can enable the existing string wrappers
    // independently of DirectoryEntries availability or legacy profile RVAs.
    bool runtimeStringSignatureResolved = false;
    CriticalHookTargetPlan shouldNotCache;
    CriticalHookTargetPlan doReaddirCommon;
    CriticalHookTargetPlan getDirectoryEntries;
    CriticalHookTargetPlan addDirectoryEntriesFromLowerFs;
    CriticalHookTargetPlan addDirectoryEntriesFromLowerFsThunk;
    CriticalHookTargetPlan pfReaddir;
    CriticalHookTargetPlan pfReaddirPostfilter;
    CriticalHookTargetPlan pfReaddirplus;
};

// Analysis and planning only: no inline/GOT patches are applied here.
std::optional<void*> ResolveTargetSymbol(const ModuleInfo& module, std::string_view symbolName);
std::optional<void*> ResolveTargetSymbolRuntime(const ModuleInfo& module,
                                                std::string_view symbolName);
std::optional<void*> ResolveImportedFunctionTarget(const ModuleInfo& importingModule,
                                                   std::string_view symbolName);

template <size_t N>
std::optional<uintptr_t> ResolveFirstAvailableSymbolOffset(const ModuleInfo& module,
                                                           const std::string_view (&symbols)[N]) {
    const bool useRuntimeElf = module.path.find("!/") != std::string::npos;
    for (const auto& symbol : symbols) {
        auto resolved = useRuntimeElf ? ResolveTargetSymbolRuntime(module, symbol)
                                      : ResolveTargetSymbol(module, symbol);
        if (resolved.has_value()) {
            DebugLogPrint(4, "resolved exact symbol=%s offset=0x%zx target=%p path=%s runtime=%d",
                          std::string(symbol).c_str(),
                          static_cast<size_t>(reinterpret_cast<uintptr_t>(*resolved) - module.base),
                          *resolved, module.path.c_str(), useRuntimeElf ? 1 : 0);
            return reinterpret_cast<uintptr_t>(*resolved) - module.base;
        }
    }
    return std::nullopt;
}

DeviceHookInstallPlan ResolveDeviceHookInstallPlan(const ModuleInfo& module);
DeviceHookProfile ResolveEffectiveDeviceHookProfile(const ModuleInfo& module);
const char* HookResolutionSourceName(HookResolutionSource source);
bool CanUseProfileBasedTarget(HookResolutionSource source);
bool CanUseClassicStringWrappers(const DeviceHookInstallPlan& installPlan);
bool CanInstallDirectoryEntriesHooks(const DeviceHookInstallPlan& installPlan);

}  // namespace fusehide
