#!/usr/bin/env bash
# Verify a real native module's callback ownership and Dobby Unhook lifecycle.
set -euo pipefail

if [[ $# -ne 4 ]]; then
    echo "Usage: $0 /path/libfusehide.so /path/fusehide_callback_lifetime_review /path/fusehide_callback_gate_review ADB_SERIAL" >&2
    exit 2
fi
library=$1
fixture=$2
gate_fixture=$3
serial=$4
for file in "$library" "$fixture" "$gate_fixture"; do
    if [[ ! -f "$file" ]]; then
        echo "Missing fixture: $file" >&2
        exit 2
    fi
done
if ! readelf -d "$library" | grep -q 'NODELETE'; then
    echo "Unsafe fixture: native API v2 callback DSO is missing DF_1_NODELETE: $library" >&2
    exit 2
fi

# Both native x86_64 and translated ARM64 run on the same AVD. Reusing a
# shared libfusehide.so pathname lets parallel suites overwrite each other's
# DSO between dlopen() and RTLD_NOLOAD, yielding misleading lifecycle errors.
# Own a unique remote directory per invocation and remove only that directory.
remote="/data/local/tmp/fusehide-lifetime-review-$$-${RANDOM}"
adb -s "$serial" shell "mkdir -p $remote"
trap 'adb -s "$serial" shell "rm -rf $remote" >/dev/null 2>&1 || :' EXIT
adb -s "$serial" push "$library" "$remote/libfusehide.so" >/dev/null
adb -s "$serial" push "$fixture" "$remote/fusehide_callback_lifetime_review" >/dev/null
adb -s "$serial" push "$gate_fixture" "$remote/fusehide_callback_gate_review" >/dev/null
adb -s "$serial" shell \
    "chmod 755 $remote/fusehide_callback_lifetime_review $remote/fusehide_callback_gate_review"
printf 'DEVICE=%s ABI=%s BRIDGE=%s\n' "$serial" \
    "$(adb -s "$serial" shell getprop ro.product.cpu.abilist | tr -d '\r')" \
    "$(adb -s "$serial" shell getprop ro.dalvik.vm.native.bridge | tr -d '\r')"
fixture_machine=$(readelf -h "$fixture" | grep 'Machine:')
case "$fixture_machine" in
    *AArch64*) expected_physical=1 ;;
    *X86-64*) expected_physical=0 ;;
    *) echo "Unsupported fixture architecture: $fixture_machine" >&2; exit 2 ;;
esac

if ! result=$(timeout 35s adb -s "$serial" shell "$remote/fusehide_callback_gate_review" 2>&1); then
    printf 'FAIL callback-gate\n%s\n' "$result" >&2
    exit 1
fi
if [[ "$result" != *'result=PASS'* ]]; then
    printf 'FAIL callback-gate\n%s\n' "$result" >&2
    exit 1
fi
printf 'PASS callback-gate %s\n' "$result"

for mode in v2 v2-no-unhook v3 v2-preclosed v3-preclosed \
            v2-init-close-race v3-init-close-race; do
    if ! result=$(timeout 35s adb -s "$serial" shell \
        "$remote/fusehide_callback_lifetime_review $remote/libfusehide.so $mode" 2>&1); then
        printf 'FAIL callback-lifetime-%s\n%s\n' "$mode" "$result" >&2
        exit 1
    fi
    expected_callbacks=50000
    expected_hook=$expected_physical
    if [[ "$mode" == *preclosed || "$mode" == *init-close-race ]]; then
        expected_callbacks=0
        expected_hook=0
    fi
    if [[ "$result" != *'result=PASS'* ||
          "$result" != *'nodelete=1'* ||
          "$result" != *"callbacks=$expected_callbacks"* ||
          "$result" != *"physical=$expected_hook"* ]]; then
        printf 'FAIL callback-lifetime-%s\n%s\n' "$mode" "$result" >&2
        exit 1
    fi
    printf 'PASS callback-lifetime-%s %s\n' "$mode" "$result"
done
echo "ALL_FUSEHIDE_CALLBACK_LIFETIME_MODES_PASSED"
