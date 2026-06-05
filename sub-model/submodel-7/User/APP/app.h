/**
 * @file app.h
 * @brief Submodel-7 应用层：命令处理 + 布局管理 + 主循环
 */
#ifndef __APP_H__
#define __APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32v10x.h"
#include "protocol.h"
#include "uart_core.h"

/*=============================================================================
 *  Submodel Command Sub-codes
 *=============================================================================*/

/* CMD_SUB_SET_MODE (0x41) sub-commands */
#define SUBCMD_SET_STATUS       0x01    /* Transfer module status */
#define SUBCMD_SET_CONTENT      0x02    /* Transfer custom content */
#define SUBCMD_CLEAR_SCREEN     0x03    /* Clear screen */
#define SUBCMD_SET_DISPLAY_MODE 0x04    /* Switch display mode (status/image) */
#define SUBCMD_BMP_TRANS        0x10    /* BMP image transfer (multi-frame) */
#define SUBCMD_LS_DEV           0x11    /* Device list transfer (multi-frame) */

/* CMD_SUB_GET_STATUS (0x42) sub-commands */
#define SUBCMD_GET_SYS_STATUS   0x00    /* Request system status */
#define SUBCMD_GET_LS_DEV       0x01    /* Request device list */
#define SUBCMD_GET_BMP          0x02    /* Request BMP image */

/* CMD_SUB_DATA_REPORT (0x43) sub-commands */
#define SUBCMD_SYS_STATUS       0x20    /* System status response */

/* Multi-frame FLAGS */
#define SUBDISP_FLAG_SOF        0x01
#define SUBDISP_FLAG_EOF        0x02

/* Display modes (for SUBCMD_SET_DISPLAY_MODE) */
#define DISPLAY_MODE_STATUS     0x00    /* 状态显示模式（双页轮换） */
#define DISPLAY_MODE_IMAGE      0x01    /* 图片显示模式 */

/* CMD_SUB_BULK_TRANSFER (0x46) sub-commands */
#define SUBCMD_BULK_HANDSHAKE   0x01    /* Image transfer handshake (Core→SubDisp) */
#define SUBCMD_BULK_COMPLETE    0x02    /* Image receive complete (SubDisp→Core) */
#define SUBCMD_BULK_DATA        0x03    /* Image data chunk (Core→SubDisp, multi-frame) */

/* Status bitmap flags (for SUBCMD_SET_STATUS DATA[0]) */
#define STATUS_FLAG_BATTERY     (1 << 0)
#define STATUS_FLAG_BLUETOOTH   (1 << 1)
#define STATUS_FLAG_TIME        (1 << 2)
#define STATUS_FLAG_APP_ID      (1 << 3)

/* Content types (for SUBCMD_SET_CONTENT DATA[0]) */
#define CONTENT_TYPE_TEXT       0x00
#define CONTENT_TYPE_ICON       0x01
#define CONTENT_TYPE_PROGRESS   0x02
#define CONTENT_TYPE_SHAPE      0x03

/*=============================================================================
 *  Firmware Version
 *=============================================================================*/

#define FW_HW_VERSION           0x01
#define FW_MAJOR_VERSION        0x01
#define FW_MINOR_VERSION        0x00

/*=============================================================================
 *  Layout Constants
 *=============================================================================*/

#define LAYOUT_STATUS_BAR_H     20      /* Top status bar height */
#define LAYOUT_CONTENT_Y        24      /* Content area start Y */
#define LAYOUT_CONTENT_H        226     /* Content area height (250-24) */

/*=============================================================================
 *  Bulk Image Transfer State
 *=============================================================================*/

/** Max raw image bytes: 122x250 @ 1bpp = ceil(122/8)*250 = 4000 */
#define BULK_IMG_MAX_BYTES      4000

/** Bulk transfer state (multi-frame protocol-based, not raw UART) */
typedef struct {
    volatile uint8_t  active;          /* 1 = bulk transfer in progress */
    uint16_t          img_width;       /* Image width from handshake */
    uint16_t          img_height;      /* Image height from handshake */
    uint32_t          total_bytes;     /* Total raw bytes expected */
    volatile uint32_t received;        /* Bytes received so far */
} bulk_transfer_t;

/*=============================================================================
 *  System Status Cache (from GET_STATUS response)
 *=============================================================================*/

#define SYS_STATUS_MAX_MODULES  6

typedef struct {
    uint8_t  version;              /* Status data format version */
    uint8_t  audio_state;          /* 0=IDLE, 1=PLAYING, 2=PAUSED, 3=STOPPED */
    uint8_t  audio_volume;         /* 0~100 */
    uint32_t audio_play_time_ms;   /* Play time in ms */
    uint8_t  battery_pct;          /* 0~100, 0xFF=unknown */
    uint8_t  charging;             /* 0=no, 1=yes */
    uint8_t  ble_connections;      /* BLE connection count */
    uint8_t  ble_discoverable;     /* 0=no, 1=yes */
    uint8_t  display_brightness;   /* 0~100 */
    uint8_t  keyboard_backlight;   /* 0~100 */
    uint8_t  ch378_device;         /* 0=USB, 1=TF */
    uint8_t  ch378_busy;           /* 0=idle, 1=busy */
    uint8_t  ch9350_hid_count;     /* Connected HID devices */
    uint8_t  rgb_mode;             /* 0~3 */
    uint8_t  rgb_brightness;       /* 0~255 */
    uint8_t  config_loaded;        /* 0=no, 1=yes */
    uint8_t  module_count;         /* Number of module entries */
    struct {
        uint8_t module_id;
        uint8_t type;
        uint8_t subtype;
        uint8_t status;            /* 0=UNKNOWN, 1=ONLINE, 2=OFFLINE */
        uint8_t miss_count;
    } modules[SYS_STATUS_MAX_MODULES];
    uint8_t valid;                 /* 1 = data is valid */
} sys_status_t;

/*=============================================================================
 *  Device List Cache (from LS_DEV response)
 *=============================================================================*/

typedef struct {
    uint8_t count;
    struct {
        uint8_t module_id;
        uint8_t type;
        uint8_t subtype;
        uint8_t status;
        uint8_t miss_count;
    } entries[SYS_STATUS_MAX_MODULES];
    uint8_t valid;
} dev_list_t;

/*=============================================================================
 *  Multi-frame Reassembly State
 *=============================================================================*/

#define MULTIFRAME_BUF_SIZE     512

typedef struct {
    uint8_t  buf[MULTIFRAME_BUF_SIZE];
    uint16_t len;
    uint8_t  subcmd;
    uint8_t  active;       /* 1 = reassembly in progress */
} multiframe_rx_t;

/*=============================================================================
 *  Page Switch & Status Request Constants
 *=============================================================================*/

#define PAGE_SWITCH_INTERVAL_MS      3000   /* Switch pages every 3 seconds */
#define STATUS_INIT_DELAY_MS         5000   /* Wait 5s after boot before first status request */
#define STATUS_REQUEST_INTERVAL_MS   15000  /* Request status every 15 seconds */
#define PAGE_COUNT                   2      /* Total pages */

extern bulk_transfer_t g_bulk;

/*=============================================================================
 *  API
 *=============================================================================*/

/** Initialize all subsystems */
void App_Init(void);

/** Main loop processing */
void App_Process(void);

/** ISR entry: feed byte to protocol parser */
void App_UART_ByteReceived(uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* __APP_H__ */
