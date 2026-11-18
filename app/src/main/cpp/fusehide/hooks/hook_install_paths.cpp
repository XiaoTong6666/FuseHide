// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#include "fusehide/hooks/hook_install_internal.hpp"

namespace fusehide {

// File-backed hooks are preferred first because they let us resolve internal symbols by name even
// when the runtime ELF image is embedded inside an APK path.
void InstallMinimalCoreHooks(const ModuleInfo& module, const FileElfContext& fileContext,
                             CoreHookStatus* status) {
    auto& process = Process();
    const DeviceHookInstallPlan installPlan = ResolveDeviceHookInstallPlan(module);
    const DeviceHookProfile& deviceProfile = installPlan.profile;
    const bool hasClassicStringAbi = CanUseClassicStringWrappers(installPlan);
    if (hasClassicStringAbi) {
        InstallFirstAvailableFileInlineHook(
            module, kIsAppAccessiblePathSymbols,
            reinterpret_cast<void*>(+WrappedIsAppAccessiblePath),
            reinterpret_cast<void**>(&process.originalIsAppAccessiblePath),
            "hook is_app_accessible_path failed");
        InstallFirstAvailableFileInlineHook(
            module, kIsPackageOwnedPathSymbols, reinterpret_cast<void*>(+WrappedIsPackageOwnedPath),
            reinterpret_cast<void**>(&process.originalIsPackageOwnedPath),
            "hook is_package_owned_path failed");
        InstallFirstAvailableFileInlineHook(
            module, kIsBpfBackingPathSymbols, reinterpret_cast<void*>(+WrappedIsBpfBackingPath),
            reinterpret_cast<void**>(&process.originalIsBpfBackingPath),
            "hook is_bpf_backing_path failed");
    }

    InstallFileCompareHookIfNeeded(fileContext.elfInfo, kStrcasecmpSymbol, kStrcasecmpSymbol,
                                   reinterpret_cast<void*>(+WrappedStrcasecmp),
                                   &process.originalStrcasecmp, "strcasecmp");
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, kEqualsIgnoreCaseSymbols[0],
                                   kEqualsIgnoreCaseSymbols[1],
                                   reinterpret_cast<void*>(+WrappedEqualsIgnoreCaseAbi),
                                   &process.originalEqualsIgnoreCase, "EqualsIgnoreCase");

    if (hasClassicStringAbi && process.originalIsAppAccessiblePath == nullptr) {
        // Reverse-engineered record: is_app_accessible_path @ 0x0017bb5c.
        // This is the shared access-policy gate reached by lookup/readdir/getattr and by several
        // inode-based handlers such as access/open/opendir. Those paths do not all have their own
        // dedicated wrappers, so leaving this unresolved would weaken hidden-path coverage on the
        // stripped production binary. Only use the device RVA after the name-based attempts above
        // have already failed.
        TryInstallDeviceProfileInlineHook(
            module, deviceProfile.isAppAccessiblePathOffset, "is_app_accessible_path",
            reinterpret_cast<void*>(+WrappedIsAppAccessiblePath),
            reinterpret_cast<void**>(&process.originalIsAppAccessiblePath),
            "hook is_app_accessible_path failed");
    }

    if (hasClassicStringAbi && Process().originalShouldNotCache == nullptr) {
        TryInstallCriticalHookFromPlan(
            module, installPlan.shouldNotCache, "ShouldNotCache", (void*)WrappedShouldNotCache,
            &Process().originalShouldNotCache, "hook ShouldNotCache failed");
    }

    RefreshCoreHookStatus(module, status);
}

// These hooks are functionally required for hidden-path semantics. Only the verbose trace logging
// inside the wrappers stays gated by kEnableDebugHooks.
void InstallMinimalDebugHooks(const ModuleInfo& module, const FileElfContext& fileContext) {
    auto& process = Process();
    if (process.originalNodeDeleted == nullptr)
        TryInstallFileInlineHook(module,
                                 "_ZN13mediaprovider4fuse11NodeTracker11NodeDeletedEPKNS0_4nodeE",
                                 (void*)WrappedNodeDeleted, &process.originalNodeDeleted,
                                 "hook NodeTracker::NodeDeleted failed");
    // These private static callbacks are optional: unknown device symbol layouts
    // must never be hooked using an assumed offset.
    if (process.originalPfForget == nullptr)
        TryInstallFileInlineHook(module, "_ZN13mediaprovider4fuseL9pf_forgetEP8fuse_reqmm",
                                 (void*)WrappedPfForget, &process.originalPfForget,
                                 "hook pf_forget failed");
    if (process.originalPfForgetMulti == nullptr)
        TryInstallFileInlineHook(
            module, "_ZN13mediaprovider4fuseL15pf_forget_multiEP8fuse_reqmP16fuse_forget_data",
            (void*)WrappedPfForgetMulti, &process.originalPfForgetMulti,
            "hook pf_forget_multi failed");
    const DeviceHookInstallPlan installPlan = ResolveDeviceHookInstallPlan(module);
    const DeviceHookProfile& deviceProfile = installPlan.profile;
    const bool canInstallDirectoryEntries = CanInstallDirectoryEntriesHooks(installPlan);
    void* getDirectoryEntriesReplacement =
        canInstallDirectoryEntries ? GetDirectoryEntriesReplacement() : nullptr;
    void* addDirectoryEntriesReplacement =
        canInstallDirectoryEntries ? AddDirectoryEntriesFromLowerFsReplacement() : nullptr;
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, "fuse_session_destroy",
                                   "fuse_session_destroy", (void*)WrappedFuseSessionDestroy,
                                   &process.originalFuseSessionDestroy, "fuse_session_destroy");
    InstallFileCompareHookIfNeeded(
        fileContext.elfInfo, "fuse_lowlevel_notify_inval_entry", "fuse_lowlevel_notify_inval_entry",
        (void*)WrappedNotifyInvalEntry, &process.originalNotifyInvalEntry, "notify_inval_entry");
    InstallFileCompareHookIfNeeded(
        fileContext.elfInfo, "fuse_lowlevel_notify_inval_inode", "fuse_lowlevel_notify_inval_inode",
        (void*)WrappedNotifyInvalInode, &process.originalNotifyInvalInode, "notify_inval_inode");
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, "fuse_reply_entry", "fuse_reply_entry",
                                   (void*)WrappedReplyEntry, &process.originalReplyEntry,
                                   "reply_entry");
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, "fuse_reply_attr", "fuse_reply_attr",
                                   (void*)WrappedReplyAttr, &process.originalReplyAttr,
                                   "reply_attr");
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, "fuse_reply_buf", "fuse_reply_buf",
                                   (void*)WrappedReplyBuf, &process.originalReplyBuf, "reply_buf");
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, "fuse_reply_err", "fuse_reply_err",
                                   (void*)WrappedReplyErr, &process.originalReplyErr, "reply_err");
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, "lstat", "lstat", (void*)WrappedLstat,
                                   &process.originalLstat, "lstat");
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, "stat", "stat", (void*)WrappedStat,
                                   &process.originalStat, "stat");
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, "getxattr", "getxattr",
                                   (void*)WrappedGetxattr, &process.originalGetxattr, "getxattr");
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, "lgetxattr", "lgetxattr",
                                   (void*)WrappedLgetxattr, &process.originalLgetxattr,
                                   "lgetxattr");
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, "mkdir", "mkdir", (void*)WrappedMkdirLibc,
                                   &process.originalMkdir, "mkdir");
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, "mknod", "mknod", (void*)WrappedMknod,
                                   &process.originalMknod, "mknod");
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, "open", "open", (void*)WrappedOpen,
                                   &process.originalOpen, "open");
    InstallFileCompareHookIfNeeded(fileContext.elfInfo, "__open_2", "__open_2", (void*)WrappedOpen2,
                                   &process.originalOpen2, "__open_2");
    if (process.originalGetDirectoryEntries == nullptr &&
        getDirectoryEntriesReplacement != nullptr) {
        TryInstallCriticalHookFromPlan(module, installPlan.getDirectoryEntries,
                                       "GetDirectoryEntries", getDirectoryEntriesReplacement,
                                       &process.originalGetDirectoryEntries,
                                       "hook GetDirectoryEntries failed");
    }
    if (process.originalAddDirectoryEntriesFromLowerFs == nullptr &&
        addDirectoryEntriesReplacement != nullptr) {
        TryInstallCriticalHookFromPlan(
            module, installPlan.addDirectoryEntriesFromLowerFs, "addDirectoryEntriesFromLowerFs",
            addDirectoryEntriesReplacement, &process.originalAddDirectoryEntriesFromLowerFs,
            "hook addDirectoryEntriesFromLowerFs failed");
    }
    if (process.originalAddDirectoryEntriesFromLowerFs == nullptr &&
        addDirectoryEntriesReplacement != nullptr) {
        TryInstallCriticalHookFromPlan(module, installPlan.addDirectoryEntriesFromLowerFsThunk,
                                       "addDirectoryEntriesFromLowerFsThunk",
                                       addDirectoryEntriesReplacement,
                                       &process.originalAddDirectoryEntriesFromLowerFs,
                                       "hook addDirectoryEntriesFromLowerFs thunk failed");
    }
    if (process.originalDoReaddirCommon == nullptr) {
        TryInstallCriticalHookFromPlan(
            module, installPlan.doReaddirCommon, "do_readdir_common", (void*)WrappedDoReaddirCommon,
            &process.originalDoReaddirCommon, "hook do_readdir_common failed");
    }
    if (process.originalPfMkdir == nullptr) {
        // Reverse-engineered record: pf_mkdir @ 0x00177050.
        // mkdir policy lives in an internal static handler, so keep the device-specific offset as a
        // backup when symbol-based lookup is unavailable.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfMkdirOffset, "pf_mkdir",
                                          (void*)WrappedPfMkdir, &process.originalPfMkdir,
                                          "hook pf_mkdir failed");
    }
    if (process.originalPfMknod == nullptr) {
        // Reverse-engineered record: pf_mknod @ 0x00176ba8.
        // Some create paths go through pf_mknod instead of pf_create on device builds.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfMknodOffset, "pf_mknod",
                                          (void*)WrappedPfMknod, &process.originalPfMknod,
                                          "hook pf_mknod failed");
    }
    if (process.originalPfUnlink == nullptr) {
        // Reverse-engineered record: pf_unlink @ 0x00177534.
        // unlink/rmdir/create handlers are internal statics in libfuse_jni, so retain the verified
        // offset fallback for devices that do not expose stable symbols.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfUnlinkOffset, "pf_unlink",
                                          (void*)WrappedPfUnlink, &process.originalPfUnlink,
                                          "hook pf_unlink failed");
    }
    if (process.originalPfRmdir == nullptr) {
        // Reverse-engineered record: pf_rmdir @ 0x00177920.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfRmdirOffset, "pf_rmdir",
                                          (void*)WrappedPfRmdir, &process.originalPfRmdir,
                                          "hook pf_rmdir failed");
    }
    if (process.originalPfRename == nullptr) {
        // Reverse-engineered record: pf_rename @ 0x00177ef4.
        // rename follows the same parent-only access pattern as create/delete handlers, so keep an
        // explicit device RVA fallback for builds that do not expose the local symbol.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfRenameOffset, "pf_rename",
                                          (void*)WrappedPfRename, &process.originalPfRename,
                                          "hook pf_rename failed");
    }
    if (process.originalPfCreate == nullptr) {
        // Reverse-engineered record: pf_create @ 0x0017a7c8.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfCreateOffset, "pf_create",
                                          (void*)WrappedPfCreate, &process.originalPfCreate,
                                          "hook pf_create failed");
    }
    if (process.originalPfReaddir == nullptr) {
        TryInstallCriticalHookFromPlan(module, installPlan.pfReaddir, "pf_readdir",
                                       (void*)WrappedPfReaddir, &process.originalPfReaddir,
                                       "hook pf_readdir failed");
    }
    if (process.originalPfReaddirplus == nullptr) {
        TryInstallCriticalHookFromPlan(module, installPlan.pfReaddirplus, "pf_readdirplus",
                                       (void*)WrappedPfReaddirplus, &process.originalPfReaddirplus,
                                       "hook pf_readdirplus failed");
    }
    if (process.originalPfReaddirPostfilter == nullptr) {
        TryInstallCriticalHookFromPlan(module, installPlan.pfReaddirPostfilter,
                                       "pf_readdir_postfilter", (void*)WrappedPfReaddirPostfilter,
                                       &process.originalPfReaddirPostfilter,
                                       "hook pf_readdir_postfilter failed");
    }

    if (process.originalPfLookup == nullptr) {
        TryInstallFileInlineHook(module, "_ZN13mediaprovider4fuseL9pf_lookupEP8fuse_reqmPKc",
                                 (void*)WrappedPfLookup, &process.originalPfLookup,
                                 "hook pf_lookup failed");
    }
    if (process.originalPfAccess == nullptr) {
        TryInstallFileInlineHook(module, "_ZN13mediaprovider4fuseL9pf_accessEP8fuse_reqmi",
                                 (void*)WrappedPfAccess, &process.originalPfAccess,
                                 "hook pf_access failed");
    }
    if (process.originalPfOpen == nullptr) {
        TryInstallFileInlineHook(
            module, "_ZN13mediaprovider4fuseL7pf_openEP8fuse_reqmP14fuse_file_info",
            (void*)WrappedPfOpen, &process.originalPfOpen, "hook pf_open failed");
    }
    if (process.originalPfOpendir == nullptr) {
        TryInstallFileInlineHook(
            module, "_ZN13mediaprovider4fuseL10pf_opendirEP8fuse_reqmP14fuse_file_info",
            (void*)WrappedPfOpendir, &process.originalPfOpendir, "hook pf_opendir failed");
    }
    if (process.originalPfMkdir == nullptr) {
        TryInstallFileInlineHook(module, "_ZN13mediaprovider4fuseL8pf_mkdirEP8fuse_reqmPKcj",
                                 (void*)WrappedPfMkdir, &process.originalPfMkdir,
                                 "hook pf_mkdir failed");
    }
    if (process.originalPfUnlink == nullptr) {
        TryInstallFileInlineHook(module, "_ZN13mediaprovider4fuseL9pf_unlinkEP8fuse_reqmPKc",
                                 (void*)WrappedPfUnlink, &process.originalPfUnlink,
                                 "hook pf_unlink failed");
    }
    if (process.originalPfRmdir == nullptr) {
        TryInstallFileInlineHook(module, "_ZN13mediaprovider4fuseL8pf_rmdirEP8fuse_reqmPKc",
                                 (void*)WrappedPfRmdir, &process.originalPfRmdir,
                                 "hook pf_rmdir failed");
    }
    if (process.originalPfRename == nullptr) {
        TryInstallFileInlineHook(module, "_ZN13mediaprovider4fuseL9pf_renameEP8fuse_reqmPKcmS4_j",
                                 (void*)WrappedPfRename, &process.originalPfRename,
                                 "hook pf_rename failed");
    }
    if (process.originalPfCreate == nullptr) {
        TryInstallFileInlineHook(
            module, "_ZN13mediaprovider4fuseL9pf_createEP8fuse_reqmPKcjP14fuse_file_info",
            (void*)WrappedPfCreate, &process.originalPfCreate, "hook pf_create failed");
    }
    if (process.originalPfLookupPostfilter == nullptr) {
        TryInstallFileInlineHook(module,
                                 "_ZN13mediaprovider4fuseL20pf_lookup_postfilterEP8fuse_"
                                 "reqmjPKcP14fuse_entry_outP18fuse_"
                                 "entry_bpf_out",
                                 (void*)WrappedPfLookupPostfilter,
                                 &process.originalPfLookupPostfilter,
                                 "hook pf_lookup_postfilter failed");
    }
    if (process.originalPfGetattr == nullptr) {
        TryInstallFileInlineHook(
            module, "_ZN13mediaprovider4fuseL10pf_getattrEP8fuse_reqmP14fuse_file_info",
            (void*)WrappedPfGetattr, &process.originalPfGetattr, "hook pf_getattr failed");
    }
    if (process.originalPfLookup == nullptr) {
        // Reverse-engineered record: pf_lookup @ 0x00175e48.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfLookupOffset, "pf_lookup",
                                          (void*)WrappedPfLookup, &process.originalPfLookup,
                                          "hook pf_lookup failed");
    }
    if (process.originalPfLookupPostfilter == nullptr) {
        // Reverse-engineered record: pf_lookup_postfilter @ 0x00175f90.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfLookupPostfilterOffset,
                                          "pf_lookup_postfilter", (void*)WrappedPfLookupPostfilter,
                                          &process.originalPfLookupPostfilter,
                                          "hook pf_lookup_postfilter failed");
    }
    if (process.originalPfGetattr == nullptr) {
        // Reverse-engineered record: pf_getattr @ 0x001762bc.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfGetattrOffset, "pf_getattr",
                                          (void*)WrappedPfGetattr, &process.originalPfGetattr,
                                          "hook pf_getattr failed");
    }
}

// When file-backed symbol lookup is unavailable, fall back to runtime relocation patching and
// verified device offsets recovered from the reverse-engineered libfuse_jni build.
void InstallAdvancedCoreHooks(const ModuleInfo& module, CoreHookStatus* status) {
    auto& process = Process();
    const DeviceHookInstallPlan installPlan = ResolveDeviceHookInstallPlan(module);
    const DeviceHookProfile& deviceProfile = installPlan.profile;
    const bool hasClassicStringAbi = CanUseClassicStringWrappers(installPlan);
    if (hasClassicStringAbi && !status->appAccessible) {
        InstallFirstAvailableInlineHook(
            kIsAppAccessiblePathSymbols, reinterpret_cast<void*>(+WrappedIsAppAccessiblePath),
            reinterpret_cast<void**>(&process.originalIsAppAccessiblePath),
            "hook is_app_accessible_path failed");
    }

    if (hasClassicStringAbi && !status->packageOwned) {
        InstallFirstAvailableInlineHook(
            kIsPackageOwnedPathSymbols, reinterpret_cast<void*>(+WrappedIsPackageOwnedPath),
            reinterpret_cast<void**>(&process.originalIsPackageOwnedPath),
            "hook is_package_owned_path failed");
    }

    if (hasClassicStringAbi && !status->bpfBacking) {
        InstallFirstAvailableInlineHook(kIsBpfBackingPathSymbols,
                                        reinterpret_cast<void*>(+WrappedIsBpfBackingPath),
                                        reinterpret_cast<void**>(&process.originalIsBpfBackingPath),
                                        "hook is_bpf_backing_path failed");
    }

    const bool useRuntimeElf = module.path.find("!/") != std::string::npos;
    const int ps = getpagesize();
    if (useRuntimeElf) {
        auto runtimeDyn = ParseRuntimeDynamicInfo(module);
        if (runtimeDyn.has_value()) {
            if (!status->strcasecmp) {
                PatchRuntimeRelocationSlots(*runtimeDyn, module.base, ps, kStrcasecmpSymbol,
                                            kStrcasecmpSymbol,
                                            reinterpret_cast<void*>(+WrappedStrcasecmp),
                                            &process.originalStrcasecmp, "strcasecmp");
            }
            if (!status->equalsIgnoreCase) {
                PatchRuntimeRelocationSlots(*runtimeDyn, module.base, ps,
                                            kEqualsIgnoreCaseSymbols[0],
                                            kEqualsIgnoreCaseSymbols[1],
                                            reinterpret_cast<void*>(+WrappedEqualsIgnoreCaseAbi),
                                            &process.originalEqualsIgnoreCase, "EqualsIgnoreCase");
            }
        }
    } else if (auto fileContext = BuildFileElfContext(module); fileContext.has_value()) {
        if (!status->strcasecmp) {
            InstallFileCompareHookIfNeeded(fileContext->elfInfo, kStrcasecmpSymbol,
                                           kStrcasecmpSymbol,
                                           reinterpret_cast<void*>(+WrappedStrcasecmp),
                                           &process.originalStrcasecmp, "strcasecmp");
        }
        if (!status->equalsIgnoreCase) {
            InstallFileCompareHookIfNeeded(fileContext->elfInfo, kEqualsIgnoreCaseSymbols[0],
                                           kEqualsIgnoreCaseSymbols[1],
                                           reinterpret_cast<void*>(+WrappedEqualsIgnoreCaseAbi),
                                           &process.originalEqualsIgnoreCase, "EqualsIgnoreCase");
        }
    }

    if (hasClassicStringAbi && process.originalIsAppAccessiblePath == nullptr) {
        // Reverse-engineered record: is_app_accessible_path @ 0x0017bb5c.
        // Repeat the same last-resort fallback here because the advanced path runs when the
        // initial file-backed install was unavailable or still failed to resolve this stripped
        // internal helper by name.
        TryInstallDeviceProfileInlineHook(
            module, deviceProfile.isAppAccessiblePathOffset, "is_app_accessible_path",
            reinterpret_cast<void*>(+WrappedIsAppAccessiblePath),
            reinterpret_cast<void**>(&process.originalIsAppAccessiblePath),
            "hook is_app_accessible_path failed");
    }

    if (hasClassicStringAbi && Process().originalShouldNotCache == nullptr) {
        TryInstallCriticalHookFromPlan(
            module, installPlan.shouldNotCache, "ShouldNotCache", (void*)WrappedShouldNotCache,
            &Process().originalShouldNotCache, "hook ShouldNotCache failed");
    }

    RefreshCoreHookStatus(module, status);
}

// Advanced debug hooks extend the functional set with device-specific inline hooks for lookup,
// create, rename, readdir, and invalidation code paths that are not reliably exposed through
// imported symbols.
void InstallAdvancedDebugHooks(const ModuleInfo& module) {
    auto& process = Process();
    if (process.originalNodeDeleted == nullptr)
        InstallHookForSymbol("_ZN13mediaprovider4fuse11NodeTracker11NodeDeletedEPKNS0_4nodeE",
                             (void*)WrappedNodeDeleted, &process.originalNodeDeleted,
                             "hook NodeTracker::NodeDeleted failed");
    if (process.originalPfForget == nullptr)
        InstallHookForSymbol("_ZN13mediaprovider4fuseL9pf_forgetEP8fuse_reqmm",
                             (void*)WrappedPfForget, &process.originalPfForget,
                             "hook pf_forget failed");
    if (process.originalPfForgetMulti == nullptr)
        InstallHookForSymbol(
            "_ZN13mediaprovider4fuseL15pf_forget_multiEP8fuse_reqmP16fuse_forget_data",
            (void*)WrappedPfForgetMulti, &process.originalPfForgetMulti,
            "hook pf_forget_multi failed");
    DebugLogPrint(4, "hide-diag forget_hook_status single=%d multi=%d node_deleted=%d",
                  process.originalPfForget != nullptr ? 1 : 0,
                  process.originalPfForgetMulti != nullptr ? 1 : 0,
                  process.originalNodeDeleted != nullptr ? 1 : 0);
    const DeviceHookInstallPlan installPlan = ResolveDeviceHookInstallPlan(module);
    const DeviceHookProfile& deviceProfile = installPlan.profile;
    const bool canInstallDirectoryEntries = CanInstallDirectoryEntriesHooks(installPlan);
    void* getDirectoryEntriesReplacement =
        canInstallDirectoryEntries ? GetDirectoryEntriesReplacement() : nullptr;
    void* addDirectoryEntriesReplacement =
        canInstallDirectoryEntries ? AddDirectoryEntriesFromLowerFsReplacement() : nullptr;
    const bool useRuntimeElf = module.path.find("!/") != std::string::npos;
    if (useRuntimeElf) {
        auto runtimeDyn = ParseRuntimeDynamicInfo(module);
        if (runtimeDyn) {
            PatchRuntimeRelocationSlots(
                *runtimeDyn, module.base, getpagesize(), "fuse_session_destroy",
                "fuse_session_destroy", (void*)WrappedFuseSessionDestroy,
                &process.originalFuseSessionDestroy, "fuse_session_destroy");
            PatchRuntimeRelocationSlots(
                *runtimeDyn, module.base, getpagesize(), "fuse_lowlevel_notify_inval_entry",
                "fuse_lowlevel_notify_inval_entry", (void*)WrappedNotifyInvalEntry,
                &process.originalNotifyInvalEntry, "notify_inval_entry");
            PatchRuntimeRelocationSlots(
                *runtimeDyn, module.base, getpagesize(), "fuse_lowlevel_notify_inval_inode",
                "fuse_lowlevel_notify_inval_inode", (void*)WrappedNotifyInvalInode,
                &process.originalNotifyInvalInode, "notify_inval_inode");
            PatchRuntimeRelocationSlots(*runtimeDyn, module.base, getpagesize(), "fuse_reply_entry",
                                        "fuse_reply_entry", (void*)WrappedReplyEntry,
                                        &process.originalReplyEntry, "reply_entry");
            PatchRuntimeRelocationSlots(*runtimeDyn, module.base, getpagesize(), "fuse_reply_attr",
                                        "fuse_reply_attr", (void*)WrappedReplyAttr,
                                        &process.originalReplyAttr, "reply_attr");
            PatchRuntimeRelocationSlots(*runtimeDyn, module.base, getpagesize(), "fuse_reply_buf",
                                        "fuse_reply_buf", (void*)WrappedReplyBuf,
                                        &process.originalReplyBuf, "reply_buf");
            PatchRuntimeRelocationSlots(*runtimeDyn, module.base, getpagesize(), "fuse_reply_err",
                                        "fuse_reply_err", (void*)WrappedReplyErr,
                                        &process.originalReplyErr, "reply_err");
            // Session creation is intercepted in InstallFuseHooks() before plan
            // discovery and reply/GOT patching. Never install it a second time.
            PatchRuntimeRelocationSlots(*runtimeDyn, module.base, getpagesize(), "lstat", "lstat",
                                        (void*)WrappedLstat, &process.originalLstat, "lstat");
            PatchRuntimeRelocationSlots(*runtimeDyn, module.base, getpagesize(), "stat", "stat",
                                        (void*)WrappedStat, &process.originalStat, "stat");
            PatchRuntimeRelocationSlots(*runtimeDyn, module.base, getpagesize(), "getxattr",
                                        "getxattr", (void*)WrappedGetxattr,
                                        &process.originalGetxattr, "getxattr");
            PatchRuntimeRelocationSlots(*runtimeDyn, module.base, getpagesize(), "lgetxattr",
                                        "lgetxattr", (void*)WrappedLgetxattr,
                                        &process.originalLgetxattr, "lgetxattr");
            PatchRuntimeRelocationSlots(*runtimeDyn, module.base, getpagesize(), "mkdir", "mkdir",
                                        (void*)WrappedMkdirLibc, &process.originalMkdir, "mkdir");
            PatchRuntimeRelocationSlots(*runtimeDyn, module.base, getpagesize(), "mknod", "mknod",
                                        (void*)WrappedMknod, &process.originalMknod, "mknod");
            PatchRuntimeRelocationSlots(*runtimeDyn, module.base, getpagesize(), "open", "open",
                                        (void*)WrappedOpen, &process.originalOpen, "open");
            PatchRuntimeRelocationSlots(*runtimeDyn, module.base, getpagesize(), "__open_2",
                                        "__open_2", (void*)WrappedOpen2, &process.originalOpen2,
                                        "__open_2");
        }
    } else if (auto fileContext = BuildFileElfContext(module); fileContext.has_value()) {
        InstallMinimalDebugHooks(module, *fileContext);
    }

#if defined(__x86_64__)
    // Exact FUSE lowlevel callback replacement is the only supported x64 handler path.
    return;
#endif

    if (process.originalGetDirectoryEntries == nullptr &&
        getDirectoryEntriesReplacement != nullptr) {
        TryInstallCriticalHookFromPlan(module, installPlan.getDirectoryEntries,
                                       "GetDirectoryEntries", getDirectoryEntriesReplacement,
                                       &process.originalGetDirectoryEntries,
                                       "hook GetDirectoryEntries failed");
    }
    if (process.originalAddDirectoryEntriesFromLowerFs == nullptr &&
        addDirectoryEntriesReplacement != nullptr) {
        TryInstallCriticalHookFromPlan(
            module, installPlan.addDirectoryEntriesFromLowerFs, "addDirectoryEntriesFromLowerFs",
            addDirectoryEntriesReplacement, &process.originalAddDirectoryEntriesFromLowerFs,
            "hook addDirectoryEntriesFromLowerFs failed");
    }
    if (process.originalAddDirectoryEntriesFromLowerFs == nullptr &&
        addDirectoryEntriesReplacement != nullptr) {
        TryInstallCriticalHookFromPlan(module, installPlan.addDirectoryEntriesFromLowerFsThunk,
                                       "addDirectoryEntriesFromLowerFsThunk",
                                       addDirectoryEntriesReplacement,
                                       &process.originalAddDirectoryEntriesFromLowerFs,
                                       "hook addDirectoryEntriesFromLowerFs thunk failed");
    }
    if (process.originalDoReaddirCommon == nullptr) {
        TryInstallCriticalHookFromPlan(
            module, installPlan.doReaddirCommon, "do_readdir_common", (void*)WrappedDoReaddirCommon,
            &process.originalDoReaddirCommon, "hook do_readdir_common failed");
    }
    if (process.originalPfMkdir == nullptr) {
        // Reverse-engineered record: pf_mkdir @ 0x00177050.
        // The advanced path still keeps explicit handler RVAs because these static functions may be
        // absent from runtime relocation metadata.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfMkdirOffset, "pf_mkdir",
                                          (void*)WrappedPfMkdir, &process.originalPfMkdir,
                                          "hook pf_mkdir failed");
    }
    if (process.originalPfMknod == nullptr) {
        // Reverse-engineered record: pf_mknod @ 0x00176ba8.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfMknodOffset, "pf_mknod",
                                          (void*)WrappedPfMknod, &process.originalPfMknod,
                                          "hook pf_mknod failed");
    }
    if (process.originalPfUnlink == nullptr) {
        // Reverse-engineered record: pf_unlink @ 0x00177534.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfUnlinkOffset, "pf_unlink",
                                          (void*)WrappedPfUnlink, &process.originalPfUnlink,
                                          "hook pf_unlink failed");
    }
    if (process.originalPfRmdir == nullptr) {
        // Reverse-engineered record: pf_rmdir @ 0x00177920.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfRmdirOffset, "pf_rmdir",
                                          (void*)WrappedPfRmdir, &process.originalPfRmdir,
                                          "hook pf_rmdir failed");
    }
    if (process.originalPfRename == nullptr) {
        // Reverse-engineered record: pf_rename @ 0x00177ef4.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfRenameOffset, "pf_rename",
                                          (void*)WrappedPfRename, &process.originalPfRename,
                                          "hook pf_rename failed");
    }
    if (process.originalPfCreate == nullptr) {
        // Reverse-engineered record: pf_create @ 0x0017a7c8.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfCreateOffset, "pf_create",
                                          (void*)WrappedPfCreate, &process.originalPfCreate,
                                          "hook pf_create failed");
    }
    if (process.originalPfReaddir == nullptr) {
        TryInstallCriticalHookFromPlan(module, installPlan.pfReaddir, "pf_readdir",
                                       (void*)WrappedPfReaddir, &process.originalPfReaddir,
                                       "hook pf_readdir failed");
    }
    if (process.originalPfReaddirplus == nullptr) {
        TryInstallCriticalHookFromPlan(module, installPlan.pfReaddirplus, "pf_readdirplus",
                                       (void*)WrappedPfReaddirplus, &process.originalPfReaddirplus,
                                       "hook pf_readdirplus failed");
    }
    if (process.originalPfReaddirPostfilter == nullptr) {
        TryInstallCriticalHookFromPlan(module, installPlan.pfReaddirPostfilter,
                                       "pf_readdir_postfilter", (void*)WrappedPfReaddirPostfilter,
                                       &process.originalPfReaddirPostfilter,
                                       "hook pf_readdir_postfilter failed");
    }

    if (process.originalPfLookup == nullptr) {
        InstallHookForSymbol("_ZN13mediaprovider4fuseL9pf_lookupEP8fuse_reqmPKc",
                             (void*)WrappedPfLookup, &process.originalPfLookup,
                             "hook pf_lookup failed");
    }
    if (process.originalPfAccess == nullptr) {
        InstallHookForSymbol("_ZN13mediaprovider4fuseL9pf_accessEP8fuse_reqmi",
                             (void*)WrappedPfAccess, &process.originalPfAccess,
                             "hook pf_access failed");
    }
    if (process.originalPfOpen == nullptr) {
        InstallHookForSymbol("_ZN13mediaprovider4fuseL7pf_openEP8fuse_reqmP14fuse_file_info",
                             (void*)WrappedPfOpen, &process.originalPfOpen, "hook pf_open failed");
    }
    if (process.originalPfOpendir == nullptr) {
        InstallHookForSymbol("_ZN13mediaprovider4fuseL10pf_opendirEP8fuse_reqmP14fuse_file_info",
                             (void*)WrappedPfOpendir, &process.originalPfOpendir,
                             "hook pf_opendir failed");
    }
    if (process.originalPfMkdir == nullptr) {
        InstallHookForSymbol("_ZN13mediaprovider4fuseL8pf_mkdirEP8fuse_reqmPKcj",
                             (void*)WrappedPfMkdir, &process.originalPfMkdir,
                             "hook pf_mkdir failed");
    }
    if (process.originalPfMknod == nullptr) {
        InstallHookForSymbol("_ZN13mediaprovider4fuseL8pf_mknodEP8fuse_reqmPKcjm",
                             (void*)WrappedPfMknod, &process.originalPfMknod,
                             "hook pf_mknod failed");
    }
    if (process.originalPfUnlink == nullptr) {
        InstallHookForSymbol("_ZN13mediaprovider4fuseL9pf_unlinkEP8fuse_reqmPKc",
                             (void*)WrappedPfUnlink, &process.originalPfUnlink,
                             "hook pf_unlink failed");
    }
    if (process.originalPfRmdir == nullptr) {
        InstallHookForSymbol("_ZN13mediaprovider4fuseL8pf_rmdirEP8fuse_reqmPKc",
                             (void*)WrappedPfRmdir, &process.originalPfRmdir,
                             "hook pf_rmdir failed");
    }
    if (process.originalPfRename == nullptr) {
        InstallHookForSymbol("_ZN13mediaprovider4fuseL9pf_renameEP8fuse_reqmPKcmS4_j",
                             (void*)WrappedPfRename, &process.originalPfRename,
                             "hook pf_rename failed");
    }
    if (process.originalPfCreate == nullptr) {
        InstallHookForSymbol("_ZN13mediaprovider4fuseL9pf_createEP8fuse_reqmPKcjP14fuse_file_info",
                             (void*)WrappedPfCreate, &process.originalPfCreate,
                             "hook pf_create failed");
    }
    if (process.originalPfLookupPostfilter == nullptr) {
        InstallHookForSymbol(
            "_ZN13mediaprovider4fuseL20pf_lookup_postfilterEP8fuse_reqmjPKcP14fuse_entry_"
            "outP18fuse_"
            "entry_bpf_out",
            (void*)WrappedPfLookupPostfilter, &process.originalPfLookupPostfilter,
            "hook pf_lookup_postfilter failed");
    }
    if (process.originalPfGetattr == nullptr) {
        InstallHookForSymbol("_ZN13mediaprovider4fuseL10pf_getattrEP8fuse_reqmP14fuse_file_info",
                             (void*)WrappedPfGetattr, &process.originalPfGetattr,
                             "hook pf_getattr failed");
    }
    if (process.originalPfLookup == nullptr) {
        // Reverse-engineered record: pf_lookup @ 0x00175e48.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfLookupOffset, "pf_lookup",
                                          (void*)WrappedPfLookup, &process.originalPfLookup,
                                          "hook pf_lookup failed");
    }
    if (process.originalPfLookupPostfilter == nullptr) {
        // Reverse-engineered record: pf_lookup_postfilter @ 0x00175f90.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfLookupPostfilterOffset,
                                          "pf_lookup_postfilter", (void*)WrappedPfLookupPostfilter,
                                          &process.originalPfLookupPostfilter,
                                          "hook pf_lookup_postfilter failed");
    }
    if (process.originalPfGetattr == nullptr) {
        // Reverse-engineered record: pf_getattr @ 0x001762bc.
        TryInstallDeviceProfileInlineHook(module, deviceProfile.pfGetattrOffset, "pf_getattr",
                                          (void*)WrappedPfGetattr, &process.originalPfGetattr,
                                          "hook pf_getattr failed");
    }
}

}  // namespace fusehide
