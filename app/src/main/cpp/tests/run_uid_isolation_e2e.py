#!/usr/bin/env python3
"""Read-only real-app-namespace verification of MediaProvider FUSE UID rules.

Requires ADB and root on the device. The app must already be running. This
does not install an APK, change hide configuration, or replace a module.
"""

import argparse
import pathlib
import re
import shlex
import subprocess
import sys
import tempfile


INNER = r"""#!/system/bin/sh
printf 'IDENTITY|%s\n' "$(id)"
root_listing=$(/system/bin/ls -1 /storage/emulated/0 2>&1)
if [ "$?" -ne 0 ]; then
    printf 'ROOT_LIST_FAILED|%s\n' "$root_listing"
    exit 3
fi
for entry in "$@"; do
    if error=$(/system/bin/stat "/storage/emulated/0/$entry" 2>&1); then
        state=visible
    else
        case "$error" in
            *"No such file or directory"*) state=missing ;;
            *"Permission denied"*) state=denied ;;
            *) state=other_error ;;
        esac
    fi
    if printf '%s\n' "$root_listing" | /system/bin/grep -Fxq "$entry"; then
        listed=yes
    else
        listed=no
    fi
    printf 'CHECK|%s|%s|%s\n' "$entry" "$state" "$listed"
done
"""


def adb(serial, *args, timeout=25, check=True):
    result = subprocess.run(
        ["adb", "-s", serial, *args], capture_output=True, text=True,
        timeout=timeout, check=False,
    )
    if check and result.returncode:
        raise RuntimeError(
            f"ADB command failed ({result.returncode}): {' '.join(args)}\n"
            f"{result.stderr.strip()}\n{result.stdout.strip()}"
        )
    return result


def parse_case(spec):
    if ":" not in spec:
        raise ValueError("Case requires package:entry=hidden,entry=visible")
    package, entries = spec.split(":", 1)
    if not re.fullmatch(r"[A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+)+", package):
        raise ValueError(f"Invalid package: {package}")
    expected = {}
    for item in entries.split(","):
        if "=" not in item:
            raise ValueError(f"Invalid entry expectation: {item}")
        name, state = item.split("=", 1)
        if not re.fullmatch(r"[A-Za-z0-9_-][A-Za-z0-9_.-]*", name) or name in (".", ".."):
            raise ValueError(f"Invalid root entry: {name}")
        if state not in ("hidden", "visible"):
            raise ValueError(f"Invalid expectation {state}; expected hidden or visible")
        expected[name] = state
    if not expected:
        raise ValueError("Case must contain at least one entry")
    return package, expected


def process_identity(serial, package):
    result = adb(serial, "shell", "pidof", package, check=False)
    pids = re.findall(r"\b[1-9]\d*\b", result.stdout)
    if not pids:
        raise ValueError(f"NOT_RUN: {package} is not running; do not launch the app implicitly")
    for pid in pids:
        cmdline = adb(serial, "shell", "su", "-c", f"cat /proc/{pid}/cmdline").stdout
        if cmdline.split("\0", 1)[0] == package:
            break
    else:
        raise ValueError(f"NOT_RUN: {package} has no matching main process")
    status = adb(serial, "shell", "su", "-c", f"cat /proc/{pid}/status").stdout
    fields = {k: v.strip() for k, v in (
        line.split(":", 1) for line in status.splitlines() if ":" in line
    )}
    uid, gid = (int(fields[key].split()[0]) for key in ("Uid", "Gid"))
    groups = [int(group) for group in fields["Groups"].split()]
    context = adb(serial, "shell", "su", "-c", f"cat /proc/{pid}/attr/current").stdout
    context = context.strip("\0\r\n")
    if not re.fullmatch(r"u:r:[A-Za-z0-9_]+:s0(?::[c0-9,]+)?", context):
        raise ValueError(f"Unsupported process SELinux context: {context!r}")
    return pid, uid, gid, groups, context


def verify_one(serial, package, expected, inner_remote, tmpdir, case_index):
    pid, uid, gid, groups, context = process_identity(serial, package)
    options = ["-g", str(gid)]
    for group in groups:
        options += ["-G", str(group)]
    options += ["-Z", context, str(uid), "-c"]
    # An outer script is required: toybox nsenter otherwise tries to parse
    # arguments such as su -Z and ls -1 as *its own* flags.
    command = "/system/bin/sh " + inner_remote + " " + " ".join(
        shlex.quote(name) for name in expected
    )
    script = "#!/system/bin/sh\nexec /system/bin/su " + " ".join(
        shlex.quote(part) for part in options + [command]
    ) + "\n"
    local = tmpdir / f"fusehide-uid-outer-{case_index}.sh"
    remote = f"/data/local/tmp/fusehide-uid-outer-{case_index}.sh"
    local.write_text(script)
    adb(serial, "push", str(local), remote)
    output = adb(
        serial, "shell", "su", "-c",
        f"/system/bin/nsenter --mount=/proc/{pid}/ns/mnt /system/bin/sh {remote}",
    ).stdout
    identity = next((line for line in output.splitlines() if line.startswith("IDENTITY|")), "")
    if f"uid={uid}(" not in identity or f"context={context}" not in identity:
        raise RuntimeError(f"Identity mismatch for {package}: {identity or output}")
    observations = {}
    for line in output.splitlines():
        if line.startswith("CHECK|"):
            _, name, state, listed = line.split("|", 3)
            observations[name] = (state, listed)
    checks = 0
    for name, desired in expected.items():
        state, listed = observations.get(name, ("unobserved", "unobserved"))
        passed = (state, listed) == (
            ("missing", "no") if desired == "hidden" else ("visible", "yes")
        )
        print(
            f"{'PASS' if passed else 'FAIL'} package={package} pid={pid} uid={uid}"
            f" entry={name} expected={desired} stat={state} dirent={listed}"
        )
        if not passed:
            raise RuntimeError(
                f"UID isolation mismatch for {package}:{name}; raw output:\n{output}"
            )
        checks += 1
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True, help="ADB device serial")
    parser.add_argument(
        "--case", action="append", required=True,
        help="package:entry=hidden,entry=visible (repeat for each UID)",
    )
    args = parser.parse_args()
    cases = [parse_case(spec) for spec in args.case]
    adb(args.serial, "get-state")
    checks = 0
    inner_remote = "/data/local/tmp/fusehide-uid-inner.sh"
    with tempfile.TemporaryDirectory(prefix="fusehide-uid-") as temp:
        tmpdir = pathlib.Path(temp)
        inner_local = tmpdir / "inner.sh"
        inner_local.write_text(INNER)
        adb(args.serial, "push", str(inner_local), inner_remote)
        try:
            for index, (package, expected) in enumerate(cases):
                checks += verify_one(
                    args.serial, package, expected, inner_remote, tmpdir, index
                )
        finally:
            paths = " ".join(
                [inner_remote] +
                [f"/data/local/tmp/fusehide-uid-outer-{i}.sh" for i in range(len(cases))]
            )
            adb(args.serial, "shell", "su", "-c", f"rm -f {paths}", check=False)
    print(f"UID_ISOLATION_E2E result=PASS cases={len(cases)} checks={checks}")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
