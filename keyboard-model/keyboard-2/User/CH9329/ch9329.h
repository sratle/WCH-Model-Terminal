/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch9329.h
 * Description        : CH9329 UART-to-USB HID chip driver header.
 *                      Supports keyboard HID reports (CMD 0x02) and
 *                      relative mouse HID reports (CMD 0x05).
 *                      USART2 (PA2-TX, PA3-RX) @ 9600 baud.
 *********************************************************************************/
#ifndef __CH9329_H
#define __CH9329_H

#include "ch32v10x.h"
#include <string.h>

/* ====================================================================
 * USART2 GPIO pin definitions (connects to CH9329)
 * ==================================================================== */
#define CH9329_UART             USART2
#define CH9329_UART_IRQn        USART2_IRQn

#define CH9329_TX_PORT          GPIOA
#define CH9329_TX_PIN           GPIO_Pin_2
#define CH9329_RX_PORT          GPIOA
#define CH9329_RX_PIN           GPIO_Pin_3

/* ====================================================================
 * CH9329 protocol constants
 * ==================================================================== */
#define CH9329_HEAD_0           0x57
#define CH9329_HEAD_1           0xAB
#define CH9329_ADDR_DEFAULT     0x00

/* Command codes (MCU -> CH9329) */
#define CH9329_CMD_GET_INFO         0x01
#define CH9329_CMD_KB_GENERAL       0x02
#define CH9329_CMD_KB_MEDIA         0x03
#define CH9329_CMD_MS_ABS           0x04
#define CH9329_CMD_MS_REL           0x05
#define CH9329_CMD_RESET            0x0F

/* Response CMD = original CMD | 0x80 (success), CMD | 0xC0 (error) */
#define CH9329_RESP_SUCCESS_MASK    0x80
#define CH9329_RESP_ERROR_MASK      0xC0

/* Status codes (in response DATA[0]) */
#define CH9329_STATUS_SUCCESS       0x00
#define CH9329_STATUS_TIMEOUT       0xE1
#define CH9329_STATUS_ERR_HEAD      0xE2
#define CH9329_STATUS_ERR_CMD       0xE3
#define CH9329_STATUS_ERR_SUM       0xE4
#define CH9329_STATUS_ERR_PARA      0xE5
#define CH9329_STATUS_ERR_OPERATE   0xE6

/* ====================================================================
 * HID report constants
 * ==================================================================== */
#define CH9329_KB_DATA_LEN      8   /* modifier + reserved + 6 key slots */
#define CH9329_KB_MAX_KEYS      6   /* Max simultaneous regular keys */

#define CH9329_MS_DATA_LEN      5   /* 0x01 + buttons + X + Y + wheel */

/* HID keyboard modifier bits */
#define HID_MOD_LCTRL           0x01
#define HID_MOD_LSHIFT          0x02
#define HID_MOD_LALT            0x04
#define HID_MOD_LGUI            0x08
#define HID_MOD_RCTRL           0x10
#define HID_MOD_RSHIFT          0x20
#define HID_MOD_RALT            0x40
#define HID_MOD_RGUI            0x80

/* HID mouse button bits */
#define HID_MS_BTN_LEFT         0x01
#define HID_MS_BTN_RIGHT        0x02
#define HID_MS_BTN_MIDDLE       0x04

/* USB HID Usage IDs for keys used by Keyboard-2 */
#define HID_KEY_W               0x1A
#define HID_KEY_A               0x04
#define HID_KEY_S               0x16
#define HID_KEY_D               0x07
#define HID_KEY_H               0x0B
#define HID_KEY_J               0x0D
#define HID_KEY_K               0x0E
#define HID_KEY_L               0x0F

/* ====================================================================
 * Frame size: HEAD(2) + ADDR(1) + CMD(1) + LEN(1) + DATA(N) + SUM(1)
 * ==================================================================== */
#define CH9329_FRAME_OVERHEAD   6   /* Bytes excluding DATA */

/* ====================================================================
 * Function declarations
 * ==================================================================== */

/* Initialize USART2 for CH9329 communication (115200, 8N1, TX only). */
void CH9329_Init(void);

/* Send 8-byte HID keyboard report via CH9329 protocol.
 * hid_data[0] = modifier byte (bitmask of Ctrl/Shift/Alt/GUI)
 * hid_data[1] = reserved (0x00)
 * hid_data[2..7] = up to 6 HID key codes (0x00 = no key)
 * Returns 1 on success (frame sent), 0 on failure. */
uint8_t CH9329_SendKeyboard(const uint8_t *hid_data);

/* Send 4-byte relative mouse report via CH9329 protocol.
 * mouse_data[0] = button bitmask (bit0=left, bit1=right, bit2=middle)
 * mouse_data[1] = X delta (int8, -128~127, positive=right)
 * mouse_data[2] = Y delta (int8, -128~127, positive=down)
 * mouse_data[3] = wheel delta (int8, positive=scroll up)
 * Returns 1 on success (frame sent), 0 on failure. */
uint8_t CH9329_SendMouse(const uint8_t *mouse_data);

#endif /* __CH9329_H */
