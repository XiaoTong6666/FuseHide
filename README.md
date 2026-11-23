# FuseHide 1.220

- **Version:** `1.220`
- **VersionCode:** `220`
- **Commit:** [`0976471`](https://github.com/XiaoTong6666/FuseHide/commit/0976471d2975d8a7a2d9b12e97c214e7d17dd7e0)
- **Build time:** `4m 57s`
- **SHA256:** `e5bbf0385fb6dee69e7ffc50e1a00dfdd637ac44d21a5311ca935da982e69983`

## Message

```text
feat(settings): add launcher icon visibility toggle, Meow

为桌面入口新增独立的 LauncherAlias，并让 MainActivity 保持可用，通过 PackageManager 切换组件启用状态，实现无需重启或重新安装的图标显示控制喵

在设置页增加隐藏桌面图标开关，直接根据实际组件状态更新界面，并补充中英文提示与 Compose 状态回调，默认保持桌面图标可见喵

保留 Xposed 模块设置、Android 应用信息及应用偏好设置入口，避免隐藏启动器图标后无法进入应用，同时提示部分新版 Android 启动器仍可能展示系统生成的应用信息入口（轻轻整理好设置页面）喵

```
