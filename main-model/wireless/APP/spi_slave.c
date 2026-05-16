/*********************************************************************
 * File Name          : spi_slave.c
 * Description        : SPI0 Slave driver for CH585F.
 *                      - Mode 0 (CPOL=Low, CPHA=1Edge), 8-bit, MSB first
 *                      - Interrupt-driven RX/TX via FIFO
 *                      - Ring buffer TX/RX
 *
 *          CH585F SPI0 FIFO is half-duplex shared (RB_SPI_FIFO_DIR):
 *            DIR=0  FIFO for TX (write R8_SPI0_FIFO to send)
 *            DIR=1  FIFO for RX (read  R8_SPI0_FIFO to receive)
 *
 *          For full-duplex slave operation we must keep FIFO in TX
 *          direction (DIR=0) so the next byte is always pre-loaded.
 *          Received bytes are read from R8_SPI0_BUFFER which mirrors
 *          the RX shift register without needing to flip FIFO_DIR.
 *
 *          ISR flow (per BYTE_END):
 *            1. Read received byte from R8_SPI0_BUFFER (no DIR change)
 *            2. Push RX byte into ring buffer
 *            3. Pop next TX byte from ring buffer (or 0x00 idle)
 *            4. Write TX byte into R8_SPI0_FIFO (DIR stays 0)
 *
 *          RB_SPI_AUTO_IF is enabled so that writing R8_SPI0_FIFO
 *          automatically clears the BYTE_END flag, saving one
 *          register write in the ISR hot path.
 *********************************************************************/

#include "spi_slave.h"
#include "data_queue.h"

/* ==================================================================== */
/*  Buffers                                                             */
/* ==================================================================== */
static uint8_t  s_rx_buf[SPI_SLAVE_RX_BUF_SIZE];
static uint8_t  s_tx_buf[SPI_SLAVE_TX_BUF_SIZE];
static data_queue_t s_rx_queue;
static data_queue_t s_tx_queue;

/* ==================================================================== */
/*  Public API                                                          */
/* ==================================================================== */

void SPI_Slave_Init(void)
{
    DataQueue_Init(&s_rx_queue, s_rx_buf, SPI_SLAVE_RX_BUF_SIZE);
    DataQueue_Init(&s_tx_queue, s_tx_buf, SPI_SLAVE_TX_BUF_SIZE);

    GPIOPinRemap(ENABLE, RB_PIN_SPI0);
    GPIOB_ModeCfg(GPIO_Pin_12, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_13, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_14, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_15, GPIO_ModeOut_PP_5mA);
    GPIOBDigitalCfg(ENABLE, GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15);

    R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;
    R8_SPI0_CTRL_MOD = RB_SPI_MISO_OE | RB_SPI_MODE_SLAVE;

    SPI0_DataMode(Mode0_HighBitINFront);

    R8_SPI0_CTRL_CFG |= RB_SPI_AUTO_IF;

    R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;

    SetFirstData(0x00);

    SPI0_ITCfg(DISABLE, SPI0_IT_FST_BYTE);
    SPI0_ITCfg(ENABLE, SPI0_IT_BYTE_END);

    PFIC_EnableIRQ(SPI0_IRQn);
}

bool SPI_Slave_HasTxData(void)
{
    return !DataQueue_IsEmpty(&s_tx_queue);
}

void SPI_Slave_NotifyMaster(void)
{
    if (GPIOB_ReadPortPin(GPIO_Pin_12) == 0) {
        return;
    }

    GPIOB_ModeCfg(GPIO_Pin_12, GPIO_ModeOut_PP_5mA);
    GPIOB_ResetBits(GPIO_Pin_12);
    DelayUs(10);
    GPIOB_SetBits(GPIO_Pin_12);
    GPIOB_ModeCfg(GPIO_Pin_12, GPIO_ModeIN_PU);
}

bool SPI_Slave_EnqueueTx(const uint8_t *data, uint16_t len)
{
    uint16_t pushed;
    uint8_t was_empty = DataQueue_IsEmpty(&s_tx_queue);

    pushed = DataQueue_PushBulk(&s_tx_queue, data, len);
    if (was_empty && pushed > 0)
    {
        SPI_Slave_NotifyMaster();
    }
    return (pushed == len);
}

uint16_t SPI_Slave_DequeueRx(uint8_t *data, uint16_t max_len)
{
    return DataQueue_PopBulk(&s_rx_queue, data, max_len);
}

uint16_t SPI_Slave_RxCount(void)
{
    return DataQueue_Count(&s_rx_queue);
}

/* ==================================================================== */
/*  SPI0 Interrupt Handler                                              */
/* ==================================================================== */

static volatile uint32_t s_spi_irq_cnt = 0;
static volatile uint32_t s_spi_rx_cnt = 0;

__INTERRUPT __HIGH_CODE void SPI0_IRQHandler(void)
{
    uint8_t rx_byte;
    uint8_t next_tx;

    if (SPI0_GetITFlag(SPI0_IT_BYTE_END)) {
        s_spi_irq_cnt++;

        rx_byte = R8_SPI0_BUFFER;

        if (DataQueue_Push(&s_rx_queue, rx_byte)) {
            s_spi_rx_cnt++;
        }

        if (DataQueue_Pop(&s_tx_queue, &next_tx)) {
            R8_SPI0_FIFO = next_tx;
        } else {
            R8_SPI0_FIFO = 0x00;
        }
    }
}

uint32_t SPI_Slave_GetIrqCount(void)
{
    return s_spi_irq_cnt;
}

uint32_t SPI_Slave_GetRxCountTotal(void)
{
    return s_spi_rx_cnt;
}

void SPI_Slave_ClearCounters(void)
{
    s_spi_irq_cnt = 0;
    s_spi_rx_cnt = 0;
}
