# 核心主控模块 (core-model)

基于CH32V307VCT6的核心主控，运行FreeRTOS操作系统，是整个系统的调度中枢。

## 硬件配置
- **MCU**: CH32V307VCT6 (128K SRAM, 192K FLASH)
- **接口**: 2×USB-A, 1×USB-C, TF卡槽
- **外设**: CH378文件管理芯片, CS43131音频DAC

## 软件架构 (FreeRTOS)
- **CommHub**: UART总线仲裁与路由
- **DisplayProxy**: UI指令打包与发送
- **FS服务**: CH378文件操作代理
- **HID服务**: 外设转发与模拟
- **AccessoryMgr**: 配件枚举与驱动抽象
- **PowerMgr**: 电源管理与热控
- **Logger**: USB-C虚拟串口日志输出

## UART分配
- UART1: 显示模块 (460800bps)
- UART2: 键盘模块 (115200bps)
- UART3-5: 配件模块1-3 (115200bps)
- UART6: CH378通信
- UART7: CH585通信
- UART8: 供电模块 (115200bps)
