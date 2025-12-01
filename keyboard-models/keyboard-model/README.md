# 40%配列键盘 (keyboard-model)

标准40%配列键盘，约40-48键。

## 硬件配置
- **主控**: CH32V103 (裸机)
- **HID转换**: CH9329 UART转HID
- **矩阵**: 6×8 或 8×8
- **轴体**: 矮轴或标准机械轴
- **可选**: RGB背光 (WS2812)

## 功能特性
- NKRO/6KRO全键无冲
- 软件去抖5-10ms
- 宏支持
- 多层布局
- 背光调节

## 通信协议
- UART: 115200bps
- 命令: KeyMatrixReport, MacroConfig, BacklightCtrl
