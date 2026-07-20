# Display-2 模块（墨水屏）

## 概述

本模块为低功耗电子墨水屏显示方案，适用于长续航阅读场景，与 Display-1 共用同一套 MiniUI 框架与通信协议。

- **主控芯片**：CH32V307RCT6
- **屏幕规格**：ZJY648480-0583AAAMFGN 5.83寸黑白墨水屏，648×480像素，138DPI
- **驱动IC**：JD79686AB（片上TCON、SRAM、LUT、DC-DC）
- **驱动方式**：4线SPI接口（BS1=L），**硬件 SPI1 @ 9MHz + DMA1 Channel3**（V2.0 起；软件位敲保留为编译回退）
- **与核心模块接口**：UART1（PA9-TX，PA10-RX），**921600/8-N-1**
- **DEBUG接口**：UART2（PA2-TX，PA3-RX），**921600/8-N-1**
- **模块 ID**：`0x10`（与 Display-1 共用显示模块 ID，通过子类型区分）
- **模块类型编号**：`0x01`
- **子类型编号**：`0x02`（E-ink）

## 硬件引脚分配

### SPI1（墨水屏数据接口）

| 功能 | 引脚 | 说明 |
|------|------|------|
| CS# | PA4 | 片选（GPIO 软件控制，低有效） |
| SCL | PA5 | SPI1 时钟（AF_PP，9MHz） |
| MISO | PA6 | SPI1 输入（浮空，读寄存器使用） |
| SDA | PA7 | SPI1 输出（MOSI，AF_PP） |

### 控制引脚（GPIO）

| 功能 | 引脚 | 方向 | 说明 |
|------|------|------|------|
| RES# | PB3 | 输出 | 硬件复位，低有效 |
| D/C# | PB4 | 输出 | 数据/命令选择，低=命令，高=数据 |
| BUSY_N | PB5 | 输入（上拉） | 忙指示，低=忙碌，高=就绪 |

### UART1（Core 通信）

| 功能 | 引脚 | 说明 |
|------|------|------|
| TX | PA9 | 发送（连接 Core UART4 RX） |
| RX | PA10 | 接收（连接 Core UART4 TX） |

波特率 921600，8-N-1，协议遵循 `Protocol_Display.md`。

### UART2（DEBUG）

| 功能 | 引脚 | 说明 |
|------|------|------|
| TX | PA2 | 调试串口发送 |
| RX | PA3 | 调试串口接收 |

波特率 921600，用于 printf 调试输出。

### TTP229 触摸矩阵接口

| 功能 | 引脚 | 方向 | 说明 |
|------|------|------|------|
| SCL1 | PB8 | 输出 | TTP229-1 串行时钟 |
| SDO1 | PB9 | 输入 | TTP229-1 串行数据 |
| SCL2 | PB10 | 输出 | TTP229-2 串行时钟 |
| SDO2 | PB11 | 输入 | TTP229-2 串行数据 |

- TTP229-1 驱动 TP0~TP15，TTP229-2 驱动 TP16~TP31
- 两片组合成 4（列）× 8（行）= 32 点的触摸矩阵
- 通过 TTP229 串行模式读取每片 16bit 的按键状态

### TIM2（系统时基）

TIM2 配置为 1kHz 自由运行更新中断（`platform_time.c`），为 MiniUI 提供不受 WCH
Delay 函数干扰的全局毫秒时基（手势计时、动画 dt、自动息屏、光标节流共用）。

## 触摸矩阵

### 概述

Display-2 在墨水屏表面覆盖 4×8 触摸矩阵，用于模拟触摸板操作。矩阵由两片 TTP229-BSF 触摸芯片驱动，每片管理 16 个通道，通过 SCL/SDO 串行接口回读 32 个触摸焊盘的按下/释放状态。

### 物理布局

触摸矩阵为 8 行 4 列，按“从左到右、从上到下”排列的焊盘编号如下：

| 行 | 列 0 | 列 1 | 列 2 | 列 3 |
|----|------|------|------|------|
| 0  | TP0  | TP1  | TP2  | TP3  |
| 1  | TP12 | TP13 | TP14 | TP15 |
| 2  | TP7  | TP6  | TP5  | TP4  |
| 3  | TP11 | TP10 | TP9  | TP8  |
| 4  | TP16 | TP17 | TP18 | TP19 |
| 5  | TP28 | TP29 | TP30 | TP31 |
| 6  | TP23 | TP22 | TP21 | TP20 |
| 7  | TP27 | TP26 | TP25 | TP24 |

> 注：焊盘编号与 TTP229 芯片的电气通道号一致，物理位置并非完全顺序排列，由硬件走线决定。

### 坐标映射（相对位移触控板模式）

触摸矩阵分辨率仅为 4×8，直接映射到 648×480 屏幕会导致指针跳跃、无法精细定位。Display-2 采用**触摸板相对位移模式**作为主工作方式，类似笔记本触控板：

- 将 8×4 网格视为一块小型触控板，手指在网格上移动产生**相对位移**（定点`*10`精度质心）
- 主循环约 100Hz 采样，行列差乘以灵敏度系数得到屏幕指针位移：
  - `SENSITIVITY_X = 50` 像素/格，`SENSITIVITY_Y = 40` 像素/格（`touch_matrix.h`）
- 指针位置在屏幕边界内裁剪；手指离开后再放下，以上一次指针位置为基准继续追踪

**绝对坐标模式**：未实现（预留）。

### TTP229 数据读取

驱动以固定周期读取两片 TTP229 的状态：

1. 等待 SDO 变高（Data Valid），超时则沿用缓存值
2. 依次产生 17 个时钟脉冲（D0=padding，D1..D16=TP0..TP15）
3. 得到 16bit 原始数据（`raw[15]=TP0 ... raw[0]=TP15`），低电平有效
4. 上电校准基线，`touched = (~raw) & baseline`，合并两片得到 32bit 触摸位图

### 手势模型（V2.0 实际实现）

| 操作 | 判定 | 产生事件 |
|------|------|----------|
| 轻触（tap） | 按下→释放，指针累计位移 < 15px | 在当前指针位置注入 TOUCH_DOWN+TOUCH_UP → 合成 CLICK（300ms/20px 窗口内再次轻触 → DOUBLE_CLICK） |
| 拖动/移动 | 持续移动 | 只移动光标，不产生控件事件（不会误触发滑动翻页） |
| 长按/滑动 | 触控板模式不产生 | LONG_PRESS 保留给鼠标/绝对触摸路径；SWIPE 由 Core 侧键盘(PageUp/Down)/BLE 注入 |

**光标刷新节流**（V2.0）：光标移动时旧+新位置合并为一个包围盒脏区；拖动期间光标更新合并限流为每 350ms 一次局部刷新，抬手立即刷新。任何非光标脏区（真实 UI 变化）不受限流影响。

## 墨水屏规格（ZJY648480-0583AAAMFGN）

| 参数 | 值 |
|------|-----|
| 尺寸 | 5.83 英寸 |
| 分辨率 | 648(H) × 480(V) 像素 |
| 有效显示区域 | 118.78 × 88.22 mm |
| 像素间距 | 0.1833 × 0.1833 mm |
| 显示颜色 | 黑白双色（1bpp） |
| 对比度 | ≥8:1 |
| 工作温度 | 0°C ~ +50°C |
| 驱动IC | JD79686AB |
| SPI接口 | 4线SPI（CPOL=0, CPHA=0, MSB first） |
| SPI最大时钟 | 10MHz（写），4.3MHz（读） |

## 软件架构

### 驱动层次

```
┌─────────────────────────────────────────┐
│  main.c（应用层）                        │
├─────────────────────────────────────────┤
│  MiniUI（页面栈/控件/1bpp 合成渲染）      │
├─────────────────────────────────────────┤
│  epaper.c / epaper.h（E-paper 驱动层）  │
│  - 初始化、温度自适应 LUT 构建           │
│  - 过渡波形 FAST/CLEAR 双套 LUT         │
│  - 全屏刷新（DTM1=OLD + DTM2=NEW + DRF）│
│  - 局部刷新（PDTM1/PDTM2 + PDRF+DFV_EN）│
│  - 深度睡眠 / 唤醒                       │
├─────────────────────────────────────────┤
│  epaper_hw.c / epaper_hw.h（硬件抽象层） │
│  - 硬件 SPI1 @9MHz + DMA1 Ch3（默认）   │
│  - 软件位敲（EPD_USE_HW_SPI=0 回退）    │
│  - GPIO 控制（CS/DC/RES/BUSY）          │
└─────────────────────────────────────────┘
```

### 像素格式

- **1bpp**（1 bit per pixel）
- 每字节表示 8 个水平像素，MSB 为最左侧像素
- **bit=1 → 黑色，bit=0 → 白色**（面板与 MiniUI 全程一致，无需反相）
- 每行 81 字节（648 / 8），全帧 38,880 字节

### LUT 波形（V2.0 过渡模式，BWR=1）

JD79686AB 在 B/W 模式下按每个像素的 (New, Old) 对选择 LUT（CDI DDX=00）：

| (New,Old) | 00 (W→W) | 01 (B→W) | 10 (W→B) | 11 (B→B) |
|-----------|----------|----------|----------|----------|
| LUT | LUTWW | LUTBW | LUTWB | LUTBB |

- **FAST LUT**（局刷）：全功率驱动，区域内所有像素（过渡与未变化）都获得 15 帧 @50Hz 满驱动（约 300ms）——W→W/B→W 用 VSL、W→B/B→B 用 VSH。每次刷新完全重建像素状态，回弹误差无法叠加（3 帧短维持脉冲已实测会导致残影叠加，不可用）
- **CLEAR LUT**（全刷/定期清除）：三相——相 0 全屏驱黑抖动（12 帧，震松颗粒滞回，消除"上一页残影回弹"），相 1 全屏驱白擦除（20 帧），相 2 驱动至目标态（20 帧）。代价：全刷时可见 黑闪→白闪→图像 过程（商用电子书同款）
- **DFV_EN=0**（默认）：未变化像素跟随数据 LUT 获得满驱动；`EPD_DFV_EN=1` 为纯差分模式（New==Old 只跟随 VCOM，不驱动）
- **大面积脏区走全刷**：脏区 ≥75% 屏幕时页管理器自动改用 CLEAR 全刷（页面切换干脆白，不会整屏发灰）
- **温度补偿**：初始化时读取片内温度（R40H TSC），低于 `LUT_COLD_THRESHOLD_C`（10°C）时帧数×2。**注意：本模组 TSC 读回恒为 0x00（MISO 读取不可用），自动模式会误触发，固件默认 `scale=1` 强制一倍帧数**；低温环境请用 CLI `scale 2` 手动切换，并按 datasheet §2.5.3-B 关注 VGL Hi-Z 结尾
- **VCOM 已调定**：默认 `s_vdcs = 0x00`（本机实测反色残影最轻）；现场微调用 CLI `vdcs`
- 每 `PARTIAL_REFRESH_CLEAR_INTERVAL`（50）次局刷自动插入一次 CLEAR 全刷
- 每次刷新后发送 POF 使 VCOM 放电到 GND（抗整屏渐灰）；`EPD_POF_AFTER_REFRESH=0` 可做 A/B 对照调试
- **启动深度清除**：上电后连续执行两次 CLEAR 全刷，消除断电残留影像
- 回退：`epaper.c` 中 `EPD_BW_TRANSITION=0` 可恢复旧的绝对（BWR=0）单相 LUT

### 刷新流程

全屏刷新：
```
DTM1(0x10) 写 OLD → DTM2(0x13) 写 NEW → PON(0x04) → CLEAR LUT → DRF(0x12)
  → 等 BUSY（约0.7秒）→ FAST LUT 恢复 → POF(0x02)
```

局部刷新：
```
PON（若已 POF）→ PDTM1(0x14) 写区域 OLD → PDTM2(0x15) 写区域 NEW
  → PDRF(0x16)（首字节 DFV_EN=0）→ 等 BUSY（约0.3秒）→ POF(0x02)
```

约束：X 和 W 必须为 8 的倍数（页管理器在 flush 前自动对齐）。

### 初始化序列

1. Hardware Reset（RES# 拉低 20ms → 拉高 → 等待 BUSY_N）
2. TDY 命令序列（F8 + 参数对；低温时 TDY 0x92=0x00 切 VGL Hi-Z）
3. 读取内部温度（R40H），确定帧数缩放
4. 用户命令（PSR=0xBF(BWR=1)/PWR/BTST/OSC/CDI/TRES/VDCS/PWS/TCON）
5. 运行时构建并写入 FAST LUT（LUTC/LUTWW/LUTBW/LUTWB/LUTBB，各 42 字节）

## MiniUI（1bpp 适配）

- **渲染**：1bpp 行缓冲合成（8 行一批）→ 全帧缓冲；双帧缓冲（new/old，共 77.8KB）支撑差分 OLD 数据
- **页面**：页面栈（深 16）、脏区域列表（64 区，重叠合并）、flush 前 X/W 对齐 8
- **输入**：触摸/鼠标/键盘统一事件；手势合成 CLICK/DOUBLE_CLICK(300ms/20px)/LONG_PRESS(500ms)/SWIPE(60px)；HID 键盘全字符映射（含 Shift 与标点），方向键/翻页键/回车/Esc 直达页面事件
- **控件**：Label、Button、Icon Button、Slider、Switch、Progress、List Item、Card、TabView、Status Dot、Dialog
- **时基**：TIM2 1kHz（`platform_time.c`），`ui_get_real_ms()` 经由此源

### 应用现状（V2.0）

| 应用 | 状态 | 说明 |
|------|------|------|
| Music | ✅ 完整移植 | 播放/暂停/上下曲/播放列表/音量/扬声器开关，`playst` 同步 |
| Files | ✅ 完整移植 | 目录浏览/面包屑/上下文菜单(Menu 键)/新建/删除/属性/设备切换 |
| Editor | ✅ 完整移植 | 4KB 编辑缓冲、32 级 undo、行号、光标、`write -s/-a/-e` 分帧保存 |
| Images | ✅ 完整移植 | \BMP 列表、1/8/24bpp BMP 解码（零拷贝解析 CLI 缓冲）、缩放+平移 |
| EBook | ✅ 完整移植 | \BOOK 列表、txt/md/json 渲染、明/暗主题、3 档字号、分页 |
| USB/Power/BT/NFC/Finger/Health/SubDisp/RGB/IRRange/EMusic/Terminal | ⏳ Stub | 标题栏 + 提示页，保留入口 |

RAM 纪律：CLI 应答缓冲 8KB（EBook/Images 零拷贝解析源）；Editor 4KB 编辑缓冲；Music 播放列表 32×64B；Files 条目 32 项；无应用持有 >10KB 私有缓冲。

### 游戏现状（V2.0）

仅保留 2048 与 Minesweeper（回合制、对局刷友好）；Tetris/Snake/Breakout/Airplane/TouchBall/Contra 已删除（释放约 33KB FLASH）。

- 2048：D-pad 按钮操作（触控板可玩）
- Minesweeper：挖/旗工具切换按钮 + 单击操作（不依赖双击/长按）

## 项目目录结构

```
display_2/
├── Common/
│   └── Common/
│       ├── Apps/                      # 应用（music/file/editor/images/ebook/stub/apps）
│       ├── Eink/                      # 墨水屏驱动（epaper + epaper_hw）
│       ├── Games/                     # 游戏（2048、minesweeper）
│       ├── MiniUI/                    # MiniUI 框架（render/page/widget/input + font）
│       │   └── ui_system.c            # 系统初始化与主循环
│       ├── Touch/                     # TTP229 触摸矩阵 + 光标管理
│       ├── UART/                      # UART1 协议栈（含 CLI 直通、ls 解析）
│       ├── UI/                        # 主页面（main/home/apps/games/models/settings/app_common）
│       ├── platform_time.c/h          # TIM2 1kHz 毫秒时基
│       └── settings.c/h               # 全局设置
├── Core/                              # RISC-V 内核文件
├── Debug/                             # 调试支持（printf over UART2）
├── Peripheral/                        # CH32V30x 外设库
├── Startup/                           # 启动文件
├── User/                              # main.c / 系统配置
├── Ld/
│   └── Link.ld                        # 链接脚本（FLASH-192K + RAM-128K）
└── Epaper.wvproj                      # MounRiver Studio 工程文件
```

## 命名规范

- **文件**：统一采用下划线加小写字母，例如 `epaper_hw.c`、`app_file.c`
- **函数**：每个单词首字母大写加下划线，例如 `Epaper_Init()`、`Platform_Millis()`
- **变量**：统一小写加下划线，例如 `g_frame_new`、`s_panel_asleep`
- **宏**：全大写加下划线，例如 `EPD_WIDTH`、`FAST_LUT_FRAMES`
- **文件夹**：下划线写法，仅首字母大写，例如 `MiniUI`、`Apps`
- **系统目录**（`Core`、`Debug`、`Peripheral`、`Startup`）保持不动

## 内存约束

- **链接脚本**：FLASH-192K + RAM-128K（`Ld/Link.ld`）
- **RAM 大头**：双帧缓冲 2 × 38,880B = 77.8KB（局刷 OLD 数据源，支撑 DFV_EN 差分刷新）
- **FLASH 占用**（V2.0 移植后估算）：约 110~130KB / 192KB
- **RAM 占用**（V2.0 移植后估算）：约 115~125KB / 128KB，应用共享/零拷贝设计见上

## 当前实现状态

| 功能 | 状态 | 说明 |
|------|------|------|
| 硬件 SPI1 + DMA 传输 | ✅ 已完成 | 9MHz，块写 DMA；软件位敲保留回退 |
| 初始化寄存器序列 | ✅ 已完成 | PSR=0xBF（BWR=1），TDY 全量 |
| 过渡 LUT（FAST/CLEAR） | ✅ 已完成 | 运行时构建 + 维持脉冲 + 温度缩放（<10°C ×2） |
| 局刷波形策略 | ✅ 已完成 | DFV_EN=0 维持脉冲保白底；大面积脏区自动走 CLEAR 全刷 |
| 全屏/局部刷新 | ✅ 已完成 | POF 保留抗渐灰；每 50 局刷自动 CLEAR；启动双清 |
| 深度睡眠/唤醒 | ✅ 已完成 | 息屏/自动息屏进 DSLP，活动唤醒重绘 |
| 温度读取与补偿 | ✅ 已完成 | R40H TSC，帧数缩放 + VGL Hi-Z 结尾 |
| TIM2 毫秒时基 | ✅ 已完成 | 手势/dt/息屏/节流共用 |
| UART1 Core 协议栈 | ✅ 已完成 | 含心跳 ACK、SHOW_PAGE、电源/模块事件 |
| CLI 直通 | ✅ 已完成 | SOF/EOF 多帧重组，8KB 缓冲 |
| MiniUI 框架 | ✅ 已完成 | 1bpp 合成渲染、页面栈、全控件 |
| 触摸矩阵 | ✅ 已完成 | 相对位移 + 光标节流（350ms） |
| 键盘字符输入 | ✅ 已完成 | HID 全字符映射（编辑/对话框可用） |
| 应用 | 🟡 5/16 | Music/Files/Editor/Images/EBook 完成，其余 stub |
| 游戏 | ✅ 已完成 | 2048 + Minesweeper（挖/旗模式） |
| CJK 字库 | ❌ 未实现 | 仅 ASCII（Montserrat 12/16 + 图标） |
| 刷新异步化 | ❌ 未实现 | 刷新期主循环阻塞（后续可做状态机） |

## 后续开发计划

1. **应用补齐**：Power/BT/Health/Finger 等 stub 应用按优先级实现
2. **刷新异步化**：PON/PDRF/POF 状态机，刷新期间继续扫描触摸
3. **局刷延迟再优化**：评估过渡 LUT 下取消每刷 POF（需硬件验证）
4. **低温波形精调**：<10°C 实际模组参数（帧数/电压）硬件标定
5. **CJK 阅读**：压缩子集字库 + UTF-8 解码（EBook 中文书籍）
6. **MCU 低功耗**：空闲期降频/WFI（当前已做面板 DSLP）

## 开发要点

1. **时基唯一来源**：所有毫秒计时必须走 `Platform_Millis()` / `ui_get_real_ms()`；SysTick 被 WCH Delay 函数独占，禁止直接读取。
2. **传输切换**：`epaper_hw.h` 的 `EPD_USE_HW_SPI`（默认 1=硬件 SPI1+DMA；0=软件位敲回退）。若 9MHz 下出现花屏/丢数据，先置 0 验证信号完整性，再排查时序。
3. **波形切换**：`epaper.c` 的 `EPD_BW_TRANSITION`（默认 1=过渡模式；0=旧绝对模式）。局刷异常（残影突然加重/不刷新）可对切验证。
4. **LUT 在线调参（UART2 调试 CLI，921600）**：连接调试串口，输入 `help` 查看命令。所有参数立即生效、无需重新烧录：
   - `dump`：查看当前参数（vdcs/温度/各阶段帧数/dfv/pof）
   - `vdcs <v>`：VCOM 电压（R82H，7bit，典型范围 0x1D~0x3C，当前 0x2C）
   - `fast <n>` / `maint <n>`：局刷驱动帧数 / 维持帧数
   - `clear <s> <e> <d>`：CLEAR 三相帧数（抖动/驱白/驱动）
   - `dfv <0|1>`：差分刷新开关；`pof <0|1>`：每刷 POF 开关
   - `scale <0|1|2>`：帧数缩放——auto（按 TSC 温度）/ 强制 x1 / 强制 x2（TSC 读数异常时用强制档）
   - `temp`：重新读取面板温度
   - `tdy <aa> <vv>`：写 TDY 寄存器（hex）
   - `refresh`：立即对当前页做一次 CLEAR 全刷
5. **反色残影排查流程**（实测顺序）：
   1. `refresh` 若干次确认 CLEAR 本身能否擦净（擦不净 → 加大 `clear` 抖动/驱白帧数）
   2. `vdcs` 以 0x00 为基准（本机已调定，残影最轻），漂移时每次 ±2 步进微调——反色残影本质是 VCOM 偏移导致的净 DC 漂移
   3. 仍有局部残影：加 `clear` 抖动相（12→20）或驱白相（20→30）
   4. 白底整体偏灰：检查 `maint` 是否等于 `fast`（不等会导致未变化像素驱动不足）
   5. TSC 在本模组读回恒 0（不可用）：帧数缩放固定 `scale 1`，不要指望 auto 模式
6. **RAM 红线**：新增应用前先算静态缓冲；内容类数据优先零拷贝复用 CLI 缓冲（8KB），可写缓冲参考 Editor 的 4KB 上限。
7. **触控板手势约束**：不产出长按/滑动的连续事件；新 UI 交互以“单击 + 工具栏按钮”为主，扫雷的挖/旗切换按钮是范式。
8. **与 Display-1 的协议兼容性**：两者共用 `0x10` 模块 ID 和同一套 Display 协议，Core 侧通过 `CMD_GET_TYPE` 的子类型（`0x01` LCD / `0x02` E-ink）区分；页面切换编号 0~4 对应侧边栏五项。
