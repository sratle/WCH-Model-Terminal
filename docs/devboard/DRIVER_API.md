# DevBoard 驱动库使用文档

驱动代码位于 `devboard/User/BSP/`，各模块独立解耦：头文件只暴露 API，
不包含任何跨模块依赖，可按需单独拷贝使用。

## 目录结构

```
User/
├─ main.c                 # 综合测试程序（按键网格 + RGB + 蜂鸣器 + 串口回显）
└─ BSP/
   ├─ bsp_lcd.h / .c      # ST7789P3 屏幕驱动 + 绘图 + 文字渲染
   ├─ bsp_font.h          # 位图字体类型定义（兼容 display-2 MiniUI 字库格式）
   ├─ font_montserrat_16.h/.c   # Montserrat 16 号字体（ASCII 32~126 + ° •）
   ├─ bsp_key.h / .c      # 74HC165×2 级联，16 键读取（消抖；DATA=PD8 飞线）
   ├─ bsp_rgb.h / .c      # WS2812×9 灯带（PD9，GRB，800kHz 模拟时序）
   ├─ bsp_buzzer.h / .c   # 无源蜂鸣器（PB15=TIM1_CH3N 硬件 PWM，定频发声）
   ├─ bsp_led.h / .c      # 板载用户 LED（PC0，低电平点亮）
   ├─ bsp_hcsr04.h / .c   # HC-SR04 超声波测距（Trig=PC14，Echo=PC13，TIM3 计时）
   ├─ bsp_cmd.h / .c      # 串口命令行框架（注册-解析-执行，非阻塞）
   └─ bsp_uart.h / .c     # USART1 全双工（PA9/PA10→CH340N），RX 环形缓冲
```

## 快速开始

```c
#include "debug.h"
#include "BSP/bsp_lcd.h"

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    Delay_Init();               /* LCD 复位延时依赖 debug 延时 */

    LCD_Init();                 /* 只需 init 一次 */

    LCD_Fill(LCD_BLACK);
    LCD_DrawRect(10, 10, 100, 60, LCD_YELLOW);
    LCD_DrawText(20, 20, "sample", LCD_WHITE);

    while (1) { }
}
```

## LCD API 一览（bsp_lcd.h）

### 基础绘图

| 函数 | 说明 |
|---|---|
| `LCD_Init()` | 初始化 SPI1/GPIO 并完成 ST7789 寄存器配置 |
| `LCD_Fill(color)` | 整屏填充 |
| `LCD_FillRect(x,y,w,h,color)` | 实心矩形（自动裁剪） |
| `LCD_DrawRect(x,y,w,h,color)` | 矩形框（1px） |
| `LCD_DrawPixel(x,y,color)` | 画点 |
| `LCD_DrawLine(x0,y0,x1,y1,color)` | 直线（Bresenham，水平/垂直走快速路径） |
| `LCD_DrawCircle(x0,y0,r,color)` / `LCD_FillCircle(...)` | 圆框 / 实心圆 |
| `LCD_DrawRoundRect(x,y,w,h,r,color)` / `LCD_FillRoundRect(...)` | 圆角矩形框 / 实心圆角矩形（r=角半径） |
| `LCD_DrawTriangle(p0,p1,p2,color)` / `LCD_FillTriangle(...)` | 三角形框 / 实心三角形（扫描线填充） |

### 图片与位图

| 函数 | 说明 |
|---|---|
| `LCD_DrawImage(x,y,w,h,data)` | 绘制 RGB565 图片（每像素2字节、行主序、高字节在前），逐行流式写入并自动裁剪 |
| `LCD_DrawMonoBitmap(x,y,w,h,bmp,fg,bg)` | 1bpp 位图（与字库同格式：行主序、MSB在前），不透明背景 |
| `LCD_DrawMonoBitmapTR(x,y,w,h,bmp,fg)` | 1bpp 位图，透明背景（适合叠加图标） |

### 文字

| 函数 | 说明 |
|---|---|
| `LCD_DrawText(x,y,text,color)` | 字符串，**透明背景** |
| `LCD_DrawTextBG(x,y,text,fg,bg)` | 字符串，**不透明背景**：按整字符单元格（advance×行高）填充背景，字形间隙无残留。注意只清理本次字符串自身宽度，覆盖更宽的旧文本时先 `LCD_FillRect` 清场 |
| `LCD_DrawTextCenter(y,text,color)` | 整行水平居中（透明背景） |
| `LCD_DrawChar(x,y,ch,color)` / `LCD_DrawCharBG(...)` | 单字符（返回步进宽度，便于排版光标自增） |
| `LCD_SetFont(font)` | 切换字体，传 NULL 恢复默认 16 号 |
| `LCD_GetFont()` / `LCD_FontHeight()` | 查询当前字体 / 行高（多行排版：`y += LCD_FontHeight()`） |
| `LCD_TextWidth(text)` | 字符串像素宽度（用于居中、右对齐） |

### 显示控制

| 函数 | 说明 |
|---|---|
| `LCD_DisplayOn()` / `LCD_DisplayOff()` | 开关显示（GRAM 内容保留，背光为硬件常亮） |
| `LCD_InvertColors(enable)` | 反色显示（可用于告警闪烁；本 IPS 屏正常显示需保持 INVON） |

### 坐标系

横屏 320×240，原点左上，x 向右、y 向下。文字 API 的 `(x, y)` 是
**文本行的左上角**（基线由字库内部处理）。

### 颜色

RGB565 格式。预定义：`LCD_BLACK/WHITE/RED/GREEN/BLUE/YELLOW/CYAN/
MAGENTA/ORANGE/GRAY/NAVY/...`，或用 `LCD_RGB565(r,g,b)` 自定义。

## 配置项（bsp_lcd.h 顶部）

| 宏 | 默认 | 说明 |
|---|---|---|
| `LCD_WIDTH / LCD_HEIGHT` | 320 / 240 | 横屏逻辑分辨率 |
| `LCD_XSTART / LCD_YSTART` | 0 / 0 | GRAM 偏移 |
| `LCD_ROTATION` | 1 | 0~3，0/2 为竖屏（需交换宽高） |
| `LCD_MADCTL_BGR` | 0 | 本板实测为 RGB 序；红蓝颠倒时改为 1 |
| `LCD_SPI_PRESCALER` | /4 (36MHz) | 点亮验证后可改 /2 (72MHz) 提速 |

## 按键 API（bsp_key.h）

| 函数 | 说明 |
|---|---|
| `KEY_Init()` | 初始化 PL/CE/CP/DATA 引脚 |
| `KEY_ReadRaw()` | 立即读取 16 键状态，返回 16bit 掩码（bit0=物理键1…bit15=键16，置位=按下） |
| `KEY_Scan()` | 消抖读取（两次采样间隔 5ms，返回稳定位） |
| `KEY_IsPressed(state, n)` | 判断掩码中第 n 键（1~16）是否按下 |

键位掩码宏：`KEY_1` ~ `KEY_16`、`KEY_ALL`。**键号按 PCB 物理位置编号**
（从左到右、从上到下 1~16），丝印/级联顺序的差异已在驱动内部处理。

```c
KEY_Init();
uint16_t st = KEY_Scan();
if (st & KEY_5) { /* S5 按下 */ }
```

## RGB 灯带 API（bsp_rgb.h）

| 函数 | 说明 |
|---|---|
| `RGB_Init()` | 初始化 PD9 并清空灯带 |
| `RGB_SetPixel(i, r, g, b)` | 设置第 i 颗颜色（只改影子缓冲；**i 按物理位置 0~8**，从左到右、从上到下） |
| `RGB_SetPixelColor(i, 0xRRGGBB)` | 打包颜色版本 |
| `RGB_SetAll(r, g, b)` / `RGB_Clear()` | 全部同色 / 全灭 |
| `RGB_SetBrightness(0~255)` | 全局亮度（刷新时缩放） |
| `RGB_GetBuffer()` | 直接访问像素缓冲（`rgb_color_t[9]`） |
| `RGB_Refresh()` | 把缓冲推送到灯带（每颗灯关中断 ~30us，UART 不受影响） |

```c
RGB_Init();
RGB_SetPixel(0, 255, 0, 0);   // LED1 红
RGB_SetPixel(1, 0, 0, 255);   // LED2 蓝
RGB_Refresh();
```

## 蜂鸣器 API（bsp_buzzer.h）

| 函数 | 说明 |
|---|---|
| `BUZZER_Init()` | 初始化 PB15（TIM1_CH3N） |
| `BUZZER_On(freq_hz)` | 按指定频率连续发声（非阻塞硬件 PWM，约 35Hz~100kHz，4kHz 最响） |
| `BUZZER_Off()` | 停止 |
| `BUZZER_Beep(freq_hz, ms)` | 阻塞式鸣叫 |
| `BUZZER_IsOn()` | 查询是否正在发声 |

实现说明：PB15 即 **TIM1_CH3N**，驱动直接输出硬件 PWM（50% 占空比，
发声期间零 CPU 占用）；停止时引脚切回普通 GPIO 并拉高，确保 PNP 关断。

```c
BUZZER_Init();
BUZZER_Beep(4000, 100);   // 4kHz 鸣叫 100ms
```

## 超声波测距 API（bsp_hcsr04.h）

HC-SR04：Trig=PC14（输出），Echo=PC13（输入）。TIM3 作 1MHz 自由运行
计数器测回波脉宽，`mm ≈ us × 17 / 100`。注意 Echo 为 5V 电平，长期
使用建议在 Echo 线上串电阻分压。

| 函数 | 说明 |
|---|---|
| `HCSR04_Init()` | 初始化引脚与 TIM3 时基 |
| `HCSR04_ReadMm()` | 触发一次测距，返回毫米数；超时/超量程返回 `HCSR04_INVALID`(-1)。阻塞，最多约 40ms |
| `HCSR04_ReadRawUs()` | 返回原始回波脉宽（us），失败返回 -1 |

```c
HCSR04_Init();
int32_t mm = HCSR04_ReadMm();
if (mm >= 0) printf("dist %ld mm\r\n", (long)mm);
```

## 用户 LED API（bsp_led.h）

板载 LED11 接法为 3V3→LED→R9→PC0，**PC0 低电平点亮**
（驱动内由 `LED_ACTIVE_LOW` 宏处理，API 无需关心极性）。

| 函数 | 说明 |
|---|---|
| `LED_Init()` | 初始化 PC0，默认熄灭 |
| `LED_On()` / `LED_Off()` / `LED_Toggle()` | 亮 / 灭 / 翻转 |
| `LED_Set(on)` | 显式设置状态 |
| `LED_Get()` | 查询当前状态（软件记录） |

## 命令行框架 API（bsp_cmd.h）

在 bsp_uart 的环形缓冲之上提供**命令注册-解析-执行**框架：
字节流按行装配（支持回显、退格、CR/LF），按空格分词成 argc/argv
后查表分发。`CMD_Task()` 完全非阻塞，放主循环即可。

| 函数 | 说明 |
|---|---|
| `CMD_Init()` | 复位行编辑器并打印提示符（需先 `UART_Init`） |
| `CMD_Register(name, handler, help)` | 注册命令；handler 原型 `int fn(int argc, char *argv[])` |
| `CMD_Task()` | 非阻塞轮询：收满一行即执行 |
| `CMD_ExecuteString(line)` | 不经串口直接执行一条命令（自测/脚本用） |
| `CMD_PrintHelp()` | 打印命令表 |

handler 返回 `CMD_OK`(0) 静默，`CMD_ERR_USAGE` 自动打印 help 文本，
`CMD_ERR_VALUE` 打印 invalid value。容量：16 条命令 / 96 字符行长 /
8 个参数（`bsp_cmd.h` 顶部可改）。

### 接线与使用

串口助手接 USB1（CH340N），**115200 8N1**，行尾任意（CR/LF 均可）。
上电后出现 `> ` 提示符，直接输入命令回车执行；支持退格修改。

### 内置示例命令（main.c）

```
help                     - 列出所有命令
rgb <id 1-9> <rrggbb>    - 设置指定 RGB 灯颜色，如 rgb 3 ff8000
rgboff                   - 全灭
led on|off|toggle        - 板载 LED
beep <freq_hz> <ms>      - 蜂鸣器鸣叫
dist                     - 打印一次测距结果 (mm)
keys                     - 打印 16 键状态掩码
text <x> <y> <string...> - 在屏幕上画文字
```

### 如何新增一条命令（三步）

以新增 `fill <rrggbb>`（全屏填充颜色）为例：

**第 1 步**：在 main.c 写处理函数。`argv[0]` 是命令名本身，参数从
`argv[1]` 开始；参数不足时返回 `CMD_ERR_USAGE`，框架会自动打印你在
注册时写的用法文本：

```c
static int Cmd_Fill(int argc, char *argv[])
{
    unsigned long color;
    if (argc < 2) return CMD_ERR_USAGE;          /* 参数不够 → 打印用法 */
    color = strtoul(argv[1], NULL, 16);          /* 16 进制解析颜色     */
    LCD_Fill((uint16_t)color);
    printf("fill %06lX\r\n", color);             /* 串口回执（可省略）  */
    return CMD_OK;
}
```

**第 2 步**：在 `Demo_RegisterCommands()` 里注册一行：

```c
CMD_Register("fill", Cmd_Fill, "fill <rrggbb> - fill screen with color");
```

**第 3 步**：烧录后串口输入 `fill f800` 即全屏变红。

要点：
- 数值解析用 `strtol/strtoul`，第二个参数填 `NULL`，base 填 0 可
  自动识别 `0x` 前缀和十进制，填 16 强制十六进制
- handler 里**不要用长阻塞操作**（如长延时），否则串口收流会堆积；
  非要阻塞时确保小于 RX 缓冲消化能力（256 字节）
- 想做"立即执行单字符"类交互（如方向键控制），不走本框架，直接在
  主循环用 `UART_ReadByte()` 取字节即可，二者互不干扰

## 串口 API（bsp_uart.h）

USART1 全双工（PA9=TX / PA10=RX → CH340N → USB1）。RX 走中断 +
**256 字节环形缓冲**，主循环随时可取，不丢字节（缓冲满时丢弃新字节）。
与 `printf`（debug.c）兼容：`UART_Init()` 可替代 `USART_Printf_Init()`。

| 函数 | 说明 |
|---|---|
| `UART_Init(baudrate)` | 初始化 TX+RX 与接收中断 |
| `UART_SendByte/Bytes/String(...)` | 阻塞发送 |
| `UART_Available()` | 缓冲中待读字节数 |
| `UART_ReadByte()` | 读 1 字节，空返回 -1（非阻塞） |
| `UART_ReadBytes(buf, max)` | 批量读取，返回实际数量 |
| `UART_RxFlush()` | 清空接收缓冲 |

```c
UART_Init(115200);
int16_t ch;
while ((ch = UART_ReadByte()) >= 0) {
    UART_SendByte((uint8_t)ch);   // 回显
}
```

## 字库格式（bsp_font.h）

与 display-2 MiniUI 完全相同的 1bpp 位图格式（行主序、MSB 在前），
LVGL 字库提取结果可直接复用：

```c
typedef struct {
    uint16_t unicode;   /* 码点                       */
    uint8_t  width;     /* 字形宽                     */
    uint8_t  height;    /* 字形高                     */
    int8_t   x_offset;  /* 相对光标 X 偏移            */
    int8_t   y_offset;  /* 相对基线 Y 偏移（负=向上） */
    uint8_t  advance;   /* 光标步进                   */
    const uint8_t *bitmap;
} lcd_glyph_t;
```

新增字体只需仿照 `font_montserrat_16.c` 提供 `lcd_font_t` 描述符，
然后 `LCD_SetFont(&your_font)`。

| 函数 | 说明 |
|---|---|
| `LCD_FontGetGlyph(font, unicode)` | 二分查找字形（字库按 unicode 升序排列），未收录返回 NULL |

## 设计说明

- **无帧缓冲**：所有绘图直接开窗流式写入 ST7789 GRAM，SRAM 占用几乎为零
  （仅 64 字节突发缓冲），64KB SRAM 全部留给应用
- **解耦**：各 BSP 模块只依赖 CH32 标准库与 debug 延时，互不包含；
  bsp_font 纯数据结构，不依赖任何硬件
- **中断/定时器分配**：TIM1_CH3N = 蜂鸣器 PWM，TIM3 = 超声波计时，
  USART1 = 串口接收，均在各自模块内注册，不与屏幕/按键冲突
