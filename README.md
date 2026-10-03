# FuseHide 1.177

- **Version:** `1.177`
- **VersionCode:** `177`
- **Commit:** [`fa4ac56`](https://github.com/XiaoTong6666/FuseHide/commit/fa4ac568a0048c519a1dd299efa3b010a6f32c41)
- **Build time:** `5m 58s`
- **SHA256:** `e06d06f748bd59b9536080521d615193ea54a4c650bc32a318e56ee97b4c0a94`

## Message

```text
feat(native): harden hook ownership and callback lifetime

将 Zygisk linker 与 Native Hook 接入 Dobby Prepare/Commit/Recover 事务，并为受控模块加入 strict v3 backup publication。
补齐 retained ticket、重复宿主绑定、callback admission/drain、NODELETE 驻留和安装重入边界，x86_64 无可信 quiescence 时保持 fail-closed。
增加 linker rollback、native ownership、callback lifetime、MediaProvider 与 UID isolation 验收脚本，并同步 Dobby/uihelper 子模块及调试导航适配。

```
