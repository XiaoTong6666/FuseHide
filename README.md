# FuseHide 1.219

- **Version:** `1.219`
- **VersionCode:** `219`
- **Commit:** [`518b820`](https://github.com/XiaoTong6666/FuseHide/commit/518b820457366774bb828d4872d95711502d0139)
- **Build time:** `5m 52s`
- **SHA256:** `d6166f1dee733213199b067d37079fb80f8c985e688ed660e3c7c84355d5d7d6`

## Message

```text
feat(settings): support runtime release diagnostics and refine UI labels, Meow

调整“当前 UI 风格”信息行的主副标题层级，让标题以主要文字样式显示，MIUIX 和 Material 作为次要信息显示喵

新增独立持久化的详细调试日志开关，通过受信任的配置 Provider、定向广播及 JNI 将状态同步到 MediaProvider，开启或关闭日志时不改变隐藏规则及其缓存状态喵

将 Native FUSE 诊断从编译期常量调整为运行时原子开关，使 Release 默认关闭详细日志、启用时仍可记录 lookup、readdir、inode 及错误路径信息，同时在关闭时跳过日志参数求值和详细日志采样（认真检查每一处热路径）喵

补全 Xposed 与 Zygisk 两种注入后端的 Native 方法注册，使用签名权限限制日志刷新广播，并将系统解锁事件作为非导出接收器处理，确保跨进程状态同步可靠喵

在日志输出前后保存和恢复 errno，避免诊断代码影响 libc 与 FUSE 包装器的错误语义，并保留已有的必要错误和警告日志（轻轻收好爪子）喵

```
