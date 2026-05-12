#ifndef __SPI_SLAVE_H
#define __SPI_SLAVE_H

#include "CH58x_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPI_SLAVE_RX_BUF_SIZE   512
#define SPI_SLAVE_TX_BUF_SIZE   512

/* SPI0 GPIO mapping for CH585F (adjust per hardware) */
/* Default: PA12=SCK, PA13=MISO, PA14=MOSI, PA15=NSS (or other pins) */
/* For this project, SPI0 pins are configured in spi_slave.c */

/* Initialize SPI0 as Slave, Mode 0, 8-bit, MSB first */
void SPI_Slave_Init(void);

/* Query if there's pending TX data */
bool SPI_Slave_HasTxData(void);

/* Enqueue data to SPI TX buffer (called from main loop / task) */
bool SPI_Slave_EnqueueTx(const uint8_t *data, uint16_t len);

/* Dequeue received data from SPI RX buffer (called from main loop / task) */
uint16_t SPI_Slave_DequeueRx(uint8_t *data, uint16_t max_len);

/* Get count of pending RX bytes */
uint16_t SPI_Slave_RxCount(void);

/* SPI0 Interrupt Handler */
__INTERRUPT __HIGH_CODE void SPI0_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_SLAVE_H */
