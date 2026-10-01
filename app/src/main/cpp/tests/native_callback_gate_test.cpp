// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#include "fusehide/native_callback_gate.hpp"

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

namespace {
bool Check(bool value, const char* label) {
    if (!value)
        std::fprintf(stderr, "callback-gate FAIL: %s\n", label);
    return value;
}
}  // namespace

int main() {
    using Gate = fusehide::NativeCallbackGate;
    Gate gate;
    std::atomic_bool release{false};
    std::atomic_uint32_t entered{0};
    std::atomic_uint32_t left{0};
    std::vector<std::thread> callers;
    callers.reserve(8);
    for (int i = 0; i < 8; ++i) {
        callers.emplace_back([&] {
            if (!gate.TryEnter())
                return;
            entered.fetch_add(1, std::memory_order_release);
            while (!release.load(std::memory_order_acquire))
                std::this_thread::yield();
            left.fetch_add(1, std::memory_order_release);
            gate.Exit();
        });
    }
    while (entered.load(std::memory_order_acquire) != 8)
        std::this_thread::yield();
    bool ok = Check(gate.InFlight() == 8, "all eight entrants pinned");
    gate.CloseAdmission();
    gate.CloseAdmission();
    ok &= Check(gate.GetState() == Gate::State::kDraining && !gate.TryEnter(),
                "new callbacks rejected after drain");
    ok &= Check(!gate.FinishDrain() && gate.InFlight() == 8,
                "cannot close while callbacks are in flight");
    release.store(true, std::memory_order_release);
    for (auto& caller : callers)
        caller.join();
    ok &= Check(left.load(std::memory_order_acquire) == 8 && gate.InFlight() == 0,
                "all admitted callbacks exited");
    ok &= Check(gate.FinishDrain() && gate.FinishDrain() &&
                    gate.GetState() == Gate::State::kClosed && !gate.TryEnter(),
                "closed gate is final; no reentry");

    for (int iteration = 0; iteration < 128; ++iteration) {
        Gate racing;
        std::atomic_bool launch{false};
        std::atomic_uint32_t count{0};
        std::vector<std::thread> workers;
        for (int j = 0; j < 4; ++j) {
            workers.emplace_back([&] {
                while (!launch.load(std::memory_order_acquire))
                    std::this_thread::yield();
                for (int n = 0; n < 2000; ++n) {
                    if (!racing.TryEnter())
                        break;
                    count.fetch_add(1, std::memory_order_relaxed);
                    racing.Exit();
                }
            });
        }
        launch.store(true, std::memory_order_release);
        racing.CloseAdmission();
        for (auto& worker : workers)
            worker.join();
        ok &= Check(racing.InFlight() == 0 && racing.FinishDrain() && !racing.TryEnter(),
                    "racing admission/close has no unaccounted callbacks");
        if (!ok)
            break;
    }
    std::printf("callback-gate admitted=%u exited=%u state=%u result=%s\n",
                entered.load(std::memory_order_relaxed), left.load(std::memory_order_relaxed),
                static_cast<unsigned>(gate.GetState()), ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
