# Submodel-6 模块（激光测距）

## 概述

本模块为激光测距扩展方案，采用 VL53L0X-V2 ToF 激光测距传感器，通过飞行时间（Time-of-Flight）技术测量前方障碍物距离，可用于手势感应、物体接近检测或简易测距应用。

- **主控芯片**：CH32V103C8T6
- **与核心模块接口**：模块侧 UART1（PA9-TX, PA10-RX），**230400/8-N-1**
- **模块 ID**：`0x40` ~ `0x42`（由 Core 通过 GET_TYPE 动态分配）
- **模块类型编号**：`0x05`
- **子类型编号**：`0x06`（Laser）

## 硬件组成

- **CH32V103C8T6**：传感器数据采集、滤波处理、协议帧打包
- **VL53L0X-V2 激光测距传感器**：ToF 飞行时间测距，I2C 接口，默认模式量程 30~1200 mm，精度 ±3%
  - **SCL**：PB14（I2C 时钟）
  - **SDA**：PB15（I2C 数据）
  - **GPIO1**：PB13（传感器中断输出，数据就绪标志）
  - **XSHUT**：PB12（传感器硬件复位/关断控制）

### 引脚分配

| 引脚 | 功能 | 说明 |
|------|------|------|
| PA9 | UART1_TX | 与 Core 通信发送 |
| PA10 | UART1_RX | 与 Core 通信接收 |
| PB14 | I2C_SCL | VL53L0X I2C 时钟 |
| PB15 | I2C_SDA | VL53L0X I2C 数据 |
| PB13 | GPIO1 | VL53L0X 数据就绪中断 |
| PB12 | XSHUT | VL53L0X 复位/关断控制 |
| — | TIM2 | 1kHz 系统时基（上报调度） |

> **注意**：PB14/PB15 默认为 JTAG 引脚（JTDI/JTDO），初始化时必须
> `GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE)` 释放为 GPIO。

## 软件职责

### 初始化阶段

- 初始化 CH32V103 时钟、GPIO、软件 I2C（PB14-SCL/PB15-SDA）、UART1（PA9/PA10, 230400）、TIM2（1kHz 时基）
- 通过 XSHUT（PB12）复位 VL53L0X，等待 tBOOT（1.2ms max）
- 校验 I2C 接口（datasheet Table 4 参考寄存器：0xC0=0xEE、0xC1=0xAA、0xC2=0x10）
- 执行 DataInit（stop_variable、信号率上限 0.25 MCPS）、StaticInit（SPAD info）、
  参考校准（VHV + Phase），配置 GPIO1 中断为"新样本就绪"
- **无开机自检**：初始化成功后直接启动连续测距并进入待机上报状态
- 模块 ID 初始为 `0x00`（未知），等待 Core 发送 GET_TYPE 时从 DST 字段学习

### 运行阶段

- **槽位学习**：
  - 收到 Core 的 GET_TYPE 命令时，从帧的 DST 字段（0x40/0x41/0x42）学习自身模块 ID
  - 后续所有响应帧的 SRC 字段使用学习到的模块 ID
- **双状态定时上报**（模块主动，TIM2 1kHz 时基调度，不依赖 GET_TYPE 轮询）：

| 状态 | 进入条件 | 上报周期 | 说明 |
|------|---------|---------|------|
| 待机（STANDBY） | 上电默认 / SET_MODE SUB=0x02 | **1000ms** | 每 1s 上报一次平均距离 |
| 测距（RANGING） | SET_MODE SUB=0x01 | **100ms** | 每 100ms 上报一次平均距离 |

- **GET_TYPE 响应**：仅回复固定 5 字节身份 ACK（类型 0x05，子类型 0x06），
  不附带测距数据（测距数据走定时上报通道）
- **采样**：传感器连续测距（约 33ms 时序预算），主循环每轮排空所有就绪样本，
  有效样本（DeviceError = RANGECOMPLETE = 11）推入滤波窗口
- **滤波算法**：窗口 8 的滑动平均（`Filter/filter.c`），上报值为窗口内
  有效样本均值；无效样本（DeviceError ≠ 11、距离为 0、I2C 错误）不入窗
- **失败上报**：上报时刻滤波窗口为空时，发送 `0x45 SUB=0x02 LR_ERR_OUT_OF_RANGE`

### 中断与事件

- TIM2 更新中断：1kHz 毫秒时基（上报周期调度）
- UART1 中断：接收 Core 的查询或配置指令（帧状态机 + ORE 清理）

## 项目目录结构

```
submodel_6/
├── Common/                         # 公共代码与系统文件
│   ├── Common/                     # 自定义公共模块
│   │   ├── VL53L0X/                # VL53L0X 传感器驱动
│   │   ├── Filter/                 # 滤波算法（滑动平均，窗口 8）
│   │   ├── Uart/                   # UART1 协议栈
│   │   ├── Protocol/               # 统一协议帧打包/解析
│   │   ├── hardware.c              # 全局调度、双状态上报与初始化入口
│   │   └── hardware.h              # 全局调度头文件
│   ├── Core/                       # RISC-V 内核相关文件
│   ├── Debug/                      # 调试支持文件
│   ├── Peripheral/                 # 芯片外设库
│   └── Startup/                    # 启动文件
│
├── User/                           # 用户代码（main.c / 中断 / 系统配置）
├── Ld/                             # 链接脚本
├── .cproject
├── .project
└── dis.wvproj                      # MounRiver Studio 工程文件
```

## 命名规范

- **文件**：统一采用下划线加小写字母，例如 `hardware.c`、`vl53l0x.h`
- **函数**：每个单词首字母大写加下划线，例如 `Hardware_Init()`、`VL53L0X_ReadDistance()`
- **变量**：统一小写加下划线，例如 `distance_mm`、`i2c_buf`
- **文件夹**：下划线写法，仅首字母大写，例如 `VL53L0X`、`Filter`
- **系统目录**（`Core`、`Debug`、`Peripheral`、`Startup`）保持不动，不参与上述命名规范

## 开发要点

1. **VL53L0X 寄存器为 8 位索引**（datasheet Figure 15/16），非 16 位；
   多字节读写 MSB first、支持自动递增（Figure 17/18）。Model ID 校验用
   参考寄存器 0xC0=0xEE（datasheet Table 4）。
2. **测量有效性判定**：RESULT_RANGE_STATUS(0x14) 的 DeviceError = bits[6:3]，
   **11（RANGECOMPLETE）才是有效测量**，0 表示无新测量，其余为错误码
   （弱信号/相位/超量程等，datasheet 2.6.3）。
3. **SYSRANGE_START**：连续测距启动写 0x02，停止写 0x01（不是 0x00）；
   启动前需恢复 DataInit 保存的 stop_variable（0x91）。
4. **GPIO1 中断模式**：已配置 SYSTEM_INTERRUPT_CONFIG=0x04（新样本就绪），
   当前采用主循环轮询 RESULT_INTERRUPT_STATUS(0x13)，未接 PB13 EXTI。
5. **上报节奏**：100ms 上报周期下模块→Core 方向每 100ms 一帧，注意 Core 侧
   双缓冲"保旧丢新"，偶发丢帧可接受（事件周期重发）。
6. **调试开关**：`vl53l0x.h` 中 `VL53L0X_DEBUG_EN` 置 1 可启用诊断输出
   （I2C 扫描/交换扫描/总线恢复/引脚翻转），需配合 main.c 的
   `USART_Printf_Init`；**生产固件必须保持 0**，printf 与 Core 协议共用 UART1。
7. **与 Core/Display 的全链路**：模块定时上报 → Core `submodels_lr_dispatch`
   打印并 `Display_SendSubmodelEvent(SUBMODEL_TYPE_LASER, ...)` 转发 →
   Display 的 L-Range 应用显示距离、20cm 过近警告与折线图。
   Core CLI 用 `lr start` / `lr stop` 切换 100ms/1s 上报周期。
8. **系统时钟可能是 HSI 8MHz 而非 72MHz**：`SetSysClockTo72_HSE()` 在 HSE
   起振失败时**静默跳过**（不报错不死等），本模块板若晶振未起振则全片跑在
   HSI 8MHz。UART/Delay 均按运行时实际时钟计算所以无感，但**任何定时器的
   分频值必须用 `RCC_GetClocksFreq()` 动态推导，禁止按 72MHz 硬编码**
   （TIM2 曾因硬编码 PSC=72 导致 1ms 节拍变 9ms，上报周期全部 ×9）。
