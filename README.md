# FuseHide 1.169

- **Version:** `1.169`
- **VersionCode:** `169`
- **Commit:** [`cbbc736`](https://github.com/XiaoTong6666/FuseHide/commit/cbbc736eb6e3cbd686586bd77c89cba22df85b68)
- **Build time:** `5m 04s`
- **SHA256:** `3953438579e75e129f76af1317db65a43a99a5036c4b4bb8f5086dfb5a1dc4a0`

## Message

```text
feat(ui): refine navigation and app list experience

本次提交围绕主界面与配置页重新整理 UI 架构，接入更新后的 uihelper，自适应处理手机与宽屏导航，并统一 MIUIX / Material 页面容器、系统栏、顶栏与底栏行为。同步迁移导航实现与主题配置，完善悬浮底栏、模糊效果、页面回弹及二级页面转场，减少不必要的延迟渲染与交互割裂。

配置页重点优化应用列表体验，整理列表分组、搜索状态与详情页结构，补充应用图标缓存与预热，降低首次滚动时的冷加载感；同时修正收起顶栏状态下搜索取消动画的锚点、配置页顶部栏 blur 采样链，以及 MIUIX 列表的 overscroll / nested scroll 顺序。

设置页同步精简 MIUIX 相关选项文案与图标，统一图标颜色和视觉层级；并补充 MainActivity 状态管理、窗口主题、baseline profile 与依赖配置等配套调整。

```
