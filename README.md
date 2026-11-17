# FuseHide 1.208

- **Version:** `1.208`
- **VersionCode:** `208`
- **Commit:** [`0379a9c`](https://github.com/XiaoTong6666/FuseHide/commit/0379a9c09fce633c7c57e641613fe92beb6ec906)
- **Build time:** `6m 59s`
- **SHA256:** `bf2ea09bc703c485891135228c156654b05071acc782cd5bf291d8cb74cc86aa`

## Message

```text
fix(native): stabilize FUSE inode lifecycle and rename tracking, Meow

MediaProvider 的 FUSE inode 实际使用 node* 指针作为身份，节点销毁后地址复用可能让新目录继承旧路径和隐藏标记喵
为 pf_forget、pf_forget_multi 与 NodeTracker::NodeDeleted 接入精确符号 Hook，把真正的跟踪状态回收放在节点销毁时，避免部分 forget 提前清掉仍存活的 inode 喵（认真守着节点的生命周期）

让可信的 lstat、readdir 和成功 lookup 及时纠正 inode 路径，并在返回 ENOENT 前重新验证历史隐藏标记，避免可见目录误继承旧的隐藏状态喵（伸爪把过期记录拨开）

普通 rename 成功后先迁移存活 inode 的路径与隐藏状态，再向内核发送成功回复，同时清理被覆盖的目标记录喵
将 UID 规则解析移到 Session 锁外，重新加锁后检查规则快照与路径版本，无法确认身份时保守清理，不擅自迁移旧标记喵（竖起耳朵盯紧并发变化）

为路径子树和近期父目录建立有序索引，只遍历实际受影响的 inode 与 UID，并统一节点退休时的清理逻辑，减少全表扫描和重复处理喵（满意地晃晃尾巴）

```
