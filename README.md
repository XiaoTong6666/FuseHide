# FuseHide 1.170

- **Version:** `1.170`
- **VersionCode:** `170`
- **Commit:** [`c981383`](https://github.com/XiaoTong6666/FuseHide/commit/c981383f0bbbd0bdc66091543e4ee8343fd424a5)
- **Build time:** `4m 01s`
- **SHA256:** `c50c9eae0653d1aeca88ab6c9e35c3840263472ce6274d80ba90a0fab8860033`

## Message

```text
feat(ui): redesign home status and runtime overview

重构首页状态展示，统一工作中、检查中和异常状态，并显示实际使用的 Xposed 或 Zygisk 注入后端。

重新整理 MediaProvider 与配置同步信息，固定运行时信息布局，避免检查过程中行数和高度跳动。

修正已应用配置的同步判定，引入查询中、未获取、需要审核和已同步状态，并为后端配置查询补充超时处理。

新增已应用配置详情页，可从首页配置同步项直接查看 MediaProvider 返回的格式化后端配置。

完善设备与系统信息展示，输出市场名称、型号、设备代号、Android 版本和 API 等首页信息。

按现有 MIUIX 信息结构重做 Material 3 首页，并参考 KernelSU 使用分段列表与状态卡组织运行时和设备信息。

更新 uihelper 依赖指针，接入新的首页组件、稳定检查态布局及独立顶部栏标题支持。

```
