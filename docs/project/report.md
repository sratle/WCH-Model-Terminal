# 摘要

本作品采用青稞RISC-V架构的CH32H417芯片作为核心芯片，旨在设计一台模块化的个人移动终端，满足用户对移动设备的多样化需求。该移动终端主要分成核心模块、供电模块、显示模块、配件模块、键盘模块五个部分，支持各个部分的独立更换和定制，提供灵活的硬件扩展能力和定制化体验。模块可在核心底板上自由组合，终端自动识别其组成，最终实现文本编辑器、电子书、播放器、电子琴、游戏机等多样功能，替代多种单一功能终端。

## 功能与特性

本作品可以通过模块的自由组合实现不同的功能。单独使用核心模块可以使用Type-C线供电，也可以使用供电模块供电，单独使用的情况下，可以采用手机APP利用蓝牙和核心模块进行可视化交互，主要实现文件系统访问、音乐播放、文字编辑等功能，也可以在手机APP上通过命令行进行交互，设计了大量CLI命令供使用。安装屏幕的情况下，可以通过触摸的方式和系统进行交互，设计了许多应用和数个小游戏可供使用，也可以通过UI的方式进行设置配置。键盘输入可以使用符合HID标准的键盘通过USB输入，也可以使用本作品设计的三种键盘键盘进行输入，满足不同的输入需求，标准键盘和游戏键盘同时支持HID输出，可以单独给其他设备使用，高度开放、可扩展。供电模块除了安装在核心模块上可以进行供电，也可以单独拿出来当充电宝使用，便于携带，支持PD和无线充电。本作品拥有三个配件槽位，支持使用设计中的七种配件给作品做功能扩展，以实现不同的独特功能。

## 应用领域

本作品应用领域较为广泛，主要目标是构建一种不同的移动终端形态，实现便携式播放器、掌上游戏机、电子书、文本编辑器、电子琴的融合。主要面向人群是喜爱模块化设计终端的电子爱好者和希望将手上不同的小型设备进行融合的日常使用者，也可以供嵌入式学习者进行研究、复刻、扩展，模块化程度高便于爱好者设计自己的模块加入其中。作为模块化的移动终端，适合用户在不同情境下使用不同的模块组合出自己想要的设备，也适合在某个模块损坏的时候进行替换。

## 主要技术特点

1. 模块化。本作品最大的特点就是模块化，高度的模块化使得作品高度可定制并且可替换，在设计实现的过程中也详细地考虑到了不同场景下的可用性，不强制用户将所有模块都安装，支持不同组合形态，也支持在缺失某些模块的情况下使用，以满足不同的便携性需求，甚至也支持外部输入和向外输出，某些模块也支持单独进行使用。模块支持热插拔，核心模块会自动识别当前的模块组成。
2. 磁吸式连接。本作品采用磁吸式连接，用户可以将不同的模块通过磁吸式连接到核心模块上，实现模块的自由组合和扩展。
3. 沁恒方案。本作品采用大量沁恒芯片，包括CH32H417、CH32V307、CH32V103、CH585、CH378、CH9350、CH9329，利用沁恒芯片的多样性实现不同的功能。

## 主要性能指标

1. 续航能力。使用供电模块的情况下，根据不同的使用情况，拥有2-20h的续航能力。
2. 性能。本作品采用分布式计算设计，每个模块都拥有各自的计算能力，可以进行独立的计算和处理，使得作品的流畅度和响应速度都得到了提升。
3. 便携性。本作品通过3D打印的方式为所有模块建立了外壳，保持了模块的独立性，同时也方便了用户的携带和使用。

## 主要创新点

1. 模块化设计。本作品创新性地分离了供电、屏幕、键盘、配件，使得用户可以根据自己的需求选择不同的模块组合，实现不同的功能。
2. 分布式设计。各模块之间采用串口进行通信，由核心模块进行统筹协调，实现了模块之间的通信和数据交换。同时赋予部分模块独立运行的能力。
3. 自建UI系统。本作品为了实现LCD和墨水屏之间的统一，设计了一套UI系统，实现了在不同屏幕上的统一操作，输入系统支持多种输入方式，渲染系统高度支持局部刷新。

## 设计流程

1. 构思。根据模块化移动终端的需求，分离出核心模块、供电模块、显示模块、配件模块、键盘模块五个部分。
2. 系统设计。根据五个模块，设计连接方式和作品形态，考虑了模块的独立性和可组合性，设计了要实现的模块种类。
3. 详细设计。根据系统设计，对每个模块进行硬件设计、外壳设计、软件设计，实现硬件软件外壳之间的协调。

# 系统组成及功能说明

## 整体介绍

系统整体组成如下：

1. 核心模块。核心模块是系统的核心，承载了系统的整体调度。核心模块本身拥有主控、文件系统、供电系统、音频系统、蓝牙系统、输入系统。核心模块向外拥有一个屏幕插槽、一个供电插槽、一个键盘插槽、三个配件插槽，均采用pogopin磁吸连接，各个子模块通过接口连接到核心模块上实现组合。
2. 供电模块。供电模块是系统供电的来源，采用Type-C线PD快充，同时支持向外PD快充和无线放电，通过pogopin连接到核心模块上。
3. 显示模块。显示模块有两种，一种是LCD显示模块，一种是Eink显示模块，通过统一的屏幕插槽pogopin连接到核心模块上。
4. 键盘模块。设计有三种键盘模块：主键盘、游戏键盘、音乐键盘。采用统一的pogopin连接到核心模块的键盘插槽上。
5. 配件模块。设计有七种配件模块：指纹模块、健康监测模块、NFC模块、触摸模块、RGB模块、测距模块、副屏模块。均采用统一的pogopin连接到核心模块的配件插槽上，个别模块拥有不同的大小，支持自由组合。

```mermaid
graph TB
    subgraph 核心模块内部组成
        main[CH32H417]
        fs[文件系统]
        ps[供电系统]
        audio[音频系统]
        bt[蓝牙系统]
        input[输入系统]
    end

    main --> fs
    main --> ps
    main --> audio
    main --> bt
    main --> input
    fs -.-> main
    ps -.-> main
    audio -.-> main
    bt -.-> main
    input -.-> main

    style main fill:#f9f,stroke:#333,stroke-width:2px
```

```mermaid
graph LR
    core[核心模块]
    pm[供电模块]
    screen_slot[屏幕插槽]
    power_slot[供电插槽]
    kb_slot[键盘插槽]
    acc1[配件插槽 1]
    acc2[配件插槽 2]
    acc3[配件插槽 3]
    lcd[LCD显示模块]
    eink[Eink显示模块]
    main_kb[主键盘]
    game_kb[游戏键盘]
    music_kb[音乐键盘]
    nfc[NFC模块]
    fp[指纹模块]
    health[健康监测模块]
    touch[触摸模块]
    rgb[RGB模块]
    distance[测距模块]
    sec_screen[副屏模块]

    core <-->|一个| screen_slot
    core <-->|一个| power_slot
    core <-->|一个| kb_slot
    core <-->|三个| acc1
    core <-->|三个| acc2
    core <-->|三个| acc3

    screen_slot <-->|二选一| lcd
    screen_slot <-->|二选一| eink
    power_slot <-->|pogopin磁吸| pm
    kb_slot <-->|三种之一| main_kb
    kb_slot <-->|三种之一| game_kb
    kb_slot <-->|三种之一| music_kb
    acc1 <-->|七种之一| nfc
    acc1 <-->|七种之一| fp
    acc1 <-->|七种之一| health
    acc1 <-->|七种之一| touch
    acc1 <-->|七种之一| rgb
    acc1 <-->|七种之一| distance
    acc1 <-->|七种之一| sec_screen

    style core fill:#f9f,stroke:#333,stroke-width:2px
```

## 软件系统介绍

### 软件整体介绍

软件上主要采用分布式设计，各个模块都有自己的主控，通过串口和核心模块的CH32H417进行通信，由核心模块进行协调和数据交换，模块间定义了完整的通信协议。主要计算在模块内部发生，传回核心模块的通常是计算结果和指令，再由核心模块进行转发和命令执行。此外有手机APP作为蓝牙连接核心模块的操作入口。支持命令行传输命令，也支持以Type-C连接核心模块进行串口命令行发送命令。

```mermaid
graph LR
    core[核心模块<br/>CH32H417]

    subgraph 板上外设
        direction TB
        audio[音频 CS43131 / I2S]
        file[文件 CH378 / SPI]
        bt[蓝牙 CH585F / SPI]
        hid[HID CH9350 / UART1]
        key[核心按键 / GPIO]
    end

    subgraph 外部模块插槽
        direction TB
        power[供电模块 / UART5]
        screen[显示插槽 / UART4]
        kb[键盘插槽 / UART3]
        acc1[配件插槽1 / UART6]
        acc2[配件插槽2 / UART7]
        acc3[配件插槽3 / UART8]
    end

    subgraph 显示模块二选一
        direction TB
        lcd[LCD显示模块]
        eink[Eink显示模块]
    end

    core <--> audio
    core <--> file
    core <--> bt
    core <--> hid
    core <--> key

    core <--> power
    core <--> screen
    core <--> kb
    core <--> acc1
    core <--> acc2
    core <--> acc3

    screen <--> lcd
    screen <--> eink

    style core fill:#f9f,stroke:#333,stroke-width:2px
```

### 软件各模块介绍

#### 核心模块

核心模块的软件基于沁恒 CH32H417 双核 RISC-V 微控制器构建，所有业务逻辑统一运行在 V5F 核心上，V3F 仅负责上电后初始化时钟并唤醒 V5F，随后进入 STOP 低功耗模式。V5F 以裸机主循环的方式调度音频 DAC、文件管理芯片、蓝牙无线芯片、显示模块、键盘模块、供电模块、配件模块以及 HID 转串口等所有外设。初始化阶段依次完成 CS43131 音频 DAC、CH378 文件系统、配置系统、命令行接口、CH585F 蓝牙、显示模块、核心按键、供电模块、CH9350 HID 转换、键盘模块和三个配件槽位的初始化，并建立心跳槽位用于模块在线检测。主循环每毫秒执行一次，依次处理显示刷新、音频数据补充、命令行交互、蓝牙轮询、按键扫描、键盘协议、供电协议、三个配件槽协议和 HID 输入，最后通过硬件心跳统一检测各模块状态。核心模块内部维护全局状态结构体 hardware_g，集中管理所有模块的运行状态、配置信息和 RGB 自定义帧等共享数据，整个调度过程不依赖 RTOS，完全通过轮询加中断实现。

核心模块的音频引擎基于 CS43131 构建了两通道虚拟混音系统。文件播放时通过 CH378 以时分复用的方式从 TF 卡或 U 盘读取 WAV 数据，为每个通道维护 16KB 的环形缓冲，DMA 双缓冲以 I2S 输出到 DAC。该引擎支持全局音量、独立通道音量与声像控制，并内置三段 EQ、压缩器和 Echo 效果器链，在 DMA 中断中按 EQ、压缩器、Echo 的顺序实时处理。文件系统方面，核心模块通过 CH378 管理 TF 卡和 USB 存储设备，音频播放期间 CH378 不会被独占，其他模块仍可通过显式锁机制进行文件读写。蓝牙系统通过 SPI 与 CH585F 通信，负责与手机 APP 的数据交换和可视化交互。HID 输入通过 CH9350 接收标准 USB 键盘和鼠标事件，并转换为系统输入。配置系统采用 JSON 持久化，保存用户设置并在启动时加载。命令行接口提供大量音频控制、文件浏览和系统调试命令，允许用户在不接屏幕的情况下通过串口与设备交互。

**关键函数执行流程**

```mermaid
flowchart TD
    A[main] --> B[系统时钟/延时/串口初始化]
    B --> C[Hardware_Init]
    C --> D[CH585F_BT_Init]
    D --> E[启用 CLI 接收中断]
    E --> F[while 1ms 主循环]
    F --> G[Display_Process]
    G --> H[Audio_Process]
    H --> I[Debug_CLI_Process]
    I --> J[CH585F_BT_Poll]
    J --> K[Key_PollAndProcess]
    K --> L[Keyboard_Process]
    L --> M[Power_Process]
    M --> N[Submodels_Process x3]
    N --> O[CH9350_Process]
    O --> P[Hardware_Heartbeat]
    P --> Q[Delay_Ms 1]
    Q --> F
```

**关键函数输入输出变量**

关键函数 `main()`（`main-model/core/V5F/User/main.c`）的输入输出变量如下：输入/输出变量包括 `display_g`（`display_t`，显示模块实例）、`keyboard_g`（`keyboard_t`，键盘模块实例）、`power_g`（`power_t`，供电模块实例）、`submodels_g[3]`（`submodels_t[3]`，三个子模块槽位）、`ch9350_g`（`ch9350_t`，CH9350 HID 转串口实例）、`CS43131_g`（`cs43131_t`，音频 DAC 实例）、`ch378_g`（`ch378_t`，文件管理芯片实例）、`ch585f_g`（`ch585f_t`，蓝牙无线芯片实例）和 `hardware_g`（`hardware_t`，全局硬件状态及心跳槽位）；输入变量包括 `SystemCoreClock`（`uint32_t`，系统时钟频率）。

#### 供电模块

供电模块运行在 CH32V103C8T6 上，其电源管理核心为 IP5568 芯片。IP5568 独立完成双向 PD 快充、无线充电、电池充放电管理以及 5V 稳压输出，并通过七段 LED 实时显示电池电量。CH32V103C8T6 不直接控制电源路径或充电协议，而是监听并解析 IP5568 驱动的七段 LED 刷新时序，从中提取当前电池电量百分比。模块上电后初始化时钟、GPIO 和 UART1，以 921600 波特率与核心模块建立通信，响应心跳和类型查询，并周期性上报电量信息。当供电模块通过 Type-C 接入电源或作为充电宝对外输出时，所有快充握手、功率协商和保护逻辑均由 IP5568 硬件完成，CH32V103C8T6 仅负责将电量状态透明转发给核心模块。核心模块根据电量决定降频、关闭背光或暂停非必要模块等策略，供电模块本身不做功耗决策。

**关键函数执行流程**

```mermaid
flowchart TD
    A[main] --> B[初始化时钟/GPIO/UART1]
    B --> C[配置定时器捕获 IP5568 七段 LED 时序]
    C --> D[以 921600 波特率连接核心模块]
    D --> E[while 1 主循环]
    E --> F{收到心跳/类型查询?}
    F -->|是| G[回复 ACK 与模块类型]
    G --> E
    F -->|否| H{检测到 LED 刷新时序?}
    H -->|是| I[解析七段 LED 段码]
    I --> J[计算当前电池百分比]
    J --> K[更新 power_g 电量状态]
    K --> E
    H -->|否| L{电量上报周期到?}
    L -->|是| M[通过 UART1 发送电量数据帧]
    M --> E
    L -->|否| E
```

**关键函数输入输出变量**

关键函数 `main()`（`power-model/power-1/User/main.c`）的输入输出变量如下：输入/输出变量包括 `report_timer`（`uint32_t`，电量主动上报定时计数）；输入变量包括 `led_segment_buf[]`（`uint8_t[]`，七段 LED 段码采样缓冲）、`led_digit_buf[]`（`uint8_t[]`，数码位选时序缓冲）和 `uart_rx_frame`（`protocol_frame_t`，核心模块下发的查询帧）；输出变量包括 `battery_percent`（`uint8_t`，解析后的电池电量百分比）、`power_status`（`power_status_t`，充电/放电/待机状态）和 `power_g`（`power_t`，供电模块状态结构体（核心侧可见））。

#### LCD显示模块

LCD 显示模块同样采用 CH32H417 双核架构，V5F 负责 UI 渲染和屏幕驱动，V3F 负责与核心模块的 UART 通信。V5F 初始化 FMC 8080 总线和 SSD1963 驱动芯片，加载自研 MiniUI V2.0 框架，维护深度为 16 的页面栈并支持脏区域跟踪局部刷新。V3F 初始化 UART1 协议帧接收状态机，以 921600 波特率接收核心模块下发的页面切换、状态栏更新、通知弹窗和背光调节等指令，并通过 HSEM 与共享内存通知 V5F 执行。与此同时，V3F 也将本地触摸事件、屏幕唤醒事件和异常状态主动上报核心模块。该模块运行本地应用和小游戏，如文件管理、音乐播放器、俄罗斯方块、2048 和贪吃蛇等，这些应用在本地独立完成，不依赖核心模块。MiniUI 框架统一抽象了触摸、鼠标和键盘输入，支持页面过渡动画、控件库和字体渲染，核心模块侧通过显示协议远程驱动副屏上的页面和内容显示。

**关键函数执行流程**

```mermaid
flowchart TD
    A[main] --> B[系统时钟与延时初始化]
    B --> C[Hardware_V5F_Init]
    C --> D[SSD1963_TE_Init]
    D --> E[配置 25 FPS 帧率限制]
    E --> F[while 循环]
    F --> G[Touch_Scan]
    G --> H[UART_Module_Poll]
    H --> I[SSD1963_WaitVSync]
    I --> J[UI_Tick]
    J --> K{当前帧 < 40 ms?}
    K -->|是| L[Delay_Ms 剩余时间]
    K -->|否| M[Delay_Ms 1]
    L --> N[更新 last_frame]
    M --> N
    N --> F
```

**关键函数输入输出变量**

关键函数 `main()`（`display-model/display-1/V5F/User/main.c`）的输入输出变量如下：输入/输出变量包括 `last_frame`（`uint32_t`，上一帧开始时间）；输入变量包括 `now`（`uint32_t`，当前 `__get_UCYCLE()`）；输出变量包括 `g_disp_state`（`disp_state_t`，显示状态（背光、状态栏等））和 `g_settings`（`settings_t`，亮度、自动关屏等设置）；`cycles_per_ms`、`frame_cycles`、`elapsed` 和 `remaining_ms` 为局部计算变量。

#### Eink显示模块

Eink 显示模块运行在 CH32V307RCT6 上，使用 5.83 寸黑白电子墨水屏，与 LCD 显示模块共用同一套通信协议和 MiniUI 框架，但渲染层针对 1bpp 像素格式进行了适配。模块初始化 SPI1 接口与 JD79686AB 驱动芯片通信，配置 BW 模式的 LUT 波形和系统寄存器，建立 UART1 协议接收状态机。墨水屏支持全屏刷新和局部刷新两种模式，全屏刷新约需 4 秒，局部刷新则可在 0.3 到 1 秒内完成，局部刷新时 X 和 W 必须为 8 的倍数，且模块内部维护 4KB 旧数据缓冲区以计算差分更新。由于 32KB RAM 无法缓存全帧，全屏刷新采用流式逐行发送数据。模块上电后默认处于待机状态，响应核心模块的心跳和类型查询，收到显示指令后执行内容渲染与刷新。后续将进一步移植 MiniUI 框架和字库渲染引擎，使墨水屏上的交互体验与 LCD 屏保持一致，并针对低功耗场景优化刷新间隙的 MCU 休眠策略。

**关键函数执行流程**

```mermaid
flowchart TD
    A[main] --> B[ui_system_init]
    B --> C[ui_main_init]
    C --> D[games_init_all]
    D --> E[while 循环 每 50ms]
    E --> F[ui_system_tick]
    F --> G[计算 dt = now - last_tick]
    G --> H[page->on_update]
    H --> I[ui_page_draw]
    I --> J[flush_dirty_region_to_epd]
    J --> K[ui_input_process]
    K --> E
```

**关键函数输入输出变量**

关键函数 `ui_system_tick()`（`display-model/display-2/Common/Common/MiniUI/ui_system.c`）的输入输出变量如下：输入/输出变量包括 `s_last_tick`（`static uint32_t`，上一次 tick 毫秒时间戳）；输入变量包括 `s_initialized`（`static bool`，系统是否已完成初始化）、`now`（`uint32_t`，当前毫秒时间戳）和 `page`（`ui_page_t *`，当前页面指针）；输出变量包括 `g_frame_new` / `g_frame_old`（`uint8_t[]`，新旧 1bpp 帧缓冲）和 `g_disp_state`（`disp_state_t`，显示状态）；`dt` 为内部计算的两次 tick 间隔。

#### 主键盘模块

主键盘模块基于 CH32V103C8T6 实现，采用约 40% 配列的机械按键矩阵，同时具备向核心模块上报和向电脑输出 HID 报告的双通道能力。初始化阶段配置按键矩阵 GPIO、UART1 与核心模块通信、UART2 与 CH9329 串口转 HID 芯片通信，并将 CH9329 设置为标准键盘模式。运行阶段以定时器中断周期扫描矩阵，检测按键按下、释放和长按事件，经软件消抖后将物理行列坐标映射为逻辑键码，支持多层配列和 Fn 组合键。同一事件同时通过 UART1 向核心模块发送自定义协议键码，并通过 UART2 经 CH9329 转换为 USB HID 键盘报告从 USB-A 接口输出。因此即使不插入移动终端，主键盘模块也可以作为独立键盘连接电脑使用。模块还响应核心模块下发的背光调节和布局查询命令，便于核心模块在 UI 上显示虚拟键盘或快捷键提示。

**关键函数执行流程**

```mermaid
flowchart TD
    A[main] --> B[初始化 NVIC/时钟/调试串口]
    B --> C[外设初始化<br>HC165/Key/CH9329/UartCore/TIM2]
    C --> D[while 1 主循环]
    D --> E{g_scan_flag == 1?}
    E -->|否| F[FlushEvents]
    F --> G[CheckProtocolRx]
    G --> D
    E -->|是| H[清 g_scan_flag]
    H --> I[HC165_Read]
    I --> J[Key_Scan]
    J --> K[Key_Process]
    K --> L[SendHIDIfNeeded]
    L --> M{报告变化?}
    M -->|是| N[CH9329_SendKeyboard]
    M -->|是| O[SendHIDToCore]
    N --> P[更新 last_hid_report]
    O --> P
    P --> D
    M -->|否| D
```

**关键函数输入输出变量**

关键函数 `main()`（`keyboard-model/keyboard-1/User/main.c`）的输入输出变量如下：输入/输出变量包括 `last_hid_report[]`（`static uint8_t[]`，上一次 HID 报告，用于变化检测）和 `key_evt_buf[]`（`extern`，按键事件缓冲）；输入变量包括 `g_scan_flag`（`volatile uint8_t`，TIM2 2ms 扫描触发标志）、`hc165_buf[]`（`static uint8_t[]`，74HC165 原始采样数据）和 `uart_core_rx_ctx.frame`（`extern`，接收到的 Core 协议帧）；输出变量包括 `hid_report_buf[]`（`static uint8_t[]`，待打印 HID 报告缓存）、`hid_pending`（`static uint8_t`，是否有 HID 报告需日志打印）、`kbd_backlight_mode`（`static uint8_t`，背光模式，由 Core 命令设置）和 `kbd_backlight_brightness`（`static uint8_t`，背光亮度，由 Core 命令设置）。

#### 游戏键盘模块

游戏键盘模块同样运行在 CH32V103C8T6 上，面向游戏操控场景，采用方向键、双轴模拟摇杆和四个功能键的布局。初始化阶段配置方向键与功能键 GPIO、双轴摇杆 ADC、UART1 与核心模块通信以及 UART2 与 CH9329 的摇杆模式通信。运行阶段定时扫描按键状态，并通过 ADC 采集摇杆 X 和 Y 轴位置，软件对摇杆做死区处理与归一化，避免机械中位漂移造成误操作。按键与摇杆事件同时通过 UART1 上报核心模块，并通过 UART2 经 CH9329 输出标准 HID 游戏控制器报告。游戏键盘与主键盘共用模块 ID，核心模块通过子类型区分两者，并将输入路由到对应的应用或游戏引擎。和主键盘一样，游戏键盘模块也能脱离移动终端单独作为电脑游戏手柄使用。

**关键函数执行流程**

```mermaid
flowchart TD
    A[main] --> B[初始化 NVIC/时钟/延时]
    B --> C[配置方向键与功能键 GPIO]
    C --> D[初始化双轴摇杆 ADC]
    D --> E[初始化 UART1 与 UART2]
    E --> F[将 CH9329 设为游戏手柄模式]
    F --> G[while 1 主循环]
    G --> H{扫描周期到?}
    H -->|否| I[CheckProtocolRx]
    I --> G
    H -->|是| J[扫描方向键与功能键]
    J --> K[ADC 采集摇杆 X/Y 轴]
    K --> L[死区处理与归一化]
    L --> M{状态变化?}
    M -->|是| N[构建 HID 游戏控制器报告]
    N --> O[通过 UART2 发送 HID 报告]
    N --> P[通过 UART1 上报核心模块]
    O --> G
    P --> G
    M -->|否| G
```

**关键函数输入输出变量**

关键函数 `main()`（`keyboard-model/keyboard-2/User/main.c`）的输入输出变量如下：输入/输出变量包括 `button_state[]`（`uint8_t[]`，方向键与功能键状态）；输入变量包括 `g_scan_flag`（`volatile uint8_t`，扫描周期触发标志）、`joystick_raw_x` / `joystick_raw_y`（`uint16_t`，摇杆 ADC 原始值）和 `uart_core_rx_ctx.frame`（`extern`，接收到的 Core 协议帧）；输出变量包括 `joystick_x` / `joystick_y`（`int16_t`，死区处理后的归一化值）和 `hid_report[]`（`uint8_t[]`，HID 游戏控制器报告）。

#### 音乐键盘模块

音乐键盘模块运行在 CH32V103C8T6 上，提供 24 个电容触摸琴键、三个调音推子和 12 个控制按钮，用于核心模块上的电子琴和合成器应用。初始化阶段配置 TTP229 串行接口读取两片触摸芯片的 24 位琴键状态，配置 ADC 采集三路推子，配置 GPIO 读取 12 个按钮，并建立 UART1 通信。由于 TTP229 响应时间为 32 毫秒，琴键与按钮扫描周期设置为 5 到 10 毫秒，推子采样则通过滑动平均滤波消除电位器抖动。模块上电后默认处于待机状态，仅响应心跳和类型查询，核心模块发送开始输出命令后进入事件驱动上报模式，仅在琴键位图、按钮位图或推子值发生变化时通过 UART1 上报。琴键采用标准钢琴布局覆盖两个八度，核心模块可直接根据键编号加 59 计算 MIDI 音符号，并由核心模块侧的 CS43131 完成音频合成与播放。该模块不含 CH9329，因此不能独立接电脑使用，必须与核心模块协同工作。

**关键函数执行流程**

```mermaid
flowchart TD
    A[main] --> B[初始化 NVIC/时钟/延时]
    B --> E[外设初始化<br>TTP229/Button/Fader/UartCore/TIM2]
    E --> F[while 1 主循环]
    F --> G[CheckProtocolRx]
    G --> H{g_scan_flag == 1?}
    H -->|否| F
    H -->|是| I[清 g_scan_flag]
    I --> N[Scan]
    N --> O{event_reporting == 1?}
    O -->|否| F
    O -->|是| P[HasStateChanged]
    P --> Q{状态变化?}
    Q -->|是| R[ReportChangedEvents]
    R --> S[SavePrevState]
    S --> F
    Q -->|否| F
```

**关键函数输入输出变量**

关键函数 `main()`（`keyboard-model/keyboard-3/User/main.c`）的输入输出变量如下：输入/输出变量包括 `event_reporting`（`static uint8_t`，事件上报使能开关）、`prev_key_bitmap[]`（`static uint8_t[3]`，上一次按键位图）、`prev_btn_bitmap[]`（`static uint8_t[2]`，上一次按钮位图）和 `prev_fader_values[]`（`static uint16_t[3]`，上一次推子值）；输入变量包括 `g_scan_flag`（`volatile uint8_t`，TIM2 10ms 扫描触发标志）和 `uart_core_rx_ctx.read_frame`（`extern`，接收到的 Core 协议帧）；输出变量包括 `key_bitmap[]`（`static uint8_t[3]`，24 键触摸状态位图）、`btn_bitmap[]`（`static uint8_t[2]`，12 按钮状态位图）和 `fader_values[]`（`static uint16_t[3]`，3 路推子 16 位值）。

#### 指纹模块

指纹模块运行在 CH32V103C8T6 上，通过 UART2 与支持 Syno 协议的光学或电容式指纹传感器通信，通过 UART1 与核心模块通信。模块上电后初始化指纹传感器，发送握手指令验证通信，读取有效模板数量，并向核心模块上报子类型。默认情况下模块处于自动验证模式，当手指按压传感器时，TOUCHOUT 引脚触发下降沿中断唤醒主控，主控发送自动验证指令，传感器内部完成采图、生成特征和搜索匹配，验证成功后通过 UART1 上报核心模块指纹 ID 和名字，验证失败则上报错误码。核心模块也可以发起注册流程，模块收到指令后启动传感器的自动注册功能，完成后将新指纹 ID 和名字上报。模块还支持删除指定指纹、清空指纹库、查询指纹列表和索引表、控制传感器呼吸灯效以及设置安全等级等操作。所有指纹模板数据存储在传感器自带的 Flash 中，核心模块不直接处理指纹图像，模块仅作为协议转换桥梁完成 Syno 协议与核心模块统一协议之间的转发。

**关键函数执行流程**

```mermaid
flowchart TD
    A[上电初始化] --> B[main 循环]
    B --> C{g_touchout_flag 手指按下?}
    C -->|是| D{state 为 IDLE/SLEEPING?}
    D -->|是| E[Fp_AutoIdentify 发送比对命令]
    C -->|否| F{uart_fp_rx_ready?}
    E --> F
    F -->|是| G[读取 rx_len 并清标志]
    G --> H[Fp_ProcessRxData 解析 Syno 帧]
    H --> I[Hardware_ProcessFpResponse]
    I --> J{state / last_ack?}
    J -->|识别完成| K[Hardware_SendIdentifyResult]
    J -->|注册完成/超时| L[Hardware_SendEnrollResult]
    K --> B
    L --> B
    J -->|继续等待| B
```

**关键函数输入输出变量**

关键函数 `Fp_HandleResponse()`（`sub-model/submodel-1/Common/Common/Fingerprint/fingerprint.c`）的输入输出变量如下：输入变量包括 `fp_ctx.rx_ctx.frame_ready`（`uint8_t`，是否已收到完整 Syno 帧）、`fp_ctx.rx_ctx.pkg_type`（`uint8_t`，包类型：CMD(0x01) / DATA(0x02)）、`fp_ctx.last_cmd`（`uint8_t`，上一次发送的 Syno 命令码）、`fp_ctx.rx_ctx.buf[]`（`uint8_t[]`，解析后的 Syno 帧数据）和 `fp_ctx.rx_ctx.pkg_dlen`（`uint16_t`，包数据长度）；输出变量包括 `fp_ctx.state`（`fp_state_t`，指纹状态机）、`fp_ctx.last_page_id`（`uint16_t`，识别到的模板 ID）、`fp_ctx.last_match_score`（`uint16_t`，匹配得分）、`fp_ctx.enroll_step`（`uint8_t`，注册流程步骤）、`fp_ctx.template_count`（`uint16_t`，有效模板数量）和 `fp_ctx.index_table[]`（`uint8_t[32]`，模板索引位图）。

#### 健康监测模块

健康监测模块运行在 CH32V103C8T6 上，通过 I2C 与 MAX30102 心率血氧传感器通信，并通过 UART1 与核心模块通信。初始化阶段配置 I2C 和 UART1，设置 MAX30102 的采样率、LED 电流和 FIFO 中断阈值，并配置传感器中断引脚。运行阶段从传感器 FIFO 读取光电容积脉搏波原始数据，经过软件滤波去除基线漂移和运动伪影后，通过峰值检测计算心率，通过红光与红外光的直流和交流分量比值估算血氧饱和度，并通过相邻 RR 间期的标准差计算心率变异性。处理后的数据通过定时主动上报或响应核心模块查询的方式回传，当心率或血氧超出预设阈值时立即上报告警事件。由于 CH32V103 算力有限，算法采用轻量级峰值检测和滑动平均，精度满足消费级场景。模块与显示模块协同工作，健康数据由核心模块转发至显示模块以图表或数字形式呈现。

**关键函数执行流程**

```mermaid
flowchart TD
    A[上电初始化] --> B[main 循环]
    B --> C{g_max30102_int_flag?}
    C -->|是| D[Max30102_ProcessInt 读取 FIFO 并缓存]
    C -->|否| E{10ms 轮询到?}
    E -->|是| F[Max30102_PollFIFO]
    D --> G[Max30102_ProcessAlgorithm]
    F --> G
    G --> H[Hardware_ProcessCoreFrame]
    H --> I{state == MONITORING?}
    I -->|是| J{报告周期到?}
    J -->|是| K[Hardware_ReportHealthData]
    I -->|否| B
    K --> B
    J -->|否| B
```

**关键函数输入输出变量**

关键函数 `Max30102_ProcessAlgorithm()`（`sub-model/submodel-2/Common/Common/Max30102/max30102.c`）的输入输出变量如下：输入/输出变量包括 `max30102_ctx.new_samples`（`uint16_t`，自上次算法新增样本数）；输入变量包括 `max30102_ctx.state`（`health_state_t`，健康监测状态）、`max30102_ctx.buf_count`（`uint16_t`，循环缓冲区有效样本数）和 `max30102_ctx.ir_buf[]` / `red_buf[]`（`uint32_t[500]`，IR / Red 原始采样）；输出变量包括 `max30102_ctx.heart_rate`（`uint8_t`，计算后心率 BPM）、`max30102_ctx.spo2`（`uint8_t`，计算后血氧饱和度）、`max30102_ctx.hrv`（`uint16_t`，心率变异性 SDNN）和 `max30102_ctx.hr_valid` / `spo2_valid`（`uint8_t`，心率/血氧有效标志）。

#### NFC模块

NFC 模块运行在 CH32V103C8T6 上，通过 UART2 接收专用 NFC 读卡模块输出的卡号数据，并通过 UART1 与核心模块通信。NFC 读卡模块以 9600 波特率每 91 毫秒主动输出一次 8 字节数据帧，包含帧头、路由号、5 字节卡号和奇偶校验字节。CH32V103 在 UART2 中断中逐字节接收并解析帧格式，验证帧头和校验字节，仅保留路由号在 0x30 到 0x33 范围内的有效帧。为避免卡片在天线边缘反复触发导致误报，软件对连续识别到的卡号做去抖处理，通常连续识别到相同卡号 5 次后才通过 UART1 向核心模块上报卡片识别事件。模块上电后向核心模块上报子类型，运行期间响应心跳和状态查询，若没有有效卡片则返回空卡信息。由于 NFC 读卡模块本身不可配置，主控仅负责数据解析和协议转发，整个逻辑相对简洁。

**关键函数执行流程**

```mermaid
flowchart TD
    A[USART2 ISR 收到字节] --> B[Nfc_ParseByte]
    B --> C[UartCore_SendData 透传到 Core]
    C --> D[Nfc_Process]
    D --> E{nfc_ctx.card_id != 0?}
    E -->|是| F{与去抖候选卡一致?}
    F -->|是| G[debounce_count++]
    F -->|否| H[更新候选卡并置 count=1]
    G --> I{count >= 阈值?}
    H --> I
    I -->|是| J[card_ready = 1 更新 reported_card]
    E -->|否| K[frame_alive_counter++]
    K --> L{超时?}
    L -->|是| M[Nfc_ResetCard]
    J --> N[Hardware_ProcessNfcCard 发送事件]
    M --> O[返回主循环]
    N --> O
```

**关键函数输入输出变量**

关键函数 `Nfc_Process()`（`sub-model/submodel-3/Common/Common/Nfc/nfc.c`）的输入输出变量如下：输入/输出变量包括 `nfc_ctx.debounce_card_id`（`uint8_t`，去抖候选卡槽号）、`nfc_ctx.debounce_card_number[5]`（`uint8_t[5]`，去抖候选卡号）、`nfc_ctx.debounce_count`（`uint8_t`，连续相同读卡次数）和 `nfc_ctx.frame_alive_counter`（`uint32_t`，无卡帧计数）；输入变量包括 `nfc_ctx.card_id`（`uint8_t`，ISR 解析得到的卡槽号）和 `nfc_ctx.card_number[5]`（`uint8_t[5]`，ISR 解析得到的卡号）；输出变量包括 `nfc_ctx.reported_card_id`（`uint8_t`，已上报给 Core 的卡槽号）、`nfc_ctx.reported_card_number[5]`（`uint8_t[5]`，已上报给 Core 的卡号）和 `nfc_ctx.card_ready`（`volatile uint8_t`，新卡去抖完成标志）。

#### 触摸模块

触摸模块运行在 CH32V103C8T6 上，通过两片串行线连接 TTP229-BSF 电容触摸芯片，读取 16 个触摸焊盘的状态。16 个触摸焊盘组成 2×2 中心方阵和 12 位环绕圆环，对应 16 个键 ID。初始化阶段配置 TTP229 为 16 键串行输出模式并启用多键触发，等待 500 毫秒上电稳定后捕获空闲状态作为基线，以消除 PCB 寄生电容影响。运行阶段以不低于 10 毫秒的周期读取 TTP229 的 16 位串行数据，将原始触摸引脚位图映射为键 ID 状态，当任意键状态变化时通过 UART1 向核心模块上报触摸事件。圆环支持顺时针和逆时针滑动手势识别，方阵则适合方向控制或功能选择。模块响应核心模块的模式设置和状态查询命令，支持多键同时检测，可用于音乐控制、菜单浏览和灯效调节等场景。

**关键函数执行流程**

```mermaid
flowchart TD
    A[TIM2 中断 10ms] --> B[置位 g_scan_flag]
    B --> C[main 循环]
    C --> D[CheckProtocolRx]
    D --> E{g_scan_flag?}
    E -->|是| F[touch_bitmap = TTP229_Read]
    F --> G{touch_bitmap != prev?}
    G -->|是| H[ReportTouchEvent 发送 CMD_SUB_DATA_REPORT]
    G -->|否| I[无状态变化]
    H --> J[prev_touch_bitmap = touch_bitmap]
    I --> J
    E -->|否| K[等待下一次扫描]
    J --> K
    K --> C
```

**关键函数输入输出变量**

关键函数 `TTP229_Read()`（`sub-model/submodel-4/User/TTP229/ttp229.c`）的输入输出变量如下：输入/输出变量包括 `prev_touch_bitmap`（`uint16_t`，上一次按键位图）；输入变量包括 `g_scan_flag`（`volatile uint8_t`，TIM2 10ms 扫描触发标志）、`cached_raw`（`static uint16_t`，上一次原始读数，DV 超时时回退）、`baseline_raw`（`static uint16_t`，上电校准基准原始值）和 `key_tp_bit[16]`（`static const uint8_t[16]`，TP 位到按键位映射表）；输出变量为返回值（`uint16_t`，校准后的按键位图）。

#### RGB模块

RGB 模块运行在 CH585F 上，使用 SPI0 的 MOSI 引脚驱动 7×7 共 49 颗 WS2812 可寻址 LED，实现动态灯光效果。模块采用纯超级循环架构，不使用蓝牙协议栈和事件调度器，以 SysTick 提供 1 毫秒时间基准，主循环交替执行 UART0 协议解析和 LED 灯效渲染。WS2812 的单线归零码通过 SPI 2.5 兆赫兹时钟以 3 个 SPI 位编码 1 个 WS2812 位的方式精确模拟。模块内部使用 HSV 色彩空间作为中间态，饱和度强制为最大值以保证颜色鲜艳，通过 V 分量全局控制亮度。模块支持自定义帧动画、常亮、呼吸和跑马灯四种模式，其中自定义模式允许核心模块逐帧传输 49 颗 LED 的 RGB888 数据，最多缓存 20 帧并按指定间隔循环播放。核心模块通过 SET_MODE 命令设置模式、颜色、亮度和速度，模块收到命令后立即更新灯效并周期性上报当前状态。由于全白时电流可达 3 安培，实际使用需根据模块供电能力控制亮度。

**关键函数执行流程**

```mermaid
flowchart TD
    A[Effect_Update] --> B[计算 effective_speed]
    B --> C{判断 mode}
    C -->|SOLID| D[RenderSolid]
    C -->|BREATHING| E[RenderBreathing]
    C -->|MARQUEE| F[RenderMarquee]
    C -->|CUSTOM| G[RenderCustom]
    D --> H[写入 s_led_buf]
    E --> H
    F --> H
    G --> H
    H --> I[WS2812_Refresh]
    I --> J[更新 s_state.tick]
    J --> K[返回 TRUE]
```

**关键函数输入输出变量**

关键函数 `Effect_Update()`（`sub-model/submodel-5/APP/effect.c`）的输入输出变量如下：输入/输出变量包括 `s_state.frame_counter`（`uint32_t`，动画步进计数器）、`s_state.direction`（`uint8_t`，方向标志）和 `s_state.custom_current_frame`（`uint8_t`，当前自定义帧索引）；输入变量包括 `s_state.mode`（`rgb_mode_t`，当前特效模式）、`s_state.r` / `g` / `b`（`uint8_t`，基础颜色）、`s_state.brightness`（`uint8_t`，全局亮度）、`s_state.speed`（`uint8_t`，动画速度）和 `s_state.custom_frame_count`（`uint8_t`，已加载自定义帧数）；输出变量包括 `s_state.tick`（`uint32_t`，每帧自增节拍）、`s_led_buf[]`（`rgb888_t[]`，WS2812 像素缓冲区）和返回值（`bool`，固定返回 TRUE）。

#### 测距模块

测距模块运行在 CH32V103C8T6 上，通过 I2C 与 VL53L0X 飞行时间激光测距传感器通信，并通过 UART1 与核心模块通信。初始化阶段通过 XSHUT 引脚复位传感器，配置 I2C 地址和默认测距模式，建立 UART1 协议状态机。模块上电时模块 ID 未知，收到核心模块的 GET_TYPE 命令后从帧的 DST 字段学习自身在配件槽中的实际地址，后续所有响应均使用学习到的 ID。核心模块发送开始测距命令后，模块启动 VL53L0X 连续测距模式，传感器数据就绪引脚触发外部中断，主控读取最新距离值并做滑动平均或中值滤波，消除单次测量跳变。核心模块通过查询 GET_TYPE 获取测距结果，模块在 ACK 之后紧接着发送测距数据帧。核心模块控制查询频率，从而控制数据更新频率。测距停止命令下达后，传感器进入待机状态，模块仅回复类型查询而不发送测距数据。该模块适用于手势感应、物体接近检测和简易测距应用。

**关键函数执行流程**

```mermaid
flowchart TD
    A[VL53L0X_ReadDistance] --> B[读取 0x0014 range_status]
    B --> C[读取 0x001E distance]
    C --> D{I2C 出错?}
    D -->|是| E[vl53l0x_ctx.last_distance_mm = 0xFFFF]
    D -->|否| F{range_status 低5位 >= 5?}
    F -->|是| E
    F -->|否| G[保存 distance]
    E --> H[VL53L0X_ClearInterrupt]
    G --> H
    H --> I[vl53l0x_ctx.data_ready = 0]
    I --> J[返回距离]
```

**关键函数输入输出变量**

关键函数 `VL53L0X_ReadDistance()`（`sub-model/submodel-6/Common/Common/VL53L0X/vl53l0x.c`）的输入输出变量如下：输入变量包括 `range_status`（`uint8_t`，从 0x0014 读取的测距状态）；输出变量包括 `distance`（`uint16_t`，从 0x001E 读取的原始距离 mm）、`vl53l0x_ctx.last_distance_mm`（`uint16_t`，缓存到上下文的有效距离）、`vl53l0x_ctx.data_ready`（`uint8_t`，数据就绪标志，读取后清零）和返回值（`uint16_t`，返回距离，无效时为 0xFFFF）。

#### 副屏模块

副屏模块运行在 CH32V103C8T6 上，通过 SPI 驱动 2.13 寸全反式反射液晶屏，并通过 UART1 与核心模块通信。全反屏无背光，依靠环境光反射显示，在强光下清晰可见，静态显示几乎不耗电，因此非常适合常显信息面板。初始化阶段配置 SPI 和 UART1，初始化 ST7305 驱动芯片的分辨率、扫描方向和刷新模式。运行阶段响应核心模块的心跳和类型查询，通过 UART1 接收状态数据、自定义内容、清屏指令和 BMP 批量传输数据。模块可以主动向核心模块请求系统运行状态、当前在线设备列表以及 BMP 图片文件，核心模块解析为 1 位位图后通过批量传输多帧发送。由于分辨率仅为 122×250，模块采用固定布局分区显示，如顶部状态栏、中部大字体时间和底部图标行，避免复杂 UI。刷新速率较慢，模块在收到指令后会对短时间内连续到达的多条更新做内容合并，减少无效刷新。刷新完成后 CH32V103 可进入睡眠模式，仅保留 UART 唤醒中断，进一步降低整机功耗。

**关键函数执行流程**

```mermaid
flowchart TD
    A["main while(1)"] --> B["App_Process"]
    B --> C{"uart_core_rx_ctx.frame_ready?"}
    C -->|"是"| D["App_HandleFrame"]
    D --> E["协议分发"]
    C -->|"否"| F{"g_bulk_ready?"}
    E --> F
    F -->|"是"| G["App_ProcessBulkComplete"]
    F -->|"否"| H{"g_refresh_pending?"}
    G --> H
    H -->|"是"| I["UI_Refresh"]
    H -->|"否"| J["Delay_Ms 1"]
    I --> K["st7305_power_low"]
    K --> J
    J --> A
```

**关键函数输入输出变量**

关键函数 `App_Process()`（`sub-model/submodel-7/User/APP/app.c`）的输入输出变量如下：输入/输出变量包括 `g_lcd`（`struct st7305_stu`，ST7305 驱动实例）；输入变量包括 `uart_core_rx_ctx.frame_ready`（`uint8_t`，是否有完整协议帧待处理）、`uart_core_rx_ctx.read_frame`（`protocol_frame_t`，待处理的协议帧）、`g_bulk_ready`（`volatile uint8_t`，Bulk 图片接收完成标志）和 `g_refresh_pending`（`volatile uint8_t`，是否有显示内容需要刷新）；输出变量包括 `g_framebuffer[]`（`uint8_t[]`，全屏帧缓冲区）和 `g_my_module_id`（`uint8_t`，子模块地址，由首帧 DST 学习得到）。

#### 无线蓝牙模块

无线蓝牙模块运行在 CH585F 上，作为 BLE 从机与手机等上位机建立连接，并通过 SPI0 从机接口与核心模块 CH32H417 进行 CLI 数据透传。模块初始化阶段先完成 BLE 协议栈与 TMOS 任务调度系统的初始化，配置 GAP Peripheral 角色、广播参数和 Just Works 配对策略，注册自定义 WCH CLI 服务（UUID 0xFFE0）及其下行写特征（UUID 0xFFF1）和上行通知特征（UUID 0xFFF2），同时初始化 SPI0 为 Mode 0 从机模式并建立协议帧接收状态机。运行阶段主循环调用 TMOS_SystemProcess() 处理事件，当手机通过 BLE 写入 CLI 数据时，模块按应用层协议重组分片，封装为子命令的 SPI 协议帧并写入发送队列，再通过 PB12 低电平脉冲通知核心 MCU 读取；当核心 MCU 通过 SPI 返回数据时，模块在 SPI 中断中接收字节并逐帧解析，对 CLI 透传数据打包后通过 BLE Notification 回传手机。模块为 SPI 收发各维护 20 KB 环形缓冲，BLE 发送队列约 4 KB，并以约 100 毫秒间隔限速通知，兼顾吞吐与协议栈稳定性。电源管理可选启用 RTC 唤醒的低功耗睡眠，由 TMOS 在空闲时自动进入睡眠，适合依赖电池供电的移动终端场景。

**关键函数执行流程**

```mermaid
flowchart TD
    A[Peripheral_ProcessEvent] --> B{SYS_EVENT_MSG?}
    B -->|是| C[接收并处理 TMOS 消息]
    C --> D[返回事件掩码]
    B -->|否| E{SBP_START_DEVICE_EVT?}
    E -->|是| F[GAPRole_PeripheralStartDevice]
    F --> D
    E -->|否| G{SBP_SPI_PROCESS_EVT?}
    G -->|是| H[ProcessSpiRxData]
    H --> I[SPI_Slave_DequeueRx]
    I --> J[Protocol_ParseByte]
    J --> K[OnProtocolStdFrame / OnProtocolStreamChunk]
    K --> D
    G -->|否| L{SBP_BLE_NOTIFY_EVT?}
    L -->|是| M[BleTxQueueProcess]
    M --> D
    L -->|否| N[其他事件处理]
    N --> D
```

**关键函数输入输出变量**

关键函数 `Peripheral_ProcessEvent()`（`main-model/wireless/APP/peripheral.c`）的输入输出变量如下：输入/输出变量包括 `peripheralConnList`（`peripheralConnItem_t`，连接句柄与连接参数）、`peripheralMTU`（`uint16_t`，当前 ATT MTU）、`s_ble_tx_queue[4096]`（`uint8_t[4096]`，BLE 发送队列）、`s_ble_tx_head` / `s_ble_tx_tail`（`uint16_t`，发送队列头尾指针）、`s_spi_fwd_buf[512]`（`uint8_t[512]`，SPI 转发缓冲区）和 `s_app_rx_reasm[2048]`（`uint8_t[2048]`，APP 侧数据重组缓冲区）；输入变量包括 `task_id`（`uint8_t`，当前任务 ID）、`events`（`uint16_t`，待处理事件位图）和 `Peripheral_TaskID`（`uint8_t`，本任务注册 ID）；输出变量为返回值（`uint16_t`，未处理事件掩码）。

#### WCH Terminal APP

WCH Terminal App 基于 Flutter 构建，运行在 Android 或 iOS 移动设备上，通过低功耗蓝牙与核心模块进行可视化交互。应用采用 Riverpod 进行状态管理，底层使用 flutter_blue_plus 插件完成 BLE 扫描、连接与数据收发，代码层分为 BLE 管理、协议编解码、CLI 命令引擎、业务状态与页面 UI 五层。初始化阶段扫描并过滤广播名包含 WCH 且服务 UUID 为 0xFFE0 的设备，连接后请求 MTU 至 247 字节，发现 RX 特征（0xFFF1）和 TX 特征（0xFFF2），订阅 TX 通知并建立 5 秒周期心跳。运行阶段应用通过 CLI 引擎向设备发送命令并异步匹配响应，提供命令行终端、文件浏览器、音乐播放器、电子书阅读器、图片浏览器和文本编辑器六大功能；文件浏览器通过 ls、cd、stat 等命令解析目录结构，音乐播放器通过 playst 解析播放状态并本地维护进度，电子书按页分页渲染，文本编辑器通过 echo 重定向保存文件。所有业务数据均按照自定义应用层帧协议进行拆分与重组，支持大数据分片传输，使手机在脱离屏幕的情况下也能完整管理终端文件、播放音频和编辑文本。

**关键函数执行流程**

```mermaid
flowchart TD
    A["开始: execute(command, timeout)"] --> B["创建 Completer 与 _PendingCommand"]
    B --> C["将 pending 加入队列"]
    C --> D["ProtocolHandler.encodeCliData 分帧"]
    D --> E["循环写入每帧到 BLE"]
    E --> F["启动 timeout 定时器"]
    F --> G["等待上行数据或超时"]
    G --> H{"收到 CLI 数据?"}
    H -->|"是"| I["解码并追加到 buffer"]
    I --> J{"帧已 EOF?"}
    J -->|"是"| K["complete CliResponse.success"]
    J -->|"否"| G
    H -->|"否"| L{"超时触发?"}
    L -->|"是"| M["complete CliResponse.error"]
    L -->|"否"| G
```

**关键函数输入输出变量**

关键函数 `CliEngine.execute()`（`main-model/wch_terminal_app/lib/core/cli/cli_engine.dart`）的输入输出变量如下：输入变量包括 `command`（`String`，待发送的 CLI 命令字符串）和 `timeout`（`Duration?`，命令超时时间，默认 5 秒）；输出变量为返回值（`Future<CliResponse>`，异步返回命令执行结果）；`completer`、`pending`、`frames` 和 `effectiveTimeout` 为函数内部使用的局部变量。

## 可扩展之处

1. 本作品使用CH585F作为蓝牙芯片，但是在利用上不够完善，只将其连接了手机APP作为无线控制，可以尝试开发无线键盘、无线鼠标等扩展外设。
2. 本作品使用的Eink屏幕，触摸功能比较羸弱，刷新率也较低，LUT优化不到位，未来可能需要考虑LUT优化和触摸板的研制。
3. 配件模块的设计不够精巧，适配性和实用性较差，应该尝试设计更高质量的配件模块来取得更实用的功能。

## 心得体会

作品在初始设计阶段，主要的设计目标就是要实现模块化的个人移动终端，因为本人在生活中经常会用到阅读器、音乐播放器的功能，我就逐渐考虑是否能够将这些功能融合到一个设备上，并且能够实现彻底的模块化，于是就开始构思这个项目，在构思阶段，我构思的模块化程度比最终版本更高，我构思接口、主控也是可以模块化的，实现了彻底的客制化，用户需要什么级别的主控、需要什么样的接口都可以进行插接配置。但是最终考虑到作品的繁杂和实用性，放弃了这些内容的模块化。本作品设计的难度非常高，无论是在软件上、硬件上、建模上，都对我们队员提出了极高的要求，由于内容庞大，开发周期较短，开发起来也比较紧张，最终成品也有一定的妥协。

本次开发前期调研非常充分，在前期设计阶段就设计完成了整体的呈现结构和所有的模块，主要是围绕模块间的连接方式和模块内部的功能来展开的设计，尺寸设计上也非常严格，一切都按照产品的需求去实现，尽量以高要求去设计。开发过程中也是先开发了验证板，将一些重要的功能先行验证，并且设计软件层面的整体接口，确保了软件结构的工程性。最终版本的开发基本上是围绕尺寸展开的，从上至下，先设计外壳尺寸，根据外壳约束设计PCB，设计最终PCB之后再进行软件的开发验证。

软件上最大的难度在于将所有的模块动态集成吧，在前期设计中，我设想这个作品应该是可以根据用户的需求任意装配的，我希望是用户去使用作品而不是我们去教用户如何去使用，因此设计了很多冗余，作品很多模块都可以单独使用，在基础上新增模块也要求尽量不要影响原有模块的使用，降低模块间的耦合程度，提高模块的可扩展性。因此在软件设计上也需要去考虑很多情况，将所有模块做到物尽其用去进行适配，是很困难的事情。

作品设计中比较有意思的是，我们最终选用了磁吸Pogopin的连接方式，在一些可靠性需求较高的连接场景，我们也设计了更多的磁体，来增加连接的稳定性，为了满足我们Pogopin上的需求，我们最终设计了一个专用的磁吸烧录器，用于给我们的作品进行烧录，实现了整个作品的磁吸连接化。

整体来说，这次项目的开发是比较成功的，我们成员在其中学习到了很多，在解决问题的过程中也锻炼了我们的思维，也提高了我们的团队合作能力。