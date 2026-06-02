/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch9329.h
 * Description        : CH9329 UART-to-USB HID chip driver header.
 *                      Protocol mode 0 (default): keyboard + mouse + custom HID.
 *                      USART2 (PA2-TX, PA3-RX) @ 115200 baud.
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
 * HID keyboard report constants
 * ==================================================================== */
#define CH9329_KB_DATA_LEN      8   /* modifier + reserved + 6 key slots */
#define CH9329_KB_MAX_KEYS      6   /* Max simultaneous regular keys */

/* ====================================================================
 * Frame size: HEAD(2) + ADDR(1) + CMD(1) + LEN(1) + DATA(N) + SUM(1)
 * ==================================================================== */
#define CH9329_FRAME_OVERHEAD   6   /* Bytes excluding DATA */

/* ====================================================================
 * Function declarations
 * ==================================================================== */

/* Initialize USART2 for CH9329 communication (115200, 8N1, TX+RX). */
void CH9329_Init(void);

/* Send 8-byte HID keyboard report via CH9329 protocol.
 * hid_data[0] = modifier byte (bitmask of Ctrl/Shift/Alt/GUI)
 * hid_data[1] = reserved (0x00)
 * hid_data[2..7] = up to 6 HID key codes (0x00 = no key)
 * Returns 1 on success (frame sent), 0 on failure. */
uint8_t CH9329_SendKeyboard(const uint8_t *hid_data);

#endif /* __CH9329_H */
