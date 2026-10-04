# FuseHide 1.182

- **Version:** `1.182`
- **VersionCode:** `182`
- **Commit:** [`c407f1c`](https://github.com/XiaoTong6666/FuseHide/commit/c407f1cbc11c3399eec8e0faae40bcfa299717e0)
- **Build time:** `5m 19s`
- **SHA256:** `b0dde73676d74a41c56ddc41d96822e080c8f778d987c3929a797dde4838010d`

## Message

```text
fix(native): model dynamic directory entry ABI

将 MediaProvider GetDirectoryEntries 与 addDirectoryEntriesFromLowerFs 的 original slot 改为动态 ABI 地址存储，仅在运行时 ABI 判定完成后恢复为 shared_ptr 或 value-vector 的准确函数类型。

保留现有 DirectoryEntries ABI 探测、strict hook publication 与 acquire/release 时序，移除 Clang 21 下不兼容函数指针之间的 reinterpret_cast，同时不扩大其他 HookOriginal 的类型擦除范围。

```
