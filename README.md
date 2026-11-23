# FuseHide 1.221

- **Version:** `1.221`
- **VersionCode:** `221`
- **Commit:** [`caffd6f`](https://github.com/XiaoTong6666/FuseHide/commit/caffd6fe4f4e82be2f322929fa6f7da274164e90)
- **Build time:** `6m 09s`
- **SHA256:** `4bd187db213e059cc068a5aaa49ac8482bf29c4df4e43a64645188776af7cc88`

## Message

```text
fix(icon): preserve adaptive splash icon masking, Meow

让启动画面直接使用现有的 AdaptiveIconDrawable，自适应系统的圆形与圆角方形遮罩，避免把完整方形矢量图当作启动图标而造成边缘裁切喵

将启动图标的额外背景设为透明，避免系统背景层覆盖自适应图标自身的圆角，同时保持日间与夜间主题行为一致，不影响桌面图标和原有矢量资源（轻轻收好爪子）喵

```
