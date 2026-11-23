// Copyright (C) 2026 XiaoTong6666
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fusehide/elf/elf_utils.hpp"
#include "fusehide/hooks/hook_install_internal.hpp"
#include "fusehide/hooks/wrappers.hpp"

namespace fusehide {

static_assert(std::is_same_v<decltype(&WrappedShouldNotCache), ShouldNotCacheFn>);
static_assert(std::is_same_v<decltype(&WrappedGetDirectoryEntriesShared), GetDirectoryEntriesFn>);
static_assert(
    std::is_same_v<decltype(&WrappedGetDirectoryEntriesValue), GetValueDirectoryEntriesFn>);
static_assert(std::is_same_v<decltype(&WrappedAddDirectoryEntriesFromLowerFsShared),
                             AddDirectoryEntriesFromLowerFsFn>);
static_assert(std::is_same_v<decltype(&WrappedAddDirectoryEntriesFromLowerFsValue),
                             AddValueDirectoryEntriesFromLowerFsFn>);
static_assert(std::is_same_v<decltype(&WrappedPfLookup), PfLookupFn>);
static_assert(std::is_same_v<decltype(&WrappedPfForget), PfForgetFn>);
static_assert(std::is_same_v<decltype(&WrappedPfForgetMulti), PfForgetMultiFn>);
static_assert(std::is_same_v<decltype(&WrappedNodeDeleted), NodeDeletedFn>);
static_assert(std::is_same_v<decltype(&WrappedPfLookupPostfilter), PfLookupPostfilterFn>);
static_assert(std::is_same_v<decltype(&WrappedPfAccess), PfAccessFn>);
static_assert(std::is_same_v<decltype(&WrappedPfOpen), PfFileInfoFn>);
static_assert(std::is_same_v<decltype(&WrappedPfOpendir), PfFileInfoFn>);
static_assert(std::is_same_v<decltype(&WrappedPfMkdir), PfMkdirFn>);
static_assert(std::is_same_v<decltype(&WrappedPfMknod), PfMknodFn>);
static_assert(std::is_same_v<decltype(&WrappedPfUnlink), PfRemoveFn>);
static_assert(std::is_same_v<decltype(&WrappedPfRmdir), PfRemoveFn>);
static_assert(std::is_same_v<decltype(&WrappedPfRename), PfRenameFn>);
static_assert(std::is_same_v<decltype(&WrappedPfCreate), PfCreateFn>);
static_assert(std::is_same_v<decltype(&WrappedPfReaddir), PfReaddirFn>);
static_assert(std::is_same_v<decltype(&WrappedPfReaddirplus), PfReaddirFn>);
static_assert(std::is_same_v<decltype(&WrappedPfReaddirPostfilter), PfReaddirPostfilterFn>);
static_assert(std::is_same_v<decltype(&WrappedDoReaddirCommon), DoReaddirCommonFn>);
static_assert(std::is_same_v<decltype(&WrappedNotifyInvalEntry), NotifyInvalEntryFn>);
static_assert(std::is_same_v<decltype(&WrappedNotifyInvalInode), NotifyInvalInodeFn>);
static_assert(std::is_same_v<decltype(&WrappedFuseSessionDestroy), FuseSessionDestroyFn>);
static_assert(std::is_same_v<decltype(&WrappedReplyEntry), FuseReplyEntryFn>);
static_assert(std::is_same_v<decltype(&WrappedReplyAttr), FuseReplyAttrFn>);
static_assert(std::is_same_v<decltype(&WrappedReplyBuf), FuseReplyBufFn>);
static_assert(std::is_same_v<decltype(&WrappedReplyErr), FuseReplyErrFn>);
static_assert(std::is_same_v<decltype(&WrappedPfGetattr), PfFileInfoFn>);
static_assert(std::is_same_v<decltype(&WrappedLstat), LibcStatFn>);
static_assert(std::is_same_v<decltype(&WrappedStat), LibcStatFn>);
static_assert(std::is_same_v<decltype(&WrappedGetxattr), LibcXattrFn>);
static_assert(std::is_same_v<decltype(&WrappedLgetxattr), LibcXattrFn>);
static_assert(std::is_same_v<decltype(&WrappedMkdirLibc), LibcMkdirFn>);
static_assert(std::is_same_v<decltype(&WrappedMknod), LibcMknodFn>);
static_assert(std::is_same_v<decltype(&WrappedOpen), LibcOpenFn>);
static_assert(std::is_same_v<decltype(&WrappedOpen2), LibcOpen2Fn>);
static_assert(std::is_same_v<decltype(&WrappedIsAppAccessiblePath), IsAppAccessiblePathFn>);
static_assert(std::is_same_v<decltype(&WrappedIsPackageOwnedPath), IsPackageOwnedPathFn>);
static_assert(std::is_same_v<decltype(&WrappedIsBpfBackingPath), IsBpfBackingPathFn>);
static_assert(std::is_same_v<decltype(&WrappedStrcasecmp), StrcasecmpFn>);
static_assert(std::is_same_v<decltype(&WrappedEqualsIgnoreCaseAbi), EqualsIgnoreCaseAbiFn>);

// Install all hooks after LSPosed reports that libfuse_jni.so has been loaded into MediaProvider.

void InstallFuseHooks() {
    // Multiple native hosts can report the same loaded image concurrently.
    // Dobby serializes its own registry, but Process() contains caller-owned
    // backup pointers and ABI metadata that must not be written concurrently.
    // A blocking mutex is unsafe here: a loader callback may still hold the
    // Android linker lock while another installer resolves more symbols.
    // Skip duplicate/reentrant attempts; a later loader event can retry.
    static std::atomic_flag installing = ATOMIC_FLAG_INIT;
    if (installing.test_and_set(std::memory_order_acquire)) {
        __android_log_print(
            ANDROID_LOG_INFO, kLogTag,
            "FUSE hook installation already in progress; skipping overlapping callback");
        return;
    }
    struct InstallAttemptExit {
        std::atomic_flag& flag;
        ~InstallAttemptExit() {
            flag.clear(std::memory_order_release);
        }
    } exit{installing};

    auto module = FindTargetModule();
    if (!module.has_value()) {
        __android_log_print(6, kLogTag, "no %s found", kTargetLibrary);
        return;
    }

    __android_log_print(4, kLogTag, "hooking libfuse_jni");
    __android_log_print(4, kLogTag, "target module base=%p path=%s",
                        reinterpret_cast<void*>(module->base), module->path.c_str());
    const bool useRuntimeElf = module->path.find("!/") != std::string::npos;
    if (useRuntimeElf) {
        __android_log_print(4, kLogTag, "using in-memory ELF parser for embedded library path");
    }

#if defined(__x86_64__) || defined(__aarch64__)
    // The FUSE daemon can create sessions while this thread is resolving other
    // symbols. Install the session-new GOT interceptor first, after complete
    // callback discovery, and before unrelated relocation or inline hooks.
    if (useRuntimeElf) {
        if (const auto earlyDyn = ParseRuntimeDynamicInfo(*module); earlyDyn.has_value()) {
            InstallLowlevelCallbackFallback(*module, *earlyDyn);
            LogEarlySessionHookStatus();
        } else {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                                "early session hook unavailable: runtime dynamic info missing");
        }
    }
#endif
    const DeviceHookInstallPlan installPlan = ResolveDeviceHookInstallPlan(*module);
#if defined(__aarch64__)
    // Resolve the actual exported MediaProvider symbol from the loaded ELF.
    // Both libc++ namespace spellings have the same const string& calling
    // convention on ARM64. No per-device RVA or Build ID table is needed.
    constexpr std::string_view kRootLookupSymbols[] = {
        "_ZN13mediaprovider4fuse4node18LookupAbsolutePathEPKS1_"
        "RKNSt6__ndk112basic_stringIcNS4_11char_traitsIcEENS4_9allocatorIcEEEE",
        "_ZN13mediaprovider4fuse4node18LookupAbsolutePathEPKS1_"
        "RKNSt3__112basic_stringIcNS4_11char_traitsIcEENS4_9allocatorIcEEEE",
    };
    void* rootLookup = nullptr;
    bool ambiguousRootLookup = false;
    if (const auto dynInfo = ParseRuntimeDynamicInfo(*module); dynInfo.has_value()) {
        for (const auto name : kRootLookupSymbols) {
            const auto symbolIndex = FindRuntimeSymbolIndex(
                *dynInfo, reinterpret_cast<const uint8_t*>(name.data()), name.size());
            if (!symbolIndex.has_value())
                continue;
            const auto& symbol = dynInfo->symtab[*symbolIndex];
            if (symbol.st_value == 0 || symbol.st_shndx == SHN_UNDEF ||
                ELF64_ST_TYPE(symbol.st_info) != STT_FUNC)
                continue;
            const uintptr_t address = module->base + symbol.st_value;
            // APK-embedded libraries map as MediaProvider.apk, not a separate
            // libfuse_jni.so path. /proc/self/maps-derived execStart can be
            // absent; use the dynamic linker's ELF program headers instead.
            bool executable = false;
            if (module->phdrs != nullptr) {
                for (uint16_t i = 0; i < module->phnum; ++i) {
                    const auto& segment = module->phdrs[i];
                    if (segment.p_type != PT_LOAD || (segment.p_flags & PF_X) == 0)
                        continue;
                    const uintptr_t start = module->base + segment.p_vaddr;
                    if (address >= start && address - start < segment.p_memsz) {
                        executable = true;
                        break;
                    }
                }
            }
            if (!executable)
                continue;
            if (rootLookup != nullptr) {
                ambiguousRootLookup = true;
                break;
            }
            rootLookup = reinterpret_cast<void*>(address);
        }
    }
    if (rootLookup != nullptr && !ambiguousRootLookup) {
        SetVerifiedPrimaryRootNodeLookup(rootLookup);
        DebugLogPrint(4, "primary root node lookup resolved from exported symbol at %p",
                      rootLookup);
    } else {
        DebugLogPrint(4, "primary root node lookup unavailable or ambiguous");
    }
#endif
#if defined(__x86_64__)
    // Prioritize session-new GOT replacement before unsupported x64 live inline hooks.
    InstallAdvancedDebugHooks(*module);
#endif
    if (Process().fuseReqCtx == nullptr) {
        // MediaProvider links libfuse.so separately, so fuse_req_ctx is normally an undefined
        // import in libfuse_jni.so. Prefer linker resolution, then its bound relocation, then
        // libfuse.so:
        // https://android.googlesource.com/platform/packages/providers/MediaProvider/+/f2abe4aec018f0522b4b1303fb25351db0604eb5/jni/Android.bp#46
        // https://android.googlesource.com/platform/external/libfuse/+/e877eedab4254ceb2af4b77c7327e4d0cde30824/lib/fuse_lowlevel.c#2771
        void* resolved = dlsym(RTLD_DEFAULT, "fuse_req_ctx");
        const char* resolutionSource = "RTLD_DEFAULT";
        if (resolved == nullptr) {
            const auto importedTarget = ResolveImportedFunctionTarget(*module, "fuse_req_ctx");
            if (importedTarget.has_value()) {
                resolved = *importedTarget;
                resolutionSource = "libfuse_jni relocation";
            }
        }
        if (resolved == nullptr) {
            const auto libfuseModule = FindModuleFromMaps("/libfuse.so");
            if (libfuseModule.has_value()) {
                const bool useRuntimeLibfuse = libfuseModule->path.find("!/") != std::string::npos;
                const auto libfuseSymbol =
                    useRuntimeLibfuse ? ResolveTargetSymbolRuntime(*libfuseModule, "fuse_req_ctx")
                                      : ResolveTargetSymbol(*libfuseModule, "fuse_req_ctx");
                if (libfuseSymbol.has_value()) {
                    resolved = *libfuseSymbol;
                    resolutionSource = "libfuse.so";
                }
            }
        }

        if (resolved != nullptr) {
            Process().fuseReqCtx = resolved;
            __android_log_print(4, kLogTag, "resolved fuse_req_ctx=%p source=%s", resolved,
                                resolutionSource);
        } else {
            __android_log_print(6, kLogTag,
                                "fuse_req_ctx unavailable; no verified libfuse layout fallback");
        }
    }

    const bool hasClassicStringAbi = CanUseClassicStringWrappers(installPlan);
    __android_log_print(hasClassicStringAbi ? 4 : 5, kLogTag,
                        "libc++ string ABI=%s profile=%s trusted=%d",
                        hasClassicStringAbi ? "classic" : "unverified", installPlan.profile.name,
                        installPlan.profileTrusted ? 1 : 0);
    if (!hasClassicStringAbi) {
        __android_log_print(5, kLogTag,
                            "skipping hooks that decode or construct libc++ string objects");
    }

    CoreHookStatus coreStatus;
    RefreshCoreHookStatus(*module, &coreStatus);

    if (!useRuntimeElf) {
        if (auto fileContext = BuildFileElfContext(*module); fileContext.has_value()) {
            __android_log_print(4, kLogTag, "installing minimal file-backed hook path first");
            InstallMinimalCoreHooks(*module, *fileContext, &coreStatus);
            InstallMinimalDebugHooks(*module, *fileContext);
            LogCoreHookStatus("after minimal", coreStatus);
        } else {
            __android_log_print(5, kLogTag,
                                "minimal file-backed hook path unavailable, falling back");
        }
    }

    if (!HasAllCoreHooks(coreStatus) || useRuntimeElf) {
        __android_log_print(4, kLogTag, "installing advanced hook fallback");
        InstallAdvancedCoreHooks(*module, &coreStatus);
        LogCoreHookStatus("after fallback", coreStatus);
    }

    // Release builds still need these hooks for reply filtering, named target interception, and
    // cache invalidation. DebugLoggingEnabled() only suppresses verbose traces inside the
    // wrappers themselves.
#if !defined(__x86_64__)
    InstallAdvancedDebugHooks(*module);
#endif

    __android_log_print(
        4, kLogTag,
        "hook summary app=%p package_ptr=%p package_compare=%d bpf=%p strcasecmp=%p equals=%p "
        "unicode=12.1.0",
        reinterpret_cast<void*>(Process().originalIsAppAccessiblePath),
        reinterpret_cast<void*>(Process().originalIsPackageOwnedPath),
        coreStatus.packageCoveredByComparePath,
        reinterpret_cast<void*>(Process().originalIsBpfBackingPath), Process().originalStrcasecmp,
        Process().originalEqualsIgnoreCase);
}

}  // namespace fusehide
