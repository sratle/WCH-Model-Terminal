# WCH-Model-Terminal

基于青稞 RISC-V 的嵌入式模块化移动终端，提供灵活的硬件扩展能力和定制化体验。

## 项目简介

一款采用模块化设计的个人移动终端。核心底板以沁恒 CH32H417（青稞 RISC-V 双核）为主控，
显示、键盘、配件模块均可热插拔、自由组合，终端自动识别模块组成，一台设备即可化身
文本编辑器、电子书、音乐播放器、电子琴、游戏机等，替代多种单一功能终端。

## 核心特性

- **模块化架构**：核心底板 + 可插拔显示（×1）/ 键盘（×1）/ 配件（×3）模块，
  500ms 心跳自动识别与热插拔恢复
- **RISC-V 生态**：CH32H417 / CH32V103 / CH32V307 / CH585F 全沁恒 MCU 阵容
- **双屏可选**：7 寸 800×480 电容触摸 LCD（SSD1963）或 5.83 寸 648×480 墨水屏（局部刷新 + 触摸板）
- **自研 MiniUI**：页面栈 / 控件库 / 动画 / 手势，LCD 与墨水屏共用一套 UI 框架
- **三种键盘**：39 键主键盘（可独立作 USB HID 键盘）、游戏键盘（摇杆 + 编码器）、
  24 键触摸电子琴（琴键 + 推子 + 鼓点）
- **音频引擎**：CS43131 DAC 双通道混音，3 段 EQ / 压缩器 / Echo 效果器，WAV 流式播放
- **丰富配件**：指纹登录、健康监测、NFC 刷卡、触摸圆环（圆盘导航器）、RGB 点阵、
  激光测距、低功耗副屏
- **统一协议**：全模块一套紧凑二进制协议（UART/SPI），心跳热插拔、身份指纹识别、反卡死填充
- **蓝牙终端**：CH585F BLE 桥接手机 APP（WCH Terminal），无线 CLI 控制与文件管理
- **用户系统**：PIN / 指纹 / NFC 三种认证，私密文件夹访问控制
- **长续航**：可拆卸供电模块（IP5568，PD 快充 + 无线充电，可作充电宝）

## 系统架构

```
┌──────────────────────────────────────────────────────────────┐
│                核心底板 main-model (CH32H417)                  │
│  CH378 文件管理 │ CS43131 音频 │ CH9350 USB HID │ CH585F BLE   │
│  CLI │ 用户系统 │ 配置持久化 │ 双通道混音 │ 心跳热插拔          │
└───┬──────────┬──────────┬──────────┬──────────┬──────────────┘
    │ UART4    │ UART3    │ UART5    │ UART6~8  │ SPI4
┌───▼────┐ ┌───▼─────┐ ┌──▼─────┐ ┌──▼─────────┐ ┌▼─────────┐
│ 显示模块 │ │ 键盘模块 │ │ 供电模块 │ │ 配件模块×3 │ │ Wireless │
│ LCD/墨水│ │主/游戏/琴│ │ IP5568 │ │ 7 种可选   │ │ CH585F   │
│ MiniUI │ │ CH9329  │ │ 3500mAh│ │            │ │ BLE↔APP  │
└────────┘ └─────────┘ └────────┘ └────────────┘ └──────────┘
```

## 模块组成

### [main-model](main-model/) - 核心底板

- **core**：CH32H417 核心固件（V5F 单核管理全部模块）
- **wireless**：CH585F BLE 固件（Peripheral + SPI 桥）
- **wch_terminal_app**：配套手机 APP（Flutter，蓝牙终端）

### [display-model](display-model/) - 显示模块

- **display-1**：7 寸 LCD（CH32H417 + SSD1963 + GT911 触摸），
  16 个应用（文件/音乐/编辑器/电子书/终端/配件管理…）+ 5 个游戏
- **display-2**：墨水屏（CH32V307），音乐/文件/编辑器/图片/电子书 + 2048/扫雷

### [keyboard-model](keyboard-model/) - 键盘模块

- **keyboard-1**：39 键主键盘（74HC165 级联 + CH9329 USB HID 输出）
- **keyboard-2**：游戏键盘（2 摇杆 + 6 按钮 + 3 钮子开关 + 2 编码器）
- **keyboard-3**：电子琴（24 触摸琴键 + 3 推子 + 12 鼓点按钮）

### [power-model](power-model/) - 供电模块

- **power-1**：IP5568 电源管理（PD 快充/无线充/充电宝），数码管解码上报电量与充电状态

### [sub-model](sub-model/) - 配件模块

- **submodel-1**：指纹识别（自动注册/识别，联动用户登录）
- **submodel-2**：健康监测（心率/血氧/HRV，手指自动检测）
- **submodel-3**：NFC 读卡（刷卡登录）
- **submodel-4**：触摸圆环（环=滚轮 + 中心 4 键=导航键，圆盘导航器）
- **submodel-5**：RGB 点阵（7×7 WS2812，灯效模式 + 游戏联动动画）
- **submodel-6**：激光测距（VL53L0X，双速主动上报）
- **submodel-7**：副屏（2.13 寸全反屏，三页轮显整机状态，常显低功耗）

## 文档

详细设计文档请参考：[docs/project](docs/project/)

- [PROJECT.md](docs/project/PROJECT.md) — 项目总览、CLI 命令、统一通信协议 V1.0
- 模块文档：Core / Wireless / Display-1·2 / Keyboard-1·2·3 / Power-1 / Submodel-1~7
- 协议文档：Protocol_Display / Keyboard / Power / Submodels / Wireless、protocol_app
- [config.md](docs/project/config.md) — 配置与用户系统持久化设计

## 开发环境

- IDE：MounRiver Studio（各模块均为独立 `.wvproj` 工程）
- 工具链：RISC-V Embedded GCC（`riscv-none-elf-gcc`）
- 调试：WCH-Link（1-wire serial）

## 许可证

Apache-2.0 License
