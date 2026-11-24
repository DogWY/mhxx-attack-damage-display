# MHGU Damage Display · 怪物猎人 GU 攻击伤害显示

在 macOS 的 Astris 模拟器上游玩《Monster Hunter Generations Ultimate》（MHGU）时，这个 Switch Mod 会在命中怪物的位置弹出本地玩家造成的伤害。多段攻击逐次显示，数字从命中位置向上飘动并淡出。无需安装 Forge。

**[下载最新版安装包](https://github.com/DogWY/mhxx-attack-damage-display/releases/latest/download/MHGU-Damage-Display.zip)** · [查看版本更新](https://github.com/DogWY/mhxx-attack-damage-display/releases) · [English quick start](#english-quick-start)

## 适用版本

| 项目 | 版本 |
| --- | --- |
| 游戏 | Nintendo Switch 国际版 MHGU **1.4.0** |
| Title ID | `0100770008DD8000` |
| 已测试的模拟器 | macOS 上的 Astris **1.0.32** |

如果你是通过“怪物猎人 XX”或“MHXX 伤害显示”找到这里，请先核对游戏的 Title ID。安装包内的补丁对应上表中的国际版游戏程序。

## 安装与更新

1. 下载上方的 **MHGU-Damage-Display.zip** 并解压。如果从 Releases 页面下载，在 **Assets** 中选择这个文件。
2. 退出游戏，在 Astris 游戏库右键 MHGU，选择 **Open Mods Folder → LayeredFS Mods**。
3. 把解压出的整个 `MHGU-Damage-Display` 文件夹放进去。以打开的目录为起点，文件应如下排列：

   ```text
   MHGU-Damage-Display/
   └── exefs/
       ├── subsdk1
       └── FB08F1D20FD1204FE7070672777F869B00000000000000000000000000000000.ips
   ```

4. 在 **Manage Mods** 中启用 **MHGU-Damage-Display**，启动游戏，进入任务后攻击一只活着的怪物即可看到数字。

**更新**时先退出游戏，用新版文件夹替换整个旧版文件夹，使 `subsdk1` 与 `.ips` 来自同一个安装包。要停用，在 **Manage Mods** 中取消勾选并重启游戏。

## 怎么看伤害数字

| 画面效果 | 含义 |
| --- | --- |
| 白色数字 | 普通命中，未触发下方的金色高亮条件 |
| 金色数字 | 对应攻击类型的物理肉质值达到 45，或本次攻击属于固定伤害。颜色依据肉质和攻击类型判定 |
| 连续出现多个数字 | 多段攻击的各次命中分别结算 |
| 击杀时出现最后一个数字 | 击杀一击照常显示，之后攻击尸体不会再生成数字 |

## 没有看到数字？

先检查游戏版本、Title ID 和 **Manage Mods** 中的启用状态，再核对上面的目录结构。常见的解压问题是多套了一层同名文件夹，导致 Astris 找不到 `exefs`。

如果同时使用多个 Mod，可先停用旧版伤害显示、Forge 前置以及其他提供 `exefs/subsdk1` 的 Mod，再重启游戏测试。本 Mod 占用 `subsdk1`，两个 Mod 共用该文件名会产生冲突。

## English quick start

MHGU Damage Display adds floating damage numbers at the point of impact in **Monster Hunter Generations Ultimate** for Nintendo Switch. It tracks the local player's hits, including multi-hit attacks. Gold indicates a physical hitzone value of 45 or higher, or fixed damage. The killing blow is shown, and subsequent hits on the defeated monster are ignored.

Tested with **MHGU 1.4.0** (Title ID `0100770008DD8000`) on **Astris 1.0.32 for macOS**. Close the game, extract the ZIP, and copy the entire `MHGU-Damage-Display` folder into **Open Mods Folder → LayeredFS Mods**. Enable it under **Manage Mods** and launch the game. [Download the latest mod ZIP](https://github.com/DogWY/mhxx-attack-damage-display/releases/latest/download/MHGU-Damage-Display.zip)

## 从源码构建

在 macOS 上准备 Python 3、Xcode Command Line Tools、liblz4，以及 LLVM 的 `ld.lld` 或 Rust 工具链自带的 `rust-lld`。从自己的游戏导出 v1.4.0 ExeFS，确保目录内包含原版 `main` 和 `subsdk0`，然后在仓库根目录运行：

```sh
python3 tools/build_mod.py /path/to/exefs
```

构建脚本会校验游戏版本和补丁位置，生成与 Release 安装包相同目录结构的 `dist/MHGU-Damage-Display.zip`。
