# FuseHide 1.199

- **Version:** `1.199`
- **VersionCode:** `199`
- **Commit:** [`a6e1d36`](https://github.com/XiaoTong6666/FuseHide/commit/a6e1d36f2392a70d416d2aafd46c79a7f656cbe7)
- **Build time:** `5m 10s`
- **SHA256:** `9f139a59771580ff63455a1bc027a382b7a2ce121217b2387c1ef497c6359ce2`

## Message

```text
fix(native): enable PAC-aware MediaProvider hooks with exact symbols Meow

在 Zygisk Native Hook adapter 中使用 PAC-SP 处理路径，保留并发安全和备份验证要求，让 Android 17 MediaProvider 的 PACIASP 入口能够进入内联 Hook 流程喵（认真盯着跳板代码）

复用现有 ELF 精确符号解析结果识别字符串参数入口，避免历史 profile 的 RVA 不匹配时连带跳过相关 Hook 喵
根据目录辅助函数的精确符号和已识别的容器 ABI 选择 DirectoryEntries Hook，同时保留历史 RVA fallback，不引入新的设备白名单或硬编码偏移喵（满意地晃晃尾巴）

同步更新 Dobby 子模块指针，接入新增的 PAC 感知 Hook 实现喵（把代码仔细收好）

```
