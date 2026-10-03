# FuseHide 1.180

- **Version:** `1.180`
- **VersionCode:** `180`
- **Commit:** [`399adb5`](https://github.com/XiaoTong6666/FuseHide/commit/399adb5295f4a61f84eb86554c1b70b818a3e9fd)
- **Build time:** `5m 29s`
- **SHA256:** `d5f5e9700eb9bc712a0f8cabbe8cb9babe002f74e7c093525114a28c83b8124f`

## Message

```text
perf(native): reduce FUSE request bookkeeping overhead

继续优化 MediaProvider FUSE 高频请求路径，降低隐藏策略判断之外的固定开销。

为 UID hide rule 增加 generation-aware TLS cache 和 borrowed rule fast path，减少同一线程高频请求中的 mutex、unordered_map 查找与 shared_ptr 引用计数；配置或 package generation 变化时自动失效，并处理配置更新与规则解析并发时的 generation retry。

优化 ShouldNotCache 路径，先使用 any-package candidate prefilter 筛掉明显无关路径，仅对可能参与隐藏策略的路径执行完整 subtree 判断，同时保持全局 shared dentry cache 语义不变。

将 ScopedPathPolicyContext、lookup name 和 NeedsSanitization 等同步热路径改为 string_view，减少正常路径上的 owned string 构造与复制；无 hide rule 的 UID 在完成必要的 kernel casefold compatibility 后直接调用原 MediaProvider helper，不再进入 HidePolicy bookkeeping。

将 readdir request context 改为 TLS-first：普通 non-target UID 不再访问全局 pending context map/mutex，target UID 仍保留全局 fallback 以兼容潜在跨线程路径，并支持嵌套 request 的 context 恢复。

为 FUSE session tracking 增加嵌套 scope 和 same-session fast path，避免同一请求调用链反复进入 session mutex。

```
