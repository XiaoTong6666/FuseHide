// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <cstdint>
#include <limits>

namespace fusehide {

// This is a *logical* admission/drain gate, not an instruction-fetch grace
// period. A host must keep this shared object mapped while it retains the
// callback pointer. In Native API v2 there is no callback unregistration,
// therefore a real module may close admission but MUST NOT dlclose its text.
class NativeCallbackGate final {
   public:
    enum class State : uint32_t { kActive = 0, kDraining = 1, kClosed = 2 };

    bool TryEnter() noexcept {
        auto current = control_.load(std::memory_order_acquire);
        for (;;) {
            if (static_cast<uint32_t>(current) != static_cast<uint32_t>(State::kActive) ||
                (current >> 32) == std::numeric_limits<uint32_t>::max())
                return false;
            if (control_.compare_exchange_weak(current, current + kEntrant,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire))
                return true;
        }
    }

    void Exit() noexcept {
        control_.fetch_sub(kEntrant, std::memory_order_acq_rel);
    }

    void CloseAdmission() noexcept {
        auto current = control_.load(std::memory_order_acquire);
        while (static_cast<uint32_t>(current) == static_cast<uint32_t>(State::kActive)) {
            const auto next = (current & ~kStateMask) | static_cast<uint32_t>(State::kDraining);
            if (control_.compare_exchange_weak(current, next, std::memory_order_acq_rel,
                                               std::memory_order_acquire))
                return;
        }
    }

    // A failed close leaves the gate Draining, not reopened. A late entrant
    // may have fetched the function pointer before CloseAdmission, so even a
    // successful close is NOT permission to unmap this shared library.
    bool FinishDrain() noexcept {
        auto expected = static_cast<uint64_t>(State::kDraining);
        return control_.compare_exchange_strong(expected, static_cast<uint64_t>(State::kClosed),
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire) ||
               expected == static_cast<uint64_t>(State::kClosed);
    }

    uint32_t InFlight() const noexcept {
        return static_cast<uint32_t>(control_.load(std::memory_order_acquire) >> 32);
    }

    State GetState() const noexcept {
        return static_cast<State>(static_cast<uint32_t>(control_.load(std::memory_order_acquire)));
    }

   private:
    static constexpr uint64_t kEntrant = uint64_t{1} << 32;
    static constexpr uint64_t kStateMask = 0xffffffffULL;
    std::atomic<uint64_t> control_{static_cast<uint64_t>(State::kActive)};
};

}  // namespace fusehide

// Logical loader callback retirement only. These calls do not unregister a
// Vector/LSPosed function pointer and do not unhook MediaProvider targets.
// The DSO must remain NODELETE even after a successful drain.
extern "C" {
__attribute__((visibility("default"))) void fusehide_close_loader_callback_admission();
__attribute__((visibility("default"))) bool fusehide_finish_loader_callback_drain();
__attribute__((visibility("default"))) uint32_t fusehide_loader_callback_in_flight();
}
