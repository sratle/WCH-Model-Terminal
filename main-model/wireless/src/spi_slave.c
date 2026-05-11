/********************************** (C) COPYRIGHT *******************************
 * File Name          : spi_slave.c
 *******************************************************************************/

#include "spi_slave.h"

static data_queue_t g_spi_rx_queue;
static uint8_t      g_spi_rx_buf[SPI_SLAVE_RX_BUF_SIZE];

static data_queue_t g_spi_tx_queue;
static uint8_t      g_spi_tx_buf[SPI_SLAVE_TX_BUF_SIZE];

/*********************************************************************
 * @fn      SPI_Slave_Init
 *
 * @brief   Init SPI0 as Slave: CPOL=Low, CPHA=1Edge, 8bit, MSB first
 */
void SPI_Slave_Init(void)
{
    /* Init ring buffers */
    DataQueue_Init(&g_spi_rx_queue, g_spi_rx_buf, SPI_SLAVE_RX_BUF_SIZE);
    DataQueue_Init(&g_spi_tx_queue, g_spi_tx_buf, SPI_SLAVE_TX_BUF_SIZE);

    /* Reset and configure SPI0 Slave */
    R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;
    R8_SPI0_CTRL_MOD = RB_SPI_MISO_OE | RB_SPI_MODE_SLAVE;
    R8_SPI0_CTRL_CFG |= RB_SPI_AUTO_IF;

    /* Mode0 (CPOL=0, CPHA=0 == 1Edge), MSB first */
    SPI0_DataMode(Mode0_HighBitINFront);

    /* Enable byte-end interrupt */
    SPI0_ITCfg(ENABLE, RB_SPI_IE_BYTE_END);
    PFIC_EnableIRQ(SPI0_IRQn);
}

/*********************************************************************
 * @fn      SPI_Slave_HasTxData
 */
bool SPI_Slave_HasTxData(void)
{
    return !DataQueue_IsEmpty(&g_spi_tx_queue);
}

/*********************************************************************
 * @fn      SPI_Slave_EnqueueTx
 *
 * @brief   Push a packed frame into SPI TX queue
 */
bool SPI_Slave_EnqueueTx(const uint8_t *data, uint16_t len)
{
    bool ok = TRUE;
    for(uint16_t i = 0; i < len; i++)
    {
        if(!DataQueue_Push(&g_spi_tx_queue, data[i]))
        {
            ok = FALSE;
            break;
        }
    }
    return ok;
}

/*********************************************************************
 * @fn      SPI_Slave_DequeueRx
 */
bool SPI_Slave_DequeueRx(uint8_t *byte)
{
    return DataQueue_Pop(&g_spi_rx_queue, byte);
}

/*********************************************************************
 * @fn      SPI_Slave_RxCount
 */
uint16_t SPI_Slave_RxCount(void)
{
    return DataQueue_Count(&g_spi_rx_queue);
}

/*********************************************************************
 * @fn      SPI0_IRQHandler
 *
 * @brief   SPI0 Slave ISR: read RX byte, push to queue;
 *          pop TX byte from queue and preload into FIFO for next
 *          Master clock.
 */
__INTERRUPT
__HIGH_CODE
void SPI0_IRQHandler(void)
{
    uint8_t rx_byte;
    uint8_t tx_byte;

    if(R8_SPI0_INT_FLAG & RB_SPI_IF_BYTE_END)
    {
        /* Read received byte */
        R8_SPI0_CTRL_MOD |= RB_SPI_FIFO_DIR;
        if(R8_SPI0_FIFO_COUNT)
        {
            rx_byte = R8_SPI0_FIFO;
            DataQueue_Push(&g_spi_rx_queue, rx_byte);
        }

        /* Preload next TX byte for the next Master SCK */
        R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;
        if(DataQueue_Pop(&g_spi_tx_queue, &tx_byte))
        {
            R8_SPI0_FIFO = tx_byte;
        }
        else
        {
            R8_SPI0_FIFO = 0x00; /* idle filler */
        }

        /* Clear flag (RB_SPI_AUTO_IF handles most, but be safe) */
        R8_SPI0_INT_FLAG = RB_SPI_IF_BYTE_END;
    }
}
