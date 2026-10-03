#!/usr/bin/env bash
# Run each native/linker fixture in a fresh Android process.
set -euo pipefail

if [[ $# -ne 3 ]]; then
    echo "Usage: $0 /path/to/fusehide_native_hook_review /path/to/fusehide_linker_review ADB_SERIAL" >&2
    exit 2
fi
native_binary=$1
linker_binary=$2
serial=$3
for binary in "$native_binary" "$linker_binary"; do
    if [[ ! -f "$binary" ]]; then
        echo "Missing fixture: $binary" >&2
        exit 2
    fi
done

remote=/data/local/tmp
adb -s "$serial" push "$native_binary" "$remote/fusehide_native_hook_review" >/dev/null
adb -s "$serial" push "$linker_binary" "$remote/fusehide_linker_review" >/dev/null
adb -s "$serial" shell chmod 755 "$remote/fusehide_native_hook_review" "$remote/fusehide_linker_review"
printf 'DEVICE=%s ABI=%s BRIDGE=%s\n' "$serial" \
    "$(adb -s "$serial" shell getprop ro.product.cpu.abilist | tr -d '\r')" \
    "$(adb -s "$serial" shell getprop ro.dalvik.vm.native.bridge | tr -d '\r')"

for mode in native linker linker-rollback; do
    case "$mode" in
        native) command_path=$remote/fusehide_native_hook_review; args=() ;;
        linker) command_path=$remote/fusehide_linker_review; args=() ;;
        linker-rollback) command_path=$remote/fusehide_linker_review; args=(--rollback) ;;
    esac
    if ! output=$(adb -s "$serial" shell "$command_path" "${args[@]}" 2>&1); then
        printf 'FAIL %s\n%s\n' "$mode" "$output" >&2
        exit 1
    fi
    if ! grep -q 'result=PASS' <<<"$output" || grep -q 'result=FAIL' <<<"$output"; then
        printf 'FAIL %s\n%s\n' "$mode" "$output" >&2
        exit 1
    fi
    printf 'PASS %-18s %s\n' "$mode" "$output"
done
echo "ALL_FUSEHIDE_NATIVE_REVIEW_MODES_PASSED"
