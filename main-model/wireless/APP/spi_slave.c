/*********************************************************************
 * File Name          : spi_slave.c
 * Description        : SPI0 Slave driver for CH585F.
 *                      - Mode 0 (CPOL=Low, CPHA=1Edge), 8-bit, MSB first
 *                      - Interrupt-driven RX/TX via FIFO
 *                      - Ring buffer TX/RX
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
    /* Initialize ring buffers */
    DataQueue_Init(&s_rx_queue, s_rx_buf, SPI_SLAVE_RX_BUF_SIZE);
    DataQueue_Init(&s_tx_queue, s_tx_buf, SPI_SLAVE_TX_BUF_SIZE);

    /* Configure SPI0 GPIO pins (remap: PB12=SCS, PB13=SCK, PB14=MOSI, PB15=MISO)
     * Enable remap first, then configure PB pins
     */
    GPIOPinRemap(ENABLE, RB_PIN_SPI0);
    GPIOB_ModeCfg(GPIO_Pin_12, GPIO_ModeIN_PU);      /* SCS:  input, pull-up */
    GPIOB_ModeCfg(GPIO_Pin_13, GPIO_ModeIN_PU);      /* SCK:  input, pull-up */
    GPIOB_ModeCfg(GPIO_Pin_14, GPIO_ModeIN_PU);      /* MOSI: input, pull-up */
    GPIOB_ModeCfg(GPIO_Pin_15, GPIO_ModeOut_PP_5mA); /* MISO: output */
    GPIOBDigitalCfg(ENABLE, GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15);

    /* Initialize SPI0 in slave mode */
    SPI0_SlaveInit();

    /* Set data mode: Mode 0, MSB first */
    SPI0_DataMode(Mode0_HighBitINFront);

    /* Preload idle byte for first transfer */
    SetFirstData(0x00);

    /* Enable interrupts: byte-end only (FST_BYTE can cause spurious triggers
     * before actual clock starts, leading to FIFO misalignment) */
    SPI0_ITCfg(DISABLE, SPI0_IT_FST_BYTE);
    SPI0_ITCfg(ENABLE, SPI0_IT_BYTE_END);

    /* Enable SPI0 IRQ in NVIC */
    PFIC_EnableIRQ(SPI0_IRQn);
}

bool SPI_Slave_HasTxData(void)
{
    return !DataQueue_IsEmpty(&s_tx_queue);
}

/* Generate a ~10us low pulse on PB12 to notify Core (PE11 EXTI) */
void SPI_Slave_NotifyMaster(void)
{
    GPIOB_ModeCfg(GPIO_Pin_12, GPIO_ModeOut_PP_5mA);
    GPIOB_ResetBits(GPIO_Pin_12);
    DelayUs(10);
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

    /* BYTE_END: one byte exchange completed in slave mode.
     * Use SDK-provided SPI0_SlaveRecvByte to ensure correct RX timing.
     */
    if (SPI0_GetITFlag(SPI0_IT_BYTE_END)) {
        s_spi_irq_cnt++;

        /* Receive byte via SDK function (handles RB_SPI_FIFO_DIR and FIFO count wait) */
        rx_byte = SPI0_SlaveRecvByte();
        if (DataQueue_Push(&s_rx_queue, rx_byte)) {
            s_spi_rx_cnt++;
        }

        SPI0_ClearITFlag(SPI0_IT_BYTE_END);

        /* Preload next transmit byte into FIFO */
        if (DataQueue_Pop(&s_tx_queue, &next_tx)) {
            R8_SPI0_FIFO = next_tx;
        } else {
            R8_SPI0_FIFO = 0x00;  /* Idle byte */
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
