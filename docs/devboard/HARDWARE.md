# WCH-DevBoard 硬件文档

> 原理图：SCH_Schematic23（嘉立创EDA，V1.0，2026-08-05）
> 主控：**CH32V307WCU**（RISC-V4F，144MHz，64KB SRAM / 256KB Flash）

## 1. 电源

| 项目 | 说明 |
|---|---|
| 输入 | 5V（USB1 / USB2 Type-C，或排针） |
| 稳压 | U2 TLV1117LV33DCYR：5V → 3.3V |
| 指示灯 | LED12 = 5V 电源指示（R10 3K）；LED10 = 3.3V 指示（R8 1K） |
| 去耦 | C15/C4/C5/C16（LDO 前后 1uF + 0.1uF）；MCU 各 VDD 就近 0.1uF |

## 2. 最小系统

| 信号 | 连接 |
|---|---|
| 晶振 | X2 8MHz，接 XI(PD0/OSC_IN) / XO(PD1/OSC_OUT)，C2/C3 22pF |
| 复位 | NRST 经 R4 10K 上拉至 3V3，按键 SW2 接地，C8 0.1uF |
| 启动 | BOOT0 / BOOT1 经 R7/R6 10K，SW1 选择 |
| 排针 | H1 / H2 引出全部 GPIO（2×28P） |

## 3. 屏幕（重点）

- 模组：**T200H7-C14-05**，2.0 寸 IPS TFT，**ST7789P3** 驱动 IC
- 分辨率：原生 **240(宽) × 320(长)**；本板**横屏使用：320 × 240**（MADCTL 置 MV|MX）
- 接口：4 线 SPI（硬件 **SPI1**），RGB565（每像素 16bit）
- 背光：LEDA 直连 +3.3V、LEDK 接地 → **常亮，无需控制**（规格书：VF=3.2V typ，IF=60mA）
- 连接器：FPC1，FPC-05F-14PH15（14Pin）

| FPC 引脚 | 信号 | MCU 引脚 | 说明 |
|---|---|---|---|
| 1, 5, 13 | GND | — | 地 |
| 2, 3 | LEDK | GND | 背光阴极 |
| 4 | LEDA | +3.3V | 背阳阳极（常亮） |
| 6 | RESET | **PA8** | 控制器复位，低有效 |
| 7 | D/C | **PA3** | 数据(高)/命令(低) |
| 8 | SDA | **PA7** (SPI1_MOSI) | 串行数据 |
| 9 | SCL | **PA5** (SPI1_SCK) | 串行时钟 |
| 10 | VCC | +3.3V | 模拟电源 |
| 11 | IOVCC | +3.3V | IO 电源 |
| 12 | CS | **PA4** (SPI1_NSS) | 片选，低有效（软件控制） |
| 14 | NC | — | 空脚 |

> PA6 (SPI1_MISO) 已在 MCU 侧引出但屏幕只写不读，软件不使用。
> 横屏后逻辑坐标：x ∈ [0,319] 向右，y ∈ [0,239] 向下，原点在左上角。

## 4. 按键（16 键，74HC165 级联）

- 芯片：U3 / U4 两片 **74HC165** 并转串移位寄存器级联（U3.DS ← U4.Q7）
- 按键：S1~S8 → U3 的 D0~D7，S9~S16 → U4 的 D0~D7
- RN1 / RN2 = 10KΩ 排阻**上拉**；按键按下将输入接地 → **读到 0 = 按下，1 = 松开**
- 读取方式：GPIO 模拟时序，一次读入 16bit（bit0=S1 … bit15=S16）

| 信号 | MCU 引脚 | 74HC165 引脚 | 说明 |
|---|---|---|---|
| PL | **PB14** | SH/LD (Pin1) | 低电平装载并行数据 |
| CE | **PB13** | CLK INH (Pin15) | 时钟使能，低有效 |
| CP | **PB12** | CLK (Pin2) | 移位时钟，上升沿 |
| DATA | **PD8** | Q7 (U3 Pin9) | 串行数据输入到 MCU（**飞线，PCB 未布线**） |

## 5. 蜂鸣器

- **PB15 (TIM1_CH3N) → R17(1K) → Q1(SS8550, PNP)** 驱动 4kHz 无源蜂鸣器，R3 10K 上拉
- PB15 输出**低电平**时蜂鸣器发声；用 **TIM1_CH3N 硬件 PWM** 输出方波（50% 占空比），~4kHz 时最响

## 6. 板载 LED 与 RGB 灯带

| 项目 | 连接 | 说明 |
|---|---|---|
| 用户 LED | **PC0** ← R9(1K) ← LED11 ← +3.3V | **低电平点亮** |
| RGB 灯带 | **PD9** → LED1~LED9 (XL-5050RGBC, WS2812 协议) 级联 | 9 颗，单线归零码，5V 供电 |

## 7. 串口与 USB

| 功能 | 引脚 | 说明 |
|---|---|---|
| UART0_TX | **PA9** | → CH340N(U5) RXD → USB1 Type-C；LED13 指示 |
| UART0_RX | **PA10** | → CH340N TXD；LED14 指示 |
| USBDM / USBDP | **PA11 / PA12** | MCU 内置 USB → USB2 Type-C（DPU/DNU） |

CH340N 用于 printf 调试（115200 8N1）。

## 8. 引脚分配总表

| 引脚 | 功能 | 引脚 | 功能 |
|---|---|---|---|
| PA8 | LCD RES | PD8 | 74HC165 DATA（飞线） |
| PA9 | UART0_TX (CH340) | PD9 | RGB 灯带 (WS2812) |
| PA10 | UART0_RX (CH340) | PA11/PA12 | USBDM/USBDP |
| PA3 | LCD D/C | PB12 | 74HC165 CP |
| PA4 | LCD CS | PB13 | 74HC165 CE |
| PA5 | LCD SCL (SPI1_SCK) | PB14 | 74HC165 PL |
| PA6 | SPI1_MISO（未用） | PB15 | 蜂鸣器 (TIM1_CH3N) |
| PA7 | LCD SDA (SPI1_MOSI) | PC0 | 用户 LED（低电平点亮） |
| PC14 | HC-SR04 Trig（飞线） | PC13 | HC-SR04 Echo（飞线，5V 电平注意） |

## 9. 关键电气参数

| 项目 | 参数 |
|---|---|
| MCU 主频 | 144MHz（PLL × 8MHz HSE） |
| SPI1 速率 | 默认 36MHz（sysclk/4），验证稳定后可升 72MHz |
| 屏幕像素格式 | RGB565，SPI Mode 0（CPOL=0, CPHA=0），MSB 先行 |
| 屏幕供电 | VCC 2.5~3.3V，IOVCC 1.65~3.3V |
| 74HC165 电平 | 3.3V CMOS，时钟可用普通 GPIO 速度模拟 |
