/********************************** (C) COPYRIGHT *******************************
* File Name          : uart_module.h
* File Name          : uart_module.h
* Author             : LCD Model Team
* Version            : V3.0.0
 * Date               : 2026/05/23
 * Description        : UART1 module communication with Core.
 *                      Protocol frame parser, command dispatch, and input
 *                      event routing.  Implements Protocol_Display.md V3.0.
 *                      V3.0: 全面迁移至 CLI 直通架构，废弃自定义文件/音乐操作码。
********************************************************************************/
#ifndef __UART_MODULE_H
#define __UART_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 *  Protocol Frame Constants  (PROJECT.md §2)
 *=============================================================================*/

#define PROTO_FRAME_HEAD        0xAA
#define PROTO_FRAME_TAIL0       0xA5
#define PROTO_FRAME_TAIL1       0x5A
#define PROTO_FRAME_TAIL2       0xFC
#define PROTO_FRAME_TAIL3       0xFD

#define PROTO_MAX_DATA_LEN      255
#define PROTO_STREAM_MARKER     0xFF   /* LEN=0xFF → stream frame */

/*=============================================================================
 *  Module IDs  (PROJECT.md §3)
 *=============================================================================*/

#define MODULE_ID_CORE          0x00
#define MODULE_ID_WIRELESS      0x01
#define MODULE_ID_DISPLAY       0x10
#define MODULE_ID_KEYBOARD      0x20
#define MODULE_ID_POWER         0x30
#define MODULE_ID_SUBMODEL1     0x40
#define MODULE_ID_SUBMODEL2     0x41
#define MODULE_ID_SUBMODEL3     0x42

/*=============================================================================
 *  Generic Opcodes  (PROJECT.md §4: 0x00–0x0F)
 *=============================================================================*/

#define CMD_NOP                 0x00
#define CMD_GET_TYPE            0x01
#define CMD_GET_STATUS          0x02
#define CMD_SET_CONFIG          0x03
#define CMD_ACK                 0x04
#define CMD_NACK                0x05
#define CMD_EVT_NOTIFY          0x06
#define CMD_DATA_STREAM         0x07

/* NACK error codes (PROJECT.md §4.1) */
#define PROTO_ERR_NONE              0x00
#define PROTO_ERR_UNSUPPORTED_CMD   0x01
#define PROTO_ERR_INVALID_PARAM     0x02
#define PROTO_ERR_LEN_MISMATCH      0x03
#define PROTO_ERR_BUSY              0x04
#define PROTO_ERR_HW_FAULT          0x05
#define PROTO_ERR_TIMEOUT           0x06

/*=============================================================================
 *  Display Basic Opcodes  (Protocol_Display.md §2.1: 0x11–0x1F)
 *=============================================================================*/

#define CMD_DISP_SET_BRIGHTNESS     0x11   /* Set  → fire-and-forget */
#define CMD_DISP_GET_BRIGHTNESS     0x12   /* Query → ACK + brightness */
#define CMD_DISP_SHOW_PAGE          0x13   /* Set  → fire-and-forget */
#define CMD_DISP_UPDATE_STATUS      0x14   /* Set  → fire-and-forget */
#define CMD_DISP_INPUT_EVENT        0x15   /* Event → no reply */
#define CMD_DISP_SET_ROTATION       0x16   /* Set  → fire-and-forget */
#define CMD_DISP_GET_ROTATION       0x17   /* Query → ACK + angle */
#define CMD_DISP_SCREEN_CONTROL     0x18   /* Set  → fire-and-forget */
#define CMD_DISP_GET_SCREEN_STATE   0x19   /* Query → ACK + state */
#define CMD_DISP_SHOW_NOTICE        0x1A   /* Set  → fire-and-forget */
/* 0x1B CMD_DISP_MUSIC_CONTROL   — 废弃 (V3.0 CLI 直通替代) */
#define CMD_DISP_MUSIC_STATUS       0x1C   /* Event (Core→Display) */
/* 0x1D CMD_DISP_VOLUME_CONTROL   — 废弃 (V3.0 CLI 直通替代) */
#define CMD_DISP_FACTORY_RESET      0x1E   /* Set  → fire-and-forget */

/* Extended opcode dispatcher: CMD=0x10, DATA[0]=sub-command */
#define CMD_DISP_EXT                0x10

/*=============================================================================
 *  Display Extended Opcodes  (Protocol_Display.md §2.2: CMD=0x10, DATA[0])
 *=============================================================================*/

#define DISP_EXT_APP_LAUNCH         0x01
#define DISP_EXT_APP_CLOSE          0x02
#define DISP_EXT_APP_DATA           0x03
#define DISP_EXT_MODULE_STATUS      0x04
#define DISP_EXT_GET_MODULE_STATUS  0x05
/* 0x06-0x0B: 废弃 (V3.0 CLI 直通替代)  — FILE_LIST/READ/SAVE/OP/PLAY_MUSIC */
#define DISP_EXT_BT_EVENT           0x0C
#define DISP_EXT_BT_CONTROL         0x0D
#define DISP_EXT_SUBMODEL_EVENT     0x0E
#define DISP_EXT_POWER_EVENT        0x0F
#define DISP_EXT_SAVE_CONFIG        0x10
#define DISP_EXT_LOAD_CONFIG        0x11
#define DISP_EXT_CONFIG_RESULT      0x12
#define DISP_EXT_SET_RGB_MODE       0x13
/* 0x14 DISP_EXT_BULK_TRANSFER — 废弃 (V3.0 CLI 直通替代) */
#define DISP_EXT_SUBDISP_CONTENT    0x15
#define DISP_EXT_SUBDISP_CONFIG     0x16
#define DISP_EXT_ERROR_REPORT       0x17
#define DISP_EXT_HID_STATUS         0x18    /* 外接 HID 设备连接/断开状态 */
/* 0x19 DISP_EXT_CD — 废弃 (V3.0 CLI 直通替代) */
#define DISP_EXT_CLI                0x1A    /* CLI 命令直通 (Display→Core) */
#define DISP_EXT_CWD_NOTIFY         0x1B    /* CWD 变更通知 (Core→Display) */

/* HID 设备类型 (DISP_EXT_HID_STATUS DATA[2]) */
#define HID_DEV_KEYBOARD            0x01    /* 外接键盘 */
#define HID_DEV_MOUSE               0x02    /* 外接鼠标 */

/* HID 设备事件 (DISP_EXT_HID_STATUS DATA[1]) */
#define HID_EVT_CONNECTED           0x01    /* 设备已连接 */
#define HID_EVT_DISCONNECTED        0x00    /* 设备已断开 */

/*=============================================================================
 *  Input Event Device Types  (Protocol_Display.md §5.1)
 *=============================================================================*/

#define INPUT_DEV_KEYBOARD      0x00
#define INPUT_DEV_MOUSE         0x01
#define INPUT_DEV_TOUCH         0x02
#define INPUT_DEV_CORE_KEY      0x03

/* Touch event sub-types */
#define TOUCH_EVT_PRESS         0x00
#define TOUCH_EVT_MOVE          0x01
#define TOUCH_EVT_RELEASE       0x02
#define TOUCH_EVT_LONG_PRESS    0x03
#define TOUCH_EVT_DOUBLE_CLICK  0x04

/* Core key actions */
#define CORE_KEY_RELEASE        0x00
#define CORE_KEY_PRESS          0x01
#define CORE_KEY_LONG_PRESS     0x02

/*=============================================================================
 *  Screen Control Sub-types  (Protocol_Display.md §5.5)
 *=============================================================================*/

#define SCREEN_CTRL_OFF             0x00
#define SCREEN_CTRL_ON              0x01
#define SCREEN_CTRL_SET_AUTO_OFF    0x02
#define SCREEN_CTRL_GET_AUTO_OFF    0x03

/*=============================================================================
 *  Module Status Event Types  (Protocol_Display.md §5.7)
 *=============================================================================*/

#define MODULE_EVT_INSERTED     0x00
#define MODULE_EVT_REMOVED      0x01
#define MODULE_EVT_LIST         0x02

/* Module type IDs (PROJECT.md §4.2) */
#define MODULE_TYPE_DISPLAY     0x01
#define MODULE_TYPE_WIRELESS    0x02
#define MODULE_TYPE_POWER       0x03
#define MODULE_TYPE_KEYBOARD    0x04
#define MODULE_TYPE_SUBMODEL    0x05

/*=============================================================================
 *  BT Event Types  (Protocol_Display.md §5.10)
 *=============================================================================*/

#define BT_EVT_CONNECTED        0x00
#define BT_EVT_DISCONNECTED     0x01
#define BT_EVT_CONN_FAILED      0x02
#define BT_EVT_SCAN_RESULT      0x03
#define BT_EVT_SCAN_COMPLETE    0x04
#define BT_EVT_PAIR_RESULT      0x05

/* BT device types */
#define BT_DEV_UNKNOWN          0x00
#define BT_DEV_BLE_KEYBOARD     0x01
#define BT_DEV_BLE_MOUSE        0x02
#define BT_DEV_BLE_HID_OTHER    0x03
#define BT_DEV_PHONE_PC         0x04

/* BT control types (Protocol_Display.md §5.11) */
#define BT_CTRL_SCAN_START      0x00
#define BT_CTRL_SCAN_STOP       0x01
#define BT_CTRL_CONNECT         0x02
#define BT_CTRL_DISCONNECT      0x03
#define BT_CTRL_DISCONNECT_ALL  0x04
#define BT_CTRL_SET_DISCOVER    0x05

/*=============================================================================
 *  Submodel Sub-types  (Protocol_Display.md §5.12)
 *=============================================================================*/

#define SUBMODEL_FINGERPRINT    0x01
#define SUBMODEL_HEALTH         0x02
#define SUBMODEL_NFC            0x03
#define SUBMODEL_TOUCH_RING     0x04
#define SUBMODEL_RGB            0x05
#define SUBMODEL_INFRARED       0x06
#define SUBMODEL_SUBDISPLAY     0x07

/*=============================================================================
 *  Power Event Types  (Protocol_Display.md §5.13)
 *=============================================================================*/

#define POWER_EVT_STATUS_CHANGE 0x00
#define POWER_EVT_CHARGE        0x01
#define POWER_EVT_ALARM         0x02

/*=============================================================================
 *  File List Entry (parsed from CLI "ls" output, Protocol_Display.md §4.5)
 *=============================================================================*/

#define FILE_NAME_MAX_LEN       64
#define FILE_LIST_MAX_ENTRIES   12

#define FILE_ATTR_IS_DIR        (1 << 0)
#define FILE_ATTR_READONLY      (1 << 1)
#define FILE_ATTR_HIDDEN        (1 << 2)
#define FILE_ATTR_HAS_LFN       (1 << 3)

typedef struct {
    uint8_t  attr;
    uint32_t size;
    char     name[FILE_NAME_MAX_LEN + 1];
} file_entry_t;

/*=============================================================================
 *  File Operation Types (convenience, internally mapped to CLI commands)
 *=============================================================================*/

#define FILE_OP_MKDIR           0x00
#define FILE_OP_DELETE_FILE     0x01
#define FILE_OP_DELETE_DIR      0x02
#define FILE_OP_RENAME          0x03

/*=============================================================================
 *  Pending Request Types (for ACK/NACK routing)
 *=============================================================================*/

typedef enum {
    PENDING_NONE = 0,
    PENDING_CLI,           /* CLI command in flight */
} pending_req_t;

extern volatile pending_req_t g_pending_req;

/*=============================================================================
 *  App-Layer Callback Interface
 *
 *  All Display→Core requests now use CLI passthrough. The Core executes
 *  CLI commands and returns text output via DISP_EXT_CLI. The app layer
 *  receives the full CLI text response and parses it as needed.
 *
 *  Event notifications (Core→Display) still use their specific opcodes
 *  (MODULE_STATUS, BT_EVENT, POWER_EVENT, etc.).
 *=============================================================================*/

typedef struct {
    /* CLI response: called when Core returns CLI output text.
     * 'output' is NOT null-terminated; use 'len' for length.
     * 'truncated' is true if output was truncated due to frame size limits.
     * 'is_last' is true for the final frame of a multi-frame response. */
    void (*on_cli_response)(const char *output, uint16_t len, bool truncated, bool is_last);

    /* Legacy: file list parsed from "ls" CLI output.
     * If set, the UART module will auto-parse ls output and call this.
     * Otherwise, raw CLI text goes to on_cli_response. */
    void (*on_file_list)(uint8_t status, const file_entry_t *entries, uint8_t count);

    /* CWD change notification: called when Core pushes a CWD_NOTIFY frame.
     * This happens when cd/device command succeeds on Core side,
     * allowing Display to synchronize its local path display. */
    void (*on_cwd_notify)(const char *path);
} uart_app_callbacks_t;

void UART_SetAppCallbacks(const uart_app_callbacks_t *cb);
void UART_ClearAppCallbacks(void);

/*=============================================================================
 *  Music Control Types  (Protocol_Display.md §5.2)
 *=============================================================================*/

#define MUSIC_CTRL_TOGGLE       0x00
#define MUSIC_CTRL_PLAY         0x01
#define MUSIC_CTRL_PAUSE        0x02
#define MUSIC_CTRL_STOP         0x03
#define MUSIC_CTRL_PREV         0x04
#define MUSIC_CTRL_NEXT         0x05
#define MUSIC_CTRL_FAST_FWD     0x06
#define MUSIC_CTRL_FAST_REV     0x07
#define MUSIC_CTRL_SET_MODE     0x08

/* Music playback states (Protocol_Display.md §5.3) */
#define MUSIC_STATE_IDLE        0x00
#define MUSIC_STATE_PLAYING     0x01
#define MUSIC_STATE_PAUSED      0x02
#define MUSIC_STATE_STOPPED     0x03

/*=============================================================================
 *  Status Bar Bitmask  (Protocol_Display.md §5.6)
 *=============================================================================*/

#define STATUS_BIT_BATTERY      (1 << 0)
#define STATUS_BIT_BLUETOOTH    (1 << 1)
#define STATUS_BIT_TIME         (1 << 2)
#define STATUS_BIT_CHARGING     (1 << 3)
#define STATUS_BIT_CURRENT_APP  (1 << 4)

/*=============================================================================
 *  Display State (internal)
 *=============================================================================*/

typedef struct {
    bool     screen_on;           /* Screen powered on */
    uint8_t  brightness;          /* Current brightness 0-100 */
    uint8_t  rotation;            /* Current rotation 0/90/180/270 */
    uint16_t auto_off_sec;        /* Auto screen-off timeout (0=disabled) */
    uint32_t last_activity_ms;    /* Last user activity timestamp */

    /* Status bar cache (from Core) */
    uint8_t  battery_pct;
    uint8_t  bt_connected;
    uint32_t unix_time;
    uint8_t  charge_state;
    uint8_t  current_app_id;
    uint8_t  status_valid;        /* bitmask of which fields are valid */

    /* Music state cache (from Core) */
    uint8_t  music_state;
    uint32_t music_pos_ms;
    uint32_t music_dur_ms;
    uint8_t  music_volume;
    char     music_track[64];
} uart_disp_state_t;

extern uart_disp_state_t g_disp_state;

/*=============================================================================
 *  UART Configuration
 *=============================================================================*/

#define UART_RX_BUF_SIZE    512

/*=============================================================================
 *  API
 *=============================================================================*/

/* Initialisation and polling */
void UART_Module_Init(void);
void UART_Module_Poll(void);

/* Send a standard protocol frame */
void UART_SendFrame(uint8_t dst, uint8_t cmd, const uint8_t *data, uint8_t len);

/* Send ACK (empty or with data) */
void UART_SendACK(uint8_t dst, const uint8_t *data, uint8_t data_len);

/* Send NACK with error code */
void UART_SendNACK(uint8_t dst, uint8_t error_code);

/* --- Display → Core request helpers (all via CLI passthrough) --- */

/* Send a CLI command to Core for execution.
 * The response will arrive via on_cli_response callback. */
void UART_SendCLI(const char *cmd);

/* Convenience wrappers that build CLI commands internally: */

/* List directory: if dir_or_null is non-NULL and non-empty, sends "cd <dir>"
 * then auto-sends "ls" on cd response; if NULL or empty, sends "ls" directly.
 * dir_or_null must be a single-level name (e.g. "DOC", "..", "\"). */
void UART_RequestFileList(const char *dir_or_null);

/* Change directory: sends "cd <dir>" */
void UART_SendCD(const char *dir);

/* Go to parent directory: sends "cd .." */
void UART_SendCDUp(void);

/* Print working directory: sends "pwd" */
void UART_SendPWD(void);

/* Play music file: sends "play <path>" */
void UART_SendPlayMusic(const char *path);

/* Music controls: send "pause", "resume", "stop", "vol <n>", etc. */
void UART_SendMusicControl(uint8_t ctrl_type, uint8_t param);

/* Volume control: sends "vol <n>" */
void UART_SendVolumeControl(uint8_t op, uint8_t volume);

/* File operations: sends "mkdir <path>", "rm <path>", etc. */
void UART_SendFileOperation(uint8_t op_type, const char *path);

/* Read file content: sends "cat <path>" */
void UART_RequestFileRead(const char *path);

/* Save file content: sends "echo <content> > <path>" (for small files) */
void UART_RequestFileSave(const char *path);

/* Send BT control request (still uses DISP_EXT_BT_CONTROL) */
void UART_SendBTControl(uint8_t ctrl_type, const uint8_t *param, uint8_t param_len);

/* Report error to Core (still uses DISP_EXT_ERROR_REPORT) */
void UART_SendErrorReport(uint8_t error_code, const char *msg);

/* Notify activity (reset auto-off timer) — called from input system */
void UART_NotifyActivity(void);

#ifdef __cplusplus
}
#endif

#endif /* __UART_MODULE_H */
