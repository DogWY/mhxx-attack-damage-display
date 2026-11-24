# MHGU 攻击伤害显示

在 macOS 的 Astris 中游玩《Monster Hunter Generations Ultimate》时，在命中位置显示本地玩家的单次伤害，支持多段攻击，无需 Forge。白色代表普通命中；对应攻击类型的物理肉质值达到 45，或攻击属于固定伤害时，数字显示为金色。

**[下载最新版 MHGU-Damage-Display.zip](https://github.com/DogWY/mhxx-attack-damage-display/releases/latest/download/MHGU-Damage-Display.zip)**

## 安装

1. 确认游戏为 Switch 版 **MHGU 1.4.0**，Title ID 为 `0100770008DD8000`。已测试的模拟器为 macOS Astris 1.0.32。
2. 退出游戏，在 Astris 游戏库右键 MHGU，选择 **Open Mods Folder → LayeredFS Mods**。
3. 解压下载的 ZIP，将整个 `MHGU-Damage-Display` 文件夹放入打开的目录。解压后的结构应为 `MHGU-Damage-Display/exefs/`，其中包含 `subsdk1` 和一个 `.ips` 文件。
4. 在 **Manage Mods** 中启用 **MHGU-Damage-Display**。如已安装 Forge、其他伤害显示版本，或其他占用 `subsdk1` 的 Mod，请先停用它们，再启动游戏。

更新时退出游戏并替换整个 Mod 文件夹，保证两个文件版本一致。停用时在 **Manage Mods** 中取消勾选并重新启动游戏。

## 从源码构建

macOS 上需要 Python 3、Xcode Command Line Tools、liblz4，以及 LLVM 的 `ld.lld` 或 Rust 工具链自带的 `rust-lld`。准备自己游戏的 v1.4.0 ExeFS 目录，其中应包含原版 `main` 和 `subsdk0`。

```sh
python3 tools/build_mod.py /path/to/exefs
```

构建脚本校验游戏版本和补丁位置后，生成 `dist/MHGU-Damage-Display.zip`。
