# FuseHide

[🇺🇸 English README](README.md)

FuseHide 是一个面向 Android 12+ 的存储可见性模块与 MediaProvider/FUSE 研究工具。它会注入 `MediaProvider` 进程，并 Hook `libfuse_jni.so` 中的用户态 FUSE 实现，根据运行时配置和请求 UID，为指定应用提供经过过滤的 `/storage/emulated/0`（`/sdcard`）视图。

FuseHide 当前提供两套注入后端：

- **LSPosed / libxposed**：将 Android 应用本身作为 Xposed 模块注入。
- **独立 Zygisk**：通过可刷入的 root 模块和基于 Dobby 的 native hook，在不依赖 LSPosed 的情况下完成注入。

两条后端最终都会进入同一个 `libfusehide.so` runtime，使用同一套路径策略和配置模型。

FuseHide 同时保留了针对 `Android/data` 与 `Android/obb` 的 MediaProvider Unicode 研究与修复链路。普通 `/sdcard` 路径隐藏与 `Android/data` / `Android/obb` 特殊路径虽然都发生在 MediaProvider 中，但它们**并不是同一条 Hook 链路**。

> [!WARNING]
> FuseHide 依赖 MediaProvider 和 FUSE 的内部实现细节。这些 ABI 并不是公开 Android API，可能随着 Android 版本、MediaProvider 更新以及厂商 ROM 改动而变化。项目在无法验证 ABI 时会主动跳过不安全的 Hook，而不是盲目套用偏移或结构布局。

## 主要能力

- 按应用隐藏 `/storage/emulated/0` 下的一级目录。
- 隐藏 `Download/private` 这类嵌套相对路径，同时保持父目录可见。
- 在全局规则之外配置应用专属隐藏规则。
- 提供“隐藏所有一级目录但保留例外项”的压力测试模式。
- 不仅拦截直接 lookup/access，也过滤目录枚举结果。
- 通过 entry/attr timeout 与主动 invalidation 减少缓存导致的可见性泄漏。
- 将配置热同步到已经注入的 MediaProvider 进程。
- 读取 MediaProvider 当前真正应用的 native 配置快照。
- 同时支持 LSPosed/libxposed 与独立 Zygisk 后端。
- 支持 file-backed ELF、runtime ELF、relocation 与可信 device profile 等多种 Hook 解析路径。
- 兼容多种 MediaProvider 目录项 ABI，包括较新的 API 37 布局。
- 提供文件系统 Probe 与 Unicode 路径调试界面。

## 运行要求

FuseHide 当前面向：

- Android 12 / API 31 及以上。
- MediaProvider 包名：
  - `com.android.providers.media.module`，或
  - `com.google.android.providers.media.module`。

使用 **LSPosed 后端** 时，需要兼容的 LSPosed/libxposed 环境。

使用 **Zygisk 后端** 时，需要可用的 root 环境和 Zygisk 实现。打包的安装器支持 Magisk 兼容安装流程以及 KernelSU 风格的模块安装，但实际运行仍要求设备上的 Zygisk 能正常工作。

## 注入方式

### LSPosed / libxposed

Xposed 元数据位于：

```text
app/src/main/resources/META-INF/xposed/
```

实际运行链路是：

```text
LSPosed / libxposed
    -> Entry.onPackageLoaded()
    -> 只接受 MediaProvider 包名
    -> System.loadLibrary("fusehide")
    -> Hook Application.attach()
    -> 获取 MediaProvider Application
    -> 注册配置与状态 Receiver
    -> native_init()
    -> PostNativeInit("libfuse_jni.so", ...)
    -> InstallFuseHooks()
```

`Entry` 会直接忽略非 MediaProvider 进程。

默认静态作用域为：

```text
com.android.providers.media.module
com.google.android.providers.media.module
```

### 独立 Zygisk

Zygisk 实现在：

```text
app/src/main/cpp/zygisk/
```

实际运行链路是：

```text
Zygisk
    -> preAppSpecialize()
    -> 匹配 MediaProvider 进程名
    -> 从模块目录预加载 injected dex 与 libfusehide.so
    -> postAppSpecialize()
    -> 使用 Dobby Hook linker do_dlopen
    -> 等待 libfuse_jni.so 被加载
    -> 调用 libfusehide native_init()
    -> 通过 native hook API bridge 提供 DobbyHook
    -> PostNativeInit("libfuse_jni.so", ...)
    -> 注册 HideConfigNativeBridge JNI 方法
    -> 使用 MediaProvider system context 启动 ZygiskEntry
```

注入 Java 代码会从 APK 的 dex payload 中提取，并通过 `InMemoryDexClassLoader` 加载。

### LSPosed 与 Zygisk 仲裁

FuseHide 不应该让两套后端同时 Hook 同一个 MediaProvider 进程。

Zygisk 模块中的 `post-fs-data.sh` 会读取 LSPosed 的模块/作用域数据库，检查 FuseHide 是否已经启用并将 MediaProvider 加入作用域。如果检测到 LSPosed 路径已经生效，会记录 `lsp_scope_enabled` 并禁用 Zygisk 模块路径。native Zygisk 入口也会检查这个标记，并在 LSPosed 路径存在时拒绝继续注入。

因此，**同一个 MediaProvider 只使用一种注入后端**。

## 路径隐藏原理

普通路径隐藏**不依赖 fuse-bpf**。FuseHide 直接在 MediaProvider 用户态 FUSE daemon 中执行策略。

可以简化为：

```text
受限应用
    -> Linux VFS
    -> /dev/fuse
    -> MediaProvider libfuse_jni worker
    -> FuseHide request/reply hooks
    -> 按 UID 解析隐藏策略
    -> 返回过滤后的 FUSE 结果
```

FuseHide 并不是只 Hook 一个 lookup 函数。为了避免应用通过不同文件系统操作重新观察到目标，当前实现覆盖了多层链路。

### 直接路径操作

native 层会 Hook 或包装包括以下路径：

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
- `stat`、`lstat`、`open`、`getxattr` 等部分 lower-fs fallback

当当前请求 UID 对应的规则命中隐藏子树时，FuseHide 会尽量让目标表现为“不存在”，而不是单纯返回“没有权限”。

### 目录枚举过滤

只让 `stat()` 或 lookup 看不到目标还不够，目录列表本身也必须把条目过滤掉。

因此 FuseHide 还覆盖：

- `pf_readdir`
- `pf_readdirplus`
- `pf_readdir_postfilter`
- `do_readdir_common`
- `GetDirectoryEntries`
- `addDirectoryEntriesFromLowerFs`
- `fuse_reply_buf`

`WrappedReplyBuf()` 是最后一层 FUSE wire payload 过滤点。它会恢复当前请求的 UID / inode / parent path 上下文，识别已支持的目录项 payload 布局，移除命中当前隐藏规则的目录项，然后把重写后的 buffer 交回原始 `fuse_reply_buf`。

### Reply 与缓存处理

隐藏目标可能在策略变化之前已经产生 positive dentry、inode 或 attribute cache。如果不处理这些缓存，即使路径策略已经改为隐藏，应用仍可能观察到旧的可见结果。

因此当前实现同时处理：

- `fuse_reply_entry`
- `fuse_reply_attr`
- `fuse_reply_err`
- `ShouldNotCache`
- `fuse_lowlevel_notify_inval_entry`
- `fuse_lowlevel_notify_inval_inode`

对隐藏目标可以将 entry / attr timeout 置零、禁止缓存并主动安排 entry/inode invalidation。运行时状态也会按 UID 与有效规则进行隔离，避免把一个 UID 学到的隐藏状态错误复用给另一个策略不同的 UID。

## `Android/data` 与 `Android/obb`

`/storage/emulated/0/Android/data` 和 `/storage/emulated/0/Android/obb` 是 MediaProvider 中的特殊路径，它们的访问控制并不等价于普通公共 `/sdcard` 目录的枚举过滤路径。

FuseHide 在这部分保留了以下 Hook 和诊断逻辑：

- `is_bpf_backing_path`
- `is_package_owned_path`
- 大小写不敏感路径比较
- ICU `Default_Ignorable_Code_Point` 处理
- debug 构建下对 `MediaProvider.isUidAllowedAccessToDataOrObbPathForFuse()` 的跟踪

Unicode policy 会识别 default-ignorable 字符并对相关比较路径进行规范化，避免通过视觉上接近但实际包含零宽/可忽略码点的路径组件绕过预期 MediaProvider 判断。

这部分不能和普通路径隐藏混为一谈：普通 `/sdcard` 配置隐藏直接发生在 FUSE request/reply 链路中，即使设备没有运行 fuse-bpf，普通路径过滤仍然可以工作。

## 配置模型

FuseHide 的运行时 `HideConfig` 主要包含：

- `enableHideAllRootEntries`
- `hideAllRootEntriesExemptions`
- `hiddenRootEntryNames`
- `hiddenRelativePaths`
- `hiddenPackages`
- `packageRules`

### 全局规则

`hiddenRootEntryNames` 用来配置 `/storage/emulated/0` 下的一级名称。

例如：

```text
su
daemonsu
```

`hiddenRelativePaths` 用来配置相对于 `/storage/emulated/0` 的路径：

```text
Download/private
Documents/internal/test
```

native policy 会规范化路径前后的 `/` 与重复分隔符。

### 哪些应用获得全局规则

`hiddenPackages` 是**全局规则**的 package allow-list。

每个 FUSE 请求到来时，FuseHide 会通过 `PackageManager#getPackagesForUid()` 获取该请求 UID 当前对应的 package set。如果其中任意包存在于 `hiddenPackages`，该 UID 的有效规则就会合并全局隐藏项。

UID 到规则的解析结果会缓存，但应用安装/卸载广播会更新 package-set generation 并清理相关派生缓存，因此不会无限期保留已经过期的 UID/package 对应关系。

### 应用专属规则

应用专属目标会保存为 `PackageHideRule`，配置编辑器支持 `[package.name]` 分段格式。

例如：

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

第一个 `[package.name]` 之前的内容属于全局目标。包含 `/` 的值会解析为相对路径，不包含 `/` 的值会解析为一级目录名称。

针对一个 UID，FuseHide 会遍历这个 UID 当前拥有的所有 package，并将命中的 package-specific rule 合并起来。因此，即使某个包不在 `hiddenPackages` 中，它自己的 `[package.name]` 专属规则仍然可以生效；`hiddenPackages` 只决定它是否继承全局规则。

### 当前源码默认值

当前 native 默认全局一级名称：

```text
su
daemonsu
```

默认继承全局规则的包：

```text
com.eltavine.duckdetector
io.github.xiaotong6666.fusehide
io.github.a13e300.fusefixer
```

默认 package-specific rule：

```text
[io.github.xiaotong6666.fusehide]
xinhao

[com.eltavine.duckdetector]
MT2
xinhao
```

`enableHideAllRootEntries` 默认关闭。

启用后，所有一级目录都会作为隐藏候选，只有 exemption 保持可见。当前源码默认 exemption 为：

```text
Android
DCIM
Document
Download
Movies
Pictures
```

## 运行时配置同步

FuseHide 的配置页面并不只是修改本地 SharedPreferences。应用可以把配置同步到已经注入的 MediaProvider 进程，并反向读取该进程当前真正应用的 native 配置。

主要组件包括：

- `HideConfigStore`
- `HideConfigProvider`
- `HideConfigRequestReceiver`
- 注入后的 `Entry` / `ZygiskEntry`
- `HideConfigNativeBridge`

注入进程会优先尝试恢复已保存的 injected-process snapshot，然后通过 provider 获取当前应用侧配置；provider 无法使用时还存在 broadcast fallback。

reload 流程会使用 reload token，fallback IPC 也包含认证检查，而不是无条件接受任意外部广播传入的配置。

新配置会原子发布给 native policy。每次 `ApplyHideConfig()` 都会推进 config generation，并清理依赖该配置的 UID rule、root snapshot、path classification cache 与已跟踪隐藏目标。

在以下系统阶段，注入进程还会重新尝试加载配置：

- `ACTION_LOCKED_BOOT_COMPLETED`
- `ACTION_BOOT_COMPLETED`
- `ACTION_USER_UNLOCKED`

## 安装

从 [GitHub Releases](https://github.com/XiaoTong6666/FuseHide/releases) 下载构建产物。

### LSPosed 模式

1. 安装 FuseHide APK。
2. 在 LSPosed 中启用 FuseHide。
3. 将作用域设置为 MediaProvider：
   - `com.android.providers.media.module`
   - `com.google.android.providers.media.module`
4. 重启对应 MediaProvider 进程，或者直接重启设备。
5. 打开 FuseHide，检查运行时状态是否已经 Hook。

你的 ROM 实际使用哪个 MediaProvider 包，就只需要让对应包生效。

### Zygisk 模式

1. 使用兼容的 root 环境，并确保 Zygisk 实现正常工作。
2. 如果需要配置/Probe UI 和 provider 配置同步，先安装 FuseHide APK。
3. 在 root 管理器中刷入 release 里的 `FuseHide-*-release.zip`。
4. 重启设备。
5. 确认没有同时在 LSPosed 中把 FuseHide 作用到 MediaProvider。
6. 打开应用检查 MediaProvider Hook 状态。

模块安装器会根据当前 ABI 提取 `libfusehide.so`，安装 Zygisk library，并从打包 APK 中提取 injected dex payload。

## 应用界面

当前应用主要分为四个区域：

- **Home**：Hook/runtime 状态、设备信息与应用状态概览。
- **Config**：全局及按应用隐藏配置。
- **Probe**：直接执行文件系统操作，用于验证隐藏策略。
- **Settings**：UI 与应用设置。

### 应用配置

配置界面会区分“正在编辑的 draft”和“MediaProvider 当前已经应用的配置”。

主要操作包括：

- **Apply**：保存当前编辑值并请求 MediaProvider reload。
- **Refresh Applied Config**：读取已注入 MediaProvider 当前真正使用的 native 配置快照。
- **Restore Defaults**：把源码默认值恢复到编辑器。
- **Detailed Diff**：比较当前 draft 与已应用配置之间的差异。

### Probe 工具

Probe 页面目前包括：

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

这些功能用于直接复现 MediaProvider/FUSE 行为，包括 Unicode 路径测试。

### 验证示例

例如给测试应用隐藏 `Download/private` 后，预期表现应为：

```text
/storage/emulated/0/Download              -> 可见
/storage/emulated/0/Download/private      -> 隐藏
list(/storage/emulated/0/Download)        -> 不出现 private
stat/open(private)                        -> 表现为隐藏/不存在
```

重复访问后，也不应该仅仅因为之前产生过 positive FUSE/VFS cache 就重新变得可见。

## Native 兼容策略

MediaProvider 内部 C++ ABI 会随着版本变化，因此 FuseHide 不会把某一台设备上的偏移直接当成全局固定值。

当前 native resolver 会组合以下证据来源：

- 普通 ELF symbol lookup
- file-backed ELF parser
- embedded APEX 场景下的 runtime/in-memory ELF parser
- relocation/import resolution
- `.gnu_debugdata`
- 已解析 feature anchor
- 只有在足够可信时才使用的 known device profile

部分 Hook 必须存在明确 ABI 证据，例如 libc++ string object layout 与 MediaProvider `DirectoryEntries` container layout。

如果无法验证 string ABI，就会跳过需要解析或构造该 string object 的 Hook；如果无法识别目录项 ABI，就会跳过不安全的 C++ container Hook。`fuse_req_ctx` 会通过 linker/import/libfuse 路径解析，不会退回到未经验证的手写结构体布局。

这是刻意设计的 fail-closed 策略：宁可少装一部分 Hook，也不应该因为猜 ABI 而破坏 MediaProvider 内存。

## 构建

请使用 submodule 克隆：

```bash
git clone --recurse-submodules https://github.com/XiaoTong6666/FuseHide.git
cd FuseHide
```

如果之前没有拉取 submodule：

```bash
git submodule update --init --recursive
```

当前构建配置：

- Gradle Wrapper `9.7.1`
- Android Gradle Plugin `9.4.0`
- Kotlin `2.4.20`
- compileSdk / targetSdk `37`
- minSdk `31`
- Java / Kotlin target `17`
- Android NDK `30.0.15729638`
- C++20

CI 使用 JDK 21；本地构建需要使用与当前 Android Gradle Plugin 配置兼容的 JDK。

构建 debug 与 release：

```bash
./gradlew assembleDebug assembleRelease
```

APK 打包流程同时会生成签名后的 Zygisk module archive：

```text
app/build/zygisk/
```

常用 Gradle 任务：

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

`flash*` 任务需要 `adb` 已连接设备，并且设备上存在对应 root 管理器。

格式化：

```bash
./gradlew format
```

当前 versionCode 来自：

```bash
git rev-list --count HEAD
```

versionName 为 `1.<versionCode>`。因此正式构建最好使用完整 Git 历史；浅克隆会改变 commit count，进而影响版本号。

## 项目结构

```text
app/
  src/main/java/io/github/xiaotong6666/fusehide/
    config/           运行时配置与 IPC
    debug/            Probe / 调试辅助
    status/           注入进程状态
    ui/               Compose 应用界面
    xposed/            LSPosed 与注入 Java 入口
  src/main/cpp/
    fusehide/          FUSE hook / policy / runtime 核心
    zygisk/            独立 Zygisk 注入后端
    third_party/Dobby/ native hook 依赖

baselineprofile/       Android Baseline Profile 模块
uihelper/              UI helper git submodule
template/module/       Zygisk/root module 模板
scripts/               构建与打包脚本
docs/                  架构与逆向分析文档
MediaProvider/          本地 AOSP MediaProvider 参考源码，不参与根 Gradle 构建
```

更详细的 FUSE Hook 架构可查看 [`docs/architecture.md`](docs/architecture.md)。

## 日志与排错

### 获取日志

可以直接：

```bash
adb logcat -v time | grep -i FuseHide
```

或者：

```bash
adb logcat -s FuseHide
```

Debug 构建会提供比 release 更完整的 Hook resolver 和请求链路日志。

### 显示“未 Hook”

请检查：

- Android 是否为 12 / API 31 或更高；
- ROM 的 MediaProvider 包名是否属于支持范围；
- 当前是否只有一套预期注入后端生效；
- LSPosed 模式下作用域是否正确；
- 独立 Zygisk 模式下 Zygisk 是否真的正常运行；
- 安装或修改配置后是否重启过 MediaProvider 或设备。

### 路径仍然可见

反馈问题时，请同时描述“直接访问”和“目录枚举”结果。例如说明 `stat`、`open` 与父目录 `list` 是否出现不一致。

建议附带：

- 设备型号
- Android / ROM 版本
- Kernel 版本
- MediaProvider 版本或 APK 信息（如果可以获取）
- 注入后端及版本
- 完整隐藏配置
- 目标应用/包名
- 目标路径
- 相关 `FuseHide` logcat

### `Android/data` 和普通目录表现不同

这是可能正常的。`Android/data` 与 `Android/obb` 使用 MediaProvider 的特殊访问控制/backing path 链路，不能简单按普通公共目录的 readdir 路径分析。

应用会显示一些有用的存储环境属性：

```text
ro.fuse.bpf.is_running
persist.sys.vold_app_data_isolation_enabled
external_storage.sdcardfs.enabled
```

这些值可以帮助描述设备环境，但 FuseHide 的普通配置路径隐藏本身并不要求 fuse-bpf 已启用。

## 发布地址

Release：

https://github.com/XiaoTong6666/FuseHide/releases

发布工作流会同时生成 APK 与独立 Zygisk module ZIP。

## 许可证

FuseHide 整体采用 [Apache License 2.0](LICENSE)。

第三方源码继续遵循各自许可证。例如仓库内的 xz-embedded 源码文件头声明了 `SPDX-License-Identifier: 0BSD`。

## 致谢

特别感谢 5ec1cff 佬提供的原型模块作为参考以及技术指导支持，谢谢喵。

同时感谢 Android/MediaProvider 生态以及本项目注入、UI、构建和 native hook 链路使用到的各个开源项目。

## 免责声明

FuseHide 仅用于学习、调试、兼容性研究，以及你有权限修改的设备与环境中的实验。Android 内部实现可能随时发生变化。将本项目用于日常或生产设备前，请自行评估相关风险。
