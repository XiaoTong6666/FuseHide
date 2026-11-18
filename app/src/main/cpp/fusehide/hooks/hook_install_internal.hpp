// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "fusehide/hooks/hook_resolver.hpp"
#include "fusehide/hooks/wrappers.hpp"

namespace fusehide {

struct HookInstallProcessState {
    void*& originalIsAppAccessiblePath = gOriginalIsAppAccessiblePath.rawStorage();
    void*& originalIsPackageOwnedPath = gOriginalIsPackageOwnedPath.rawStorage();
    void*& originalIsBpfBackingPath = gOriginalIsBpfBackingPath.rawStorage();
    void*& originalStrcasecmp = gOriginalStrcasecmp.rawStorage();
    void*& originalEqualsIgnoreCase = gOriginalEqualsIgnoreCase.rawStorage();
    void*& originalPfLookup = gOriginalPfLookup.rawStorage();
    void*& originalPfForget = gOriginalPfForget.rawStorage();
    void*& originalPfForgetMulti = gOriginalPfForgetMulti.rawStorage();
    void*& originalNodeDeleted = gOriginalNodeDeleted.rawStorage();
    void*& originalPfLookupPostfilter = gOriginalPfLookupPostfilter.rawStorage();
    void*& originalPfAccess = gOriginalPfAccess.rawStorage();
    void*& originalPfOpen = gOriginalPfOpen.rawStorage();
    void*& originalPfOpendir = gOriginalPfOpendir.rawStorage();
    void*& originalPfMknod = gOriginalPfMknod.rawStorage();
    void*& originalPfMkdir = gOriginalPfMkdir.rawStorage();
    void*& originalPfUnlink = gOriginalPfUnlink.rawStorage();
    void*& originalPfRmdir = gOriginalPfRmdir.rawStorage();
    void*& originalPfRename = gOriginalPfRename.rawStorage();
    void*& originalPfCreate = gOriginalPfCreate.rawStorage();
    void*& originalPfReaddir = gOriginalPfReaddir.rawStorage();
    void*& originalPfReaddirPostfilter = gOriginalPfReaddirPostfilter.rawStorage();
    void*& originalPfReaddirplus = gOriginalPfReaddirplus.rawStorage();
    void*& originalDoReaddirCommon = gOriginalDoReaddirCommon.rawStorage();
    void*& originalPfGetattr = gOriginalPfGetattr.rawStorage();
    void*& originalOpen = gOriginalOpen.rawStorage();
    void*& originalOpen2 = gOriginalOpen2.rawStorage();
    void*& fuseReqCtx = gFuseReqCtx.rawStorage();
    void*& originalMkdir = gOriginalMkdir.rawStorage();
    void*& originalMknod = gOriginalMknod.rawStorage();
    void*& originalLstat = gOriginalLstat.rawStorage();
    void*& originalStat = gOriginalStat.rawStorage();
    void*& originalGetxattr = gOriginalGetxattr.rawStorage();
    void*& originalLgetxattr = gOriginalLgetxattr.rawStorage();
    void*& originalShouldNotCache = gOriginalShouldNotCache.rawStorage();
    void*& originalNotifyInvalEntry = gOriginalNotifyInvalEntry.rawStorage();
    void*& originalNotifyInvalInode = gOriginalNotifyInvalInode.rawStorage();
    void*& originalFuseSessionDestroy = gOriginalFuseSessionDestroy.rawStorage();
    void*& originalReplyAttr = gOriginalReplyAttr.rawStorage();
    void*& originalReplyEntry = gOriginalReplyEntry.rawStorage();
    void*& originalReplyBuf = gOriginalReplyBuf.rawStorage();
    void*& originalReplyErr = gOriginalReplyErr.rawStorage();
    void*& originalGetDirectoryEntries = gOriginalGetDirectoryEntries.rawStorage();
    void*& originalAddDirectoryEntriesFromLowerFs =
        gOriginalAddDirectoryEntriesFromLowerFs.rawStorage();
};

struct CoreHookStatus {
    bool appAccessible = false;
    bool packageOwned = false;
    bool packageCoveredByComparePath = false;
    bool bpfBacking = false;
    bool strcasecmp = false;
    bool equalsIgnoreCase = false;
};

struct ElfInfo {
    uintptr_t loadBias = 0;  // used for slot address computation
    int pageSize = 0;        // ~pageSize mask for mprotect
    int pageSizeRaw = 0;     // pageSize value for mprotect length

    // These come from the parsed ELF, stored once by the init function
    const MappedFile* mapped = nullptr;
    const DynamicInfo* dynInfo = nullptr;
};

struct FileElfContext {
    MappedFile mapped;
    DynamicInfo dynInfo;
    ElfInfo elfInfo;
};

// Shared installer state is defined in one .cpp; never duplicate backups.
HookInstallProcessState& Process();
void* GetDirectoryEntriesReplacement();
void* AddDirectoryEntriesFromLowerFsReplacement();
void RefreshCoreHookStatus(const ModuleInfo& module, CoreHookStatus* status);
bool HasAllCoreHooks(const CoreHookStatus& status);
void LogCoreHookStatus(const char* stage, const CoreHookStatus& status);
std::optional<FileElfContext> BuildFileElfContext(const ModuleInfo& module);
bool TryInstallInlineHookAt(void* target, void* replacement, void** backup,
                            const char* failureMessage);
bool TryInstallCriticalHookFromPlan(const ModuleInfo& module, const CriticalHookTargetPlan& target,
                                    const char* hookName, void* replacement, void** backup,
                                    const char* failureMessage);
bool TryInstallDeviceProfileInlineHook(const ModuleInfo& module, uintptr_t offset,
                                       const char* hookName, void* replacement, void** backup,
                                       const char* failureMessage);
bool TryInstallFileInlineHook(const ModuleInfo& module, std::string_view symbolName,
                              void* replacement, void** backup, const char* failureMessage);
bool InstallHookForSymbol(std::string_view symbolName, void* replacement, void** backup,
                          const char* failureMessage);
template <size_t N>
bool InstallFirstAvailableFileInlineHook(const ModuleInfo& module,
                                         const std::string_view (&symbols)[N], void* replacement,
                                         void** backup, const char* failureMessage) {
    for (const auto& sym : symbols) {
        if (TryInstallFileInlineHook(module, sym, replacement, backup, failureMessage)) {
            return true;
        }
    }
    return false;
}

template <size_t N>
bool InstallFirstAvailableInlineHook(const std::string_view (&symbols)[N], void* replacement,
                                     void** backup, const char* failureMessage) {
    for (const auto& sym : symbols) {
        if (InstallHookForSymbol(sym, replacement, backup, failureMessage)) {
            return true;
        }
    }
    return false;
}

void InstallFileCompareHookIfNeeded(const ElfInfo& elfInfo, std::string_view primary,
                                    std::string_view alt, void* replacement, void** backup,
                                    const char* label);
void PatchRuntimeRelocationSlots(const RuntimeDynamicInfo& runtimeDyn, uintptr_t moduleBase,
                                 int pageSize, std::string_view primary, std::string_view alt,
                                 void* replacement, void** backup, const char* label);

void InstallMinimalCoreHooks(const ModuleInfo& module, const FileElfContext& fileContext,
                             CoreHookStatus* status);
void InstallMinimalDebugHooks(const ModuleInfo& module, const FileElfContext& fileContext);
void InstallAdvancedCoreHooks(const ModuleInfo& module, CoreHookStatus* status);
void InstallAdvancedDebugHooks(const ModuleInfo& module);
#if defined(__x86_64__) || defined(__aarch64__)
void InstallLowlevelCallbackFallback(const ModuleInfo& module, const RuntimeDynamicInfo& dyn);
void LogEarlySessionHookStatus();
#endif

}  // namespace fusehide
