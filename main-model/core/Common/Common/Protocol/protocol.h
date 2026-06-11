#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"
#include <string.h>

/* ============================================================================
 * 协议常量定义
 * ============================================================================ */

/* 帧头 */
#define PROTO_FRAME_HEAD        0xAA

/* 帧尾 */
#define PROTO_FRAME_TAIL0       0xA5
#define PROTO_FRAME_TAIL1       0x5A
#define PROTO_FRAME_TAIL2       0xFC
#define PROTO_FRAME_TAIL3       0xFD

/* 通用操作码 (0x00 ~ 0x0F) */
#define CMD_NOP                 0x00
#define CMD_GET_TYPE            0x01
#define CMD_GET_STATUS          0x02
#define CMD_SET_CONFIG          0x03
#define CMD_ACK                 0x04
#define CMD_NACK                0x05
#define CMD_EVT_NOTIFY          0x06
#define CMD_DATA_STREAM         0x07

/* ============================================================================
 * Display 基础操作码 (CMD = 0x10 ~ 0x1F)
 * ============================================================================ */
#define CMD_DISP_EXTENSION              0x10
#define CMD_DISP_SET_BRIGHTNESS         0x11
#define CMD_DISP_GET_BRIGHTNESS         0x12
#define CMD_DISP_SHOW_PAGE              0x13
#define CMD_DISP_UPDATE_STATUS          0x14
#define CMD_DISP_INPUT_EVENT            0x15
#define CMD_DISP_SET_ROTATION           0x16
#define CMD_DISP_GET_ROTATION           0x17
#define CMD_DISP_SCREEN_CONTROL         0x18
#define CMD_DISP_GET_SCREEN_STATE       0x19
#define CMD_DISP_SHOW_NOTICE            0x1A
/* 0x1B CMD_DISP_MUSIC_CONTROL — 废弃 (V3.0 CLI 直通替代) */
#define CMD_DISP_MUSIC_STATUS           0x1C
/* 0x1D CMD_DISP_VOLUME_CONTROL — 废弃 (V3.0 CLI 直通替代) */
#define CMD_DISP_FACTORY_RESET          0x1E
/* 0x1F 预留 */

/* ============================================================================
 * Display 扩展操作码 (CMD = 0x10, DATA[0] 为子命令)
 * ============================================================================ */
#define CMD_DISP_EXT_APP_LAUNCH         0x01
#define CMD_DISP_EXT_APP_CLOSE          0x02
#define CMD_DISP_EXT_APP_DATA           0x03
#define CMD_DISP_EXT_MODULE_STATUS      0x04
#define CMD_DISP_EXT_GET_MODULE_STATUS  0x05
/* 0x06~0x0B: 废弃 (V3.0 CLI 直通替代) — FILE_LIST/READ/SAVE/OP/PLAY_MUSIC */
#define CMD_DISP_EXT_BT_EVENT           0x0C
#define CMD_DISP_EXT_BT_CONTROL         0x0D
#define CMD_DISP_EXT_SUBMODEL_EVENT     0x0E
#define CMD_DISP_EXT_POWER_EVENT        0x0F
#define CMD_DISP_EXT_SAVE_CONFIG        0x10
#define CMD_DISP_EXT_LOAD_CONFIG        0x11
#define CMD_DISP_EXT_CONFIG_RESULT      0x12
#define CMD_DISP_EXT_SET_RGB_MODE       0x13
/* 0x14: 废弃 (V3.0 CLI 直通替代) — BULK_TRANSFER */
#define CMD_DISP_EXT_SUBDISP_CONTENT    0x15
#define CMD_DISP_EXT_SUBDISP_CONFIG     0x16
#define CMD_DISP_EXT_ERROR_REPORT       0x17
#define CMD_DISP_EXT_HID_STATUS         0x18    /* 外接 HID 设备连接/断开状态 */
/* 0x19: 废弃 (V3.0 CLI 直通替代) — CD */
#define CMD_DISP_EXT_CLI                0x1A    /* CLI 命令直通 (Display→Core) */
#define CMD_DISP_EXT_CWD_NOTIFY         0x1B    /* CWD 变更通知 (Core→Display) */

/* ---- HID 设备类型 (CMD_DISP_EXT_HID_STATUS DATA[2]) ---- */
#define HID_DEV_KEYBOARD                0x01    /* 外接键盘 */
#define HID_DEV_MOUSE                   0x02    /* 外接鼠标 */

/* ---- HID 设备事件 (CMD_DISP_EXT_HID_STATUS DATA[1]) ---- */
#define HID_EVT_CONNECTED               0x01    /* 设备已连接 */
#define HID_EVT_DISCONNECTED            0x00    /* 设备已断开 */

/* ---- 输入事件设备类型 (CMD_DISP_INPUT_EVENT DATA[0]) ---- */
#define INPUT_DEV_KEYBOARD              0x00
#define INPUT_DEV_MOUSE                 0x01
#define INPUT_DEV_TOUCH                 0x02
#define INPUT_DEV_CORE_KEY              0x03

/* ---- Core 按键标识 (INPUT_DEV_CORE_KEY DATA[2]) ---- */
#define CORE_KEY_PLUS                   0x01
#define CORE_KEY_SUB                    0x02
#define CORE_KEY_ENTER                  0x03

/* ---- Core 按键事件 (INPUT_DEV_CORE_KEY DATA[1]) ---- */
#define CORE_KEY_EVT_RELEASE            0x00
#define CORE_KEY_EVT_PRESS              0x01
#define CORE_KEY_EVT_LONG_PRESS         0x02

/* ---- 音乐控制类型 (已废弃，V3.0 CLI 直通替代：pause/resume/stop/prev/next/vol) ---- */
/* MUSIC_CTRL_* 常量仅在 Display 侧 uart_module.h 中保留作为便利封装 */

/* ---- 播放模式 (已废弃，V3.0 CLI 直通替代：mode 命令) ---- */
#define MUSIC_MODE_SINGLE              0x00
#define MUSIC_MODE_SEQUENCE             0x01
#define MUSIC_MODE_SHUFFLE              0x02

/* ---- 播放状态 (CMD_DISP_MUSIC_STATUS DATA[0]) ---- */
#define MUSIC_STATE_IDLE                0x00
#define MUSIC_STATE_PLAYING             0x01
#define MUSIC_STATE_PAUSED              0x02
#define MUSIC_STATE_STOPPED             0x03

/* ---- 音量控制操作 (已废弃，V3.0 CLI 直通替代：vol 命令) ---- */
#define VOLUME_OP_SET                   0x00
#define VOLUME_OP_GET                   0x01

/* ---- 屏幕控制类型 (CMD_DISP_SCREEN_CONTROL DATA[0]) ---- */
#define SCREEN_CTRL_SLEEP               0x00
#define SCREEN_CTRL_WAKEUP              0x01
#define SCREEN_CTRL_SET_TIMEOUT         0x02
#define SCREEN_CTRL_GET_TIMEOUT         0x03

/* ---- 蓝牙事件类型 (CMD_DISP_EXT_BT_EVENT DATA[1]) ---- */
#define BT_EVT_CONNECTED                0x00
#define BT_EVT_DISCONNECTED             0x01
#define BT_EVT_CONN_FAILED              0x02
#define BT_EVT_SCAN_RESULT              0x03
#define BT_EVT_SCAN_COMPLETE            0x04
#define BT_EVT_PAIR_RESULT              0x05

/* ---- 蓝牙控制类型 (CMD_DISP_EXT_BT_CONTROL DATA[1]) ---- */
#define BT_CTRL_SCAN_START              0x00
#define BT_CTRL_SCAN_STOP               0x01
#define BT_CTRL_CONNECT                 0x02
#define BT_CTRL_DISCONNECT              0x03
#define BT_CTRL_DISCONNECT_ALL          0x04
#define BT_CTRL_SET_DISCOVERABLE        0x05

/* ---- Submodel 子类型 (CMD_DISP_EXT_SUBMODEL_EVENT DATA[1]) ---- */
#define SUBMODEL_TYPE_FINGERPRINT       0x01
#define SUBMODEL_TYPE_HEALTH            0x02
#define SUBMODEL_TYPE_NFC               0x03
#define SUBMODEL_TYPE_TOUCH_RING        0x04
#define SUBMODEL_TYPE_RGB               0x05
#define SUBMODEL_TYPE_INFRARED          0x06
#define SUBMODEL_TYPE_SUB_DISPLAY       0x07

/* ---- 电源事件类型 (CMD_DISP_EXT_POWER_EVENT DATA[1]) ---- */
#define POWER_EVT_STATUS_CHANGE         0x00
#define POWER_EVT_CHARGE                0x01
#define POWER_EVT_ALARM                 0x02

/* ---- 文件操作类型 (已废弃，V3.0 CLI 直通替代) ---- */
/* FILE_OP_MKDIR/DELETE/RMDIR/RENAME → 使用 CLI mkdir/rm/rm -rf 命令 */

/* ============================================================================
 * Keyboard 基础操作码 (CMD = 0x21 ~ 0x2F)
 * ============================================================================ */
#define CMD_KBD_SET_BACKLIGHT           0x21
#define CMD_KBD_GET_BACKLIGHT           0x22
#define CMD_KBD_SET_CONFIG              0x23
#define CMD_KBD_GET_STATUS              0x24
#define CMD_KBD_HID_REPORT              0x25
#define CMD_KBD_MUSIC_KEYS              0x26
#define CMD_KBD_MUSIC_BUTTONS           0x27
#define CMD_KBD_MUSIC_FADERS            0x28
#define CMD_KBD_GAME_INPUT              0x29
#define CMD_KBD_MUSIC_EVENT_CTRL        0x2A

/* ============================================================================
 * Power 基础操作码 (CMD = 0x31 ~ 0x3F)
 * ============================================================================ */
#define CMD_PWR_GET_STATUS              0x31
#define CMD_PWR_GET_BATTERY_INFO        0x32
#define CMD_PWR_SET_CHARGE_POLICY       0x33
#define CMD_PWR_SET_OUTPUT_POLICY       0x34
#define CMD_PWR_SET_ALARM_THRESHOLD     0x35
#define CMD_PWR_STATUS_REPORT           0x36
#define CMD_PWR_CHARGE_EVENT            0x37
#define CMD_PWR_ALARM_EVENT             0x38
#define CMD_PWR_SET_REPORT_INTERVAL     0x39

/* ============================================================================
 * Submodel 基础操作码 (CMD = 0x40 ~ 0x4F)
 * ============================================================================
 * 所有 Submodel 子类型（指纹/健康/NFC/RGB/触摸/红外/副屏）共用以下操作码，
 * 通过 DATA[0] 子命令区分具体功能，各子类型的子命令定义见对应协议文档。
 * ============================================================================ */
#define CMD_SUB_EVT_NOTIFY              0x40
#define CMD_SUB_SET_MODE                0x41
#define CMD_SUB_GET_STATUS              0x42
#define CMD_SUB_DATA_REPORT             0x43
#define CMD_SUB_SET_CONFIG              0x44
#define CMD_SUB_ACTION_RESULT           0x45
#define CMD_SUB_BULK_TRANSFER           0x46

/* ============================================================================
 * Wireless 基础操作码 (CMD = 0x50 ~ 0x5F)
 * ============================================================================ */
#define CMD_BT_EXTENSION                0x50
#define CMD_BT_GET_STATUS               0x51
#define CMD_BT_SEND_DATA                0x52
/* 0x53 预留 */
#define CMD_BT_CONN_EVT                 0x54
#define CMD_BT_RECV_DATA                0x55
#define CMD_BT_HID_REPORT               0x56
/* 0x57 预留 */
#define CMD_BT_SET_DISCOVERABLE         0x58
#define CMD_BT_RESET                    0x59
/* 0x5A ~ 0x5F 预留 */

/* ============================================================================
 * Wireless 扩展操作码 (CMD = 0x50, DATA[0] 为子命令)
 * ============================================================================ */
#define CMD_BT_EXT_SCAN                 0x01
#define CMD_BT_EXT_DEVICE_LIST          0x02
#define CMD_BT_EXT_CONNECT              0x03
#define CMD_BT_EXT_PAIRING_MGMT         0x04
#define CMD_BT_EXT_SET_MODE             0x05
/* 0x06 预留 */
#define CMD_BT_EXT_CLI_DATA             0x07
/* 0x08~0x3F 预留 */

/* 模块 ID */
#define MODULE_ID_CORE          0x00
#define MODULE_ID_WIRELESS      0x01
#define MODULE_ID_DISPLAY       0x10
#define MODULE_ID_KEYBOARD      0x20
#define MODULE_ID_POWER         0x30
#define MODULE_ID_SUBMODEL_1    0x40
#define MODULE_ID_SUBMODEL_2    0x41
#define MODULE_ID_SUBMODEL_3    0x42

/* 模块类型编号 (CMD_GET_TYPE 响应 DATA[0]) */
#define MODULE_TYPE_RESERVED    0x00
#define MODULE_TYPE_DISPLAY     0x01
#define MODULE_TYPE_WIRELESS    0x02
#define MODULE_TYPE_POWER       0x03
#define MODULE_TYPE_KEYBOARD    0x04
#define MODULE_TYPE_SUBMODEL    0x05

/* ============================================================================
 * 模块子类型编号 (CMD_GET_TYPE 响应 DATA[1])
 * ----------------------------------------------------------------------------
 * 规则:
 *   - 0x00 : 保留 (RESERVED)
 *   - 0x01 : 该大类的标准/自身实现 (Standard / Self)
 *   - 0x02 ~ 0xFF : 扩展实现
 * ----------------------------------------------------------------------------
 * 模块的完整身份 = 模块类型 (DATA[0]) + 模块子类型 (DATA[1])
 * ============================================================================ */

/* Display 子类型 */
#define MODULE_SUBTYPE_DISPLAY_RESERVED     0x00
#define MODULE_SUBTYPE_DISPLAY_LCD          0x01
#define MODULE_SUBTYPE_DISPLAY_EINK         0x02

/* Wireless 子类型 */
#define MODULE_SUBTYPE_WIRELESS_RESERVED    0x00
#define MODULE_SUBTYPE_WIRELESS_BT          0x01    /* 标准蓝牙/无线模块 (CH585F) */

/* Power 子类型 */
#define MODULE_SUBTYPE_POWER_RESERVED       0x00
#define MODULE_SUBTYPE_POWER_STANDARD       0x01    /* 标准供电模块 */

/* Keyboard 子类型 */
#define MODULE_SUBTYPE_KEYBOARD_RESERVED    0x00
#define MODULE_SUBTYPE_KEYBOARD_MAIN        0x01
#define MODULE_SUBTYPE_KEYBOARD_GAME        0x02
#define MODULE_SUBTYPE_KEYBOARD_MUSIC       0x03

/* Submodel 子类型 */
#define MODULE_SUBTYPE_SUBMODEL_RESERVED    0x00
#define MODULE_SUBTYPE_SUBMODEL_FINGERPRINT 0x01
#define MODULE_SUBTYPE_SUBMODEL_HEALTH      0x02
#define MODULE_SUBTYPE_SUBMODEL_NFC         0x03
#define MODULE_SUBTYPE_SUBMODEL_TOUCH_RING  0x04
#define MODULE_SUBTYPE_SUBMODEL_RGB         0x05
#define MODULE_SUBTYPE_SUBMODEL_INFRARED    0x06
#define MODULE_SUBTYPE_SUBMODEL_SUB_DISPLAY 0x07

/* ---- Fingerprint (0x01) 子命令 ---- */
/* CMD_SUB_SET_MODE (0x41) sub-commands */
#define FP_SUB_ENROLL_START     0x01
#define FP_SUB_ENROLL_CANCEL    0x02
#define FP_SUB_SET_LED          0x03
#define FP_SUB_SET_SECURITY     0x04

/* CMD_SUB_SET_CONFIG (0x44) sub-commands */
#define FP_SUB_DELETE           0x01
#define FP_SUB_DELETE_ALL       0x02
#define FP_SUB_SLEEP            0x03

/* CMD_SUB_GET_STATUS (0x42) sub-commands */
#define FP_SUB_QUERY_LIST       0x00
#define FP_SUB_QUERY_COUNT      0x01

/* CMD_SUB_EVT_NOTIFY (0x40) sub-commands */
#define FP_SUB_IDENTIFY_OK      0x01
#define FP_SUB_IDENTIFY_FAIL    0x02

/* CMD_SUB_ACTION_RESULT (0x45) sub-commands */
#define FP_SUB_ENROLL_OK        0x01
#define FP_SUB_ENROLL_FAIL      0x02
#define FP_SUB_ENROLL_PROGRESS  0x03

/* CMD_SUB_BULK_TRANSFER (0x46) sub-commands (Fingerprint) */
#define FP_BULK_SUB_HANDSHAKE   0x01
#define FP_BULK_SUB_DATA        0x02
#define FP_BULK_SUB_COMPLETE    0x03

/* LED effect codes */
#define FP_LED_BREATH           0x01
#define FP_LED_FLICKER          0x02
#define FP_LED_ON               0x03
#define FP_LED_OFF              0x04
#define FP_LED_GRADUAL_ON       0x05
#define FP_LED_GRADUAL_OFF      0x06
#define FP_LED_HORSE            0x07

/* LED colors */
#define FP_COLOR_OFF            0x00
#define FP_COLOR_BLUE           0x01
#define FP_COLOR_GREEN          0x02
#define FP_COLOR_CYAN           0x03
#define FP_COLOR_RED            0x04
#define FP_COLOR_MAGENTA        0x05
#define FP_COLOR_YELLOW         0x06
#define FP_COLOR_WHITE          0x07

/* Security levels */
#define FP_SECURITY_LOW         0x01
#define FP_SECURITY_MEDIUM      0x02
#define FP_SECURITY_HIGH        0x03

/* ---- Health (0x02) 子命令 ---- */
/* CMD_SUB_SET_CONFIG (0x44) sub-commands */
#define HEALTH_SUB_SET_INTERVAL     0x01    /* 设置监测间隔(秒) [interval:2(BE)] */
#define HEALTH_SUB_START_MONITOR    0x02    /* 开始监测 */
#define HEALTH_SUB_STOP_MONITOR     0x03    /* 停止监测 */

/* CMD_SUB_GET_STATUS (0x42) sub-commands */
#define HEALTH_SUB_QUERY_DATA       0x00    /* 查询当前数据 → ACK [HR:1][SpO2:1][HRV:2(BE)] */

/* CMD_SUB_DATA_REPORT (0x43) sub-commands */
#define HEALTH_SUB_DATA_REPORT      0x01    /* 健康数据上报 [HR:1][SpO2:1][HRV:2(BE)] */

/* ---- NFC (0x03) 子命令 ---- */
/* CMD_SUB_EVT_NOTIFY (0x40) sub-commands */
#define NFC_SUB_CARD_DETECT         0x01    /* 卡片识别 [card_id:1][card_number:5] */

/* CMD_SUB_GET_STATUS (0x42) sub-commands */
#define NFC_SUB_QUERY_STATUS        0x00    /* 查询卡状态 → ACK [card_id:1][card_number:5] */

/* ---- SubDisplay (0x07) 子命令 ---- */
/* CMD_SUB_SET_MODE (0x41) sub-commands */
#define SUBDISP_SUBCMD_SET_STATUS       0x01
#define SUBDISP_SUBCMD_SET_CONTENT      0x02
#define SUBDISP_SUBCMD_CLEAR_SCREEN     0x03
#define SUBDISP_SUBCMD_SET_DISPLAY_MODE 0x04    /* 切换显示模式（状态/图片） */
#define SUBDISP_SUBCMD_SET_BMP_NAME     0x05    /* 设置 BMP 文件名 */
#define SUBDISP_SUBCMD_BMP_TRANS        0x10    /* BMP 图片传输（多帧） */
#define SUBDISP_SUBCMD_LS_DEV           0x11    /* 设备列表传输（多帧） */

/* CMD_SUB_GET_STATUS (0x42) sub-commands */
#define SUBDISP_SUBCMD_GET_SYS_STATUS   0x00    /* 请求系统状态 */
#define SUBDISP_SUBCMD_GET_LS_DEV       0x01    /* 请求设备列表 */
#define SUBDISP_SUBCMD_GET_BMP          0x02    /* 请求 BMP 图片 */

/* CMD_SUB_DATA_REPORT (0x43) sub-commands */
#define SUBDISP_SUBCMD_SYS_STATUS       0x20    /* 系统状态响应 */

/* Multi-frame FLAGS (same as CLI_FLAG_SOF/EOF pattern) */
#define SUBDISP_FLAG_SOF                0x01
#define SUBDISP_FLAG_EOF                0x02

/* SubDisplay display modes (for SUBDISP_SUBCMD_SET_DISPLAY_MODE) */
#define SUBDISP_MODE_STATUS             0x00    /* 状态显示模式（双页轮换） */
#define SUBDISP_MODE_IMAGE              0x01    /* 图片显示模式 */

/* 长度限制 */
#define PROTO_MAX_DATA_LEN      512
#define PROTO_MAX_FRAME_LEN     (5 + PROTO_MAX_DATA_LEN + 4)

/* 流式帧 LEN 特殊值 */
#define PROTO_STREAM_LEN        0xFF

/* 流式帧 DATA 建议上限（20KB），受实现限制 */
#define PROTO_STREAM_MAX_DATA   20480

/* 打包/解析辅助宏 */
#define PROTO_PACK_ERR              0       /* PackFrame 错误返回值 */
#define PROTO_DATA_LEN(frame)       ((frame).len > 0 ? (uint16_t)(frame).len - 1 : 0)
#define PROTO_FRAME_TOTAL_LEN(dlen) (5 + (dlen) + 4)   /* HEAD+SRC+DST+LEN+CMD+DATA+TAIL(4) */
#define PROTO_IS_STREAM_FRAME(frame) ((frame).len == PROTO_STREAM_LEN)

/* 各模块建议接收缓冲区大小 */
#define PROTO_BUF_SIZE_DISPLAY  512
#define PROTO_BUF_SIZE_KEYBOARD 64
#define PROTO_BUF_SIZE_POWER    64
#define PROTO_BUF_SIZE_SUBMODEL 256
#define PROTO_BUF_SIZE_WIRELESS 512

/* ============================================================================
 * 数据结构定义
 * ============================================================================ */

/* 接收状态机状态 */
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
    PROTO_STATE_WAIT_STREAM_DATA,       /* 流式帧：逐字节接收 DATA，扫描帧尾 */
    PROTO_STATE_TENTATIVE_TAIL1,        /* 流式帧：收到 TAIL0，试探 TAIL1 */
    PROTO_STATE_TENTATIVE_TAIL2,        /* 流式帧：收到 TAIL0+TAIL1，试探 TAIL2 */
    PROTO_STATE_TENTATIVE_TAIL3,        /* 流式帧：收到 TAIL0+TAIL1+TAIL2，试探 TAIL3 */
    PROTO_STATE_FRAME_READY
} protocol_state_t;

/* 协议帧结构（ unpacked 形式，便于访问） */
typedef struct {
    uint8_t  head;                      /* 0xAA */
    uint8_t  src;                       /* 源模块ID */
    uint8_t  dst;                       /* 目标模块ID */
    uint16_t len;                       /* CMD + DATA 总字节数；0xFF 表示流式帧 */
    uint8_t  cmd;                       /* 操作码 */
    uint8_t  data[PROTO_MAX_DATA_LEN];  /* 数据域 */
} protocol_frame_t;

/* 模块身份（CMD_GET_TYPE 响应 DATA[0..4] 的标准解析形式） */
typedef struct {
    uint8_t type;       /* DATA[0]: 模块类型编号 */
    uint8_t subtype;    /* DATA[1]: 模块子类型编号 */
    uint8_t hw_ver;     /* DATA[2]: 硬件版本号 */
    uint8_t fw_major;   /* DATA[3]: 固件主版本号 */
    uint8_t fw_minor;   /* DATA[4]: 固件次版本号 */
} module_identity_t;

/* 接收上下文 */
typedef struct {
    protocol_state_t state;             /* 当前解析状态 */
    protocol_frame_t frame;             /* 解析中的帧 */
    uint16_t data_idx;                  /* 当前数据域接收索引（支持流式帧 >255） */
    uint8_t  frame_ready;               /* 帧就绪标志 (1=就绪) */
    /* 错误统计（调试用，Init 时清零，Reset 不清零） */
    uint16_t err_len_zero;              /* LEN=0 非法帧计数 */
    uint16_t err_overflow;              /* DATA 域溢出计数 */
    uint16_t err_frame_ready;           /* 帧未取走又收到新帧计数 */
} protocol_rx_ctx_t;

/* ============================================================================
 * 函数声明
 * ============================================================================ */

/**
 * @brief  初始化接收上下文
 * @param  ctx 接收上下文指针
 * @note   建议在模块初始化时调用
 */
void Protocol_InitRxCtx(protocol_rx_ctx_t *ctx);

/**
 * @brief  重置接收上下文
 * @param  ctx 接收上下文指针
 * @note   在取出就绪帧后调用，以准备接收下一帧
 */
void Protocol_ResetRxCtx(protocol_rx_ctx_t *ctx);

/**
 * @brief  打包协议帧
 * @param  src       源模块ID
 * @param  dst       目标模块ID
 * @param  cmd       操作码
 * @param  data      数据域指针（可为NULL，当data_len=0时）
 * @param  data_len  数据域长度（0 ~ 255）
 * @param  out_buf   输出缓冲区
 * @param  out_size  输出缓冲区大小
 * @return 实际写入的字节数；0表示参数错误或缓冲区不足
 *
 * @note   输出格式: [HEAD][SRC][DST][LEN][CMD][DATA...][TAIL0][TAIL1][TAIL2][TAIL3]
 *         LEN = 1 + data_len（包含CMD自身）
 *         TAIL = A5 5A FC FD
 */
uint16_t Protocol_PackFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                           const uint8_t *data, uint8_t data_len,
                           uint8_t *out_buf, uint16_t out_size);

/**
 * @brief  打包流式协议帧（LEN = 0xFF，DATA 长度不受 255 限制）
 * @param  src       源模块ID
 * @param  dst       目标模块ID
 * @param  cmd       操作码
 * @param  data      数据域指针
 * @param  data_len  数据域长度（1 ~ PROTO_STREAM_MAX_DATA）
 * @param  out_buf   输出缓冲区
 * @param  out_size  输出缓冲区大小
 * @return 实际写入的字节数；0 表示参数错误或缓冲区不足
 *
 * @note   流式帧格式: [HEAD][SRC][DST][0xFF][CMD][DATA...][TAIL0][TAIL1][TAIL2][TAIL3]
 *         DATA 长度由 out_buf 容量决定，建议单帧不超过 20KB
 */
uint16_t Protocol_PackStreamFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                                  const uint8_t *data, uint16_t data_len,
                                  uint8_t *out_buf, uint16_t out_size);

/**
 * @brief  接收状态机逐字节处理
 * @param  ctx   接收上下文指针
 * @param  byte  新接收到的字节
 * @return 1=帧已就绪（ctx->frame 有效），0=未就绪
 *
 * @note   - 返回1后，必须通过 Protocol_ResetRxCtx() 重置状态机才能继续接收。
 *         - 仅在 WAIT_HEAD 状态下识别 0xAA 为新帧起始，数据域中的 0xAA 不会触发重解析。
 *         - 若 LEN 字段为 0（非法），状态机自动丢弃并回到 WAIT_HEAD。
 *         - 若数据域长度超过 PROTO_MAX_DATA_LEN，超出的字节会被丢弃但不影响帧完成判断。
 */
uint8_t Protocol_ParseByte(protocol_rx_ctx_t *ctx, uint8_t byte);

/**
 * @brief  从协议帧中解析模块身份
 * @param  frame  协议帧指针（需确保 frame->len >= 2）
 * @param  out    输出模块身份结构体
 * @return 1=解析成功，0=帧数据不足
 *
 * @note   仅解析 DATA[0..4]，不检查 frame->cmd 是否为 CMD_GET_TYPE。
 *         调用者应自行确认当前帧是类型响应帧。
 */
uint8_t Protocol_ParseIdentity(const protocol_frame_t *frame, module_identity_t *out);

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H__ */
