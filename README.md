# FuseHide 1.183

- **Version:** `1.183`
- **VersionCode:** `183`
- **Commit:** [`751765c`](https://github.com/XiaoTong6666/FuseHide/commit/751765cc4b0c696dc11fe909390b5c0aba2af3d2)
- **Build time:** `5m 24s`
- **SHA256:** `f922c1ecd720fccf4e64b50d4e85bf53c2446841a2506f9433e930af2535016b`

## Message

```text
perf(native): smooth path cache hot paths

将 HiddenPathClassification cache 的 4096 项满表 clear 改为复用 unordered_map node 的单项增量替换，避免 target app 大量 unique path 扫描时集中释放并重建整张缓存。

为 tracked inode path cache 增加同锁保护的 path->inode 反向索引与 string_view 透明查找，将 LookupTrackedInodeForPath 从 O(N) 扫描降为平均 O(1)，并在 inode 路径更新与 session clear 时保持双向索引一致。

```
