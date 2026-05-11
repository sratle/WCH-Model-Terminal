/********************************** (C) COPYRIGHT *******************************
 * File Name          : spi_slave.h
 * Description        : SPI0 Slave driver + ISR + TX/RX queues
 *******************************************************************************/

#ifndef __SPI_SLAVE_H__
#define __SPI_SLAVE_H__

#include "CH58x_common.h"
#include "data_queue.h"

#define SPI_SLAVE_RX_BUF_SIZE   512
#define SPI_SLAVE_TX_BUF_SIZE   512

void SPI_Slave_Init(void);
bool SPI_Slave_HasTxData(void);
bool SPI_Slave_EnqueueTx(const uint8_t *data, uint16_t len);
bool SPI_Slave_DequeueRx(uint8_t *byte);
uint16_t SPI_Slave_RxCount(void);

#endif /* __SPI_SLAVE_H__ */
