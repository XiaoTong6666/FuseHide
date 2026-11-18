// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#include "fusehide/hooks/hook_install_internal.hpp"

namespace fusehide {

HookInstallProcessState& Process() {
    static HookInstallProcessState process;
    return process;
}

void* GetDirectoryEntriesReplacement() {
    switch (gDirectoryEntriesAbi.load(std::memory_order_acquire)) {
        case DirectoryEntriesAbi::kSharedPtr:
            return reinterpret_cast<void*>(+WrappedGetDirectoryEntriesShared);
        case DirectoryEntriesAbi::kValue:
            return reinterpret_cast<void*>(+WrappedGetDirectoryEntriesValue);
        case DirectoryEntriesAbi::kUnknown:
            return nullptr;
    }
    return nullptr;
}

void* AddDirectoryEntriesFromLowerFsReplacement() {
    switch (gDirectoryEntriesAbi.load(std::memory_order_acquire)) {
        case DirectoryEntriesAbi::kSharedPtr:
            return reinterpret_cast<void*>(+WrappedAddDirectoryEntriesFromLowerFsShared);
        case DirectoryEntriesAbi::kValue:
            return reinterpret_cast<void*>(+WrappedAddDirectoryEntriesFromLowerFsValue);
        case DirectoryEntriesAbi::kUnknown:
            return nullptr;
    }
    return nullptr;
}

// Compare hook installer
// Original signature (simplified):
//   void InstallCompareHook(ElfInfo* elfInfo, const char* symbolName,
//                           const char* altSymbolName, void* replacement,
//                           void** backup, const char* altMangledName)
//
// It calls function to find relocation slots for symbolName.
// If altSymbolName is provided and no slots found, tries altSymbolName.
// Then iterates slots, mprotect + patch + flush.
// Log format on no slots: "no %s found" with the label name.
// Log format on mprotect failure: "failed with %d %s: mprotect"
// Restores memory permissions to PROT_READ after patching.

void InstallCompareHook(const ElfInfo& elfInfo, std::string_view symbolName,
                        std::string_view altSymbolName, void* replacement, void** backup,
                        const char* displayLabel) {
    // Find symbol index via GNU hash
    const auto* nameData = reinterpret_cast<const uint8_t*>(symbolName.data());
    const size_t nameLen = symbolName.size();

    auto symIdx = FindDynamicSymbolIndex(*elfInfo.mapped, *elfInfo.dynInfo, nameData, nameLen);

    std::vector<uintptr_t> slots;
    if (symIdx.has_value()) {
        slots = FindRelocationSlotsForSymbol(*elfInfo.mapped, *elfInfo.dynInfo, *symIdx,
                                             elfInfo.loadBias);
    }

    // Try alt symbol name if no slots found and alt is provided
    if (!altSymbolName.empty() && slots.empty()) {
        const auto* altData = reinterpret_cast<const uint8_t*>(altSymbolName.data());
        const size_t altLen = altSymbolName.size();

        symIdx = FindDynamicSymbolIndex(*elfInfo.mapped, *elfInfo.dynInfo, altData, altLen);
        if (symIdx.has_value()) {
            slots = FindRelocationSlotsForSymbol(*elfInfo.mapped, *elfInfo.dynInfo, *symIdx,
                                                 elfInfo.loadBias);
        }
    }

    if (slots.empty()) {
        __android_log_print(6, kLogTag, "no %s found", displayLabel);
        return;
    }

    __android_log_print(4, kLogTag, "compare hook %s slots=%zu primary=%s alt=%s", displayLabel,
                        slots.size(), std::string(symbolName).c_str(),
                        altSymbolName.empty() ? "" : std::string(altSymbolName).c_str());

    // Patch each slot
    for (const auto slotAddr : slots) {
        auto* slot = reinterpret_cast<void**>(slotAddr);
        const uintptr_t pageMask = ~static_cast<uintptr_t>(elfInfo.pageSize - 1);
        const auto* pageStart = reinterpret_cast<void*>(slotAddr & pageMask);

        if (mprotect(const_cast<void*>(pageStart), elfInfo.pageSizeRaw, PROT_READ | PROT_WRITE) <
            0) {
            const int err = errno;
            __android_log_print(6, kLogTag, "failed with %d %s: mprotect", err, strerror(err));
            continue;
        }

        // Store backup (first slot only, matching original)
        if (backup != nullptr) {
            *backup = *slot;
        }

        // Patch
        *slot = replacement;

        __android_log_print(3, kLogTag, "patched %s slot=%p old=%p new=%p", displayLabel,
                            reinterpret_cast<void*>(slotAddr),
                            backup != nullptr ? *backup : nullptr, replacement);

        // Flush instruction cache
        const auto flushEnd = reinterpret_cast<void*>((slotAddr + elfInfo.pageSizeRaw) & pageMask);
        FlushCodeRange(const_cast<void*>(pageStart), flushEnd);

        // Restore original memory permissions
        mprotect(const_cast<void*>(pageStart), elfInfo.pageSizeRaw, PROT_READ);
    }
}

bool TryInstallInlineHookAt(void* target, void* replacement, void** backup,
                            const char* failureMessage);

bool TryInstallCriticalHookFromPlan(const ModuleInfo& module, const CriticalHookTargetPlan& target,
                                    const char* hookName, void* replacement, void** backup,
                                    const char* failureMessage) {
    if (!CanUseProfileBasedTarget(target.source)) {
        DebugLogPrint(4, "skip profile-based hook %s source=%s on unsupported profile arch path=%s",
                      hookName, HookResolutionSourceName(target.source), module.path.c_str());
        return false;
    }
    __android_log_print(4, kLogTag, "using hook target %s source=%s offset=0x%zx target=%p",
                        hookName, HookResolutionSourceName(target.source),
                        static_cast<size_t>(target.offset),
                        reinterpret_cast<void*>(module.base + target.offset));
    return TryInstallInlineHookAt(reinterpret_cast<void*>(module.base + target.offset), replacement,
                                  backup, failureMessage);
}

bool TryInstallDeviceProfileInlineHook(const ModuleInfo& module, uintptr_t offset,
                                       const char* hookName, void* replacement, void** backup,
                                       const char* failureMessage) {
    if (offset == 0) {
        DebugLogPrint(4, "skip unresolved device profile hook %s path=%s", hookName,
                      module.path.c_str());
        return false;
    }
    if constexpr (!kSupportsDeviceProfileOffsets) {
        DebugLogPrint(4, "skip device profile inline hook %s on unsupported profile arch path=%s",
                      hookName, module.path.c_str());
        return false;
    }
    return TryInstallInlineHookAt(reinterpret_cast<void*>(module.base + offset), replacement,
                                  backup, failureMessage);
}

// Inline hook installer (for path functions)

template <size_t N>
bool HasResolvableSymbol(const ModuleInfo& module, const std::string_view (&symbols)[N]) {
    const bool useRuntimeElf = module.path.find("!/") != std::string::npos;
    for (const auto& symbol : symbols) {
        auto resolved = useRuntimeElf ? ResolveTargetSymbolRuntime(module, symbol)
                                      : ResolveTargetSymbol(module, symbol);
        if (resolved.has_value()) {
            return true;
        }
    }
    return false;
}

void RefreshCoreHookStatus(const ModuleInfo& module, CoreHookStatus* status) {
    if (status == nullptr) {
        return;
    }
    status->appAccessible = Process().originalIsAppAccessiblePath != nullptr;
    status->packageCoveredByComparePath = Process().originalIsPackageOwnedPath == nullptr &&
                                          Process().originalEqualsIgnoreCase != nullptr &&
                                          Process().originalStrcasecmp != nullptr &&
                                          HasResolvableSymbol(module, kContainsMountSymbols);
    status->packageOwned =
        Process().originalIsPackageOwnedPath != nullptr || status->packageCoveredByComparePath;
    status->bpfBacking = Process().originalIsBpfBackingPath != nullptr;
    status->strcasecmp = Process().originalStrcasecmp != nullptr;
    status->equalsIgnoreCase = Process().originalEqualsIgnoreCase != nullptr;
}

bool HasAllCoreHooks(const CoreHookStatus& status) {
    return status.appAccessible && status.packageOwned && status.bpfBacking && status.strcasecmp &&
           status.equalsIgnoreCase;
}

void LogCoreHookStatus(const char* stage, const CoreHookStatus& status) {
    __android_log_print(4, kLogTag,
                        "%s core hooks app=%d package=%d package_compare=%d bpf=%d strcasecmp=%d "
                        "equals=%d",
                        stage, status.appAccessible, status.packageOwned,
                        status.packageCoveredByComparePath, status.bpfBacking, status.strcasecmp,
                        status.equalsIgnoreCase);
}

std::optional<FileElfContext> BuildFileElfContext(const ModuleInfo& module) {
    std::optional<MappedFile> mapped;
    if (module.path.find("!/") != std::string::npos) {
        mapped = MapEmbeddedStoredElf(module.path);
    } else {
        mapped = MapReadOnlyFile(module.path, module.fileOffset);
    }
    if (!mapped.has_value()) {
        return std::nullopt;
    }

    auto dynInfo = ParseDynamicInfo(*mapped);
    if (!dynInfo.has_value()) {
        return std::nullopt;
    }

    FileElfContext context;
    context.mapped = std::move(*mapped);
    context.dynInfo = std::move(*dynInfo);
    const int pageSize = getpagesize();
    context.elfInfo.loadBias = module.base;
    context.elfInfo.pageSize = pageSize;
    context.elfInfo.pageSizeRaw = pageSize;
    context.elfInfo.mapped = &context.mapped;
    context.elfInfo.dynInfo = &context.dynInfo;
    return context;
}

bool TryInstallInlineHookAt(void* target, void* replacement, void** backup,
                            const char* failureMessage) {
    // The strict ABI publishes via an atomic store before DobbyCommitHook.
    // A failed Commit that was ever visible must keep its backup callable;
    // such a slot must NOT be misreported as a successfully installed hook.
    static std::mutex failedPublicationMutex;
    static std::array<void**, 256> failedPublicationSlots{};
    auto publicationFailed = [&]() {
        std::lock_guard lock(failedPublicationMutex);
        return std::find(failedPublicationSlots.begin(), failedPublicationSlots.end(), backup) !=
               failedPublicationSlots.end();
    };
    auto retainFailedPublication = [&]() {
        std::lock_guard lock(failedPublicationMutex);
        for (auto& slot : failedPublicationSlots) {
            if (slot == backup)
                return;
            if (slot == nullptr) {
                slot = backup;
                return;
            }
        }
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "failed publication table full: %s",
                            failureMessage);
    };
    auto loadBackup = [&]() -> void* {
        return backup ? __atomic_load_n(backup, __ATOMIC_ACQUIRE) : nullptr;
    };
    std::string_view hookName(failureMessage);
    if (hookName.starts_with("hook ")) {
        hookName.remove_prefix(5);
    }
    if (hookName.ends_with(" failed")) {
        hookName.remove_suffix(7);
    }
    if (backup != nullptr && publicationFailed()) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "previous hook may have published: %s target=%p", failureMessage,
                            target);
        return false;
    }
    if (backup != nullptr && loadBackup() != nullptr) {
        __android_log_print(
            4, kLogTag, "hook already installed %.*s target=%p replacement=%p backup=%p",
            static_cast<int>(hookName.size()), hookName.data(), target, replacement, loadBackup());
        return true;
    }
    if (gHookInstaller == nullptr) {
        __android_log_print(6, kLogTag, "hook installer is null for %s", failureMessage);
        return false;
    }
    const int status = gStrictHookInstaller && backup
                           ? gStrictHookInstaller(
                                 target, replacement, backup,
                                 +[](void* user_data, void* original) {
                                     auto* location = static_cast<void**>(user_data);
                                     __atomic_store_n(location, original, __ATOMIC_RELEASE);
                                 })
                           : [&]() {
                                 // Legacy v2 has no publication callback. Never let it write to
                                 // storage read by atomic HookOriginal::get() with a plain store.
                                 void* candidate = nullptr;
                                 const int legacy_status = gHookInstaller(
                                     target, replacement, backup ? &candidate : nullptr);
                                 if (backup && candidate)
                                     __atomic_store_n(backup, candidate, __ATOMIC_RELEASE);
                                 return legacy_status;
                             }();
    if (status != 0) {
        if (backup && loadBackup())
            retainFailedPublication();
        __android_log_print(6, kLogTag, "%s: %d", failureMessage, status);
        return false;
    }
    if (backup && !loadBackup()) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "successful hook returned no original: %s",
                            failureMessage);
        retainFailedPublication();
        return false;
    }
    __android_log_print(4, kLogTag, "hook installed %.*s target=%p replacement=%p backup=%p",
                        static_cast<int>(hookName.size()), hookName.data(), target, replacement,
                        loadBackup());
    return true;
}

bool TryInstallFileInlineHook(const ModuleInfo& module, std::string_view symbolName,
                              void* replacement, void** backup, const char* failureMessage) {
    auto target = ResolveTargetSymbol(module, symbolName);
    if (!target.has_value()) {
        __android_log_print(3, kLogTag, "resolve failed %s", std::string(symbolName).c_str());
        return false;
    }
    DebugLogPrint(4, "install file inline symbol=%s target=%p replacement=%p path=%s",
                  std::string(symbolName).c_str(), *target, replacement, module.path.c_str());
    return TryInstallInlineHookAt(*target, replacement, backup, failureMessage);
}

bool TryInstallRuntimeInlineHook(const ModuleInfo& module, std::string_view symbolName,
                                 void* replacement, void** backup, const char* failureMessage) {
    auto target = ResolveTargetSymbolRuntime(module, symbolName);
    if (!target.has_value()) {
        __android_log_print(3, kLogTag, "runtime resolve failed %s",
                            std::string(symbolName).c_str());
        return false;
    }
    DebugLogPrint(4, "install runtime inline symbol=%s target=%p replacement=%p path=%s",
                  std::string(symbolName).c_str(), *target, replacement, module.path.c_str());
    return TryInstallInlineHookAt(*target, replacement, backup, failureMessage);
}

void InstallFileCompareHookIfNeeded(const ElfInfo& elfInfo, std::string_view primary,
                                    std::string_view alt, void* replacement, void** backup,
                                    const char* label) {
    if (backup != nullptr && *backup != nullptr) {
        return;
    }
    InstallCompareHook(elfInfo, primary, alt, replacement, backup, label);
}

void PatchRuntimeRelocationSlots(const RuntimeDynamicInfo& runtimeDyn, uintptr_t moduleBase,
                                 int pageSize, std::string_view primary, std::string_view alt,
                                 void* replacement, void** backup, const char* label) {
    if (backup != nullptr && *backup != nullptr) {
        return;
    }

    auto idx = FindRuntimeSymbolIndex(runtimeDyn, reinterpret_cast<const uint8_t*>(primary.data()),
                                      primary.size());
    std::vector<uintptr_t> slots;
    if (idx.has_value()) {
        slots = FindRuntimeRelocationSlotsForSymbol(runtimeDyn, *idx, moduleBase);
    }
    if (slots.empty() && !alt.empty()) {
        idx = FindRuntimeSymbolIndex(runtimeDyn, reinterpret_cast<const uint8_t*>(alt.data()),
                                     alt.size());
        if (idx.has_value()) {
            slots = FindRuntimeRelocationSlotsForSymbol(runtimeDyn, *idx, moduleBase);
        }
    }
    if (slots.empty()) {
        __android_log_print(6, kLogTag, "no %s found", label);
        return;
    }

    __android_log_print(4, kLogTag, "compare hook %s slots=%zu", label, slots.size());
    for (const auto slotAddr : slots) {
        auto* slot = reinterpret_cast<void**>(slotAddr);
        const uintptr_t pageMask = ~static_cast<uintptr_t>(pageSize - 1);
        auto* pageStart = reinterpret_cast<void*>(slotAddr & pageMask);
        if (mprotect(pageStart, pageSize, PROT_READ | PROT_WRITE) < 0) {
            const int err = errno;
            __android_log_print(6, kLogTag, "failed with %d %s: mprotect", err, strerror(err));
            continue;
        }
        if (backup != nullptr) {
            *backup = *slot;
        }
        *slot = replacement;
        __android_log_print(3, kLogTag, "patched %s slot=%p old=%p new=%p", label,
                            reinterpret_cast<void*>(slotAddr),
                            backup != nullptr ? *backup : nullptr, replacement);
        FlushCodeRange(pageStart, reinterpret_cast<void*>((slotAddr + pageSize) & pageMask));
        mprotect(pageStart, pageSize, PROT_READ);
    }
}

// Called by the Zygisk module in preAppSpecialize, before app threads load
// libfuse_jni.so. Patches the importer's single GOT slot, never a live code
// instruction and never a private fuse_session callback table.
extern "C" __attribute__((visibility("default"))) bool FuseHideInstallEarlyLoaderGotHook(
    void* replacement, void** originalOut) {
#if defined(__x86_64__)
    if (!replacement || !originalOut || *originalOut != nullptr)
        return false;
    ModuleInfo importer;
    dl_iterate_phdr(
        [](dl_phdr_info* info, size_t, void* opaque) -> int {
            if (!info || !info->dlpi_name ||
                std::string_view(info->dlpi_name).find("/libnativeloader.so") ==
                    std::string_view::npos)
                return 0;
            auto& result = *static_cast<ModuleInfo*>(opaque);
            result.base = static_cast<uintptr_t>(info->dlpi_addr);
            result.path = info->dlpi_name;
            result.phdrs = reinterpret_cast<const ElfProgramHeader*>(info->dlpi_phdr);
            result.phnum = info->dlpi_phnum;
            return 1;
        },
        &importer);
    if (!importer.base || !importer.phdrs || !importer.phnum) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "early loader hook unavailable: libnativeloader ELF not loaded");
        return false;
    }
    const auto dynamic = ParseRuntimeDynamicInfo(importer);
    if (!dynamic) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "early loader hook unavailable: libnativeloader dynamic data absent");
        return false;
    }
    constexpr std::string_view symbol = "android_dlopen_ext";
    const auto index = FindRuntimeSymbolIndex(
        *dynamic, reinterpret_cast<const uint8_t*>(symbol.data()), symbol.size());
    if (!index) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "early loader hook unavailable: android_dlopen_ext import absent");
        return false;
    }
    const auto slots = FindRuntimeRelocationSlotsForSymbol(*dynamic, *index, importer.base);
    if (slots.size() != 1) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "early loader hook refused: relocation count=%zu", slots.size());
        return false;
    }
    auto* slot = reinterpret_cast<void**>(slots.front());
    const void* expectedOriginal = dlsym(RTLD_DEFAULT, "android_dlopen_ext");
    if (expectedOriginal == nullptr || *slot != expectedOriginal) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "early loader hook refused: unexpected relocation target");
        return false;
    }
    const uintptr_t pageSize = static_cast<uintptr_t>(getpagesize());
    const uintptr_t page = slots.front() & ~(pageSize - 1);
    if (mprotect(reinterpret_cast<void*>(page), pageSize, PROT_READ | PROT_WRITE) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "early loader hook mprotect failed errno=%d", errno);
        return false;
    }
    *originalOut = const_cast<void*>(expectedOriginal);
    __atomic_store_n(slot, replacement, __ATOMIC_RELEASE);
    const int restore = mprotect(reinterpret_cast<void*>(page), pageSize, PROT_READ);
    if (restore != 0) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "early loader hook page protection restore failed errno=%d", errno);
        // Leave the hook active so its original pointer remains valid.
    }
    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "early loader GOT hook installed module=%s slot=%p", importer.path.c_str(),
                        slot);
    return true;
#else
    (void)replacement;
    (void)originalOut;
    return false;
#endif
}

bool InstallHookForSymbol(std::string_view symbolName, void* replacement, void** backup,
                          const char* failureMessage) {
    if (backup != nullptr && *backup != nullptr) {
        return true;
    }
    auto module = FindTargetModule();
    if (!module.has_value()) {
        __android_log_print(6, kLogTag, "no %s found", kTargetLibrary);
        return false;
    }
    const bool useRuntimeElf = module->path.find("!/") != std::string::npos;
    return useRuntimeElf
               ? TryInstallRuntimeInlineHook(*module, symbolName, replacement, backup,
                                             failureMessage)
               : TryInstallFileInlineHook(*module, symbolName, replacement, backup, failureMessage);
}

}  // namespace fusehide
