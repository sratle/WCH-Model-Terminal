# Keyboard-1 模块（主键盘）

## 概述

本模块为标准配列键盘输入方案，既作为移动终端的主输入设备，也可独立作为 USB HID 设备连接电脑使用。

- **主控芯片**：CH32V103C8T6
- **配列**：约 40% 配列，**39 键**（4 行：10 + 10 + 10 + 9），键位顺序与 `keyboard-layout.json` 一致
- **与核心模块接口**：模块侧 UART1，**230400/8-N-1**
- **模块 ID**：`0x20`
- **模块类型编号**：`0x04`
- **子类型编号**：`0x01`（Main Keyboard）

## 硬件组成

- **CH32V103C8T6**：按键扫描、协议帧打包、CH9329 通信
- **按键读取**：**5 片 74HC165 级联**移位寄存器（40 bit，使用 39 bit），
  主控通过移位时钟逐位读回全部按键电平（低电平有效），无需行列矩阵 GPIO 扫描
- **CH9329**：串口转 HID 芯片，将键码转换为标准 USB HID 键盘报告
- **USB-A 接口**：Device 口，输出 HID 信号，可接 PC 等主机

## 键位布局

39 个按键按 4 行排列（键索引 0~38，左到右、上到下）：

| 行 | 按键 |
|----|------|
| Row 1 (0~9) | Q W E R T Y U I O Back(映射为 Esc) |
| Row 2 (10~19) | Caps A S D F G H J P Enter |
| Row 3 (20~29) | Shift Z X C V B N M K L |
| Row 4 (30~38) | Ctrl Fn Tab Space , . ; ' / |

- Shift/Ctrl 映射为 HID 修饰键（L_Shift/L_Ctrl，进入报告修饰键位图而非键码槽）
- **Fn 键当前为保留键**：无 HID 键码、不产生任何输出，预留本地层切换扩展
- 每键的 HID Usage ID 与修饰键位由 `key.c` 的 `key_hid_codes[]` / `key_mod_bits[]` 表定义

## 软件职责

### 初始化阶段

- 初始化 CH32V103 时钟、GPIO、UART1（230400，接 Core）、UART2（115200，接 CH9329）
- 初始化 74HC165 级联读取 GPIO（LD/CLK/DATA）
- 初始化 CH9329：发送初始化命令，设置为标准键盘模式

### 运行阶段

- **按键扫描**：TIM2 每 **2ms** 触发一次，读回 5 字节（40 bit）按键电平；
  每键独立计数消抖，连续 5 次（**10ms**）同态才翻转稳定状态
- **HID 报告构建**：将所有按下的按键合成为一帧标准 HID Keyboard Boot Report
  （修饰键位图 + 最多 6 个键码槽，6KRO），键码槽满时多余按键忽略
- **双通道输出**（变化触发 + 节流）：
  - 报告内容与上一帧不同才发送，且两次发送至少间隔 **4 个扫描周期（约 8ms）**
  - 通过 **UART1** 向 Core 上报 `CMD_KBD_HID_REPORT`（`0x25`，报告类型 0x00 + 8 字节键盘报告）
  - 通过 **UART2 → CH9329 → USB-A** 输出同一帧 HID 键盘报告，接电脑即插即用
- **背光**：接收 Core 的 `CMD_KBD_SET_BACKLIGHT`（0x21）保存模式/亮度，
  响应 `CMD_KBD_GET_BACKLIGHT`（0x22）回读（当前硬件无背光电路，仅保存状态）
- **心跳**：响应 `CMD_GET_TYPE`（身份：类型 0x04 / 子类型 0x01 / HW 0x01 / FW 1.0）与 `CMD_NOP`

### 中断与事件

- TIM2 中断：2ms 扫描节拍
- UART1 中断：接收 Core 下发的背光/查询指令
- UART2：CH9329 仅作 HID 输出通道

## 项目目录结构

```
keyboard-1/
├── User/                           # 用户代码
│   ├── main.c                      # 主程序（扫描调度、HID 报告构建、协议分发）
│   ├── 74HC165/                    # 74HC165 级联读取 + 按键消抖/键码映射
│   │   ├── 74HC165.c/h             #   移位寄存器驱动
│   │   └── key.c/h                 #   39 键状态机、键名/HID 键码表
│   ├── CH9329/                     # CH9329 串口转 HID 驱动（UART2 @115200）
│   ├── Uart/                       # UART1 协议栈（230400，接 Core）
│   ├── protocol/                   # 统一协议帧打包/解析
│   └── ...                         # 中断/系统配置
├── Core/  Debug/  Peripheral/  Startup/  Ld/
└── keyboard_1.wvproj               # MounRiver Studio 工程文件
```

## 命名规范

- **文件**：统一采用下划线加小写字母，例如 `hardware.c`、`ch9329.h`
- **函数**：每个单词首字母大写加下划线，例如 `Hardware_Init()`、`Matrix_Scan()`
- **变量**：统一小写加下划线，例如 `key_state`、`fn_pressed`
- **文件夹**：下划线写法，仅首字母大写，例如 `Keyboard`、`CH9329`
- **系统目录**（`Core`、`Debug`、`Peripheral`、`Startup`）保持不动，不参与上述命名规范

## 开发要点

1. **CH9329 波特率**：固定 115200，上电后发送配置帧设定为标准键盘模式。
2. **双通道输出一致性**：同一帧 HID Boot Report 同时发往 Core（`CMD_KBD_HID_REPORT`）和
   CH9329（USB HID），两者共用 `key_hid_codes[]` 键码表；接电脑时无需依赖 Core 即可工作。
3. **报告节流**：HID 报告按变化触发，且两次发送间隔 ≥4 个扫描周期，避免 Core 侧
   双缓冲"保旧丢新"导致事件丢失；释放报告（全空）同样经此路径下发，保证按键弹起可靠传达。
4. **Fn 键预留**：Fn 无 HID 键码，当前不做层切换；如需 Fn 层，应在本地完成层映射后再上报最终键码。
5. **调试输出**：当前固件 `USART_Printf_Init` 与 Core 协议共用 UART1（协议 230400），
   printf 日志字节会进入 Core 接收流（Core 解析器忽略非帧头字节，但属开发期行为），
   正式部署应移除 printf。
