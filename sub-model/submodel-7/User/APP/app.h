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
