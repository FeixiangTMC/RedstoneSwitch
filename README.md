# RedstoneSwitch

适用于 Windows x64 的 Minecraft 基岩版 1.26.51.1 和 1.26.60.5 客户端。命令由客户端托管的本地世界执行，需要游戏管理权限。

| 命令 | 效果 |
| --- | --- |
| `/rsrule observer true` | 阻止侦测器产生新的红石脉冲。 |
| `/rsrule observer false` | 恢复侦测器的原版脉冲。 |
| `/rsrule dispenser true` | 阻止发射器发射和消耗物品。投掷器继续按原版工作。 |
| `/rsrule dispenser false` | 恢复发射器的原版行为。 |

每次打开本地世界时，两条规则均默认为 `false`，侦测器和发射器按原版工作。设置只在当前世界会话中有效。

用 xmake 3.0.0+ 和 `clang-cl` 构建。在项目目录中选择对应的 LeviLamina SDK。

| Minecraft 客户端 | LeviLamina SDK | 模组包目录 |
| --- | --- | --- |
| 1.26.51.1 | 26.51.9 | `bin/RedstoneSwitch` |
| 1.26.60.5 | 26.60.9 | `bin/1.26.60.5/RedstoneSwitch` |

```powershell
xmake f -y -p windows -a x64 -m release --ll_version=26.51.9 -o build/26.51
xmake -y

xmake f -y -p windows -a x64 -m release --ll_version=26.60.9 -o build/26.60
xmake -y
```

将对应的 `RedstoneSwitch` 文件夹整体复制到该客户端实例的 `mods` 目录，其中包含 DLL、清单和简体中文、英文、日文语言文件。
