# FuseHide

[🇨🇳 中文 README](README.zh-CN.md)

FuseHide is an Android 12+ storage-visibility module and MediaProvider/FUSE research tool. It injects into the MediaProvider process and hooks the userspace FUSE implementation in `libfuse_jni.so`, allowing selected apps to see a filtered view of `/storage/emulated/0` (`/sdcard`) according to runtime-configurable, per-UID rules.

FuseHide currently provides two injection backends:

- **LSPosed / libxposed**, using the Android app as an Xposed module.
- **Standalone Zygisk**, using a flashable root-module package and Dobby-based native hooks without requiring LSPosed.

Both backends converge on the same `libfusehide.so` runtime, the same path policy, and the same configuration model.

FuseHide also keeps the original MediaProvider-oriented Unicode research and mitigation path for `Android/data` and `Android/obb`. Normal `/sdcard` path hiding and the special `Android/data` / `Android/obb` path are related to the same MediaProvider process, but they are **not the same hook path**.

> [!WARNING]
> FuseHide works by hooking internal MediaProvider and FUSE implementation details. These ABIs are not public Android APIs and can change between Android releases, MediaProvider updates, and vendor ROMs. The project deliberately skips unsafe hooks when an ABI cannot be verified instead of blindly applying offsets or layouts.

## Features

- Hide first-level entries under `/storage/emulated/0` for selected packages.
- Hide nested relative paths such as `Download/private` without hiding their visible parent directory.
- Define package-specific hide rules in addition to global rules.
- Optional stress-test mode for hiding all first-level entries except configured exemptions.
- Filter directory enumeration as well as direct lookup/access paths.
- Reduce cache-based visibility leaks through entry/attribute timeout control and active invalidation.
- Hot-reload hide configuration into the already injected MediaProvider process.
- Query the actual native configuration currently applied inside MediaProvider.
- LSPosed/libxposed backend and standalone Zygisk backend.
- Runtime ELF, file-backed ELF, relocation and verified device-profile based hook resolution.
- Compatibility handling for multiple MediaProvider directory-entry ABIs, including newer API 37 layouts.
- Debug/probe UI for filesystem operations and Unicode-path experiments.

## Requirements

FuseHide currently targets:

- Android 12 / API 31 or newer.
- MediaProvider package:
  - `com.android.providers.media.module`, or
  - `com.google.android.providers.media.module`.

For the **LSPosed backend**, a compatible LSPosed/libxposed environment is required.

For the **Zygisk backend**, a compatible root environment and Zygisk implementation are required. The packaged installer supports Magisk-compatible installation flows and KernelSU-style module installation; the actual runtime still depends on a working Zygisk implementation on the device.

## Injection modes

### LSPosed / libxposed

The Xposed metadata is under `app/src/main/resources/META-INF/xposed/`.

The runtime chain is:

```text
LSPosed / libxposed
    -> Entry.onPackageLoaded()
    -> only accept MediaProvider package names
    -> System.loadLibrary("fusehide")
    -> hook Application.attach()
    -> capture MediaProvider Application
    -> register config/status receivers
    -> native_init()
    -> PostNativeInit("libfuse_jni.so", ...)
    -> InstallFuseHooks()
```

`Entry` intentionally ignores non-MediaProvider processes.

The default static scope contains:

```text
com.android.providers.media.module
com.google.android.providers.media.module
```

### Standalone Zygisk

The Zygisk implementation lives under `app/src/main/cpp/zygisk/`.

The runtime chain is:

```text
Zygisk
    -> preAppSpecialize()
    -> match MediaProvider process name
    -> preload injected dex + libfusehide.so from the module directory
    -> postAppSpecialize()
    -> Dobby-hook linker do_dlopen
    -> wait until libfuse_jni.so is loaded
    -> call libfusehide native_init()
    -> provide DobbyHook through the native hook API bridge
    -> PostNativeInit("libfuse_jni.so", ...)
    -> register HideConfigNativeBridge JNI methods
    -> start ZygiskEntry with the MediaProvider system context
```

The injected Java classes are loaded from the APK dex payload through `InMemoryDexClassLoader`.

### LSPosed and Zygisk arbitration

FuseHide should not install both hook backends into the same MediaProvider process.

The Zygisk module contains `post-fs-data.sh` logic that checks LSPosed's module/scope database for an enabled FuseHide MediaProvider scope. When that scope is detected, the module records `lsp_scope_enabled` and disables the Zygisk module path. The native Zygisk entry also checks the marker and refuses to inject when the LSPosed path is active.

In other words, **use one injection backend for MediaProvider at a time**.

## How path hiding works

Normal path hiding does **not** require fuse-bpf. FuseHide performs the policy enforcement inside MediaProvider's userspace FUSE daemon.

A simplified request flow is:

```text
restricted app
    -> Linux VFS
    -> /dev/fuse
    -> MediaProvider libfuse_jni worker
    -> FuseHide request/reply hooks
    -> per-UID hide policy
    -> filtered FUSE result
```

FuseHide does not rely on only one lookup hook. The implementation covers several layers so that a path cannot remain visible simply because a different filesystem operation was used.

### Direct path operations

The native layer hooks or wraps relevant MediaProvider paths such as:

- `is_app_accessible_path`
- `pf_lookup`
- `pf_lookup_postfilter`
- `pf_getattr`
- `pf_create`
- `pf_mkdir`
- `pf_mknod`
- `pf_unlink`
- `pf_rmdir`
- `pf_rename`
- selected lower-filesystem fallbacks such as `stat`, `lstat`, `open`, `getxattr`, and related calls

When a resolved per-UID rule says that the requested path belongs to a hidden subtree, FuseHide attempts to make the target behave as absent rather than merely inaccessible.

### Directory enumeration

Hiding a path from `stat()` or lookup is not enough: the entry must also disappear from directory listings.

FuseHide therefore covers multiple enumeration stages, including:

- `pf_readdir`
- `pf_readdirplus`
- `pf_readdir_postfilter`
- `do_readdir_common`
- `GetDirectoryEntries`
- `addDirectoryEntriesFromLowerFs`
- `fuse_reply_buf`

`WrappedReplyBuf()` acts as a final FUSE-wire filtering layer. It recovers the request UID / inode / parent-path context, recognizes supported directory-entry payload layouts, removes entries matching the resolved hide rule, and forwards the rewritten reply buffer.

### Reply and cache handling

A hidden target may already have a positive dentry, inode, or attribute cache entry. Without cache handling, an app could observe a path even after the policy says it should be hidden.

FuseHide therefore also handles:

- `fuse_reply_entry`
- `fuse_reply_attr`
- `fuse_reply_err`
- `ShouldNotCache`
- `fuse_lowlevel_notify_inval_entry`
- `fuse_lowlevel_notify_inval_inode`

For hidden targets the implementation can zero entry/attribute timeouts, avoid caching, and schedule entry/inode invalidation. Runtime tracking is scoped carefully so that state learned from one UID is not blindly reused for another UID with a different hide policy.

## `Android/data` and `Android/obb`

`/storage/emulated/0/Android/data` and `/storage/emulated/0/Android/obb` are special MediaProvider paths. Their access control is not simply the same directory-enumeration path used for ordinary public `/sdcard` folders.

FuseHide retains hooks and diagnostics around components such as:

- `is_bpf_backing_path`
- `is_package_owned_path`
- case-insensitive path comparison
- ICU `Default_Ignorable_Code_Point` handling
- `MediaProvider.isUidAllowedAccessToDataOrObbPathForFuse()` tracing in debuggable builds

The Unicode policy recognizes default-ignorable characters and normalizes the affected comparison paths so that visually disguised path components do not escape the intended MediaProvider checks.

This special-path work should not be confused with normal configured path hiding: ordinary `/sdcard` filtering is implemented through the FUSE request/reply path even when fuse-bpf is not running.

## Configuration model

FuseHide maintains a runtime `HideConfig` with these main fields:

- `enableHideAllRootEntries`
- `hideAllRootEntriesExemptions`
- `hiddenRootEntryNames`
- `hiddenRelativePaths`
- `hiddenPackages`
- `packageRules`

### Global rules

`hiddenRootEntryNames` contains first-level names under `/storage/emulated/0`.

For example:

```text
su
daemonsu
```

`hiddenRelativePaths` contains paths relative to `/storage/emulated/0`:

```text
Download/private
Documents/internal/test
```

Leading/trailing slashes and repeated separators are normalized by the native policy.

### Which apps receive global rules

`hiddenPackages` is the package allow-list for the **global** hide rule.

For each FUSE request, FuseHide resolves the request UID to its current package set through `PackageManager#getPackagesForUid()`. If one of those packages is present in `hiddenPackages`, the global rule is merged into that UID's effective rule.

The UID-to-rule result is cached, but package add/remove broadcasts invalidate the package-set generation and derived caches so a stale package association is not kept indefinitely.

### Package-specific rules

Package-specific targets are stored as `PackageHideRule` objects and can be edited with `[package.name]` sections.

Example:

```text
su
daemonsu

[io.github.xiaotong6666.fusehide]
xinhao
Download/private

[com.eltavine.duckdetector]
MT2
xinhao
```

Rules before the first section are global targets. A value containing `/` is treated as a relative path; a value without `/` is treated as a first-level entry name.

Package-specific rules are matched against every package currently belonging to the request UID and merged together. A package-specific section can therefore apply even when that package is not listed in `hiddenPackages`; `hiddenPackages` specifically controls inheritance of the global rule.

### Current source defaults

The current native defaults are:

Global root names:

```text
su
daemonsu
```

Packages receiving the global rule:

```text
com.eltavine.duckdetector
io.github.xiaotong6666.fusehide
io.github.a13e300.fusefixer
```

Package-specific defaults:

```text
[io.github.xiaotong6666.fusehide]
xinhao

[com.eltavine.duckdetector]
MT2
xinhao
```

`enableHideAllRootEntries` is disabled by default.

When enabled, all first-level entries are considered hidden except configured exemptions. The source defaults currently keep these names visible:

```text
Android
DCIM
Document
Download
Movies
Pictures
```

## Runtime configuration sync

The configuration UI is not just an editor for local preferences. FuseHide can synchronize a configuration into the already injected MediaProvider process and ask that process to report the native configuration it actually has applied.

The main pieces are:

- `HideConfigStore`
- `HideConfigProvider`
- `HideConfigRequestReceiver`
- the injected `Entry` / `ZygiskEntry`
- `HideConfigNativeBridge`

The injected process first attempts to recover a persisted injected-process snapshot, then obtains current app-side configuration through the provider path. A broadcast fallback is available when the provider path cannot be used.

Reload transactions use reload tokens, and the fallback IPC path contains authentication checks rather than accepting arbitrary unauthenticated configuration broadcasts.

Configuration changes are published atomically to the native policy. Applying a new configuration increments its generation and invalidates dependent UID rules, root snapshots, path-classification caches, and tracked hidden targets.

The injected process also retries configuration loading around boot/user lifecycle events such as:

- `ACTION_LOCKED_BOOT_COMPLETED`
- `ACTION_BOOT_COMPLETED`
- `ACTION_USER_UNLOCKED`

## Installation

Download artifacts from [GitHub Releases](https://github.com/XiaoTong6666/FuseHide/releases).

### LSPosed mode

1. Install the FuseHide APK.
2. Enable FuseHide in LSPosed.
3. Scope it to MediaProvider:
   - `com.android.providers.media.module`
   - `com.google.android.providers.media.module`
4. Restart the scoped MediaProvider process or reboot the device.
5. Open FuseHide and check the runtime status page.

Only the MediaProvider package present on your ROM needs to be active.

### Zygisk mode

1. Use a compatible root environment with a working Zygisk implementation.
2. Install the FuseHide APK if you want the configuration/probe UI and provider-side runtime configuration.
3. Flash the `FuseHide-*-release.zip` module from the release assets with your root manager.
4. Reboot the device.
5. Make sure FuseHide is not simultaneously scoped to MediaProvider in LSPosed.
6. Open the app and verify that the MediaProvider hook is active.

The module installer extracts the correct ABI's `libfusehide.so`, installs the Zygisk library, and extracts the injected dex payload from the packaged APK.

## Using the app

The current app has four main areas:

- **Home** — hook/runtime status, device information, applied-state overview.
- **Config** — global and per-app hide configuration.
- **Probe** — direct filesystem checks for debugging policy behavior.
- **Settings** — UI and application settings.

### Applying configuration

The configuration UI distinguishes the editable draft from the configuration currently applied in MediaProvider.

Relevant actions include:

- **Apply** — persist the edited configuration and request MediaProvider to reload it.
- **Refresh Applied Config** — ask the injected MediaProvider process for its current native configuration snapshot.
- **Restore Defaults** — restore source defaults into the editor.
- **Detailed Diff** — compare the current draft against the applied MediaProvider snapshot.

### Probe tools

The probe screen includes operations such as:

- `Stat`
- `Access`
- `List`
- `Open`
- `Get Con`
- `Create`
- `Mkdir`
- `Rename/Move`
- `Rmdir`
- `Unlink`
- `All PKG`
- `App Data`
- `Insert ZWJ`

These tools are intended for debugging and reproducing MediaProvider/FUSE behavior, including Unicode-path cases.

### Example validation

If `Download/private` is hidden for the test app, the expected behavior is:

```text
/storage/emulated/0/Download              -> visible
/storage/emulated/0/Download/private      -> hidden
list(/storage/emulated/0/Download)        -> does not expose "private"
stat/open(private)                        -> behaves as hidden/absent
```

Repeated access should not make the target visible again merely because a positive FUSE/VFS cache entry was created earlier.

## Native compatibility strategy

MediaProvider's internal C++ ABI changes over time, so FuseHide does not treat one device's offsets as universal.

The native hook resolver can combine several evidence sources:

- normal ELF symbol lookup
- file-backed ELF parsing
- runtime/in-memory ELF parsing for embedded APEX-style mappings
- relocation/import resolution
- `.gnu_debugdata`
- resolved feature anchors
- known device profiles, but only when the profile is sufficiently verified

Some hooks require explicit ABI evidence. Examples include the libc++ string object layout and the MediaProvider `DirectoryEntries` container layout.

If the string ABI cannot be verified, hooks that would decode or construct those string objects are skipped. If the directory-entry ABI is unknown, unsafe C++ container hooks are skipped. `fuse_req_ctx` is resolved from the linker/import/libfuse path and does not fall back to an unverified hand-written struct layout.

This is intentional: an incomplete hook set is preferable to corrupting MediaProvider memory with an ABI guess.

## Build

Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/XiaoTong6666/FuseHide.git
cd FuseHide
```

If the repository was cloned without them:

```bash
git submodule update --init --recursive
```

The current build uses:

- Gradle Wrapper `9.7.1`
- Android Gradle Plugin `9.4.0`
- Kotlin `2.4.20`
- compileSdk / targetSdk `37`
- minSdk `31`
- Java / Kotlin target `17`
- Android NDK `30.0.15729638`
- C++20

CI builds with JDK 21; local builds require a JDK compatible with the current Android Gradle Plugin configuration.

Build debug and release artifacts:

```bash
./gradlew assembleDebug assembleRelease
```

The APK packaging pipeline also produces signed Zygisk module archives under:

```text
app/build/zygisk/
```

Useful Gradle tasks include:

```bash
./gradlew zipDebug
./gradlew zipRelease

./gradlew pushDebug
./gradlew pushRelease

./gradlew flashDebug
./gradlew flashRelease

./gradlew flashWithMagiskRelease
./gradlew flashWithKsudRelease

./gradlew flashAndRebootRelease
./gradlew flashWithMagiskAndRebootRelease
./gradlew flashWithKsudAndRebootRelease
```

`flash*` tasks use `adb` and therefore require a connected device and an appropriate root manager on that device.

For formatting:

```bash
./gradlew format
```

The version code is derived from:

```bash
git rev-list --count HEAD
```

and the version name is `1.<versionCode>`. Use a full-history clone for reproducible release versioning; a shallow clone changes the commit count.

## Project layout

```text
app/
  src/main/java/io/github/xiaotong6666/fusehide/
    config/           runtime configuration and IPC
    debug/            probe/debug helpers
    status/           injected-process status handling
    ui/               Compose application UI
    xposed/            LSPosed and injected Java entry points
  src/main/cpp/
    fusehide/          core FUSE hook/policy/runtime implementation
    zygisk/            standalone Zygisk injection backend
    third_party/Dobby/ native hook dependency

baselineprofile/       Android baseline-profile module
uihelper/              UI helper git submodule
template/module/       Zygisk/root-module template
scripts/               packaging/build helper scripts
docs/                  architecture and reverse-engineering notes
MediaProvider/          local AOSP MediaProvider reference tree; not part of the Gradle build
```

For the detailed FUSE hook architecture, see [`docs/architecture.md`](docs/architecture.md).

## Troubleshooting

### Capture logs

A simple filter is:

```bash
adb logcat -v time | grep -i FuseHide
```

or:

```bash
adb logcat -s FuseHide
```

Debug builds provide significantly more hook-resolution and request-path information than release builds.

### Module reports “not hooked”

Check that:

- the device is Android 12 / API 31 or newer;
- the ROM uses one of the supported MediaProvider package names;
- exactly one intended injection backend is active;
- LSPosed scope is correct when using LSPosed;
- Zygisk is actually active when using the standalone Zygisk module;
- MediaProvider or the device was restarted after installation/configuration changes.

### A path is still visible

When reporting a visibility problem, include both direct access and directory enumeration behavior. For example, say whether `stat`, `open`, and the parent `list` operation disagree.

Also include:

- device model
- Android version and ROM version
- kernel version
- MediaProvider version/APK information if available
- injection backend and version
- exact hidden configuration
- target application/package
- target path
- relevant `FuseHide` logcat

### `Android/data` behavior differs from normal folders

That can be expected. `Android/data` and `Android/obb` use special MediaProvider access-control/backing-path logic and should not be debugged as if they were ordinary public-directory entries.

The app exposes useful system state such as:

```text
ro.fuse.bpf.is_running
persist.sys.vold_app_data_isolation_enabled
external_storage.sdcardfs.enabled
```

These values help describe the device's storage environment, but normal configured FuseHide path filtering itself is not dependent on fuse-bpf being enabled.

## Releases

Release builds are published at:

https://github.com/XiaoTong6666/FuseHide/releases

The release workflow publishes both APK artifacts and a standalone Zygisk module ZIP.

## License

FuseHide is licensed under the [Apache License 2.0](LICENSE).

Third-party code keeps its own license terms. In particular, the bundled xz-embedded sources declare `SPDX-License-Identifier: 0BSD` in their source headers.

## Acknowledgements

Special thanks to **5ec1cff** for the prototype module reference and technical guidance.

FuseHide also builds on the Android/MediaProvider ecosystem and open-source projects used by the injection, UI, build, and native-hook layers.

## Disclaimer

FuseHide is intended for learning, debugging, compatibility research, and experiments on devices and environments you are authorized to modify. Internal Android implementation details may change without notice. You are responsible for evaluating the risks before using the project on a production device.
