#include "fusehide/core/state.hpp"

#include <algorithm>

namespace fusehide {

namespace {

inline constexpr const char* kExternalPayloadInitSymbol = "FuseHidePayloadInitV1";

bool HasCompatibleAbiRange(uint32_t localMinVersion, uint32_t localMaxVersion,
                           uint32_t remoteMinVersion, uint32_t remoteMaxVersion) {
    return localMinVersion <= remoteMaxVersion && remoteMinVersion <= localMaxVersion;
}

template <typename T>
bool HasCompatibleStructPrefix(uint32_t remoteStructSize) {
    return remoteStructSize >= offsetof(T, structSize) + sizeof(T::structSize);
}

bool IsCompatibleAnchorApi(const FuseHideAnchorApi& anchorApi) {
    if (!HasCompatibleStructPrefix<FuseHideAnchorApi>(anchorApi.structSize)) {
        return false;
    }
    return HasCompatibleAbiRange(
        kFuseHidePayloadAbiMinSupportedVersion, kFuseHidePayloadAbiMaxSupportedVersion,
        anchorApi.abiMinSupportedVersion, anchorApi.abiMaxSupportedVersion);
}

bool IsCompatiblePayloadApi(const FuseHidePayloadApi& payloadApi) {
    if (!HasCompatibleStructPrefix<FuseHidePayloadApi>(payloadApi.structSize)) {
        return false;
    }
    return HasCompatibleAbiRange(
        kFuseHidePayloadAbiMinSupportedVersion, kFuseHidePayloadAbiMaxSupportedVersion,
        payloadApi.abiMinSupportedVersion, payloadApi.abiMaxSupportedVersion);
}

bool HasRequiredPayloadCallbacks(const FuseHidePayloadApi& payloadApi) {
    return payloadApi.pfLookup != nullptr && payloadApi.pfReaddirPostfilter != nullptr &&
           payloadApi.pfLookupPostfilter != nullptr && payloadApi.pfMkdir != nullptr &&
           payloadApi.pfMknod != nullptr && payloadApi.pfUnlink != nullptr &&
           payloadApi.pfRmdir != nullptr && payloadApi.pfRename != nullptr &&
           payloadApi.pfCreate != nullptr && payloadApi.pfReaddir != nullptr &&
           payloadApi.doReaddirCommon != nullptr && payloadApi.pfReaddirplus != nullptr &&
           payloadApi.replyEntry != nullptr && payloadApi.replyAttr != nullptr &&
           payloadApi.replyBuf != nullptr && payloadApi.replyErr != nullptr &&
           payloadApi.pfGetattr != nullptr && payloadApi.getDirectoryEntries != nullptr &&
           payloadApi.addDirectoryEntriesFromLowerFs != nullptr && payloadApi.lstat != nullptr &&
           payloadApi.stat != nullptr && payloadApi.getxattr != nullptr &&
           payloadApi.lgetxattr != nullptr && payloadApi.mkdirLibc != nullptr &&
           payloadApi.mknodLibc != nullptr && payloadApi.openLibc != nullptr &&
           payloadApi.open2Libc != nullptr && payloadApi.shouldNotCache != nullptr &&
           payloadApi.isAppAccessiblePath != nullptr;
}

std::shared_ptr<NativeGeneration> CreateGeneration(FuseHidePayloadApi api, uint64_t versionCode,
                                                   std::string versionHash, std::string payloadPath,
                                                   void* payloadHandle, bool external) {
    AnchorState& anchor = Anchor();
    auto generation = std::make_shared<NativeGeneration>();
    generation->id = anchor.nextGenerationId.fetch_add(1, std::memory_order_relaxed);
    generation->versionCode = versionCode;
    generation->versionHash = std::move(versionHash);
    generation->payloadPath = std::move(payloadPath);
    generation->payloadHandle = payloadHandle;
    generation->external = external;
    generation->api = api;
    return generation;
}

void CleanupRetiredGenerationsLocked(AnchorState& anchor) {
    auto& retired = anchor.retiredGenerations;
    retired.erase(
        std::remove_if(retired.begin(), retired.end(),
                       [](const auto& generation) {
                           if (!generation) {
                               return true;
                           }
                           if (generation->activeCalls.load(std::memory_order_acquire) != 0) {
                               return false;
                           }
                           if (!generation->external || generation->payloadHandle == nullptr) {
                               return true;
                           }
                           dlclose(generation->payloadHandle);
                           generation->payloadHandle = nullptr;
                           return true;
                       }),
        retired.end());
}

bool ActivateGenerationLocked(AnchorState& anchor, std::shared_ptr<NativeGeneration> generation,
                              uint64_t* outGenerationId) {
    if (!generation) {
        return false;
    }
    CleanupRetiredGenerationsLocked(anchor);
    if (anchor.activeGeneration != nullptr) {
        anchor.activeGeneration->draining.store(true, std::memory_order_release);
        anchor.retiredGenerations.push_back(anchor.activeGeneration);
    }
    anchor.activeGeneration = generation;
    if (outGenerationId != nullptr) {
        *outGenerationId = generation->id;
    }
    CleanupRetiredGenerationsLocked(anchor);
    return true;
}

FuseHideAnchorApi MakeAnchorApi() {
    return FuseHideAnchorApi{
        .abiVersion = kFuseHidePayloadAbiVersion,
        .abiMinSupportedVersion = kFuseHidePayloadAbiMinSupportedVersion,
        .abiMaxSupportedVersion = kFuseHidePayloadAbiMaxSupportedVersion,
        .structSize = sizeof(FuseHideAnchorApi),
        .currentHideConfig = +[]() { return CurrentHideConfig(); },
        .applyHideConfig = +[](HideConfig config) { ApplyHideConfig(std::move(config)); },
        .reqUid = +[](fuse_req_t req) { return RuntimeState::ReqUid(req); },
        .rememberFuseSession = +[](fuse_req_t req) { RuntimeState::RememberFuseSession(req); },
        .scheduleHiddenEntryInvalidation =
            +[]() { RuntimeState::ScheduleHiddenEntryInvalidation(); },
        .scheduleSpecificEntryInvalidation =
            +[](uint64_t parent, std::string_view name) {
                RuntimeState::ScheduleSpecificEntryInvalidation(parent, name);
            },
        .scheduleHiddenInodeInvalidation =
            +[](uint64_t ino) { RuntimeState::ScheduleHiddenInodeInvalidation(ino); },
    };
}

void AnchorWorkerLoop() {
    AnchorState& anchor = Anchor();
    std::unique_lock<std::mutex> lock(anchor.schedulerMutex);
    for (;;) {
        if (anchor.schedulerStop) {
            anchor.scheduledTasks.clear();
            return;
        }
        if (anchor.scheduledTasks.empty()) {
            anchor.schedulerCv.wait(
                lock, [&]() { return anchor.schedulerStop || !anchor.scheduledTasks.empty(); });
            continue;
        }
        std::sort(anchor.scheduledTasks.begin(), anchor.scheduledTasks.end(),
                  [](const ScheduledAnchorTask& lhs, const ScheduledAnchorTask& rhs) {
                      return lhs.dueAt < rhs.dueAt;
                  });
        const auto nextDueAt = anchor.scheduledTasks.front().dueAt;
        anchor.schedulerCv.wait_until(lock, nextDueAt, [&]() {
            return anchor.schedulerStop || anchor.scheduledTasks.empty() ||
                   anchor.scheduledTasks.front().dueAt < nextDueAt;
        });
        if (anchor.schedulerStop) {
            continue;
        }
        const auto now = std::chrono::steady_clock::now();
        if (anchor.scheduledTasks.empty() || anchor.scheduledTasks.front().dueAt > now) {
            continue;
        }
        auto task = std::move(anchor.scheduledTasks.front().task);
        anchor.scheduledTasks.pop_front();
        lock.unlock();
        task();
        lock.lock();
    }
}

void EnsureAnchorWorkerLocked(AnchorState& anchor) {
    if (anchor.schedulerStarted) {
        return;
    }
    anchor.schedulerStop = false;
    anchor.schedulerThread = std::thread(AnchorWorkerLoop);
    anchor.schedulerStarted = true;
}

bool ActivateGeneration(FuseHidePayloadApi api, uint64_t versionCode, std::string versionHash,
                        std::string payloadPath, void* payloadHandle, bool external,
                        uint64_t* outGenerationId) {
    AnchorState& anchor = Anchor();
    auto generation = CreateGeneration(std::move(api), versionCode, std::move(versionHash),
                                       std::move(payloadPath), payloadHandle, external);
    std::lock_guard<std::mutex> lock(anchor.generationMutex);
    return ActivateGenerationLocked(anchor, std::move(generation), outGenerationId);
}

}  // namespace

AnchorState& Anchor() {
    static auto* anchor = new AnchorState();
    return *anchor;
}

bool EnsureAnchorGenerationInitialized() {
    AnchorState& anchor = Anchor();

    {
        std::lock_guard<std::mutex> lock(anchor.generationMutex);
        if (anchor.activeGeneration != nullptr) {
            return true;
        }
    }

    FuseHidePayloadApi api;
    const FuseHideAnchorApi anchorApi = MakeAnchorApi();
    if (!BuiltinFuseHidePayloadInit(&anchorApi, &api)) {
        return false;
    }

    auto generation = CreateGeneration(std::move(api), 0, "builtin", "", nullptr, false);
    std::lock_guard<std::mutex> lock(anchor.generationMutex);
    if (anchor.activeGeneration != nullptr) {
        return true;
    }
    return ActivateGenerationLocked(anchor, std::move(generation), nullptr);
}

std::shared_ptr<NativeGeneration> AcquireActiveGeneration() {
    if (!EnsureAnchorGenerationInitialized()) {
        return nullptr;
    }
    AnchorState& anchor = Anchor();
    std::lock_guard<std::mutex> lock(anchor.generationMutex);
    CleanupRetiredGenerationsLocked(anchor);
    if (anchor.activeGeneration != nullptr) {
        anchor.activeGeneration->activeCalls.fetch_add(1, std::memory_order_acq_rel);
    }
    return anchor.activeGeneration;
}

bool SwitchToBuiltinGeneration(uint64_t versionCode, std::string versionHash,
                               uint64_t* outGenerationId) {
    FuseHidePayloadApi api;
    const FuseHideAnchorApi anchorApi = MakeAnchorApi();
    if (!BuiltinFuseHidePayloadInit(&anchorApi, &api)) {
        return false;
    }

    return ActivateGeneration(std::move(api), versionCode, std::move(versionHash), "", nullptr,
                              false, outGenerationId);
}

bool SwitchToExternalGeneration(std::string payloadPath, uint64_t versionCode,
                                std::string versionHash, uint64_t* outGenerationId) {
    if (payloadPath.empty()) {
        return false;
    }

    void* payloadHandle = dlopen(payloadPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (payloadHandle == nullptr) {
        const char* error = dlerror();
        DebugLogPrint(6, "external payload dlopen failed path=%s err=%s", payloadPath.c_str(),
                      error != nullptr ? error : "unknown");
        return false;
    }

    auto initFn =
        reinterpret_cast<FuseHidePayloadInitFn>(dlsym(payloadHandle, kExternalPayloadInitSymbol));
    if (initFn == nullptr) {
        DebugLogPrint(6, "external payload missing symbol path=%s symbol=%s", payloadPath.c_str(),
                      kExternalPayloadInitSymbol);
        dlclose(payloadHandle);
        return false;
    }

    FuseHidePayloadApi api;
    const FuseHideAnchorApi anchorApi = MakeAnchorApi();
    if (!initFn(&anchorApi, &api)) {
        DebugLogPrint(6, "external payload init failed path=%s", payloadPath.c_str());
        dlclose(payloadHandle);
        return false;
    }

    if (!IsCompatiblePayloadApi(api)) {
        DebugLogPrint(
            6,
            "external payload ABI mismatch path=%s payload_abi=%u payload_range=%u-%u "
            "payload_size=%u supported_range=%u-%u required_prefix=%zu",
            payloadPath.c_str(), api.abiVersion, api.abiMinSupportedVersion,
            api.abiMaxSupportedVersion, api.structSize, kFuseHidePayloadAbiMinSupportedVersion,
            kFuseHidePayloadAbiMaxSupportedVersion,
            offsetof(FuseHidePayloadApi, structSize) + sizeof(FuseHidePayloadApi::structSize));
        dlclose(payloadHandle);
        return false;
    }
    if (!HasRequiredPayloadCallbacks(api)) {
        DebugLogPrint(6, "external payload missing required callbacks path=%s",
                      payloadPath.c_str());
        dlclose(payloadHandle);
        return false;
    }

    if (!IsCompatibleAnchorApi(anchorApi)) {
        DebugLogPrint(
            6,
            "anchor API ABI mismatch path=%s anchor_abi=%u anchor_range=%u-%u "
            "anchor_size=%u supported_range=%u-%u required_prefix=%zu",
            payloadPath.c_str(), anchorApi.abiVersion, anchorApi.abiMinSupportedVersion,
            anchorApi.abiMaxSupportedVersion, anchorApi.structSize,
            kFuseHidePayloadAbiMinSupportedVersion, kFuseHidePayloadAbiMaxSupportedVersion,
            offsetof(FuseHideAnchorApi, structSize) + sizeof(FuseHideAnchorApi::structSize));
        dlclose(payloadHandle);
        return false;
    }

    return ActivateGeneration(std::move(api), versionCode, std::move(versionHash),
                              std::move(payloadPath), payloadHandle, true, outGenerationId);
}

uint64_t CurrentNativeGenerationId() {
    AnchorState& anchor = Anchor();
    std::lock_guard<std::mutex> lock(anchor.generationMutex);
    CleanupRetiredGenerationsLocked(anchor);
    return anchor.activeGeneration != nullptr ? anchor.activeGeneration->id : 0;
}

void MaybeCleanupRetiredGenerations() {
    AnchorState& anchor = Anchor();
    std::lock_guard<std::mutex> lock(anchor.generationMutex);
    CleanupRetiredGenerationsLocked(anchor);
}

void EnqueueAnchorTask(std::chrono::milliseconds delay, std::function<void()> task) {
    AnchorState& anchor = Anchor();
    std::lock_guard<std::mutex> lock(anchor.schedulerMutex);
    EnsureAnchorWorkerLocked(anchor);
    ScheduledAnchorTask scheduledTask{
        .dueAt = std::chrono::steady_clock::now() + delay,
        .task = std::move(task),
    };
    const auto insertPos = std::upper_bound(
        anchor.scheduledTasks.begin(), anchor.scheduledTasks.end(), scheduledTask.dueAt,
        [](const std::chrono::steady_clock::time_point& dueAt, const ScheduledAnchorTask& task) {
            return dueAt < task.dueAt;
        });
    anchor.scheduledTasks.insert(insertPos, std::move(scheduledTask));
    anchor.schedulerCv.notify_all();
}

}  // namespace fusehide
