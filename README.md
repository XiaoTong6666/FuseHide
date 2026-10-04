# FuseHide 1.181

- **Version:** `1.181`
- **VersionCode:** `181`
- **Commit:** [`6136230`](https://github.com/XiaoTong6666/FuseHide/commit/6136230bd17948e0f161dd39f67534214e76f52c)
- **Build time:** `4m 27s`
- **SHA256:** `b01f7db49977d5dd455e0445eb86a5c65a94863a3d84a7312d64b5eee497324c`

## Message

```text
fix(native): harden loader hook validation and fallback

将 MediaProvider linker monitor 从私有 do_dlopen inline hook 改为 dl_iterate_phdr observer 优先，并仅在 ARM64 上尝试 strict public android_dlopen_ext/dlopen Hook；不满足 pristine、backup 或 landing-pad 安全条件时保持 observer-only。

接入 Dobby strict ELF validation，保留 BTI landing pad、拒绝不安全 PAuth/control-flow 路径，并在 generic Native API 中继续允许 anonymous/JIT target，仅强制 backup validation 与 landing-pad 保护。

补齐 public loader Hook 的 Prepare/Commit/Recover 状态管理、x86_64 无可信 quiescence 时的 fail-closed 行为，以及 linker/native callback/ownership 回归测试和架构文档；同步 Dobby 子模块到 strict hook safety 提交。

```
