# FuseHide 1.202

- **Version:** `1.202`
- **VersionCode:** `202`
- **Commit:** [`34ca0ce`](https://github.com/XiaoTong6666/FuseHide/commit/34ca0ceca07651e16ca1a72cd07aff3eb5f6b6b0)
- **Build time:** `7m 00s`
- **SHA256:** `cc007cc09db8d3b1b89bd63d1cc380533740c2c0d3b2a9a8b852c139eb380a27`

## Message

```text
chore(native): add debug diagnostics for FUSE hide mismatches

记录隐藏子树 inode 标记与基于父 inode 的隐藏判定来源喵
捕获 FUSE ENOENT 与 lower-fs stat/lstat 原始错误并保留 errno 喵
限制常规日志采样，持续报告策略与 inode 跟踪矛盾喵

```
