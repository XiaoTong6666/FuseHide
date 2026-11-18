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

#include "fusehide/hooks/wrapper_context.hpp"
#include "fusehide/hooks/wrappers.hpp"

#include "fusehide/policy/path_policy.hpp"

namespace fusehide {

// Path hook wrappers

bool HiddenPathPolicy::IsTestHiddenUid(uint32_t uid) {
    return BorrowHideRuleForUid(uid) != nullptr;
}

bool HiddenPathPolicy::ShouldHideTestPath(uint32_t uid, std::string_view path) {
    return IsHiddenPathClassification(ClassifyHiddenPath(uid, path));
}

// Mirror the original app-accessible gate: sanitize only when needed, then delegate.
bool WrappedIsAppAccessiblePath(void* fuse, AbiStringParam pathArg, uint32_t uid) {
    const auto original = gOriginalIsAppAccessiblePath.get();
    if (original == nullptr) {
        return false;
    }
    const std::string_view path = AbiStringView(pathArg);
    const bool hasHideRule = BorrowHideRuleForUid(uid) != nullptr;
    if (!UnicodePolicy::NeedsSanitization(path)) {
        UnicodePolicy::LogSuspiciousDirectPath("app_accessible", path);
        if (ShouldLogLimited(gAppAccessibleLogCount)) {
            DebugLogPrint(3, "app_accessible direct uid=%u path=%s", uid,
                          DebugPreview(path).c_str());
        }
        if (!hasHideRule) {
            return original(fuse, pathArg, uid);
        }
        const ScopedPathPolicyContext scopedPath(uid, path);
        NoteHiddenSubtreePathForCache(uid, path);
        if (HiddenPathPolicy::ShouldHideTestPath(uid, path)) {
            DebugLogPrint(4, "hide test path uid=%u path=%s", static_cast<unsigned>(uid),
                          DebugPreview(path).c_str());
            return false;
        }
        return original(fuse, pathArg, uid);
    }
    std::string sanitized(path);
    UnicodePolicy::RewriteString(sanitized);
    if (ShouldLogLimited(gAppAccessibleLogCount)) {
        DebugLogPrint(3, "app_accessible rewrite uid=%u old=%s new=%s", uid,
                      DebugPreview(path).c_str(), DebugPreview(sanitized).c_str());
    }
    const ScopedAbiStringParam sanitizedArg(sanitized);
    if (!hasHideRule) {
        return original(fuse, sanitizedArg.get(), uid);
    }
    const ScopedPathPolicyContext scopedPath(uid, sanitized);
    NoteHiddenSubtreePathForCache(uid, sanitized);
    if (HiddenPathPolicy::ShouldHideTestPath(uid, sanitized)) {
        DebugLogPrint(4, "hide test path uid=%u path=%s src=%s", static_cast<unsigned>(uid),
                      DebugPreview(sanitized).c_str(), DebugPreview(path).c_str());
        return false;
    }
    return original(fuse, sanitizedArg.get(), uid);
}

// The package-owned helper only sanitizes the first path argument on the device build.
bool WrappedIsPackageOwnedPath(AbiStringParam lhsArg, AbiStringParam rhsArg) {
    const auto original = gOriginalIsPackageOwnedPath.get();
    if (original == nullptr) {
        return false;
    }
    const std::string_view lhs = AbiStringView(lhsArg);
    const std::string_view rhs = AbiStringView(rhsArg);
    if (!UnicodePolicy::NeedsSanitization(lhs)) {
        UnicodePolicy::LogSuspiciousDirectPath("package_owned", lhs);
        if (ShouldLogLimited(gPackageOwnedLogCount)) {
            DebugLogPrint(3, "package_owned direct lhs=%s rhs=%s", DebugPreview(lhs).c_str(),
                          DebugPreview(rhs).c_str());
        }
        return original(lhsArg, rhsArg);
    }
    std::string sanitizedLhs(lhs);
    UnicodePolicy::RewriteString(sanitizedLhs);
    if (ShouldLogLimited(gPackageOwnedLogCount)) {
        DebugLogPrint(3, "package_owned rewrite lhs=%s new=%s rhs=%s", DebugPreview(lhs).c_str(),
                      DebugPreview(sanitizedLhs).c_str(), DebugPreview(rhs).c_str());
    }
    const ScopedAbiStringParam sanitizedArg(sanitizedLhs);
    return original(sanitizedArg.get(), rhsArg);
}

// WrappedIsBpfBackingPath
bool WrappedIsBpfBackingPath(AbiStringParam pathArg) {
    const auto original = gOriginalIsBpfBackingPath.get();
    if (original == nullptr) {
        return false;
    }
    const std::string_view path = AbiStringView(pathArg);
    if (!UnicodePolicy::NeedsSanitization(path)) {
        UnicodePolicy::LogSuspiciousDirectPath("bpf_backing", path);
        if (ShouldLogLimited(gBpfBackingLogCount)) {
            DebugLogPrint(3, "bpf_backing direct path=%s", DebugPreview(path).c_str());
        }
        return original(pathArg);
    }
    std::string sanitized(path);
    UnicodePolicy::RewriteString(sanitized);
    if (ShouldLogLimited(gBpfBackingLogCount)) {
        DebugLogPrint(3, "bpf_backing rewrite old=%s new=%s", DebugPreview(path).c_str(),
                      DebugPreview(sanitized).c_str());
    }
    const ScopedAbiStringParam sanitizedArg(sanitized);
    return original(sanitizedArg.get());
}

// Keep libc strcasecmp behavior aligned with the original case-folding compare.
extern "C" int WrappedStrcasecmp(const char* lhs, const char* rhs) {
    const char* lhsSafe = lhs != nullptr ? lhs : "";
    const char* rhsSafe = rhs != nullptr ? rhs : "";
    const auto finishAscii = [&](int result) {
        if constexpr (kEnableDebugHooks) {
            if (ShouldLogLimited(gStrcasecmpLogCount)) {
                const size_t lhsLen = std::strlen(lhsSafe);
                const size_t rhsLen = std::strlen(rhsSafe);
                DebugLogPrint(3, "strcasecmp lhs=%s rhs=%s result=%d",
                              DebugPreview(std::string_view(lhsSafe, lhsLen)).c_str(),
                              DebugPreview(std::string_view(rhsSafe, rhsLen)).c_str(), result);
            }
        }
        return result;
    };

    // Avoid the two strlen() passes and the Unicode machinery for the overwhelmingly common
    // ASCII case. If either side contains UTF-8, restart in the full comparator so default-
    // ignorable handling remains byte-for-byte compatible with the kernel casefold behavior.
    size_t index = 0;
    while (true) {
        const uint8_t lhsByte = static_cast<uint8_t>(lhsSafe[index]);
        const uint8_t rhsByte = static_cast<uint8_t>(rhsSafe[index]);
        if ((lhsByte | rhsByte) >= 0x80) {
            break;
        }
        const uint8_t lhsFolded = static_cast<uint8_t>(
            lhsByte >= 'A' && lhsByte <= 'Z' ? lhsByte + ('a' - 'A') : lhsByte);
        const uint8_t rhsFolded = static_cast<uint8_t>(
            rhsByte >= 'A' && rhsByte <= 'Z' ? rhsByte + ('a' - 'A') : rhsByte);
        if (lhsFolded != rhsFolded) {
            return finishAscii(static_cast<int>(lhsFolded) - static_cast<int>(rhsFolded));
        }
        if (lhsByte == 0) {
            return finishAscii(0);
        }
        ++index;
    }

    const size_t lhsLen = std::strlen(lhsSafe);
    const size_t rhsLen = std::strlen(rhsSafe);
    const int result = UnicodePolicy::CompareCaseFoldIgnoringDefaultIgnorables(
        reinterpret_cast<const uint8_t*>(lhsSafe), lhsLen,
        reinterpret_cast<const uint8_t*>(rhsSafe), rhsLen);
    if (ShouldLogLimited(gStrcasecmpLogCount)) {
        DebugLogPrint(3, "strcasecmp lhs=%s rhs=%s result=%d",
                      DebugPreview(std::string_view(lhsSafe, lhsLen)).c_str(),
                      DebugPreview(std::string_view(rhsSafe, rhsLen)).c_str(), result);
    }
    return result;
}

// ABI adapter for android::base::EqualsIgnoreCase(string_view, string_view).
extern "C" bool WrappedEqualsIgnoreCaseAbi(const char* lhsData, size_t lhsSize, const char* rhsData,
                                           size_t rhsSize) {
    const int result = UnicodePolicy::CompareCaseFoldIgnoringDefaultIgnorables(
        reinterpret_cast<const uint8_t*>(lhsData ? lhsData : ""), lhsSize,
        reinterpret_cast<const uint8_t*>(rhsData ? rhsData : ""), rhsSize);
    if (ShouldLogLimited(gEqualsIgnoreCaseLogCount)) {
        DebugLogPrint(3, "equals_ignore_case lhs=%s rhs=%s result=%d",
                      DebugPreview(std::string_view(lhsData ? lhsData : "", lhsSize)).c_str(),
                      DebugPreview(std::string_view(rhsData ? rhsData : "", rhsSize)).c_str(),
                      result);
    }
    return result == 0;
}

bool IsTestHiddenUid(uint32_t uid) {
    return HiddenPathPolicy::IsTestHiddenUid(uid);
}

bool ShouldHideTestPath(uint32_t uid, std::string_view path) {
    return HiddenPathPolicy::ShouldHideTestPath(uid, path);
}

}  // namespace fusehide
