# FuseHide 1.190

- **Version:** `1.190`
- **VersionCode:** `190`
- **Commit:** [`2ea586d`](https://github.com/XiaoTong6666/FuseHide/commit/2ea586d2e9169b7261e910f05d6fe591efb645cf)
- **Build time:** `5m 26s`
- **SHA256:** `f995e2f1418314812552b0eeaf418997619c620938303ac01f0fe799c03bf3c3`

## Message

```text
fix(native): Bound FUSE invalidations and drain session teardown Meow

这次把 FuseHide 的 FUSE 缓存失效调度收紧了喵。原来根目录 entry、指定 entry 和 inode 失效会按请求创建 detached 线程，大量目录项同时被过滤时容易堆积线程；线程还会持有原始 fuse_session 指针，MediaProvider 销毁 session 后存在继续访问旧指针的风险喵（尾巴警觉地竖了起来）

Native 这边改为进程内共享的单 worker 和有界任务队列，保留原来的 entry 25 ms、根目录 entry 和 inode 50 ms 延迟，并对排队中的同类目标去重。队列最多容纳 4096 个任务，超出时限制警告日志频率并拒绝继续入队，避免高负载下无上限地消耗线程与内存喵（一个一个来，不许偷偷分身）

生命周期方面新增 fuse_session_destroy 的 ELF 导入槽 Hook，兼容文件映射与 APK 内嵌 ELF 的安装路径。销毁前先阻止新任务、清空待执行队列，再等待正在执行的通知退出；新 session 被识别时更新代际并清理旧的跟踪状态。未能安装销毁 Hook 时不安排异步通知，避免继续使用无法保证生命周期的 session 指针喵（认真守住销毁顺序）

```
