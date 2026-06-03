/*********************************************************************
 * File Name          : rgb_app.h
 * Description        : RGB application layer.
 *                      UART0 communication with Core (PA14-TX, PA15-RX).
 *                      Protocol command dispatch and effect control.
 *********************************************************************/

#ifndef __RGB_APP_H
#define __RGB_APP_H

#include "CH58x_common.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================== */
/*  UART0 Configuration                                                 */
/* ==================================================================== */
#define UART_BAUDRATE           115200
#define UART_RX_BUF_SIZE        512

/* ==================================================================== */
/*  API Functions                                                       */
/* ==================================================================== */

/**
 * @brief  Initialize UART0 for Core communication.
 *         PA14 = TX, PA15 = RX (remapped pins).
 *         115200/8-N-1, RX interrupt enabled.
 */
void App_UART_Init(void);

/**
 * @brief  Initialize all application modules (protocol, ws2812, effect).
 */
void App_Init(void);

/**
 * @brief  Process incoming UART data (call in main loop).
 *         Reads RX bytes and feeds them to protocol parser.
 */
void App_ProcessUART(void);

/**
 * @brief  Update effect engine (call in main loop).
 */
void App_UpdateEffect(void);

/**
 * @brief  Send a protocol frame to Core via UART0.
 */
void App_SendFrame(uint8_t cmd, const uint8_t *data, uint8_t data_len);

/**
 * @brief  Send ACK to Core.
 */
void App_SendAck(const uint8_t *rsp_data, uint8_t rsp_len);

/**
 * @brief  Send NACK to Core.
 */
void App_SendNack(uint8_t err_code);

#ifdef __cplusplus
}
#endif

#endif /* __RGB_APP_H */
