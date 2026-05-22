/********************************** (C) COPYRIGHT *******************************
* File Name          : uart_module.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2026/05/22
* Description        : UART1 module communication with Core.
*                      Protocol frame parser and input event dispatch.
********************************************************************************/
#ifndef __UART_MODULE_H
#define __UART_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 *  Protocol Constants
 *=============================================================================*/

#define PROTO_FRAME_HEAD        0xAA
#define PROTO_FRAME_TAIL0       0xA5
#define PROTO_FRAME_TAIL1       0x5A
#define PROTO_FRAME_TAIL2       0xFC
#define PROTO_FRAME_TAIL3       0xFD

#define PROTO_MAX_DATA_LEN      255
#define PROTO_STREAM_DATA_LEN   0xFF

/* Module IDs */
#define MODULE_ID_CORE          0x00
#define MODULE_ID_WIRELESS      0x01
#define MODULE_ID_DISPLAY       0x10
#define MODULE_ID_KEYBOARD      0x20
#define MODULE_ID_POWER         0x30
#define MODULE_ID_SUBMODEL1     0x40

/* Generic opcodes */
#define CMD_NOP                 0x00
#define CMD_GET_TYPE            0x01
#define CMD_GET_STATUS          0x02
#define CMD_SET_CONFIG          0x03
#define CMD_ACK                 0x04
#define CMD_NACK                0x05
#define CMD_EVT_NOTIFY          0x06
#define CMD_DATA_STREAM         0x07

/* Display-specific opcodes */
#define CMD_DISP_INPUT_EVENT    0x15
#define CMD_DISP_UPDATE_STATUS  0x14
#define CMD_DISP_SHOW_PAGE      0x13
#define CMD_DISP_SET_BRIGHTNESS 0x11
#define CMD_DISP_SCREEN_CONTROL 0x18
#define CMD_DISP_SHOW_NOTICE    0x1A
#define CMD_DISP_MUSIC_STATUS   0x1C
#define CMD_DISP_EXT_APP_DATA   0x10  /* CMD=0x10, DATA[0]=sub-cmd */

/* Input event device types (DATA[0] of CMD_DISP_INPUT_EVENT) */
#define INPUT_DEV_KEYBOARD      0x00
#define INPUT_DEV_MOUSE         0x01
#define INPUT_DEV_TOUCH         0x02
#define INPUT_DEV_CORE_KEY      0x03

/* UART RX buffer size */
#define UART_RX_BUF_SIZE        512

/*=============================================================================
 *  API
 *=============================================================================*/

void UART_Module_Init(void);
void UART_Module_Poll(void);

/* Send a protocol frame to Core */
void UART_SendFrame(uint8_t dst, uint8_t cmd, const uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __UART_MODULE_H */
