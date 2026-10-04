# FuseHide 1.183

- **Version:** `1.183`
- **VersionCode:** `183`
- **Commit:** [`95cafc9`](https://github.com/XiaoTong6666/FuseHide/commit/95cafc93339da25276a6d01833876c8d3dbb3fec)
- **Build time:** `3m 13s`
- **SHA256:** `9356fec1d0c9004b6d06a3e37c94fabfb6142d6331645b48ec216e2fdf4ed873`

## Message

```text
perf(native): smooth path cache hot paths

将 HiddenPathClassification cache 的 4096 项满表 clear 改为复用 unordered_map node 的单项增量替换，避免 target app 大量 unique path 扫描时集中释放并重建整张缓存。

为 tracked inode path cache 增加同锁保护的 path->inode 反向索引与 string_view 透明查找，将 LookupTrackedInodeForPath 从 O(N) 扫描降为平均 O(1)，并在 inode 路径更新与 session clear 时保持双向索引一致。

```
