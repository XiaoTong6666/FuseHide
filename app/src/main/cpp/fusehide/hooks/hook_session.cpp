// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#include "fusehide/hooks/hook_install_internal.hpp"
#include "fusehide/hooks/callback_table_probe.hpp"
#include "fusehide/hooks/callback_coverage.hpp"
#include "fusehide/hooks/session_new_abi.hpp"
#include <malloc.h>

namespace fusehide {

#if defined(__x86_64__) || defined(__aarch64__)
// Patch the callback table passed to libfuse at Session creation, never the
// private table of an already-running Session. This also handles ARM64 MediaProvider
// callbacks whose PACIASP entry cannot safely be skipped by a near inline branch.
// AOSP MediaProvider imports either fuse_session_new@FUSE_3.0 (four args) or
// _fuse_session_new_317@FUSE_3.17 (five args) on ARM64 as well as x86_64.
// Keep distinct typed backups and select the wrapper from the ELF import.
using FuseSessionNewFourFn = void* (*)(void*, const void*, size_t, void*);
using FuseSessionNewFiveFn = void* (*)(void*, const void*, size_t, const void*, void*);
HookOriginal<FuseSessionNewFourFn> gOriginalFuseSessionNewFour;
HookOriginal<FuseSessionNewFiveFn> gOriginalFuseSessionNewFive;
struct FuseArgsPrefix {
    int argc;
    char** argv;
    int allocated;
};
struct LowlevelCallbackSpec {
    void* target = nullptr;
    void* replacement = nullptr;
    void** backup = nullptr;
    const char* name = nullptr;
};
std::array<LowlevelCallbackSpec, 8> gLowlevelCallbacks{};
std::atomic<size_t> gLowlevelCallbackCount{0};

bool BackfillSessionLowlevelCallbacks(void* session) {
#if !defined(FUSEHIDE_ENABLE_UNSAFE_LIVE_CALLBACK_PATCH)
    // A complete pointer match is not enough to make concurrent libfuse callback reads
    // atomic. Until FUSE workers can be quiesced, prefer a visible capability failure
    // over modifying a live Session's private callback table.
    (void)session;
    return false;
#else
    const size_t required = gLowlevelCallbackCount.load(std::memory_order_acquire);
    if (session == nullptr || required != 6)
        return false;
    // fuse_session_new_versioned() calloc's the session and embeds a copy of op.
    // Never assume a fixed private fuse_session offset on an unknown libfuse build.
    const size_t allocationBytes = malloc_usable_size(session);
    if (allocationBytes < 512 || allocationBytes > 32768)
        return false;
    const auto* base = static_cast<const unsigned char*>(session);
    std::array<void*, 8> originals{};
    std::array<void*, 8> replacements{};
    for (size_t n = 0; n < required; ++n) {
        originals[n] = gLowlevelCallbacks[n].target;
        replacements[n] = gLowlevelCallbacks[n].replacement;
    }
    const auto locations = testing::LocateCallbackTable<8>(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(base), allocationBytes),
        originals, replacements, required, 376);
    if (!locations)
        return false;
    for (size_t n = 0; n < required; ++n) {
        auto& spec = gLowlevelCallbacks[n];
        void* previous = __atomic_load_n(spec.backup, __ATOMIC_ACQUIRE);
        if (previous != nullptr && previous != spec.target)
            return false;
    }
    for (size_t n = 0; n < required; ++n) {
        auto& spec = gLowlevelCallbacks[n];
        void* expectedBackup = nullptr;
        __atomic_compare_exchange_n(spec.backup, &expectedBackup, spec.target, false,
                                    __ATOMIC_RELEASE, __ATOMIC_ACQUIRE);
        auto* slot = reinterpret_cast<void**>(const_cast<unsigned char*>(base) +
                                              (*locations)[n] * sizeof(void*));
        void* expected = spec.target;
        if (!__atomic_compare_exchange_n(slot, &expected, spec.replacement, false, __ATOMIC_ACQ_REL,
                                         __ATOMIC_ACQUIRE) &&
            expected != spec.replacement)
            return false;
    }
    __android_log_print(ANDROID_LOG_WARN, kLogTag,
                        "late FUSE callback takeover se=%p replaced=%zu; previous requests "
                        "may have cached entries",
                        session, required);
    return true;
#endif
}

template <typename InvokeOriginal>
void* CreateSessionWithPatchedCallbacks(void* args, const void* ops, size_t opsSize, void* userdata,
                                        InvokeOriginal&& invokeOriginal) {
    // Android's fuse_lowlevel_ops is a function-pointer-only table. Exact callback identity
    // and the supplied size are checked; unknown layouts pass through unchanged.
    std::vector<void*> patched;
    size_t installed = 0;
    const size_t required = gLowlevelCallbackCount.load(std::memory_order_acquire);
    if (ops && required == kRequiredFUSECallbacks && opsSize >= 376 && opsSize <= 4096 &&
        opsSize % sizeof(void*) == 0) {
        patched.resize(opsSize / sizeof(void*));
        std::memcpy(patched.data(), ops, opsSize);
        // Validate all six unique callbacks before publishing any replacement.
        // A partial table must pass through unmodified, not silently hide only
        // lookup or readdir while the other operations stay unfiltered.
        std::array<size_t, kRequiredFUSECallbacks> positions{};
        positions.fill(SIZE_MAX);
        bool valid = true;
        for (size_t n = 0; n < required && valid; ++n) {
            const auto& spec = gLowlevelCallbacks[n];
            if (!spec.target || !spec.replacement || !spec.backup ||
                spec.target == spec.replacement) {
                valid = false;
                break;
            }
            for (size_t i = 0; i < patched.size(); ++i) {
                if (patched[i] == spec.target) {
                    if (positions[n] != SIZE_MAX) {
                        valid = false;
                        break;
                    }
                    positions[n] = i;
                }
            }
            if (positions[n] == SIZE_MAX)
                valid = false;
        }
        for (size_t n = 0; n < required && valid; ++n) {
            const auto& spec = gLowlevelCallbacks[n];
            const auto previous = __atomic_load_n(spec.backup, __ATOMIC_ACQUIRE);
            if (previous != nullptr && previous != spec.target)
                valid = false;
            for (size_t j = 0; j < n; ++j)
                if (positions[j] == positions[n] || gLowlevelCallbacks[j].target == spec.target)
                    valid = false;
        }
        if (valid) {
            for (size_t n = 0; n < required; ++n) {
                const auto& spec = gLowlevelCallbacks[n];
                void* expected = nullptr;
                __atomic_compare_exchange_n(spec.backup, &expected, spec.target, false,
                                            __ATOMIC_RELEASE, __ATOMIC_ACQUIRE);
                patched[positions[n]] = spec.replacement;
            }
            installed = required;
        } else {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                                "FUSE Session callback layout incomplete; leaving ops unchanged");
        }
    }
    void* se = invokeOriginal(installed ? patched.data() : ops);
    if (se) {
        const auto token = RegisterSession(se);
        BindSessionFUSEUserdata(token, userdata);
        MarkSessionLowlevelCallbacks(token, installed,
                                     gLowlevelCallbackCount.load(std::memory_order_acquire));
        const auto* fuseArgs = static_cast<const FuseArgsPrefix*>(args);
        if (fuseArgs && fuseArgs->argc > 0 && fuseArgs->argc < 64 && fuseArgs->argv &&
            fuseArgs->argv[0]) {
            RegisterSessionRootPath(token, fuseArgs->argv[0]);
        }
        __android_log_print(ANDROID_LOG_INFO, kLogTag,
                            "lowlevel callback table se=%p replaced=%zu ops_size=%zu", se,
                            installed, opsSize);
    }
    return se;
}

extern "C" void* WrappedFuseSessionNewFour(void* args, const void* ops, size_t opsSize,
                                           void* userdata) {
    const auto original = gOriginalFuseSessionNewFour.get();
    if (original == nullptr)
        return nullptr;
    return CreateSessionWithPatchedCallbacks(
        args, ops, opsSize, userdata,
        [&](const void* callbacks) { return original(args, callbacks, opsSize, userdata); });
}

extern "C" void* WrappedFuseSessionNewFive(void* args, const void* ops, size_t opsSize,
                                           const void* version, void* userdata) {
    const auto original = gOriginalFuseSessionNewFive.get();
    if (original == nullptr)
        return nullptr;
    return CreateSessionWithPatchedCallbacks(
        args, ops, opsSize, userdata, [&](const void* callbacks) {
            return original(args, callbacks, opsSize, version, userdata);
        });
}

static_assert(std::is_same_v<decltype(&WrappedFuseSessionNewFour), FuseSessionNewFourFn>);
static_assert(std::is_same_v<decltype(&WrappedFuseSessionNewFive), FuseSessionNewFiveFn>);

void InstallSessionNewImport(const RuntimeDynamicInfo& dyn, uintptr_t moduleBase) {
    const auto relocationSlots = [&](std::string_view symbol) {
        const auto idx = FindRuntimeSymbolIndex(
            dyn, reinterpret_cast<const uint8_t*>(symbol.data()), symbol.size());
        return idx ? FindRuntimeRelocationSlotsForSymbol(dyn, *idx, moduleBase)
                   : std::vector<uintptr_t>{};
    };
    const auto four = relocationSlots(kSessionNewFourArgSymbol);
    const auto five = relocationSlots(kSessionNewFiveArgSymbol);
    const SessionNewAbi abi = ResolveSessionNewAbi(four.size(), five.size());
    if (abi == SessionNewAbi::Unsupported) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "Session-new ABI unsupported/ambiguous four_slots=%zu five_slots=%zu",
                            four.size(), five.size());
        return;
    }
    if (abi == SessionNewAbi::FourArgs) {
        PatchRuntimeRelocationSlots(dyn, moduleBase, getpagesize(), kSessionNewFourArgSymbol, {},
                                    reinterpret_cast<void*>(+WrappedFuseSessionNewFour),
                                    &gOriginalFuseSessionNewFour.rawStorage(),
                                    "fuse_session_new (4 args)");
    } else {
        PatchRuntimeRelocationSlots(dyn, moduleBase, getpagesize(), kSessionNewFiveArgSymbol, {},
                                    reinterpret_cast<void*>(+WrappedFuseSessionNewFive),
                                    &gOriginalFuseSessionNewFive.rawStorage(),
                                    "_fuse_session_new_317 (5 args)");
    }
    __android_log_print(
        ANDROID_LOG_INFO, kLogTag, "Session-new import ABI=%d installed=%d",
        abi == SessionNewAbi::FourArgs ? 4 : 5,
        (abi == SessionNewAbi::FourArgs ? gOriginalFuseSessionNewFour.get() != nullptr
                                        : gOriginalFuseSessionNewFive.get() != nullptr)
            ? 1
            : 0);
}

void InstallLowlevelCallbackFallback(const ModuleInfo& module, const RuntimeDynamicInfo& dyn) {
    // Fast path: AOSP MediaProvider APKs may contain the exact local callback
    // symbols. Map its ELF image once instead of repeatedly parsing the APK
    // and resolving unrelated profile features before intercepting Session new.
    if (const auto mapped = MapEmbeddedStoredElf(module.path); mapped.has_value()) {
        constexpr std::array<std::string_view, 9> names = {
            kPfLookupSymbols[0],
            kPfLookupPostfilterSymbols[0],
            kPfGetattrSymbols[0],
            kPfReaddirSymbols[0],
            kPfReaddirSymbols[1],
            kPfReaddirplusSymbols[0],
            kPfReaddirplusSymbols[1],
            kPfReaddirPostfilterSymbols[0],
            kPfReaddirPostfilterSymbols[1],
        };
        const auto values = FindSymbolOffsetsOnce(*mapped, names);
        const std::array<std::optional<uintptr_t>, 6> offsets = {
            values[0],
            values[1],
            values[2],
            values[3].has_value() ? values[3] : values[4],
            values[5].has_value() ? values[5] : values[6],
            values[7].has_value() ? values[7] : values[8],
        };
        if (std::all_of(offsets.begin(), offsets.end(),
                        [](const auto& value) { return value.has_value() && *value != 0; })) {
            gLowlevelCallbacks[0] = {reinterpret_cast<void*>(module.base + *offsets[0]),
                                     reinterpret_cast<void*>(+WrappedPfLookup),
                                     &gOriginalPfLookup.rawStorage(), "lookup"};
            gLowlevelCallbacks[1] = {reinterpret_cast<void*>(module.base + *offsets[1]),
                                     reinterpret_cast<void*>(+WrappedPfLookupPostfilter),
                                     &gOriginalPfLookupPostfilter.rawStorage(),
                                     "lookup_postfilter"};
            gLowlevelCallbacks[2] = {reinterpret_cast<void*>(module.base + *offsets[2]),
                                     reinterpret_cast<void*>(+WrappedPfGetattr),
                                     &gOriginalPfGetattr.rawStorage(), "getattr"};
            gLowlevelCallbacks[3] = {reinterpret_cast<void*>(module.base + *offsets[3]),
                                     reinterpret_cast<void*>(+WrappedPfReaddir),
                                     &gOriginalPfReaddir.rawStorage(), "readdir"};
            gLowlevelCallbacks[4] = {reinterpret_cast<void*>(module.base + *offsets[4]),
                                     reinterpret_cast<void*>(+WrappedPfReaddirplus),
                                     &gOriginalPfReaddirplus.rawStorage(), "readdirplus"};
            gLowlevelCallbacks[5] = {reinterpret_cast<void*>(module.base + *offsets[5]),
                                     reinterpret_cast<void*>(+WrappedPfReaddirPostfilter),
                                     &gOriginalPfReaddirPostfilter.rawStorage(),
                                     "readdir_postfilter"};
            gLowlevelCallbackCount.store(offsets.size(), std::memory_order_release);
            __android_log_print(ANDROID_LOG_INFO, kLogTag,
                                "early Session callback symbols resolved in one ELF mapping");
            InstallSessionNewImport(dyn, module.base);
            return;
        }
    }
    __android_log_print(ANDROID_LOG_WARN, kLogTag,
                        "early Session exact symbols incomplete; using conservative resolver");
    const auto plan = ResolveDeviceHookInstallPlan(module);
    size_t count = 0;
    auto add = [&](std::optional<uintptr_t> offset, void* replacement, void** backup,
                   const char* name) {
        if (!offset || *offset == 0 || count >= gLowlevelCallbacks.size())
            return;
        gLowlevelCallbacks[count++] = {reinterpret_cast<void*>(module.base + *offset), replacement,
                                       backup, name};
    };
    const auto resolved = [](const CriticalHookTargetPlan& hook) -> std::optional<uintptr_t> {
        if (!CanUseProfileBasedTarget(hook.source) || hook.offset == 0)
            return std::nullopt;
        return hook.offset;
    };
    add(ResolveFirstAvailableSymbolOffset(module, kPfLookupSymbols),
        reinterpret_cast<void*>(+WrappedPfLookup), &gOriginalPfLookup.rawStorage(), "lookup");
    add(ResolveFirstAvailableSymbolOffset(module, kPfLookupPostfilterSymbols),
        reinterpret_cast<void*>(+WrappedPfLookupPostfilter),
        &gOriginalPfLookupPostfilter.rawStorage(), "lookup_postfilter");
    add(ResolveFirstAvailableSymbolOffset(module, kPfGetattrSymbols),
        reinterpret_cast<void*>(+WrappedPfGetattr), &gOriginalPfGetattr.rawStorage(), "getattr");
    add(resolved(plan.pfReaddir), reinterpret_cast<void*>(+WrappedPfReaddir),
        &gOriginalPfReaddir.rawStorage(), "readdir");
    add(resolved(plan.pfReaddirplus), reinterpret_cast<void*>(+WrappedPfReaddirplus),
        &gOriginalPfReaddirplus.rawStorage(), "readdirplus");
    add(resolved(plan.pfReaddirPostfilter), reinterpret_cast<void*>(+WrappedPfReaddirPostfilter),
        &gOriginalPfReaddirPostfilter.rawStorage(), "readdir_postfilter");
    gLowlevelCallbackCount.store(count, std::memory_order_release);
    InstallSessionNewImport(dyn, module.base);
}
#endif

#if defined(__x86_64__) || defined(__aarch64__)
void LogEarlySessionHookStatus() {
    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "early session hook setup complete callbacks=%zu installed=%d",
                        gLowlevelCallbackCount.load(std::memory_order_acquire),
                        (gOriginalFuseSessionNewFour.get() != nullptr ||
                         gOriginalFuseSessionNewFive.get() != nullptr)
                            ? 1
                            : 0);
}
#endif

}  // namespace fusehide
