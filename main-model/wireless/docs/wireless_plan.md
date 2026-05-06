# CH585F Wireless 模块固件开发计划

> 本文档为 CH585F 独立 BLE MCU 的顶层设计规划，覆盖从工程骨架到完整功能的全部实现路径。  
> 基于 WCH 官方 SDK（`evt/` 下例程）移植适配，遵循项目统一命名规范与通信协议。

---

## 1. 设计目标

| 阶段 | 目标 | 验收标准 |
|------|------|---------|
| P0 | 工程骨架 + SPI 双向通信 + 协议帧处理 | Core 能查询 CH585F 状态并收到正确 ACK |
| P1 | BLE Central/Peripheral 双角色运行 | 能扫描、连接、配对 BLE 设备；APP 能连接 |
| P2 | HID over GATT Host | BLE 键鼠输入能上报给 Core |
| P3 | APP CLI 数据通道 + 文件传输 | APP 能远程执行 CLI 命令并收发文件 |

---

## 2. 目录结构设计

```
wireless/
├── docs/
│   ├── PROJECT.md              # 核心项目总体文档（已存在）
│   ├── Wireless.md             # Wireless 模块需求文档（已存在）
│   ├── Protocol_Wireless.md    # Wireless 通信协议（已存在）
│   └── wireless_plan.md        # 本文档：开发计划与顶层设计
│
├── src/                        # 【应用层代码】用户实现
│   ├── main.c                  # 系统入口：时钟、GPIO、BLE、任务初始化
│   ├── hardware.c / hardware.h # 全局调度、板级初始化、TMOS 任务注册
│   ├── spi_slave.c / spi_slave.h   # SPI4 Slave 驱动 + 中断 + 收发缓冲
│   ├── protocol.c / protocol.h     # 统一协议帧：打包、解析、状态机
│   ├── bt_core.c / bt_core.h       # BLE 核心管理：初始化、模式切换、状态机
│   ├── bt_central.c / bt_central.h # BLE Central：扫描、连接、配对、列表管理
│   ├── bt_peripheral.c / bt_peripheral.h # BLE Peripheral：广播、APP 连接管理
│   ├── bt_hid_host.c / bt_hid_host.h     # HID over GATT Host：键鼠报告接收与解析
│   ├── bt_app_profile.c / bt_app_profile.h   # 自定义 GATT Profile：APP 数据通道
│   └── data_queue.c / data_queue.h   # 环形队列：SPI 与 BLE 之间的数据缓冲
│
├── StdPeriphDriver/            # 【官方标准外设驱动】已存在，按需使用
│   ├── inc/
│   └── CH58x_*.c
│
├── RVMSIS/
│   └── core_riscv.h
│
├── Startup/
│   └── startup_CH585.S
│
├── Ld/
│   └── Link.ld
│
├── HAL/                        # 【移植的官方 HAL 层】直接参与编译
│   ├── include/
│   │   ├── CONFIG.h            # BLE 配置宏（基于官方，按需修改）
│   │   ├── HAL.h               # HAL 总头文件
│   │   ├── RTC.h / SLEEP.h     # 系统时钟与低功耗（直接复用）
│   │   └── ...                 # 其他 HAL 头文件按需引入
│   ├── MCU.c                   # BLE 库初始化 + RF 校准（基于官方移植）
│   ├── RTC.c                   # TMOS 系统时钟（直接复用）
│   └── SLEEP.c                 # 低功耗管理（可选，默认禁用）
│
├── LIB/                        # 【BLE 协议栈库】官方闭源库，直接链接
│   ├── CH58xBLE_LIB.h          # BLE 协议栈 API 头文件
│   ├── CH58xBLE_ROM.h          # ROM 版头文件（可选）
│   └── libCH58xBLE.a           # BLE 5.4 协议栈静态库
│
├── evt/                        # 【官方 SDK 与例程】仅作为参考，不参与编译
│   ├── HAL/                    # 原始 HAL 层参考源
│   ├── LIB/                    # 原始协议栈库参考源
│   └── MultiCentPeri/          # 双角色参考例程
│
├── .cproject / .project / wireless.wvproj  # MRS 工程文件
└── .calibrate / .template
```

> **原则**：`src/` 下全部为项目自定义代码；`evt/` 下仅作为移植参考，不直接参与编译。

---

## 3. 模块职责与接口设计

### 3.1 hardware.c / hardware.h

**职责**：
- 板级初始化：时钟、GPIO、UART1 Debug、SPI4、中断优先级
- TMOS 任务注册与全局任务 ID 管理
- 系统滴答与低功耗（后续可选）

**关键接口**：

```c
/* hardware.h */
#ifndef __HARDWARE_H
#define __HARDWARE_H

#include "CH58x_common.h"

/* 全局任务 ID */
extern tmosTaskID g_spi_task_id;
extern tmosTaskID g_bt_task_id;

/* 初始化 */
void Hardware_Init(void);
void Hardware_GPIOInit(void);
void Hardware_UART1Init(void);      /* Debug 输出 */
void Hardware_SPI4Init(void);       /* SPI Slave */

/* 系统时间（供协议超时使用） */
uint32_t Hardware_GetMillis(void);

#endif
```

---

### 3.2 spi_slave.c / spi_slave.h

**职责**：
- SPI4 外设配置为 Slave 模式
- 字节中断接收，推入协议解析状态机
- 发送缓冲管理：CH585F 无法主动发起 SPI，需利用 Master 读取窗口发送

**关键接口**：

```c
/* spi_slave.h */
#ifndef __SPI_SLAVE_H
#define __SPI_SLAVE_H

#include "CH58x_common.h"

#define SPI_SLAVE_RX_BUF_SIZE   512
#define SPI_SLAVE_TX_BUF_SIZE   512

/* SPI 初始化 */
void SPI_Slave_Init(void);

/* 查询是否有待发送的上行数据 */
bool SPI_Slave_HasTxData(void);

/* 将一包上行数据写入 SPI 发送缓冲（由 protocol/bt 模块调用） */
bool SPI_Slave_EnqueueTx(const uint8_t *data, uint16_t len);

/* SPI 中断服务函数 */
__INTERRUPT __HIGH_CODE void SPI0_IRQHandler(void);  /* 或对应 SPI 向量 */

#endif
```

**实现要点**：
- SPI 模式：`CPOL_Low`, `CPHA_1Edge`, 8bit, MSB first
- 接收：每收到一字节，直接喂给 `Protocol_ParseByte()`
- 发送：维护一个环形发送缓冲区，当 Master 发起时钟时，从缓冲区取出字节放到 `SPI0_DAT`
- **INT 中断线**：若硬件支持，配置 GPIO 外部中断，有上行数据时通知 V5F；若无，先采用轮询方案

---

### 3.3 protocol.c / protocol.h

**职责**：
- 统一紧凑二进制协议帧的打包、解析、校验
- 与模块无关，纯协议层

**关键接口**：

```c
/* protocol.h */
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include "CH58x_common.h"

#define PROTO_HEAD          0xAA
#define PROTO_TAIL0         0xA5
#define PROTO_TAIL1         0x5A
#define PROTO_TAIL2         0xFC
#define PROTO_TAIL3         0xFD

#define PROTO_FRAME_MAX_LEN 260
#define PROTO_DATA_MAX_LEN  255

/* 模块 ID */
#define MODULE_ID_CORE          0x00
#define MODULE_ID_WIRELESS      0x01

/* 通用操作码 */
#define CMD_NOP                 0x00
#define CMD_GET_TYPE            0x01
#define CMD_GET_STATUS          0x02
#define CMD_SET_CONFIG          0x03
#define CMD_ACK                 0x04
#define CMD_NACK                0x05
#define CMD_EVT_NOTIFY          0x06
#define CMD_DATA_STREAM         0x07

/* Wireless 专用操作码 */
#define CMD_BT_GET_STATUS       0x51
#define CMD_BT_SEND_DATA        0x52
#define CMD_BT_CONN_EVT         0x54
#define CMD_BT_RECV_DATA        0x55
#define CMD_BT_HID_REPORT       0x56
#define CMD_BT_SET_DISCOVERABLE 0x58
#define CMD_BT_RESET            0x59

/* 扩展操作码（基码 0x50 + 子命令） */
#define CMD_BT_EXT_BASE         0x50
#define CMD_BT_EXT_SCAN         0x01
#define CMD_BT_EXT_DEVICE_LIST  0x02
#define CMD_BT_EXT_CONNECT      0x03
#define CMD_BT_EXT_PAIRING_MGMT 0x04
#define CMD_BT_EXT_SET_MODE     0x05
#define CMD_BT_EXT_CLI_DATA     0x07

/* NACK 错误码 */
#define PROTO_ERR_NONE                  0x00
#define PROTO_ERR_UNSUPPORTED_CMD       0x01
#define PROTO_ERR_INVALID_PARAM         0x02
#define PROTO_ERR_LEN_MISMATCH          0x03
#define PROTO_ERR_BUSY                  0x04
#define PROTO_ERR_HW_FAULT              0x05
#define PROTO_ERR_TIMEOUT               0x06

/* 帧结构 */
typedef struct {
    uint8_t head;
    uint8_t src;
    uint8_t dst;
    uint8_t len;
    uint8_t cmd;
    uint8_t data[PROTO_DATA_MAX_LEN];
    uint8_t tail[4];
} proto_frame_t;

/* 解析状态机状态 */
typedef enum {
    PROTO_STATE_WAIT_HEAD = 0,
    PROTO_STATE_WAIT_SRC,
    PROTO_STATE_WAIT_DST,
    PROTO_STATE_WAIT_LEN,
    PROTO_STATE_WAIT_CMD,
    PROTO_STATE_WAIT_DATA,
    PROTO_STATE_WAIT_TAIL0,
    PROTO_STATE_WAIT_TAIL1,
    PROTO_STATE_WAIT_TAIL2,
    PROTO_STATE_WAIT_TAIL3,
    PROTO_STATE_FRAME_READY
} proto_state_t;

/* 初始化 */
void Protocol_Init(void);

/* 逐字节解析，当帧完成时返回 true，frame 被填充 */
bool Protocol_ParseByte(uint8_t byte, proto_frame_t *frame);

/* 打包一帧到 buffer，返回打包后的总长度 */
uint16_t Protocol_PackFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                            const uint8_t *data, uint8_t data_len,
                            uint8_t *out_buf, uint16_t out_buf_len);

/* 便捷函数：打包 ACK/NACK */
uint16_t Protocol_PackAck(uint8_t src, uint8_t dst, uint8_t *rsp_data, uint8_t rsp_len,
                          uint8_t *out_buf, uint16_t out_buf_len);
uint16_t Protocol_PackNack(uint8_t src, uint8_t dst, uint8_t err_code,
                           uint8_t *out_buf, uint16_t out_buf_len);

#endif
```

---

### 3.4 data_queue.c / data_queue.h

**职责**：
- 提供线程安全的环形缓冲区（中断 / 主循环间数据传递）

```c
/* data_queue.h */
#ifndef __DATA_QUEUE_H
#define __DATA_QUEUE_H

#include "CH58x_common.h"

typedef struct {
    volatile uint16_t head;
    volatile uint16_t tail;
    uint16_t size;
    uint8_t *buf;
} data_queue_t;

void DataQueue_Init(data_queue_t *q, uint8_t *buf, uint16_t size);
bool DataQueue_Push(data_queue_t *q, uint8_t byte);
bool DataQueue_Pop(data_queue_t *q, uint8_t *byte);
uint16_t DataQueue_Count(data_queue_t *q);
bool DataQueue_IsEmpty(data_queue_t *q);
bool DataQueue_IsFull(data_queue_t *q);

#endif
```

---

### 3.5 bt_core.c / bt_core.h

**职责**：
- BLE 协议栈初始化（基于 `MCU.c` 中的 `CH58x_BLEInit` 移植）
- 工作模式管理：关闭 / HID Host / Peripheral / 并发
- 全局连接状态维护
- 事件分发：将协议命令分发给 Central / Peripheral / HID / Profile 子模块

```c
/* bt_core.h */
#ifndef __BT_CORE_H
#define __BT_CORE_H

#include "CH58x_common.h"

/* 蓝牙工作模式 */
typedef enum {
    BT_MODE_OFF = 0,
    BT_MODE_HID_HOST,
    BT_MODE_PERIPHERAL,
    BT_MODE_DUAL
} bt_mode_t;

/* 全局状态 */
typedef struct {
    bt_mode_t mode;
    uint8_t   is_scanning;
    uint8_t   hid_connected;
    uint8_t   app_connected;
    uint8_t   conn_dev_num;
    uint8_t   main_conn_mac[6];
} bt_status_t;

extern bt_status_t g_bt_status;

/* 初始化 */
void BT_Core_Init(void);

/* 模式切换 */
void BT_Core_SetMode(bt_mode_t mode);

/* 状态查询 */
void BT_Core_GetStatus(uint8_t *out_buf, uint8_t *out_len);

/* 处理来自 Core 的协议帧（由 protocol 层调用） */
void BT_Core_ProcessCmd(const uint8_t *data, uint8_t len);

/* 上报事件给 Core（封装为协议帧后交 SPI 发送） */
void BT_Core_NotifyEvent(uint8_t evt_cmd, const uint8_t *data, uint8_t len);

#endif
```

---

### 3.6 bt_central.c / bt_central.h

**职责**：
- BLE Central 角色管理（基于 `multiCentral.c` 移植）
- 扫描、过滤、设备列表缓存
- 按 MAC 地址连接/断开
- 配对绑定（依赖 BLE SNV 自动持久化 LTK）
- 开机自动回连

```c
/* bt_central.h */
#ifndef __BT_CENTRAL_H
#define __BT_CENTRAL_H

#include "CH58x_common.h"

#define BT_SCAN_MAX_RESULTS     16
#define BT_MAX_PAIRED_DEVICES   8
#define BT_DEVICE_NAME_LEN      16

/* 设备信息（扫描结果 / 配对列表通用） */
typedef struct {
    uint8_t status;                 /* 0=已配对, 1=已连接, 2=发现中 */
    uint8_t name[BT_DEVICE_NAME_LEN];
    uint8_t mac[6];
} bt_device_info_t;

/* 初始化 */
void BT_Central_Init(uint8_t task_id);
uint16_t BT_Central_ProcessEvent(uint8_t task_id, uint16_t events);

/* 协议命令入口 */
void BT_Central_StartScan(uint8_t timeout_sec, uint8_t filter);
void BT_Central_StopScan(void);
void BT_Central_Connect(uint8_t *mac);
void BT_Central_Disconnect(uint8_t *mac);
void BT_Central_DisconnectAll(void);

/* 配对管理 */
void BT_Central_GetPairedList(void);
void BT_Central_RemovePaired(uint8_t *mac);
void BT_Central_ClearPaired(void);

/* 获取扫描结果（供上报 Core 使用） */
uint8_t BT_Central_GetScanResults(bt_device_info_t *out_list, uint8_t max_num);

#endif
```

---

### 3.7 bt_peripheral.c / bt_peripheral.h

**职责**：
- BLE Peripheral 角色管理（基于 `multiPeripheral.c` 移植）
- 广播参数配置（名称、间隔、UUID）
- 连接状态维护
- 与 `bt_app_profile.c` 配合提供 APP 数据通道

```c
/* bt_peripheral.h */
#ifndef __BT_PERIPHERAL_H
#define __BT_PERIPHERAL_H

#include "CH58x_common.h"

/* 初始化 */
void BT_Peripheral_Init(uint8_t task_id);
uint16_t BT_Peripheral_ProcessEvent(uint8_t task_id, uint16_t events);

/* 设置广播名称 */
void BT_Peripheral_SetAdvName(const uint8_t *name, uint8_t len);

/* 开关广播 */
void BT_Peripheral_SetAdvertising(bool enable);

/* 主动断开 APP 连接 */
void BT_Peripheral_Disconnect(void);

/* 获取连接句柄（供 Profile 发送 Notify 使用） */
uint16_t BT_Peripheral_GetConnHandle(void);

#endif
```

---

### 3.8 bt_hid_host.c / bt_hid_host.h

**职责**：
- HID over GATT Host（HOGP）实现
- 在 Central 连接后，发现对端的 HID Service
- 订阅 HID Report Characteristic 的 Notify
- 解析 Boot Report 格式（键盘/鼠标）
- 通过 SPI 上报 `CMD_BT_HID_REPORT`

```c
/* bt_hid_host.h */
#ifndef __BT_HID_HOST_H
#define __BT_HID_HOST_H

#include "CH58x_common.h"

/* HID 报告类型 */
#define HID_RPT_TYPE_KEYBOARD   0x00
#define HID_RPT_TYPE_MOUSE      0x01

/* 键盘报告（8 字节） */
typedef struct {
    uint8_t modifier;   /* bit0=L_CTRL, bit1=L_SHIFT, ... */
    uint8_t reserved;
    uint8_t keys[6];    /* USB HID Usage ID */
} hid_keyboard_report_t;

/* 鼠标报告（4 字节） */
typedef struct {
    uint8_t buttons;    /* bit0=L, bit1=R, bit2=M */
    int8_t  x;
    int8_t  y;
    int8_t  wheel;
} hid_mouse_report_t;

/* 初始化 */
void BT_HID_Host_Init(void);

/* 在连接建立后启动 HID Service 发现 */
void BT_HID_Host_StartDiscovery(uint16_t conn_handle);

/* 当收到 HID Report Notify 时调用 */
void BT_HID_Host_OnReport(uint8_t report_type, const uint8_t *data, uint8_t len);

/* 解析并上报 */
void BT_HID_Host_ReportKeyboard(const uint8_t *rpt);
void BT_HID_Host_ReportMouse(const uint8_t *rpt);

#endif
```

---

### 3.9 bt_app_profile.c / bt_app_profile.h

**职责**：
- 自定义 GATT Profile，提供 APP 数据通道
- 参考 `gattprofile.c` 实现，但调整为更适合 CLI/文件传输的数据长度
- 使用 Notify + Write 双向传输

```c
/* bt_app_profile.h */
#ifndef __BT_APP_PROFILE_H
#define __BT_APP_PROFILE_H

#include "CH58x_common.h"

#define APP_PROFILE_SERV_UUID       0xFFF0
#define APP_PROFILE_CHAR_TX_UUID    0xFFF1   /* CH585F -> APP (Notify) */
#define APP_PROFILE_CHAR_RX_UUID    0xFFF2   /* APP -> CH585F (Write) */

/* 回调：当 APP 写入数据时 */
typedef void (*app_profile_rx_cb_t)(const uint8_t *data, uint16_t len);

/* 初始化服务 */
bStatus_t AppProfile_AddService(void);

/* 注册接收回调 */
void AppProfile_RegisterRxCb(app_profile_rx_cb_t cb);

/* 向 APP 发送数据（Notify） */
bStatus_t AppProfile_SendData(uint16_t conn_handle, const uint8_t *data, uint16_t len);

#endif
```

---

### 3.10 main.c

**职责**：
- 系统最顶层入口
- 初始化顺序：时钟 → GPIO → UART1(Debug) → SPI4 → BLE → HAL → 应用任务
- 进入 TMOS 主循环

```c
/* main.c 伪代码 */
__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

int main(void)
{
    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);

    /* Debug UART1 */
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();

    PRINT("CH585F Wireless Boot\n");

    /* 硬件层初始化 */
    Hardware_Init();
    SPI_Slave_Init();
    Protocol_Init();

    /* BLE 协议栈初始化 */
    CH58x_BLEInit();
    HAL_Init();

    /* GAP 角色初始化 */
    GAPRole_PeripheralInit();
    GAPRole_CentralInit();

    /* 应用层初始化 */
    BT_Core_Init();

    /* 进入 TMOS 主循环 */
    while(1) {
        TMOS_SystemProcess();
    }
}
```

---

## 4. 数据流设计

### 4.1 下行：Core → CH585F

```
Core (V5F SPI Master)
    ↓ SPI4 时钟
SPI_Slave_IRQHandler (CH585F)
    ↓ 逐字节
Protocol_ParseByte() —— 状态机解析
    ↓ 帧完成回调
BT_Core_ProcessCmd()
    ↓ 按 CMD 分发
    ├─ CMD_BT_GET_STATUS      → BT_Core_GetStatus() → 回 ACK
    ├─ CMD_BT_EXT_SCAN        → BT_Central_StartScan()/StopScan()
    ├─ CMD_BT_EXT_CONNECT     → BT_Central_Connect()/Disconnect()
    ├─ CMD_BT_EXT_SET_MODE    → BT_Core_SetMode()
    ├─ CMD_BT_EXT_CLI_DATA    → 透传给 Core CLI（或本地处理）
    └─ ...
```

### 4.2 上行：CH585F → Core

```
BT 子模块产生事件/数据
    ↓ BT_Core_NotifyEvent()
Protocol_PackFrame() —— 打包为标准协议帧
    ↓ SPI_Slave_EnqueueTx()
SPI Slave TX Buffer
    ↓ 等待 V5F Master 发起时钟
SPI_Slave_IRQHandler —— 从缓冲区取字节发出
    ↓ MISO
Core (V5F) 接收并解析
```

### 4.3 BLE 数据流

```
┌─────────────────────────────────────────────┐
│              CH585F (独立 MCU)               │
│  ┌─────────────┐      ┌──────────────────┐  │
│  │ BLE Central │      │ BLE Peripheral   │  │
│  │  (HID Host) │      │  (APP 数据通道)  │  │
│  └──────┬──────┘      └────────┬─────────┘  │
│         │                      │             │
│         ▼                      ▼             │
│  BLE 键盘/鼠标           手机 APP            │
│         │                      │             │
│         │ HID Report           │ Write/Notify│
│         │                      │             │
│  ┌──────┴──────────────────────┴──────┐      │
│  │     bt_core / protocol / spi_slave  │      │
│  │         统一封装 → SPI 上报          │      │
│  └─────────────────────────────────────┘      │
└─────────────────────────────────────────────┘
```

---

## 5. 移植路径（按文件逐步实施）

### Step 0：工程配置（准备工作）

1. 在 MRS 工程配置中：
   - 添加 `src/` 为源码目录
   - 添加 `evt/HAL/include`、`evt/LIB`、`evt/MultiCentPeri/APP/include`、`evt/MultiCentPeri/Profile/include` 为头文件搜索路径
   - 链接 `evt/LIB/libCH58xBLE.a`
   - 定义全局宏：`CHIP_ID=ID_CH585`、`DEBUG=1`

2. 复制官方文件到工程（作为移植基础，后续逐步替换）：
   - `evt/HAL/RTC.c` → `src/`
   - `evt/HAL/MCU.c` → `src/`（后续改名为 `bt_ble_init.c` 或合并到 `bt_core.c`）

### Step 1：基础骨架（P0）

| 优先级 | 文件 | 说明 |
|--------|------|------|
| 1 | `data_queue.c/h` | 无依赖，先实现 |
| 2 | `protocol.c/h` | 纯逻辑，可单元测试 |
| 3 | `spi_slave.c/h` | 依赖 protocol，对接外设 |
| 4 | `hardware.c/h` | 板级初始化，整合上述模块 |
| 5 | `main.c` | 搭出可编译运行的骨架 |

**验收**：`main.c` 能运行，UART1 输出初始化日志，SPI 中断能触发。

### Step 2：协议握手（P0 闭环）

| 优先级 | 任务 | 说明 |
|--------|------|------|
| 1 | 实现 `CMD_BT_GET_STATUS` 处理 | CH585F 回复 ACK + 状态数据 |
| 2 | 实现 `CMD_BT_RESET` 处理 | 软复位确认 |
| 3 | 实现 `CMD_GET_TYPE` 响应 | 返回模块类型 `0x02`、子类型 `0x01` |
| 4 | 与 Core 联调 | V5F 发送查询，CH585F 正确回复 |

### Step 3：BLE Central（P1）

| 优先级 | 文件/任务 | 移植来源 |
|--------|-----------|---------|
| 1 | `bt_central.c/h` | 基于 `multiCentral.c` |
| 2 | 实现扫描请求/响应 | `CMD_BT_EXT_SCAN` / `CMD_BT_EXT_DEVICE_LIST` |
| 3 | 实现连接/断开 | `CMD_BT_EXT_CONNECT` |
| 4 | 实现配对管理 | `CMD_BT_EXT_PAIRING_MGMT` |
| 5 | 开机自动回连 | 从 SNV 读取已配对列表 |

### Step 4：BLE Peripheral + APP Profile（P1-P3）

| 优先级 | 文件/任务 | 移植来源 |
|--------|-----------|---------|
| 1 | `bt_peripheral.c/h` | 基于 `multiPeripheral.c` |
| 2 | `bt_app_profile.c/h` | 基于 `gattprofile.c` 改造 |
| 3 | 广播名称/UUID 可配置 | `CMD_BT_EXT_SET_MODE` |
| 4 | APP CLI 数据透传 | `CMD_BT_EXT_CLI_DATA` |

### Step 5：HID Host（P2）

| 优先级 | 文件/任务 | 说明 |
|--------|-----------|------|
| 1 | `bt_hid_host.c/h` | 新增，参考 HOGP 规范 |
| 2 | Service/Characteristic 发现 | 在连接后自动执行 |
| 3 | CCCD 订阅 | 使能 HID Report Notify |
| 4 | Boot Report 解析 | 键盘 8 字节 / 鼠标 4 字节 |
| 5 | 上报 `CMD_BT_HID_REPORT` | 通过 SPI 发给 Core |

### Step 6：优化与完善

- 长数据分包传输（APP 文件传输 > MTU）
- INT 中断线通知（若硬件支持）
- 低功耗模式（`HAL_SLEEP`）
- 配对数限制与 LRU 淘汰

---

## 6. 关键配置项（CONFIG.h 建议）

```c
/* 在官方 CONFIG.h 基础上增加/修改 */

#define CHIP_ID                     ID_CH585

/* BLE 内存与连接数 */
#define BLE_MEMHEAP_SIZE            (1024 * 8)      /* 8KB 协议栈内存 */
#define PERIPHERAL_MAX_CONNECTION   1               /* APP 连接 */
#define CENTRAL_MAX_CONNECTION      3               /* HID 设备连接 */

/* SNV：配对绑定持久化 */
#define BLE_SNV                     TRUE
#define BLE_SNV_ADDR                (0x77000 - FLASH_ROM_MAX_SIZE)
#define BLE_SNV_BLOCK               256
#define BLE_SNV_NUM                 8               /* 最多 8 个配对设备 */

/* 数据包长度 */
#define BLE_BUFF_MAX_LEN            251             /* ATT_MTU=247，支持长数据 */
#define BLE_BUFF_NUM                10

/* 本项目不需要的功能 */
#define HAL_SLEEP                   FALSE
#define HAL_KEY                     FALSE
#define HAL_LED                     FALSE

/* 自定义广播名称长度 */
#define ADV_DEVICE_NAME_LEN         16
```

---

## 7. 命名规范 checklist

| 类别 | 规范 | 示例 |
|------|------|------|
| 文件 | 下划线 + 小写 | `bt_central.c`、`spi_slave.h` |
| 函数 | 首字母大写 + 下划线分隔 | `BT_Central_Init()`、`SPI_Slave_EnqueueTx()` |
| 变量 | 小写 + 下划线 | `bt_conn_status`、`spi_rx_buf` |
| 结构体 | 小写 + 下划线 + `_t` | `bt_device_info_t`、`proto_frame_t` |
| 宏/常量 | 全大写 + 下划线 | `PROTO_HEAD`、`CMD_BT_EXT_SCAN` |
| 全局变量 | `g_` 前缀 | `g_bt_status`、`g_spi_task_id` |

---

## 8. 风险与注意事项

1. **SPI Slave 上行数据时机**：CH585F 作为 Slave 无法主动发起传输。Phase 1 先采用**轮询/全双工复用**方案（V5F 定期发起 SPI 读取），后续硬件有 INT 线再改中断通知。

2. **BLE 吞吐量限制**：物理层 2Mbps，实际应用层约 100~200KB/s。文件传输需设计分块确认机制。

3. **TMOS 任务栈与中断**：BLE 协议栈运行在 TMOS 调度器中，SPI 中断需尽快退出，-heavy 工作放在任务中处理。

4. **配对密钥存储**：LTK 由 BLE 库通过 SNV 回调自动存入 Flash，本项目只需提供 `Lib_Read_Flash` / `Lib_Write_Flash` 即可（官方 `MCU.c` 已有实现）。

5. **多连接资源**：Central 3 + Peripheral 1 共 4 个连接，需确保 `BLE_MEMHEAP_SIZE` 和 `BLE_BUFF_NUM` 足够。

---

## 9. 版本规划

| 版本 | 阶段 | 内容 |
|------|------|------|
| v0.1 | P0 | 工程骨架 + SPI 通信 + 协议握手 |
| v0.2 | P1 | BLE Central/Peripheral 双角色运行 |
| v0.3 | P2 | HID over GATT Host |
| v0.4 | P3 | APP CLI 数据通道 + 文件传输 |
| v1.0 | —— | 稳定发布 |

---

> **下一步行动**：按 Step 1 开始实现基础骨架文件（`data_queue`、`protocol`、`spi_slave`、`hardware`、`main`）。
