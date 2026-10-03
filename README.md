# FuseHide 1.178

- **Version:** `1.178`
- **VersionCode:** `178`
- **Commit:** [`8313aa6`](https://github.com/XiaoTong6666/FuseHide/commit/8313aa6d387131ef6253ead6584806e495a3bf06)
- **Build time:** `5m 10s`
- **SHA256:** `edcea2769b2802e24b88e19b71e0d9241358205a8b585e1932409108d652aadf`

## Message

```text
perf(native): reduce MediaProvider FUSE overhead

优化 MediaProvider/FUSE 热路径中的主要性能瓶颈。

移除 FuseHide 对 ICU Default_Ignorable_Code_Point 查询的依赖，改用与 Android kernel casefold 对齐的 Unicode 12.1 静态范围表，并为 ASCII compare、NeedsSanitization 和 RewriteString 增加快速路径，减少普通路径上的 Unicode 处理开销。

优化 relative path canonicalization，减少重复 Normalize/Canonicalize 和逐字符 std::string::push_back；同时让无 hide rule 的 UID 在进入 pathname classification cache 前直接返回，避免非目标应用产生无意义的分配、hash 和缓存污染。

收缩 inode->path tracking 范围，仅保留与隐藏策略相关的路径，并移除 16384 项缓存满时的 O(N) pseudo-LRU eviction，避免全盘扫描时反复遍历整个 inode cache。

保留 native hook 诊断开关，但默认恢复正常 hook 安装，避免诊断状态影响实际隐藏逻辑。

```
