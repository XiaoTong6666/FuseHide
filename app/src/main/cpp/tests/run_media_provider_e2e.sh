#!/usr/bin/env bash
# Evidence gate for a real MediaProvider process. Never installs modules,
# changes LSPosed scope, or reboots a device.
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo "Usage: $0 ADB_SERIAL {zygisk-v3|lsposed} [--restart]" >&2
    exit 2
fi
serial=$1
backend=$2
if [[ $backend != zygisk-v3 && $backend != lsposed ]]; then
    echo "Unsupported backend: $backend" >&2
    exit 2
fi
if [[ $# -eq 3 && $3 != --restart ]]; then
    exit 2
fi

process=
for name in com.android.providers.media.module com.google.android.providers.media.module; do
    pid=$(adb -s "$serial" shell pidof "$name" 2>/dev/null | tr -d '\r' || true)
    if [[ -n $pid ]]; then process=$name; break; fi
done
if [[ -z $process ]]; then
    echo "NOT_RUN: MediaProvider process is not running" >&2
    exit 2
fi
if [[ $# -eq 3 ]]; then
    adb -s "$serial" logcat -c
    adb -s "$serial" shell su -c "am force-stop $process" >/dev/null
    for i in $(seq 1 30); do
        adb -s "$serial" shell 'ls -ld /sdcard/Android' >/dev/null 2>&1 || true
        pid=$(adb -s "$serial" shell pidof "$process" 2>/dev/null | tr -d '\r' || true)
        [[ -n $pid ]] && break
        sleep 1
    done
    if [[ -z $pid ]]; then echo "FAIL: MediaProvider failed to restart" >&2; exit 1; fi
fi

pid=$(adb -s "$serial" shell pidof "$process" 2>/dev/null | tr -d '\r' || true)
if [[ -z $pid ]]; then echo "FAIL: MediaProvider disappeared" >&2; exit 1; fi
maps=$(adb -s "$serial" shell su -c "grep -m 1 -i fusehide /proc/$pid/maps" 2>/dev/null || true)
if [[ -z $maps ]]; then echo "FAIL: no FuseHide mapping in MediaProvider $pid" >&2; exit 1; fi

log=$(adb -s "$serial" logcat -d -v threadtime -s FuseHide:V '*:S' | awk -v pid="$pid" '$3 == pid')
if ! grep -q 'hooking libfuse_jni' <<<"$log"; then
    echo "FAIL: actual MediaProvider has no libfuse_jni hook initialization" >&2
    exit 1
fi
if ! grep -q 'hook summary' <<<"$log"; then
    echo "FAIL: missing FuseHide hook summary" >&2
    exit 1
fi
if [[ $backend == zygisk-v3 ]]; then
    for marker in 'Native API v3 strict publication enabled' \
                  'FuseHide initialized after'; do
        if ! grep -q "$marker" <<<"$log"; then
            echo "NOT_PROVEN: real MediaProvider $pid has no $marker" >&2
            exit 1
        fi
    done
fi

adb -s "$serial" shell 'ls -ld /sdcard/Android' >/dev/null
still=$(adb -s "$serial" shell pidof "$process" 2>/dev/null | tr -d '\r' || true)
if [[ $still != "$pid" ]]; then echo "FAIL: MediaProvider restarted during FUSE smoke" >&2; exit 1; fi
printf 'MEDIA_PROVIDER_E2E backend=%s process=%s pid=%s mapped=1 hook_summary=1 fuse_smoke=1 result=PASS\n' \
    "$backend" "$process" "$pid"
