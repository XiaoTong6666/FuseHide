// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0

#include "native_hook_adapter.hpp"

#include <array>
#include <mutex>

#include "dobby.h"

#if defined(__ANDROID__)
#include <android/log.h>
#else
#include <cstdio>
#endif

namespace fusehide::zygisk {
namespace {

enum class SlotState : uint8_t {
    kFree,
    kPreparing,
    kPrepared,
    kInstalled,
    kRecoveryRequired,
    kUnhooking,
};

struct HookSlot {
    void* target = nullptr;
    DobbyHookHandle ticket = 0;
    SlotState state = SlotState::kFree;
};

// This adapter must never allocate or free heap containers while patching
// arbitrary libc functions. The capacity is explicit and fails closed.
constexpr size_t kNativeHookCapacity = 256;
std::mutex gSlotsMutex;
std::array<HookSlot, kNativeHookCapacity> gSlots{};

void LogFailure(const char* phase, void* target, const DobbyHookResult& result) {
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_ERROR, "FuseHide",
                        "native hook %s target=%p status=%u cause=%u retained=%u", phase, target,
                        result.status, result.cause, result.handle != 0);
#else
    std::fprintf(stderr, "native hook %s target=%p status=%u cause=%u retained=%u\n", phase, target,
                 result.status, result.cause, result.handle != 0);
#endif
}

void ReleaseSlot(HookSlot* slot) {
    std::lock_guard lock(gSlotsMutex);
    *slot = {};
}

DobbyHookResult EmptyResult() {
    DobbyHookResult result{};
    result.struct_size = sizeof(result);
    return result;
}

}  // namespace

int InstallNativeHookInternal(void* target, void* replacement, void** backup, void* user_data,
                              HookBackupPublisher publish) {
    if (backup)
        *backup = nullptr;
    if (!target || !replacement || (user_data && !publish))
        return RT_FAILED;

    HookSlot* slot = nullptr;
    {
        std::lock_guard lock(gSlotsMutex);
        for (auto& candidate : gSlots) {
            if (candidate.state != SlotState::kFree && candidate.target == target)
                return RT_FAILED;
            if (!slot && candidate.state == SlotState::kFree)
                slot = &candidate;
        }
        if (!slot)
            return RT_FAILED;
        slot->target = target;
        slot->state = SlotState::kPreparing;
    }

    DobbyHookOptions options{
        sizeof(options),
#if defined(__aarch64__)
        DOBBY_BRANCH_REQUIRE_NEAR,
        DOBBY_HOOK_REQUIRE_CONCURRENT_SAFE,
#elif defined(__x86_64__)
        // A live MediaProvider is not a cooperative quiescent host. Its x64
        // long entry patch must fail closed until all execution can be parked.
        DOBBY_BRANCH_FORCE_LONG,
        DOBBY_HOOK_REQUIRE_CONCURRENT_SAFE,
#else
        DOBBY_BRANCH_LEGACY,
        0,
#endif
        0,
        target,
        reinterpret_cast<dobby_dummy_func_t>(replacement),
    };
    auto result = EmptyResult();
    if (DobbyPrepareHook(&options, &result) != RT_SUCCESS) {
        LogFailure("prepare", target, result);
        // A failed Prepare must not return physical ownership. If the backend
        // violates this contract, retain the slot for explicit recovery.
        if (result.handle) {
            std::lock_guard lock(gSlotsMutex);
            slot->ticket = result.handle;
            slot->state = result.status == DOBBY_HOOK_RECOVERY_REQUIRED
                              ? SlotState::kRecoveryRequired
                              : SlotState::kPrepared;
        } else {
            ReleaseSlot(slot);
        }
        return result.handle ? kNativeHookRecoveryRequired : kNativeHookFailedNeverPublished;
    }

    const auto ticket = result.handle;
    if (!ticket || !result.original) {
        auto aborted = EmptyResult();
        const bool released = ticket && DobbyAbortHook(ticket, &aborted) == RT_SUCCESS;
        if (released || !ticket) {
            ReleaseSlot(slot);
        } else {
            std::lock_guard lock(gSlotsMutex);
            slot->ticket = ticket;
            slot->state = SlotState::kPrepared;
        }
        return ticket && !released ? kNativeHookRecoveryRequired : kNativeHookFailedNeverPublished;
    }
    {
        std::lock_guard lock(gSlotsMutex);
        slot->ticket = ticket;
        slot->state = SlotState::kPreparing;
    }

    // The v3 publisher executes synchronously AFTER generating a callable
    // original and BEFORE the first physical entry write. Do not invoke
    // user callbacks under gSlotsMutex or Dobby's mutation lock.
    if (publish)
        publish(user_data, reinterpret_cast<void*>(result.original));
    else if (backup)
        *backup = reinterpret_cast<void*>(result.original);
    if (DobbyCommitHook(ticket, &result) == RT_SUCCESS) {
        std::lock_guard lock(gSlotsMutex);
        slot->state = SlotState::kInstalled;
        return RT_SUCCESS;
    }

    LogFailure("commit", target, result);
    if (result.handle == ticket && !result.ever_published && !result.target_may_be_patched &&
        result.status != DOBBY_HOOK_RECOVERY_REQUIRED) {
        auto aborted = EmptyResult();
        if (DobbyAbortHook(ticket, &aborted) == RT_SUCCESS)
            result.handle = 0;
    }
    if (!result.handle) {
        ReleaseSlot(slot);
    } else {
        std::lock_guard lock(gSlotsMutex);
        slot->state = result.status == DOBBY_HOOK_RECOVERY_REQUIRED ? SlotState::kRecoveryRequired
                                                                    : SlotState::kPrepared;
    }
    // An already-fetched replacement can still need its original even when
    // recovery restored the entry. Do not clear a published backup.
    if (publish && !result.ever_published && !result.handle)
        publish(user_data, nullptr);
    if (backup && !result.ever_published)
        *backup = nullptr;
    if (result.handle)
        return kNativeHookRecoveryRequired;
    return result.ever_published ? kNativeHookFailedAfterPublished
                                 : kNativeHookFailedNeverPublished;
}

int InstallNativeHook(void* target, void* replacement, void** backup) {
    return InstallNativeHookInternal(target, replacement, backup, nullptr, nullptr) == kNativeHookOk
               ? RT_SUCCESS
               : RT_FAILED;
}

int InstallNativeHookWithPublication(void* target, void* replacement, void* user_data,
                                     HookBackupPublisher publish) {
    if (!publish)
        return kNativeHookFailedNeverPublished;
    return InstallNativeHookInternal(target, replacement, nullptr, user_data, publish);
}

int UninstallNativeHook(void* target) {
    if (!target)
        return RT_FAILED;
    HookSlot* slot = nullptr;
    DobbyHookHandle ticket = 0;
    SlotState prior = SlotState::kFree;
    {
        std::lock_guard lock(gSlotsMutex);
        for (auto& candidate : gSlots) {
            if (candidate.state != SlotState::kFree && candidate.target == target) {
                slot = &candidate;
                break;
            }
        }
        if (!slot || !slot->ticket || slot->state == SlotState::kPreparing ||
            slot->state == SlotState::kUnhooking)
            return RT_FAILED;
        ticket = slot->ticket;
        prior = slot->state;
        slot->state = SlotState::kUnhooking;
    }

    auto result = EmptyResult();
    int rc = RT_FAILED;
    if (prior == SlotState::kPrepared)
        rc = DobbyAbortHook(ticket, &result);
    else if (prior == SlotState::kRecoveryRequired)
        rc = DobbyRecoverHook(ticket, &result);
    else
        rc = DobbyDestroyHook(ticket, &result);
    if (rc == RT_SUCCESS) {
        ReleaseSlot(slot);
        return RT_SUCCESS;
    }
    LogFailure("unhook", target, result);
    {
        std::lock_guard lock(gSlotsMutex);
        slot->state = prior;
    }
    return RT_FAILED;
}

const NativeApiEntries& GetNativeHookApi() {
    static const NativeApiEntries api{2, InstallNativeHook, UninstallNativeHook};
    return api;
}

const NativeApiEntriesV3& GetStrictNativeHookApi() {
    static const NativeApiEntriesV3 api{
        {3, InstallNativeHook, UninstallNativeHook},
        sizeof(NativeApiEntriesV3),
        0,
        InstallNativeHookWithPublication,
    };
    return api;
}

}  // namespace fusehide::zygisk
