// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0
#include <cstdio>
#include <dlfcn.h>
#include <jni.h>
#include <atomic>
#include <cstring>

#include "dobby.h"

#include "zygisk/dlopen_monitor.hpp"

// This standalone process tests the real Android linker hook without
// injecting into system_server or changing a persistent Magisk module.
JavaVM* gVm = nullptr;
void* gFuseHideHandle = nullptr;
void RegisterAllNativeMethods(JNIEnv*) {
}
bool StartInjectedJava(JNIEnv*) {
    return false;
}

extern "C" int FuseHideDlopenMonitorReviewState();
extern "C" int FuseHideDlopenMonitorReviewRecoveryState();
extern "C" void* FuseHideDlopenMonitorReviewTarget();

std::atomic_bool gInjectPostCommitFailure{false};
extern "C" MemoryOperationError __real_DobbyCodePatch(void*, uint8_t*, uint32_t);
extern "C" MemoryOperationError __wrap_DobbyCodePatch(void* address, uint8_t* data, uint32_t size) {
    const auto result = __real_DobbyCodePatch(address, data, size);
    if (result == kMemoryOperationSuccess && address == FuseHideDlopenMonitorReviewTarget() &&
        gInjectPostCommitFailure.exchange(false, std::memory_order_acq_rel))
        return kMemoryOperationError;
    return result;
}

int main(int argc, char** argv) {
    const bool inject = argc == 2 && strcmp(argv[1], "--rollback") == 0;
    if (inject)
        gInjectPostCommitFailure.store(true, std::memory_order_release);
    InstallDlopenMonitor();
    if (inject) {
        if (gInjectPostCommitFailure.load(std::memory_order_acquire) ||
            FuseHideDlopenMonitorReviewRecoveryState() != 1 ||
            FuseHideDlopenMonitorReviewState() != 0) {
            std::fprintf(stderr, "linker-monitor FAIL: failed Commit did not retain its ticket\n");
            return 1;
        }
        void* interim = dlopen("libz.so", RTLD_NOW | RTLD_LOCAL);
        if (!interim || !dlsym(interim, "deflate")) {
            std::fprintf(stderr, "linker-monitor FAIL: published rollback lost original\n");
            return 1;
        }
        dlclose(interim);
        // Must Recover the first transaction, not simply race it with another
        // Prepare on the same linker address.
        InstallDlopenMonitor();
    }
    if (FuseHideDlopenMonitorReviewState() != 1) {
        std::fprintf(stderr, "linker-monitor FAIL: do_dlopen installation not active\n");
        return 1;
    }
    // Idempotence must not register a second physical hook.
    InstallDlopenMonitor();
    void* handle = dlopen("libz.so", RTLD_NOW | RTLD_LOCAL);
    if (!handle || !dlsym(handle, "deflate")) {
        std::fprintf(stderr, "linker-monitor FAIL: original linker invocation\n");
        return 1;
    }
    dlclose(handle);
    if (FuseHideDlopenMonitorReviewState() != 1) {
        std::fprintf(stderr, "linker-monitor FAIL: hook lost after dlopen\n");
        return 1;
    }
    std::printf("linker-monitor mode=%s result=PASS\n", inject ? "rollback" : "normal");
    return 0;
}
