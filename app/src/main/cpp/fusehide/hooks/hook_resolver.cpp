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

#include "fusehide/hooks/hook_resolver.hpp"

namespace fusehide {

const char* DirectoryEntriesAbiName(DirectoryEntriesAbi abi) {
    switch (abi) {
        case DirectoryEntriesAbi::kSharedPtr:
            return "shared_ptr";
        case DirectoryEntriesAbi::kValue:
            return "value";
        case DirectoryEntriesAbi::kUnknown:
            return "unknown";
    }
    return "unknown";
}

// Unified symbol resolution (dynamic-first, section fallback)

std::optional<void*> ResolveTargetSymbol(const ModuleInfo& module, std::string_view symbolName) {
    std::optional<MappedFile> mapped;
    if (module.path.find("!/") != std::string::npos) {
        mapped = MapEmbeddedStoredElf(module.path);
    } else {
        mapped = MapReadOnlyFile(module.path, module.fileOffset);
    }
    if (!mapped.has_value()) {
        return std::nullopt;
    }
    std::optional<uintptr_t> offset;
    if (auto dynamicInfo = ParseDynamicInfo(*mapped); dynamicInfo.has_value()) {
        // Share GNU/SysV hash and linear fallback with the other ELF callers.
        // Unlike relocation lookup, an inline hook target must be defined.
        const auto* nameData = reinterpret_cast<const uint8_t*>(symbolName.data());
        const auto index =
            FindDefinedDynamicSymbolIndex(*mapped, *dynamicInfo, nameData, symbolName.size());
        if (index.has_value()) {
            if (const ElfSymbol* symbols = DynamicSymbolTable(*mapped, *dynamicInfo);
                symbols != nullptr)
                offset = static_cast<uintptr_t>(symbols[*index].st_value);
        }
    }

    // Section fallback
    if (!offset.has_value()) {
        offset = FindSymbolOffset(*mapped, symbolName);
    }

    if (!offset.has_value()) {
        return std::nullopt;
    }
    return reinterpret_cast<void*>(module.base + *offset);
}

std::optional<void*> ResolveTargetSymbolRuntime(const ModuleInfo& module,
                                                std::string_view symbolName) {
    auto dynInfo = ParseRuntimeDynamicInfo(module);
    if (dynInfo.has_value()) {
        const auto* nameData = reinterpret_cast<const uint8_t*>(symbolName.data());
        const size_t nameLen = symbolName.size();
        auto symIdx = FindRuntimeSymbolIndex(*dynInfo, nameData, nameLen);
        if (symIdx.has_value()) {
            const auto& sym = dynInfo->symtab[*symIdx];
            if (sym.st_value != 0) {
                return reinterpret_cast<void*>(module.base + sym.st_value);
            }
        }
    }

    std::optional<MappedFile> mapped;
    if (module.path.find("!/") != std::string::npos) {
        mapped = MapEmbeddedStoredElf(module.path);
    } else {
        mapped = MapReadOnlyFile(module.path, module.fileOffset);
    }

    if (!mapped.has_value()) {
        return std::nullopt;
    }

    auto offset = FindSymbolOffset(*mapped, symbolName);
    if (!offset.has_value()) {
        return std::nullopt;
    }

    return reinterpret_cast<void*>(module.base + *offset);
}

bool IsLibfuseAddress(void* address) {
    Dl_info info{};
    if (address == nullptr || dladdr(address, &info) == 0 || info.dli_fname == nullptr) {
        return false;
    }
    const std::string_view path(info.dli_fname);
    return path == "libfuse.so" || path.ends_with("/libfuse.so");
}

std::optional<void*> ResolveImportedFunctionTarget(const ModuleInfo& importingModule,
                                                   std::string_view symbolName) {
    const auto dynamicInfo = ParseRuntimeDynamicInfo(importingModule);
    if (!dynamicInfo.has_value()) {
        return std::nullopt;
    }
    const auto symbolIndex = FindRuntimeSymbolIndex(
        *dynamicInfo, reinterpret_cast<const uint8_t*>(symbolName.data()), symbolName.size());
    if (!symbolIndex.has_value()) {
        return std::nullopt;
    }

    const auto slots =
        FindRuntimeRelocationSlotsForSymbol(*dynamicInfo, *symbolIndex, importingModule.base);
    for (const uintptr_t slotAddress : slots) {
        void* target = *reinterpret_cast<void* const*>(slotAddress);
        if (IsLibfuseAddress(target)) {
            return target;
        }
    }
    return std::nullopt;
}

DirectoryEntriesAbi DetectDirectoryEntriesAbi(const ModuleInfo& module) {
    // An ordinary Itanium C++ function name does not encode its return type, so the
    // GetDirectoryEntries symbol is identical for shared_ptr- and value-vector returns. Its helper
    // takes the vector by pointer, which does encode the element type; require one exact helper
    // signature and refuse these two STL hooks when neither (or both) can be proven.
    const bool hasShared =
        ResolveFirstAvailableSymbolOffset(module, kAddSharedDirectoryEntriesFromLowerFsSymbols)
            .has_value();
    const bool hasValue =
        ResolveFirstAvailableSymbolOffset(module, kAddValueDirectoryEntriesFromLowerFsSymbols)
            .has_value();
    if (hasShared == hasValue) {
        return DirectoryEntriesAbi::kUnknown;
    }
    if (hasShared) {
        return DirectoryEntriesAbi::kSharedPtr;
    }
#if defined(__aarch64__)
    return DirectoryEntriesAbi::kValue;
#else
    // The value-vector object layout and ownership boundary have only been verified on arm64.
    return DirectoryEntriesAbi::kUnknown;
#endif
}

struct ResolvedHookFeatureOffsets {
    std::optional<uintptr_t> isAppAccessiblePathOffset;
    HookResolutionSource isAppAccessiblePathSource = HookResolutionSource::kUnavailable;
    std::optional<uintptr_t> pfLookupOffset;
    std::optional<uintptr_t> pfLookupPostfilterOffset;
    std::optional<uintptr_t> pfGetattrOffset;
    std::optional<uintptr_t> shouldNotCacheOffset;
    std::optional<uintptr_t> doReaddirCommonOffset;
    std::optional<uintptr_t> getDirectoryEntriesOffset;
    std::optional<uintptr_t> addDirectoryEntriesFromLowerFsOffset;
    std::optional<uintptr_t> pfMkdirOffset;
    std::optional<uintptr_t> pfMknodOffset;
    std::optional<uintptr_t> pfUnlinkOffset;
    std::optional<uintptr_t> pfRmdirOffset;
    std::optional<uintptr_t> pfRenameOffset;
    std::optional<uintptr_t> pfCreateOffset;
    std::optional<uintptr_t> pfReaddirOffset;
    std::optional<uintptr_t> pfReaddirPostfilterOffset;
    std::optional<uintptr_t> pfReaddirplusOffset;
    HookResolutionSource pfReaddirSource = HookResolutionSource::kProfileFallback;
    HookResolutionSource pfReaddirPostfilterSource = HookResolutionSource::kProfileFallback;
    HookResolutionSource pfReaddirplusSource = HookResolutionSource::kProfileFallback;
    HookResolutionSource shouldNotCacheSource = HookResolutionSource::kProfileFallback;
    HookResolutionSource doReaddirCommonSource = HookResolutionSource::kProfileFallback;
    HookResolutionSource getDirectoryEntriesSource = HookResolutionSource::kProfileFallback;
    HookResolutionSource addDirectoryEntriesFromLowerFsSource =
        HookResolutionSource::kProfileFallback;
};

std::optional<uintptr_t> ResolveLargestSymbolOffsetContaining(const ModuleInfo& module,
                                                              std::string_view needle) {
    std::optional<MappedFile> mapped;
    if (module.path.find("!/") != std::string::npos) {
        mapped = MapEmbeddedStoredElf(module.path);
    } else {
        mapped = MapReadOnlyFile(module.path, module.fileOffset);
    }
    if (!mapped.has_value()) {
        return std::nullopt;
    }
    auto match = FindLargestSymbolContaining(*mapped, needle);
    if (!match.has_value()) {
        return std::nullopt;
    }
    DebugLogPrint(4, "resolved contains needle=%s picked=%s offset=0x%zx target=%p path=%s",
                  std::string(needle).c_str(), match->name.c_str(),
                  static_cast<size_t>(match->value),
                  reinterpret_cast<void*>(module.base + match->value), module.path.c_str());
    return match->value;
}

template <size_t N>
std::optional<uintptr_t> ResolveFeatureOffsetBySymbols(const ModuleInfo& module,
                                                       const std::string_view (&symbols)[N],
                                                       HookResolutionSource* source) {
    auto offset = ResolveFirstAvailableSymbolOffset(module, symbols);
    if (offset.has_value() && source != nullptr) {
        *source = HookResolutionSource::kResolvedSymbol;
    }
    return offset;
}

std::optional<uintptr_t> ResolveFeatureOffsetByContains(const ModuleInfo& module,
                                                        std::string_view needle,
                                                        HookResolutionSource* source) {
    auto offset = ResolveLargestSymbolOffsetContaining(module, needle);
    if (offset.has_value() && source != nullptr) {
        *source = HookResolutionSource::kResolvedContains;
    }
    return offset;
}

const char* HookResolutionSourceName(HookResolutionSource source) {
    switch (source) {
        case HookResolutionSource::kUnavailable:
            return "unavailable";
        case HookResolutionSource::kProfileFallback:
            return "profile";
        case HookResolutionSource::kResolvedSymbol:
            return "resolved_symbol";
        case HookResolutionSource::kResolvedContains:
            return "resolved_contains";
        case HookResolutionSource::kDerivedLayout:
            return "derived_layout";
    }
    return "unknown";
}

bool IsEmbeddedX86Module(const ModuleInfo& module) {
    return module.path.find("/lib/x86/") != std::string::npos ||
           module.path.find("/lib/x86_64/") != std::string::npos;
}

bool CanUseProfileBasedTarget(HookResolutionSource source) {
    if (source == HookResolutionSource::kUnavailable) {
        return false;
    }
    if constexpr (kSupportsDeviceProfileOffsets) {
        return true;
    }
    return source != HookResolutionSource::kProfileFallback &&
           source != HookResolutionSource::kDerivedLayout;
}

struct DerivedHookFeatureFlags {
    bool shouldNotCache = false;
    bool doReaddirCommon = false;
    bool getDirectoryEntries = false;
    bool addDirectoryEntriesFromLowerFs = false;
    bool addDirectoryEntriesFromLowerFsThunk = false;
    bool pfReaddir = false;
    bool pfReaddirPostfilter = false;
    bool pfReaddirplus = false;
};

// A matching mangled symbol proves the entry's parameter type, not the
// private libc++ string-object layout. Keep the older trusted profile path;
// runtime symbols enable the existing classic-layout wrappers provisionally.
bool CanUseClassicStringWrappers(const DeviceHookInstallPlan& installPlan) {
    return installPlan.runtimeStringSignatureResolved ||
           (installPlan.profileTrusted &&
            installPlan.profile.stringLayout == AbiStringLayout::kClassic);
}

bool CanInstallDirectoryEntriesHooks(const DeviceHookInstallPlan& installPlan) {
    if (!CanUseClassicStringWrappers(installPlan)) {
        return false;
    }
    const DirectoryEntriesAbi abi = gDirectoryEntriesAbi.load(std::memory_order_acquire);
    if (abi == DirectoryEntriesAbi::kSharedPtr) {
        return true;
    }
    return abi == DirectoryEntriesAbi::kValue &&
           installPlan.getDirectoryEntries.source == HookResolutionSource::kResolvedSymbol &&
           installPlan.addDirectoryEntriesFromLowerFs.source ==
               HookResolutionSource::kResolvedSymbol;
}

struct DerivedOffsetResult {
    uintptr_t offset = 0;
    int votes = 0;
    int inputs = 0;
};

using ProfileOffsetField = uintptr_t DeviceHookProfile::*;
using FeatureOffsetField = std::optional<uintptr_t> ResolvedHookFeatureOffsets::*;

struct LayoutAnchorDescriptor {
    const char* name;
    ProfileOffsetField profileField;
    FeatureOffsetField featureField;
};

inline constexpr LayoutAnchorDescriptor kLayoutAnchorDescriptors[] = {
    {"is_app_accessible_path", &DeviceHookProfile::isAppAccessiblePathOffset,
     &ResolvedHookFeatureOffsets::isAppAccessiblePathOffset},
    {"pf_lookup", &DeviceHookProfile::pfLookupOffset, &ResolvedHookFeatureOffsets::pfLookupOffset},
    {"pf_lookup_postfilter", &DeviceHookProfile::pfLookupPostfilterOffset,
     &ResolvedHookFeatureOffsets::pfLookupPostfilterOffset},
    {"pf_getattr", &DeviceHookProfile::pfGetattrOffset,
     &ResolvedHookFeatureOffsets::pfGetattrOffset},
    {"ShouldNotCache", &DeviceHookProfile::shouldNotCacheOffset,
     &ResolvedHookFeatureOffsets::shouldNotCacheOffset},
    {"do_readdir_common", &DeviceHookProfile::doReaddirCommonOffset,
     &ResolvedHookFeatureOffsets::doReaddirCommonOffset},
    {"GetDirectoryEntries", &DeviceHookProfile::getDirectoryEntriesOffset,
     &ResolvedHookFeatureOffsets::getDirectoryEntriesOffset},
    {"addDirectoryEntriesFromLowerFs", &DeviceHookProfile::addDirectoryEntriesFromLowerFsOffset,
     &ResolvedHookFeatureOffsets::addDirectoryEntriesFromLowerFsOffset},
    {"pf_mkdir", &DeviceHookProfile::pfMkdirOffset, &ResolvedHookFeatureOffsets::pfMkdirOffset},
    {"pf_mknod", &DeviceHookProfile::pfMknodOffset, &ResolvedHookFeatureOffsets::pfMknodOffset},
    {"pf_unlink", &DeviceHookProfile::pfUnlinkOffset, &ResolvedHookFeatureOffsets::pfUnlinkOffset},
    {"pf_rmdir", &DeviceHookProfile::pfRmdirOffset, &ResolvedHookFeatureOffsets::pfRmdirOffset},
    {"pf_rename", &DeviceHookProfile::pfRenameOffset, &ResolvedHookFeatureOffsets::pfRenameOffset},
    {"pf_create", &DeviceHookProfile::pfCreateOffset, &ResolvedHookFeatureOffsets::pfCreateOffset},
    {"pf_readdir", &DeviceHookProfile::pfReaddirOffset,
     &ResolvedHookFeatureOffsets::pfReaddirOffset},
    {"pf_readdir_postfilter", &DeviceHookProfile::pfReaddirPostfilterOffset,
     &ResolvedHookFeatureOffsets::pfReaddirPostfilterOffset},
    {"pf_readdirplus", &DeviceHookProfile::pfReaddirplusOffset,
     &ResolvedHookFeatureOffsets::pfReaddirplusOffset},
};

ResolvedHookFeatureOffsets ResolveHookFeatureOffsets(const ModuleInfo& module) {
    ResolvedHookFeatureOffsets features;
    features.isAppAccessiblePathOffset = ResolveFeatureOffsetBySymbols(
        module, kIsAppAccessiblePathSymbols, &features.isAppAccessiblePathSource);
    features.pfLookupOffset = ResolveFirstAvailableSymbolOffset(module, kPfLookupSymbols);
    features.pfLookupPostfilterOffset =
        ResolveFirstAvailableSymbolOffset(module, kPfLookupPostfilterSymbols);
    features.pfGetattrOffset = ResolveFirstAvailableSymbolOffset(module, kPfGetattrSymbols);
    features.shouldNotCacheOffset = ResolveFeatureOffsetBySymbols(module, kShouldNotCacheSymbols,
                                                                  &features.shouldNotCacheSource);
    features.doReaddirCommonOffset = ResolveFeatureOffsetBySymbols(module, kDoReaddirCommonSymbols,
                                                                   &features.doReaddirCommonSource);
    const DirectoryEntriesAbi directoryEntriesAbi =
        gDirectoryEntriesAbi.load(std::memory_order_acquire);
    if (directoryEntriesAbi != DirectoryEntriesAbi::kUnknown) {
        features.getDirectoryEntriesOffset = ResolveFeatureOffsetBySymbols(
            module, kGetDirectoryEntriesSymbols, &features.getDirectoryEntriesSource);
        if (directoryEntriesAbi == DirectoryEntriesAbi::kSharedPtr) {
            features.addDirectoryEntriesFromLowerFsOffset =
                ResolveFeatureOffsetBySymbols(module, kAddSharedDirectoryEntriesFromLowerFsSymbols,
                                              &features.addDirectoryEntriesFromLowerFsSource);
        } else {
            features.addDirectoryEntriesFromLowerFsOffset =
                ResolveFeatureOffsetBySymbols(module, kAddValueDirectoryEntriesFromLowerFsSymbols,
                                              &features.addDirectoryEntriesFromLowerFsSource);
        }
    }
    features.pfMkdirOffset = ResolveFirstAvailableSymbolOffset(module, kPfMkdirSymbols);
    features.pfMknodOffset = ResolveFirstAvailableSymbolOffset(module, kPfMknodSymbols);
    features.pfUnlinkOffset = ResolveFirstAvailableSymbolOffset(module, kPfUnlinkSymbols);
    features.pfRmdirOffset = ResolveFirstAvailableSymbolOffset(module, kPfRmdirSymbols);
    features.pfRenameOffset = ResolveFirstAvailableSymbolOffset(module, kPfRenameSymbols);
    features.pfCreateOffset = ResolveFirstAvailableSymbolOffset(module, kPfCreateSymbols);
    features.pfReaddirOffset =
        ResolveFeatureOffsetBySymbols(module, kPfReaddirSymbols, &features.pfReaddirSource);
    features.pfReaddirPostfilterOffset = ResolveFeatureOffsetBySymbols(
        module, kPfReaddirPostfilterSymbols, &features.pfReaddirPostfilterSource);
    features.pfReaddirplusOffset =
        ResolveFeatureOffsetBySymbols(module, kPfReaddirplusSymbols, &features.pfReaddirplusSource);
    if (!features.isAppAccessiblePathOffset.has_value()) {
        features.isAppAccessiblePathOffset = ResolveFeatureOffsetByContains(
            module, "is_app_accessible_path", &features.isAppAccessiblePathSource);
    }
    if (!features.pfLookupOffset.has_value()) {
        features.pfLookupOffset =
            ResolveFeatureOffsetByContains(module, "pf_lookupEP8fuse_req", nullptr);
    }
    if (!features.pfLookupPostfilterOffset.has_value()) {
        features.pfLookupPostfilterOffset =
            ResolveFeatureOffsetByContains(module, "pf_lookup_postfilter", nullptr);
    }
    if (!features.pfGetattrOffset.has_value()) {
        features.pfGetattrOffset =
            ResolveFeatureOffsetByContains(module, "pf_getattrEP8fuse_req", nullptr);
    }
    if (!features.pfMkdirOffset.has_value()) {
        features.pfMkdirOffset =
            ResolveFeatureOffsetByContains(module, "pf_mkdirEP8fuse_req", nullptr);
    }
    if (!features.pfMknodOffset.has_value()) {
        features.pfMknodOffset =
            ResolveFeatureOffsetByContains(module, "pf_mknodEP8fuse_req", nullptr);
    }
    if (!features.pfUnlinkOffset.has_value()) {
        features.pfUnlinkOffset =
            ResolveFeatureOffsetByContains(module, "pf_unlinkEP8fuse_req", nullptr);
    }
    if (!features.pfRmdirOffset.has_value()) {
        features.pfRmdirOffset =
            ResolveFeatureOffsetByContains(module, "pf_rmdirEP8fuse_req", nullptr);
    }
    if (!features.pfRenameOffset.has_value()) {
        features.pfRenameOffset =
            ResolveFeatureOffsetByContains(module, "pf_renameEP8fuse_req", nullptr);
    }
    if (!features.pfCreateOffset.has_value()) {
        features.pfCreateOffset =
            ResolveFeatureOffsetByContains(module, "pf_createEP8fuse_req", nullptr);
    }
    if (!features.shouldNotCacheOffset.has_value()) {
        features.shouldNotCacheOffset = ResolveFeatureOffsetByContains(
            module, "ShouldNotCache", &features.shouldNotCacheSource);
    }
    if (!features.doReaddirCommonOffset.has_value()) {
        features.doReaddirCommonOffset = ResolveFeatureOffsetByContains(
            module, "do_readdir_common", &features.doReaddirCommonSource);
    }
    if (!features.pfReaddirOffset.has_value()) {
        features.pfReaddirOffset = ResolveFeatureOffsetByContains(module, "pf_readdirEP8fuse_req",
                                                                  &features.pfReaddirSource);
    }
    if (!features.pfReaddirPostfilterOffset.has_value()) {
        features.pfReaddirPostfilterOffset = ResolveFeatureOffsetByContains(
            module, "pf_readdir_postfilterEP8fuse_req", &features.pfReaddirPostfilterSource);
    }
    if (!features.pfReaddirplusOffset.has_value()) {
        features.pfReaddirplusOffset = ResolveFeatureOffsetByContains(
            module, "pf_readdirplusEP8fuse_req", &features.pfReaddirplusSource);
    }
    return features;
}

int CountResolvedHookFeatures(const ResolvedHookFeatureOffsets& features) {
    int count = 0;
    auto add = [&](const std::optional<uintptr_t>& value) {
        if (value.has_value()) {
            ++count;
        }
    };
    add(features.isAppAccessiblePathOffset);
    add(features.pfLookupOffset);
    add(features.pfLookupPostfilterOffset);
    add(features.pfGetattrOffset);
    add(features.shouldNotCacheOffset);
    add(features.doReaddirCommonOffset);
    add(features.getDirectoryEntriesOffset);
    add(features.addDirectoryEntriesFromLowerFsOffset);
    add(features.pfMkdirOffset);
    add(features.pfMknodOffset);
    add(features.pfUnlinkOffset);
    add(features.pfRmdirOffset);
    add(features.pfRenameOffset);
    add(features.pfCreateOffset);
    add(features.pfReaddirOffset);
    add(features.pfReaddirPostfilterOffset);
    add(features.pfReaddirplusOffset);
    return count;
}

int ScoreHookProfile(const DeviceHookProfile& profile, const ResolvedHookFeatureOffsets& features) {
    int score = 0;
    auto match = [&](const std::optional<uintptr_t>& value, uintptr_t expected) {
        if (value.has_value() && *value == expected) {
            ++score;
        }
    };
    match(features.isAppAccessiblePathOffset, profile.isAppAccessiblePathOffset);
    match(features.pfLookupOffset, profile.pfLookupOffset);
    match(features.pfLookupPostfilterOffset, profile.pfLookupPostfilterOffset);
    match(features.pfGetattrOffset, profile.pfGetattrOffset);
    match(features.shouldNotCacheOffset, profile.shouldNotCacheOffset);
    match(features.doReaddirCommonOffset, profile.doReaddirCommonOffset);
    match(features.getDirectoryEntriesOffset, profile.getDirectoryEntriesOffset);
    match(features.addDirectoryEntriesFromLowerFsOffset,
          profile.addDirectoryEntriesFromLowerFsOffset);
    match(features.pfMkdirOffset, profile.pfMkdirOffset);
    match(features.pfMknodOffset, profile.pfMknodOffset);
    match(features.pfUnlinkOffset, profile.pfUnlinkOffset);
    match(features.pfRmdirOffset, profile.pfRmdirOffset);
    match(features.pfRenameOffset, profile.pfRenameOffset);
    match(features.pfCreateOffset, profile.pfCreateOffset);
    match(features.pfReaddirOffset, profile.pfReaddirOffset);
    match(features.pfReaddirPostfilterOffset, profile.pfReaddirPostfilterOffset);
    match(features.pfReaddirplusOffset, profile.pfReaddirplusOffset);
    return score;
}

void ApplyResolvedHookFeatureOverrides(const ResolvedHookFeatureOffsets& features,
                                       DeviceHookProfile* profile, int* overrideCount) {
    auto apply = [&](const std::optional<uintptr_t>& value, uintptr_t DeviceHookProfile::* field) {
        if (!value.has_value()) {
            return;
        }
        profile->*field = *value;
        ++(*overrideCount);
    };
    apply(features.isAppAccessiblePathOffset, &DeviceHookProfile::isAppAccessiblePathOffset);
    apply(features.pfLookupOffset, &DeviceHookProfile::pfLookupOffset);
    apply(features.pfLookupPostfilterOffset, &DeviceHookProfile::pfLookupPostfilterOffset);
    apply(features.pfGetattrOffset, &DeviceHookProfile::pfGetattrOffset);
    apply(features.shouldNotCacheOffset, &DeviceHookProfile::shouldNotCacheOffset);
    apply(features.doReaddirCommonOffset, &DeviceHookProfile::doReaddirCommonOffset);
    apply(features.getDirectoryEntriesOffset, &DeviceHookProfile::getDirectoryEntriesOffset);
    apply(features.addDirectoryEntriesFromLowerFsOffset,
          &DeviceHookProfile::addDirectoryEntriesFromLowerFsOffset);
    apply(features.pfMkdirOffset, &DeviceHookProfile::pfMkdirOffset);
    apply(features.pfMknodOffset, &DeviceHookProfile::pfMknodOffset);
    apply(features.pfUnlinkOffset, &DeviceHookProfile::pfUnlinkOffset);
    apply(features.pfRmdirOffset, &DeviceHookProfile::pfRmdirOffset);
    apply(features.pfRenameOffset, &DeviceHookProfile::pfRenameOffset);
    apply(features.pfCreateOffset, &DeviceHookProfile::pfCreateOffset);
    apply(features.pfReaddirOffset, &DeviceHookProfile::pfReaddirOffset);
    apply(features.pfReaddirPostfilterOffset, &DeviceHookProfile::pfReaddirPostfilterOffset);
    apply(features.pfReaddirplusOffset, &DeviceHookProfile::pfReaddirplusOffset);
}

void ClearUnresolvedHookProfileOffsets(const ResolvedHookFeatureOffsets& features,
                                       DeviceHookProfile* profile) {
    for (const auto& anchor : kLayoutAnchorDescriptors) {
        if (!(features.*(anchor.featureField)).has_value()) {
            profile->*(anchor.profileField) = 0;
        }
    }
    profile->addDirectoryEntriesFromLowerFsThunkOffset = 0;
}

std::optional<DerivedOffsetResult> DeriveOffsetFromProfileLayout(
    const DeviceHookProfile& profile, const ResolvedHookFeatureOffsets& features,
    ProfileOffsetField targetField) {
    std::unordered_map<uintptr_t, int> votes;
    int inputs = 0;
    for (const auto& anchor : kLayoutAnchorDescriptors) {
        const auto& featureValue = features.*(anchor.featureField);
        if (!featureValue.has_value()) {
            continue;
        }
        const int64_t delta = static_cast<int64_t>(profile.*targetField) -
                              static_cast<int64_t>(profile.*(anchor.profileField));
        const int64_t candidate = static_cast<int64_t>(*featureValue) + delta;
        if (candidate < 0) {
            return std::nullopt;
        }
        ++votes[static_cast<uintptr_t>(candidate)];
        ++inputs;
    }
    if (votes.empty()) {
        return std::nullopt;
    }

    uintptr_t bestOffset = 0;
    int bestVotes = -1;
    int ties = 0;
    for (const auto& [offset, count] : votes) {
        if (count > bestVotes) {
            bestOffset = offset;
            bestVotes = count;
            ties = 1;
        } else if (count == bestVotes) {
            ++ties;
        }
    }
    // Layout derivation is safe only when at least two independent anchors agree on one delta.
    if (ties > 1 || bestVotes < 2 || bestVotes != inputs) {
        return std::nullopt;
    }
    return DerivedOffsetResult{bestOffset, bestVotes, inputs};
}

void ApplyDerivedHookFeatureOverrides(const DeviceHookProfile& bestProfile,
                                      const ResolvedHookFeatureOffsets& features,
                                      DeviceHookProfile* effectiveProfile, int* derivedCount,
                                      DerivedHookFeatureFlags* derivedFlags) {
    bool shouldNotCacheDerived = false;
    bool doReaddirCommonDerived = false;
    bool getDirectoryEntriesDerived = false;
    bool addDirectoryEntriesBodyDerived = false;
    bool pfReaddirDerived = false;
    bool pfReaddirPostfilterDerived = false;
    bool pfReaddirplusDerived = false;
    auto derive = [&](const char* label, const std::optional<uintptr_t>& alreadyResolved,
                      ProfileOffsetField targetField, bool* derivedFlag = nullptr) {
        if (alreadyResolved.has_value()) {
            return;
        }
        auto derived = DeriveOffsetFromProfileLayout(bestProfile, features, targetField);
        if (!derived.has_value()) {
            return;
        }
        effectiveProfile->*targetField = derived->offset;
        ++(*derivedCount);
        if (derivedFlag != nullptr) {
            *derivedFlag = true;
        }
        __android_log_print(4, kLogTag, "derived hook offset %s=0x%zx votes=%d/%d base_profile=%s",
                            label, static_cast<size_t>(derived->offset), derived->votes,
                            derived->inputs, bestProfile.name);
    };

    derive("ShouldNotCache", features.shouldNotCacheOffset,
           &DeviceHookProfile::shouldNotCacheOffset, &shouldNotCacheDerived);
    derive("do_readdir_common", features.doReaddirCommonOffset,
           &DeviceHookProfile::doReaddirCommonOffset, &doReaddirCommonDerived);
    derive("GetDirectoryEntries", features.getDirectoryEntriesOffset,
           &DeviceHookProfile::getDirectoryEntriesOffset, &getDirectoryEntriesDerived);
    derive("addDirectoryEntriesFromLowerFs", features.addDirectoryEntriesFromLowerFsOffset,
           &DeviceHookProfile::addDirectoryEntriesFromLowerFsOffset,
           &addDirectoryEntriesBodyDerived);
    derive("pf_readdir", features.pfReaddirOffset, &DeviceHookProfile::pfReaddirOffset,
           &pfReaddirDerived);
    derive("pf_readdir_postfilter", features.pfReaddirPostfilterOffset,
           &DeviceHookProfile::pfReaddirPostfilterOffset, &pfReaddirPostfilterDerived);
    derive("pf_readdirplus", features.pfReaddirplusOffset, &DeviceHookProfile::pfReaddirplusOffset,
           &pfReaddirplusDerived);

    if (derivedFlags != nullptr) {
        derivedFlags->shouldNotCache = shouldNotCacheDerived;
        derivedFlags->doReaddirCommon = doReaddirCommonDerived;
        derivedFlags->getDirectoryEntries = getDirectoryEntriesDerived;
        derivedFlags->addDirectoryEntriesFromLowerFs = addDirectoryEntriesBodyDerived;
        derivedFlags->pfReaddir = pfReaddirDerived;
        derivedFlags->pfReaddirPostfilter = pfReaddirPostfilterDerived;
        derivedFlags->pfReaddirplus = pfReaddirplusDerived;
    }
}

CriticalHookTargetPlan BuildCriticalHookTargetPlan(uintptr_t effectiveOffset,
                                                   const std::optional<uintptr_t>& resolvedOffset,
                                                   HookResolutionSource resolvedSource,
                                                   bool derived, bool profileTrusted) {
    CriticalHookTargetPlan plan;
    plan.offset = effectiveOffset;
    if (resolvedOffset.has_value()) {
        plan.source = resolvedSource;
    } else if (derived) {
        plan.source = HookResolutionSource::kDerivedLayout;
    } else if (profileTrusted && effectiveOffset != 0) {
        plan.source = HookResolutionSource::kProfileFallback;
    } else {
        plan.offset = 0;
        plan.source = HookResolutionSource::kUnavailable;
    }
    return plan;
}

DeviceHookInstallPlan ResolveDeviceHookInstallPlan(const ModuleInfo& module) {
    static std::mutex cacheMutex;
    static std::unordered_map<std::string, DeviceHookInstallPlan> cache;

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto cached = cache.find(module.path);
        if (cached != cache.end()) {
            return cached->second;
        }
    }

    // Callback discovery can request this cached plan before the main hook-install path.
    const DirectoryEntriesAbi directoryEntriesAbi = DetectDirectoryEntriesAbi(module);
    gDirectoryEntriesAbi.store(directoryEntriesAbi, std::memory_order_release);
    __android_log_print(4, kLogTag, "directory entries ABI=%s",
                        DirectoryEntriesAbiName(directoryEntriesAbi));
    if (directoryEntriesAbi == DirectoryEntriesAbi::kUnknown) {
        __android_log_print(6, kLogTag,
                            "unknown DirectoryEntries ABI; skipping unsafe C++ container hooks");
    }

    const ResolvedHookFeatureOffsets features = ResolveHookFeatureOffsets(module);
    const int resolvedCount = CountResolvedHookFeatures(features);
    const DeviceHookProfile* bestProfile = &kDeviceHookProfileLegacy;
    int bestScore = -1;
    int bestTies = 0;
    for (const auto& profile : kKnownDeviceHookProfiles) {
        const int score = ScoreHookProfile(profile, features);
        if (score > bestScore) {
            bestScore = score;
            bestProfile = &profile;
            bestTies = 1;
        } else if (score == bestScore) {
            ++bestTies;
        }
    }
    const bool profileTrusted = bestScore >= 2 && bestScore == resolvedCount && bestTies == 1;

    DeviceHookInstallPlan installPlan;
    installPlan.profile = *bestProfile;
    installPlan.profileTrusted = profileTrusted;
    // Reuse the existing resolution results. DirectoryEntries has a separate
    // ABI gate, so a missing directory helper must not block string hooks.
    installPlan.runtimeStringSignatureResolved =
        features.isAppAccessiblePathSource == HookResolutionSource::kResolvedSymbol;
    if (installPlan.runtimeStringSignatureResolved)
        DebugLogPrint(4, "string hook signature resolved exactly; classic object layout assumed");
    int overrideCount = 0;
    ApplyResolvedHookFeatureOverrides(features, &installPlan.profile, &overrideCount);
    int derivedCount = 0;
    DerivedHookFeatureFlags derivedFlags;
    if (profileTrusted) {
        ApplyDerivedHookFeatureOverrides(*bestProfile, features, &installPlan.profile,
                                         &derivedCount, &derivedFlags);
    } else {
        ClearUnresolvedHookProfileOffsets(features, &installPlan.profile);
    }

    installPlan.shouldNotCache = BuildCriticalHookTargetPlan(
        installPlan.profile.shouldNotCacheOffset, features.shouldNotCacheOffset,
        features.shouldNotCacheSource, derivedFlags.shouldNotCache, profileTrusted);
    installPlan.doReaddirCommon = BuildCriticalHookTargetPlan(
        installPlan.profile.doReaddirCommonOffset, features.doReaddirCommonOffset,
        features.doReaddirCommonSource, derivedFlags.doReaddirCommon, profileTrusted);
    installPlan.getDirectoryEntries = BuildCriticalHookTargetPlan(
        installPlan.profile.getDirectoryEntriesOffset, features.getDirectoryEntriesOffset,
        features.getDirectoryEntriesSource, derivedFlags.getDirectoryEntries, profileTrusted);
    installPlan.addDirectoryEntriesFromLowerFs =
        BuildCriticalHookTargetPlan(installPlan.profile.addDirectoryEntriesFromLowerFsOffset,
                                    features.addDirectoryEntriesFromLowerFsOffset,
                                    features.addDirectoryEntriesFromLowerFsSource,
                                    derivedFlags.addDirectoryEntriesFromLowerFs, profileTrusted);
    installPlan.addDirectoryEntriesFromLowerFsThunk = {
        installPlan.profile.addDirectoryEntriesFromLowerFsThunkOffset,
        derivedFlags.addDirectoryEntriesFromLowerFsThunk
            ? HookResolutionSource::kDerivedLayout
            : (profileTrusted ? HookResolutionSource::kProfileFallback
                              : HookResolutionSource::kUnavailable),
    };
    installPlan.pfReaddir = BuildCriticalHookTargetPlan(
        installPlan.profile.pfReaddirOffset, features.pfReaddirOffset, features.pfReaddirSource,
        derivedFlags.pfReaddir, profileTrusted);
    installPlan.pfReaddirPostfilter = BuildCriticalHookTargetPlan(
        installPlan.profile.pfReaddirPostfilterOffset, features.pfReaddirPostfilterOffset,
        features.pfReaddirPostfilterSource, derivedFlags.pfReaddirPostfilter, profileTrusted);
    installPlan.pfReaddirplus = BuildCriticalHookTargetPlan(
        installPlan.profile.pfReaddirplusOffset, features.pfReaddirplusOffset,
        features.pfReaddirplusSource, derivedFlags.pfReaddirplus, profileTrusted);

    const int safeScore = std::max(bestScore, 0);
    if (resolvedCount == 0) {
        __android_log_print(5, kLogTag,
                            "device hook profile unavailable=%s without resolved features path=%s; "
                            "profile offsets disabled",
                            installPlan.profile.name, module.path.c_str());
    } else if (!profileTrusted) {
        __android_log_print(
            5, kLogTag,
            "device hook profile untrusted name=%s score=%d/%d ties=%d overrides=%d "
            "path=%s; unresolved offsets disabled",
            installPlan.profile.name, safeScore, resolvedCount, bestTies, overrideCount,
            module.path.c_str());
    } else {
        __android_log_print(4, kLogTag,
                            "selected device hook profile=%s score=%d/%d overrides=%d derived=%d "
                            "path=%s",
                            installPlan.profile.name, safeScore, resolvedCount, overrideCount,
                            derivedCount, module.path.c_str());
    }

    std::lock_guard<std::mutex> lock(cacheMutex);
    auto [it, _] = cache.insert_or_assign(module.path, installPlan);
    return it->second;
}

DeviceHookProfile ResolveEffectiveDeviceHookProfile(const ModuleInfo& module) {
    return ResolveDeviceHookInstallPlan(module).profile;
}

}  // namespace fusehide
