# SuperMarioWorld2YoshisIslandRecomp（超级马里奥世界2：耀西岛 静态重编译）

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](#源码编译)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-blue)](#环境依赖)
[![Language](https://img.shields.io/badge/language-C11%20%2F%20C%2B%2B17-orange)](#关于本项目静态重编译)
[![License](https://img.shields.io/badge/license-PolyForm--Noncommercial--1.0.0-blue)](LICENSE)

基于 [snesrecomp](https://github.com/RetroPortingToolKit/snesrecomp) 框架与 [recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui) 启动器的 **《超级马里奥世界2：耀西岛》（Super Mario World 2: Yoshi's Island，SNES USA Rev 1.0）** 原生 C 静态重编译 PC 独立运行版本。

---

## 关于本项目静态重编译

原始 ROM 中的 65816 CPU 机器码已通过深度静态分析直接转译为纯原生 C 代码——游戏在主 SNES CPU 上执行的所有子程序与中断处理均在 `src/game/` 目录中编译为原生机器码执行。

**SNES 其它硬件架构与专用协处理器采用精准硬件级模拟协同运行**：
* **Super FX (GSU-2) 协处理器**：耀西岛的核心硬件特色。负责 Mode 7 3D 旋转岛屿开场、动态多边形投影、同屏大型 Boss 实时缩放变形（Morphmation），与主 65816 CPU 维持硬件周期级精确同步。
* **PPU（图像处理单元）**：扫描线级光栅时序对齐，HDMA 渐变过渡、图层优先级与精灵渲染由位于 `snesrecomp/runner/src/snes/` 的硬件仿真核心驱动。
* **APU（音频处理单元）**：与 SPC700 协处理器异步运行，内置 44.1 kHz 原生重采样与互斥锁同步，杜绝音频卡顿与欠载。

这遵循了现代静态重编译的成熟工程范式：**重编译 CPU 代码，硬件级精准模拟外围硅片与协处理器**。

---

## 当前功能特性

- [x] **原生 recomp-ui 图形启动器**：集成 Game、Controls、Video、Audio 页面，支持便携式配置持久化。
- [x] **完整可玩与流程稳定**：通过开场剧情、3D 旋转岛屿标题、文件存档栏、大地图、关卡实机游玩与通关验证。
- [x] **原生 60 FPS 流畅帧率**：亚毫秒级高精度累加器维持严格 60.0988 Hz 原生硬件时钟。
- [x] **Super FX (GSU-2) 协处理器完整支持**：原生驱动所有 3D 变换、动态缩放与同屏变形效果。
- [x] **扫描线光栅时序对齐**：彻底消除中途扫描线锁存错位、画面撕裂与调色板频闪。
- [x] **单人现代手柄即插即用**：基于 SDL2 GameController 支持 Xbox、PlayStation、Switch Pro 等各类手柄热插拔与左摇杆死区调节。
- [x] **按键重映射与持久化**：支持单人键盘完整键位自定义、手柄设备指定与 `keybinds.ini` 持久化读取。
- [x] **严格 ROM 身份校验**：SHA-256 与 CRC32 严格验证，兼容 Headerless（`.sfc`，2,097,152 字节）及自动剥离 512 字节 SMC 头部（`.smc`，2,097,664 字节）格式。
- [x] **便携式 SRAM 存档与自动迁移**：默认规范化存储至 `saves/save.srm`，向下兼容根目录旧版 `save.srm`。
- [x] **独立自包含二进制**：静态链接 C/C++ 运行库，仅依赖同级目录 `SDL2.dll`，无需额外安装 MinGW 动态库。

---

## 快速开始（ROM 格式与要求）

> [!IMPORTANT]
> **法律说明**：本项目仓库**不包含**任何受版权保护的任天堂游戏 ROM、音画素材或商业资产。用户必须自行提供合法获取的原始游戏文件。

### 验证支持的 ROM 信息
* **游戏名称**：Super Mario World 2: Yoshi's Island (USA) (Rev 1.0)
* **支持格式**：
  * **Headerless（`.sfc`）**：精确大小 `2,097,152` 字节
  * **Copier-Headered（`.smc`）**：精确大小 `2,097,664` 字节（前置 512 字节 Copier 头部会在内存中自动剥离并验证有效 Payload）
* **SHA-256**：`9b4957466798bbdb5b43a450bbb60b2591ae81d95b891430f62d53ca62e8bc7b`
* **CRC32**：`D138F224`
* **SHA-1**：`c807f2856f44fb84326fac5b462340dcdd0471f8`

双击 `YoshisIslandRecomp.exe` 启动原生 GUI，点击 **Browse** 选择您的 ROM 文件即可。启动器将自动校验文件哈希并记住选择。

---

## 图形启动器操作指南

双击运行 `YoshisIslandRecomp.exe` 即可唤起原生配置启动器：

### 1. Game 页面
* 显示游戏标题信息与当前选择 ROM 的校验状态（显示绿勾通过或红叉错误提示）。
* **Browse**：打开原生文件选择器挑选游戏 ROM。
* **Play**：启动并进入游戏（未验证通过前禁用）。
* **Skip Launcher**：勾选后未来双击将直接跳过启动器进入游戏。
* **Reset Settings**：一键恢复全部初始设置。

### 2. Controls 页面
* **Input Source**：选择输入设备（Keyboard 键盘、Gamepad 手柄、或两者混合）。
* **Deadzone**：调节手柄模拟摇杆死区百分比（默认 10%）。
* **Keybinds**：显示当前单人控制键位映射，并写入 `keybinds.ini` 持久化保存。

### 3. Video 页面
* **Aspect Ratio（显示比例）**：
  * `4:3 (CRT)`：复现经典 4:3 CRT 显像管电视比例（默认）。
  * `8:7 (Square pixels)`：点对点 1:1 正方形物理像素渲染。
  * `1:1 (Square frame)`：正方形画幅呈现。
* **Window Scale（窗口缩放）**：支持 1x 至 6x 自由缩放。
* **Fullscreen（全屏）**：无缝全屏切换。
* **Linear Filtering（双线性过滤）**：开启后平滑放大，关闭后保持复古像素锐利。

### 4. Audio 页面
* **Master Volume**：主音量滑块调节（0%～100%）。
* **Mute**：一键静音切换。

### 5. 命令行选项
* `--launcher`：强制唤起启动器界面（即使勾选了 Skip Launcher）。
* `--rom <path>`：指定 ROM 路径启动。
* `-h` 或 `--help`：显示命令行帮助信息。
* 直接兼容旧式调用：`.\YoshisIslandRecomp.exe "path\to\game.sfc"`。

---

## 游戏默认按键

| 游戏动作 | 默认键盘 | 推荐手柄（Xbox） | 推荐手柄（PS） | SNES 原机对应 |
| :--- | :--- | :--- | :--- | :--- |
| **移动 / 仰视瞄准 / 下蹲** | `W / A / S / D` 或 `方向键` | `十字键` / `左摇杆` | `十字键` / `左摇杆` | `方向键` |
| **跳跃 / 悬空伸腿跳（Flutter Jump）** | `J` 或 `空格键` | `A` 键 | `×` 键 | `B` 键 |
| **吐舌 / 吞食 / 吐出敌人** | `K` 或 `Z` | `X` 键 | `□` 键 | `Y` 键 |
| **扔蛋 / 呼出瞄准准星** | `L` 或 `X` | `B` 键 | `○` 键 | `A` 键 |
| **锁定瞄准角度** | `I` 或 `C` | `Y` 键 | `△` 键 | `X` 键 |
| **左肩键** | `Q` | `LB` / `LT` | `L1` / `L2` | `L` 键 |
| **右肩键** | `E` | `RB` / `RT` | `R1` / `R2` | `R` 键 |
| **暂停 / 地图** | `回车键 (Enter)` | `Start` 键 | `Options` | `START` |
| **道具背包菜单** | `Tab` 或 `右 Shift` | `Back` 键 | `Share` / 触摸板 | `SELECT` |
| **全屏切换** | `F11` 或 `Alt+Enter` | - | - | - |
| **退出游戏** | `Esc` | - | - | - |

> [!TIP]
> **耀西长跳操作技巧**：在跳跃滞空期间持续按住 `J` 或 `空格键`，耀西将在空中持续蹬腿进行 Flutter Jump，大幅延长滞空时间并获得额外高度。

---

## 存档兼容性与 SRAM 策略

《耀西岛》使用 64 KB（65,536 字节）电池备份 SRAM 保存游戏进度。

* **便携式存储目录**：当前统一规范化存储在程序所在目录下的 `saves/save.srm`。
* **历史存档兼容**：启动时自动按序探测：
  1. `saves/save.srm`
  2. `save.srm`（旧版根目录存档）
  3. 若仅在外部存在历史存档，程序会自动无损迁移副本至 `saves/save.srm`，绝对不破坏或静默覆盖玩家旧有存档。
* **存档安全**：每 300 帧（约 5 秒）进行一次静默后台自动存盘，并在游戏正常退出时完整写盘。

---

## 源码编译

### 环境依赖
* **CMake**：>= 3.20
* **C/C++ 编译器**：MinGW-w64 GCC（支持 C11 与 C++17）、Clang 或 MSVC
* **构建系统**：Ninja（推荐）或 Make
* **SDL2 开发库**：已包含在 `vendor/SDL2` 中，无需手动下载配置

### Windows 构建命令

```powershell
# 克隆仓库与子模块
git clone https://github.com/Zaxaerith/SuperMarioWorld2YoshisIslandRecomp.git
cd SuperMarioWorld2YoshisIslandRecomp
git submodule update --init --recursive

# 建立 build 目录并使用 Ninja 编译（受控并行度 Jobs=2）
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
ninja -C build -j 2 yoshis_island_recomp

# 运行主程序
.\build\YoshisIslandRecomp.exe
```

或直接执行自动化脚本：
```powershell
.\build.bat
# 或
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

---

## 自动化测试与验证

本项目包含独立的启动器契约测试套件与 Headless 诊断工具：

```powershell
# 1. 运行启动器契约测试（ROM 校验、512 字节头部剥离、输入映射、SRAM 读写、配置持久化）
ninja -C build -j 2 launcher_contract
.\build\launcher_contract.exe

# 2. 运行 Headless 诊断测试（无 GUI 运行 300 帧硬件回放验证）
ninja -C build -j 2 yi_headless
.\build\yi_headless.exe "Super Mario World 2 - Yoshi's Island (USA).sfc" 300
```

---

## 目录结构

```
YoshisIslandRecomp/
├── CMakeLists.txt              # 统一 CMake 构建配置文件
├── rom_identity.txt            # 严格 ROM 身份特征定义
├── config.ini                  # 启动器默认设置
├── keybinds.ini                # 默认键盘按键映射
├── manifest.json               # 发行包清单与元数据
├── README.md                   # 英文说明文档
├── README.zh-CN.md             # 中文说明文档
├── LICENSE                     # PolyForm Noncommercial License 1.0.0
├── build.bat / build.ps1       # 便捷构建脚本（默认 Jobs=2）
├── run.bat                     # 便捷启动脚本
├── recomp-ui/                  # 原生图形启动器模块（submodule）
├── snesrecomp/                 # SNESRecomp 运行时与硬件模拟层
│   └── runner/                 # PPU、APU、Super FX GSU-2 核心
├── src/
│   ├── desktop.c               # 交互式桌面宿主（SDL2 渲染、启动器集成、输入与音频）
│   ├── game_rtl.c / game_rtl.h # 游戏与运行时桥接
│   ├── headless.c              # Headless 诊断工具
│   └── game/                   # 65816 静态重编译 C 语言代码库（受控保护）
├── tests/
│   └── launcher_contract.c     # 启动器契约与规范自动化测试
├── vendor/SDL2/                # Windows SDL2 开发包
└── saves/                      # 电池备份存档目录（save.srm）
```

---

## 已知局限与说明 (Known Issues)

1. **仅支持单玩家模式**：《耀西岛》原始设计为单人动作冒险游戏，启动器输入设置专注于 Player 1 的深度按键重映射与摇杆死区调节。
2. **Super FX 协处理器特性**：目前以原始硬件分辨率与时序精准呈现，第一版未开启实验性宽屏补丁以保证 100% 画面稳定与无图形撕裂。
3. **ROM 区域兼容**：本重编译版本代码专为美版 V1.0（USA Rev 1.0）静态生成，日版、欧版或改版 ROM 将被严格身份校验拦截，无法直接加载。

---

## 许可证

本项目属于基于 [snesrecomp](https://github.com/RetroPortingToolKit/snesrecomp) 框架的衍生静态重编译工程，遵循 **PolyForm Noncommercial License 1.0.0** 许可证。

* **允许非商业用途**：允许个人学习、技术研究、私人娱乐及非盈利社区测试。
* **严禁商业化使用**：禁止任何形式的商业分发、变相收费或盈利行为。
* **上游版权声明**：`Copyright (c) 2026 Matthew Stanley`。
* 完整法律文本见 [LICENSE](LICENSE)。

---

## 致谢

* **[Zaxaerith](https://github.com/Zaxaerith)**：本项目移植、Super FX 协处理器同步集成与桌面宿主开发。
* **Nintendo**：原版游戏《超级马里奥世界2：耀西岛》（1995）的创作者。
* **[Matthew Stanley (mstan)](https://github.com/mstan)**：[snesrecomp](https://github.com/RetroPortingToolKit/snesrecomp) 与 [recomp-ui](https://github.com/RetroPortingToolKit/recomp-ui) 框架的作者。
* **[Raidenthequick, TheGreekBrit, and brunovalads](https://github.com/brunovalads/yoshisisland-disassembly)**：耀西岛全面反汇编工程的杰出贡献者。
