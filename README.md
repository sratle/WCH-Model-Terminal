# WCH-Model-Terminal

基于青稞RISC-V的嵌入式模块化移动终端，提供灵活的硬件扩展能力和定制化体验。

## 项目简介

一款采用模块化设计的个人移动终端，核心采用沁恒CH32V系列RISC-V微控制器，支持显示、键盘、配件模块的热插拔与自由组合，满足不同用户的多样化需求。

## 核心特性

- **模块化架构**: 核心底板 + 可插拔显示/键盘/配件模块
- **RISC-V生态**: 基于CH32V307/H417/V103系列MCU
- **多屏支持**: TFT液晶屏（800×480）、墨水屏可选
- **多样键盘**: 40%配列、手柄布局、电子琴布局
- **丰富配件**: 指纹、旋钮、健康监测、NFC等8+种扩展模块
- **统一通信**: UART协议（CRC16校验，支持枚举与热插拔）
- **无线连接**: 板载蓝牙/WiFi（CH585）
- **长续航**: 10000mAh可拆卸电池，支持PD快充与无线充电

## 系统架构

```
┌─────────────────────────────────────────────────┐
│          核心底板 (CH32V307 + FreeRTOS)          │
│  ├─ CommHub: UART总线仲裁                        │
│  ├─ DisplayProxy: UI指令打包                     │
│  ├─ AccessoryMgr: 配件枚举                       │
│  ├─ HID/FS/Power/Logger服务                     │
└────────┬──────────┬──────────┬──────────────────┘
         │          │          │
    ┌────▼────┐ ┌──▼────┐ ┌───▼───────┐
    │ 显示模块 │ │键盘模块│ │配件模块×3 │
    │ (LVGL)  │ │(HID)  │ │ (传感器)  │
    └─────────┘ └───────┘ └───────────┘
```

## 模块组成

### [main-model](main-model/) - 核心底板
- **core-model**: CH32V307主控，FreeRTOS操作系统
- **wireless-model**: CH585蓝牙/WiFi模块

### [display-models](display-models/) - 显示模块
- **lcd-model**: 7寸TFT屏（800×480，CH32H417 + LTDC）
- **epd-model**: 7寸墨水屏（低功耗，SPI驱动）

### [keyboard-models](keyboard-models/) - 键盘模块
- **keyboard-model**: 40%标准配列键盘
- **joycon-model**: 游戏手柄布局
- **piano-model**: 电子琴布局

### [extension-models](extension-models/) - 配件模块
- **fingerprint-model**: 指纹识别（本地匹配）
- **knob-model**: 旋钮模块（音量/参数调节）
- **health-model**: 健康监测（心率/血氧/温湿度）
- **NFC-model**: NFC读卡
- **touch-ring-model**: 触摸圆环
- **RGB-model**: RGB点阵
- **distance-model**: 红外测距
- **screen-model**: 副屏显示

## 技术栈

- **主控**: CH32V307VCT6 (RISC-V, 128K RAM, 192K Flash)
- **显示**: CH32H417 (LTDC) + LVGL 9.x
- **配件**: CH32V103 (裸机)
- **OS**: FreeRTOS (核心/显示模块)
- **通信**: UART (115200/460800bps, CRC16校验)
- **文件**: CH378 (FAT32/exFAT)
- **音频**: CS43131 DAC

## 通信协议

UART帧结构（改进版）：
```
┌─────┬────────┬──────────┬──────┬─────┬────────┬──────┬───────┐
│ SOF │ Device │ Instance │ Type │ Seq │ Length │ Data │ CRC16 │
├─────┼────────┼──────────┼──────┼─────┼────────┼──────┼───────┤
│ 1B  │   1B   │    1B    │  1B  │ 1B  │   2B   │ 0-512│  2B   │
└─────┴────────┴──────────┴──────┴─────┴────────┴──────┴───────┘
```

- 支持模块热插拔与自动枚举（HELLO → ASSIGN_ID → READY）
- 心跳机制（1秒周期）与超时重连
- ACK/NACK确认与重传机制

## 快速开始

详细设计文档请参考：[docs/pre.md](docs/pre.md)

每个子模块包含独立的README，说明硬件配置、功能特性和开发指南。

## 许可证

Apache-2.0 License
