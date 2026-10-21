# FuseHide 1.191

- **Version:** `1.191`
- **VersionCode:** `191`
- **Commit:** [`a1d3c7a`](https://github.com/XiaoTong6666/FuseHide/commit/a1d3c7a7cd3afaf6f499d7d15756b93b3acbb877)
- **Build time:** `7m 24s`
- **SHA256:** `fe7851878c3d98524dbab9453db2a53d65f15c88f9ca572de0030f9d7af662b4`

## Message

```text
fix(fusehide): Harden FUSE invalidation and hot reload, Meow

之前有些目录明明该隐藏，却会因为别的 UID 先访问过，就从 FUSE 缓存里漏出来喵（伸爪按住乱跑的 dentry）。这次把 lookup 和 READDIRPLUS 的缓存一起管好，不让不同 UID 再串缓存喵。

顺手理了理 Session 生命周期和失效队列，让通知跟着正确的 Session 代次走喵。热更新碰到 root_ino=0 时，会先查看 AOSP 和设备实际 libfuse_jni 的节点树喵（认真翻翻节点树）。只有确认这个 Session 没创建对应子节点、也没有缓存要清，才跳过没必要的根目录通知喵；有东西要清的还是照常处理喵。

另外补好了 KernelSU 热安装、模块操作和配置重载的细节喵。根 inode 和回调入口的诊断日志继续留着，但只在全局 Debug 开关打开时才打印，Release 就安静一点喵（把日志本收好）。测试源码也从 app/src/main 移走，不放进正式项目喵。

```
