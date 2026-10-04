# FuseHide / Dobby Native Hook Review

These opt-in executables test the same adapter and linker-monitor sources
linked into Zygisk, but run in **isolated processes**. They do not replace a
Magisk module or modify system_server.

Build with the project's supported Android NDK:

```sh
cmake -S app/src/main/cpp -B /tmp/fusehide-native-review -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-31 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFUSEHIDE_NATIVE_HOOK_REVIEW_TEST=ON \
  -DFUSEHIDE_LINKER_REVIEW_TEST=ON
cmake --build /tmp/fusehide-native-review --target \
  fusehide_native_hook_review fusehide_linker_review
```

On each **native ARM64** or **ARM64-translated** Android device, push the
two binaries to `/data/local/tmp/`, then run each in its own process:

```sh
./fusehide_native_hook_review
./fusehide_linker_review
./fusehide_linker_review --rollback
```

Or run the automated wrapper, which checks every mode's exit status and
`result=PASS` marker:

```sh
bash app/src/main/cpp/tests/run_android_native_review.sh \
  /tmp/fusehide-native-review/fusehide_native_hook_review \
  /tmp/fusehide-native-review/fusehide_linker_review DEVICE_SERIAL
```

- Native v2: callable original, replacement, duplicate/concurrent ownership,
  foreign-hook Unhook refusal, and retained partial-commit recovery.
- Linker monitor: starts the `dl_iterate_phdr` observer first, then on ARM64
  only attempts strict file-backed hooks of public `android_dlopen_ext` /
  `dlopen`. A pristine public entry may be hooked with a four-byte near
  branch. A leading ARM64 BTI landing pad is preserved in place and the
  physical patch moves to `target+4`; foreign/prepatched, unverifiable, or
  unvalidated control-flow entries are rejected and remain observer-only.
  The smoke test verifies that ordinary `dlopen(libz.so)` remains usable in
  either mode.
- On x86_64 MediaProvider there is no trusted stop-the-world lease, so the
  production loader monitor and generic Native API intentionally do not
  publish multi-byte inline patches. The observer path is the expected
  production behavior. Dobby's separate `dobby_android_x64_quiescence_review`
  covers the managed-host case where a valid quiescence lease exists.
- Each program exits nonzero on a failed assertion and prints `result=PASS`
  only on success.

**Limits:** These tests do not prove MediaProvider JNI/native business
initialization or module unload. Native API v2 has no caller-specific
backup-ready callback: storing a pointer before Commit does not, by itself,
make every third-party callback owner data-race-free. Non-ARM64 long patches
also lack the required process-wide execution-safety protocol.

Real MediaProvider verification is deliberately a separate gate:

```sh
bash app/src/main/cpp/tests/run_media_provider_e2e.sh SERIAL lsposed
bash app/src/main/cpp/tests/run_media_provider_e2e.sh SERIAL zygisk-v3
```

The latter requires evidence from the **same actual MediaProvider PID** for
the v3 handshake, post-init and FUSE hook summary. Neither a standalone
`native_init_v3` ABI test nor Vector's LSPosed/native-v2 initialization can
pass it. The optional `--restart` flag only force-stops MediaProvider and
triggers a FUSE lookup; it does not install modules or reboot the device.

Real per-UID hiding is a further, **read-only** gate. Run it only after
MediaProvider and the relevant applications are already running:

```sh
python3 app/src/main/cpp/tests/run_uid_isolation_e2e.py --serial SERIAL \
  --case 'com.termux:MT2=hidden,NP=visible,Android=visible' \
  --case 'com.github.metacubex.clash.meta:MT2=visible,NP=visible,Android=visible' \
  --case 'io.github.xiaotong6666.fusehide:MT2=hidden,xinhao=hidden,NP=visible,Android=visible' \
  --case 'com.termux:MT2=hidden,NP=visible'
```

Adjust package names, entries and expectations to the actual configured
rules and existing directories; these example expectations are not universal
defaults. The probe reads each process's UID, supplementary groups and
SELinux context from `/proc`, enters its **actual mount namespace**, executes
`stat` and root directory enumeration under the matching identity, then
checks both visibility surfaces. It checks exact package process identity,
never starts applications, changes the hide configuration, creates a storage
file or installs a module, and cleans up its temporary `/data/local/tmp`
scripts. An unavailable target process is `NOT_RUN`, not an invented pass.

Do not replace it with `adb shell run-as PACKAGE ls ...`: `run-as` alone
inherits the ADB shell's mount namespace and supplementary groups, which can
produce misleading results even when its numeric UID matches the package.

### Cross-module callback drain / unload lifetime

Native API v2 does not provide callback unregistration. A host such as Vector
can therefore retain `native_init()`'s returned `onModuleLoaded` pointer for
the process lifetime even if the original owner calls `dlclose()`. FuseHide
links `libfusehide.so` with `-Wl,-z,nodelete`, suppresses duplicate
`native_init` callback publication, rejects a different host/API family from
rebinding the process-global hook functions, and guards every loader callback
with `NativeCallbackGate`.

The gate is intentionally a **logical** drain only:

1. close admission;
2. callbacks that had already entered remain counted;
3. finish succeeds only after the count reaches zero;
4. the closed state cannot reopen;
5. the DSO still remains mapped because an old host callback pointer or a
   previously fetched physical branch may exist.

Run:

```sh
bash app/src/main/cpp/tests/run_android_callback_lifetime_review.sh \
  /path/to/libfusehide.so \
  /path/to/fusehide_callback_lifetime_review \
  /path/to/fusehide_callback_gate_review DEVICE_SERIAL
```

The runner refuses a library without `NODELETE`. It tests v2, legacy v2 with
no Unhook function, strict v3, pre-closed initialization, duplicate/foreign
host binding, init-vs-close admission races, a cross-DSO physical Hook +
Unhook race on ARM64, callback drain while `dlclose()` runs, `RTLD_NOLOAD`,
executable-VMA retention, and late callback calls after retirement. An
init-vs-close race may linearize before or after callback publication, but
must always finish `Closed` with zero in-flight entrants; a published pointer
must become a safe no-op after closure. On unmanaged x86_64, the physical
Hook must remain rejected by the long-patch concurrency policy while the
callback lifetime checks still pass.

This does **not** claim true native-module unloading. True unmapping requires
a future generation-scoped host callback registration/unregistration API plus
physical Hook retirement and instruction-fetch grace. With Native API v2,
`dlclose()` is allowed only as a handle release; the module text stays
resident.

## Cross-module callback lifetime (native v2 / strict v3)

Build with `-DFUSEHIDE_CALLBACK_LIFETIME_TEST=ON`; compile the real
`libfusehide.so` from the same checkout. Its dynamic section must contain
`FLAGS_1: NODELETE`. Run:

```sh
bash app/src/main/cpp/tests/run_android_callback_lifetime_review.sh \
  /path/to/libfusehide.so \
  /tmp/fusehide-native-review/fusehide_callback_lifetime_review \
  /tmp/fusehide-native-review/fusehide_callback_gate_review DEVICE_SERIAL
```

The gate fixture pins eight entrants, rejects admission during drain, then
checks 128 admission/close races. The real-DSO fixture obtains `native_init`
or `native_init_v3`, checks that repeated registration and a foreign host
cannot steal the callback, physically hooks a synthetic **host target** to
the module's callback through Dobby (strict backup-ready for v3), and races
50,000 host/callback calls against Unhook, admission closure and `dlclose`.
On a native x86_64 host without a quiescence lease, it instead requires
`physical=0`: the unsafe multi-byte Hook must fail closed. On ARM64 native
or translated execution, it requires `physical=1` and exercises the real
cross-DSO Hook/Unhook path. Both must report `nodelete=1`.
The module must stay mapped and a stale callback must safely no-op after
logical retirement. v2 with a null `unhookFunc` is tested independently to
preserve the old host ABI. The v2/v3 `*-preclosed` modes also close logical
admission **before the first native_init**: they must reject new callback
registration but retain executable module text after `dlclose`.
The fixture verifies the retained callback PC against executable
`/proc/self/maps` VMAs; `RTLD_NOLOAD` is reported separately because it is
a name-based handle lookup rather than proof that the old code page exists.

**An admitted callback drain is not instruction-fetch grace or FUSE-hook
retirement.** Vector's existing native API has no callback unregister, so
the actual module remains `NODELETE` even after logical closure. This review
does not claim the MediaProvider business hooks can be physically removed
or that releasing the `.so` is supported.

The cross-DSO retirement fixture uses this exact order:

1. close new loader-callback admission;
2. restore the owned physical Dobby entry;
3. wait until all already-admitted callbacks have exited;
4. drop only the explicit `dlopen` handle;
5. keep the DSO executable mapping resident because Vector still owns the
   callback address.

The physical target thread has an explicit before/after-Unhook barrier. A
fast translated ARM64 worker can otherwise finish all calls before the
closing thread is scheduled, which makes a "restored target was called"
assertion nondeterministic even when Unhook itself is correct.

## Native callback and module lifetime

Vector Native API v2 stores the callback returned from `native_init()` in a
process-wide list and does not provide an unregister operation. `Unhook` can
restore an entry point but cannot revoke a callback already stored by Vector
or prove that a thread has stopped executing module text. Consequently,
`libfusehide.so` is linked with `-Wl,-z,nodelete`. This also protects a
previously fetched replacement when its Dobby ticket has been released.

Build the isolated callback test alongside the real module:

```sh
cmake -S app/src/main/cpp -B /tmp/fusehide-native-review -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-31 \
  -DFUSEHIDE_CALLBACK_LIFETIME_TEST=ON
cmake --build /tmp/fusehide-native-review --target fusehide \
  fusehide_callback_lifetime_review fusehide_callback_gate_review
```

Push the two executables and the freshly built `libfusehide.so` to a
temporary directory on the device. Run `fusehide_callback_gate_review` and
then `fusehide_callback_lifetime_review /absolute/path/libfusehide.so MODE`
in separate processes with `MODE=v2`, `v2-no-unhook`, and `v3`. The latter
verifies repeated initialization by one owner returns **nullptr rather
than registering the callback twice** (Vector appends every non-null
return), refuses a competing host or v2/v3 mode switch, performs
50,000 callback calls concurrently with `dlclose`, checks `RTLD_NOLOAD`,
and invokes the retained callback after close. It never calls FUSE hook
installation. Check `readelf -d` for `FLAGS_1: NODELETE` on each release
ABI; a successful `dlclose` return is not itself proof of unmapping.

The gate fixture uses the exact `NativeCallbackGate` type included by real
`PostNativeInit`: eight pinned callbacks block closure, new admissions are
rejected while draining, and 128 admission/close races must finish without
unaccounted entrants. This proves **logical** drain only. The production v2
host still has no callback unregister operation, so even a Closed gate
cannot authorize unmapping the `.so`.

The standalone Native Hook regression independently holds a replacement
callback in flight across an owned physical Unhook. Once the callback is
released, its retained original trampoline must still be callable and its
ticket must not allow a second Unhook.

**This is safe process-lifetime residency, not hot unload.** A future
unloadable native-host ABI needs ordered callback unregister/admission
closure, in-flight entrant drain, physical Unhook + synchronization,
trampoline/stale-fetch grace, and only then `dlclose`. The existing v2
layout does not carry this contract. Do not free this module or DartPlant's
`NODELETE` module just because a logical Hook has retired.
