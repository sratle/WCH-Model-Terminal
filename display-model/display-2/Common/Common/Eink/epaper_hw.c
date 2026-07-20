/**
 * @file    epaper_hw.c
 * @brief   JD79686AB hardware abstraction.
 *
 * Two selectable transports (EPD_USE_HW_SPI in epaper_hw.h):
 *   1 = hardware SPI1 @ 9 MHz (144/16) with DMA1 Channel 3 for block writes.
 *       PA5 = SCK (AF), PA7 = MOSI (AF).  ~35 ms for a full 38880-byte frame.
 *   0 = legacy GPIO bit-bang (reference timing kept for troubleshooting).
 *
 * Command/byte framing matches the manufacturer reference:
 *   - WriteCmd:  DC=LOW -> CS=LOW -> shift -> CS=HIGH -> DC=HIGH
 *   - WriteData: DC=HIGH -> CS=LOW -> shift -> CS=HIGH
 */
#include "epaper_hw.h"

#if EPD_USE_HW_SPI

/*=============================================================================
 *  Hardware SPI1 + DMA implementation
 *===========================================================================*/

/* Low-level: transmit one byte over SPI1 (full duplex, RX discarded) */
static void spi_tx_byte(uint8_t b)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) { }
    SPI_I2S_SendData(SPI1, b);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET) { }
}

/* Low-level: transmit a buffer via DMA1 Channel 3 (SPI1_TX) */
static void spi_tx_dma(const uint8_t *buf, uint32_t len)
{
    if (len == 0) return;
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA1_Channel3->MADDR = (uint32_t)buf;
    DMA1_Channel3->CNTR  = len;
    DMA_ClearFlag(DMA1_FLAG_TC3);
    DMA_Cmd(DMA1_Channel3, ENABLE);
    while (DMA_GetFlagStatus(DMA1_FLAG_TC3) == RESET) { }
    DMA_Cmd(DMA1_Channel3, DISABLE);
    /* Wait for the last byte to actually leave the shift register */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET) { }
}

/*********************************************************************
 * @fn      Epaper_Hw_Init
 */
void Epaper_Hw_Init(void)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef  spi;
    DMA_InitTypeDef  dma;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_AFIO  | RCC_APB2Periph_SPI1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    /* Disable JTAG to free PB3/PB4 */
    AFIO->PCFR1 &= ~(uint32_t)AFIO_PCFR1_SWJ_CFG;
    AFIO->PCFR1 |= AFIO_PCFR1_SWJ_CFG_JTAGDISABLE;

    /* PA4 (CS): GPIO push-pull output */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin   = EPD_CS_BIT;
    GPIO_Init(GPIOA, &gpio);

    /* PA5 (SCK) / PA7 (MOSI): SPI1 alternate function */
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Pin  = EPD_SCK_BIT | EPD_MOSI_BIT;
    GPIO_Init(GPIOA, &gpio);

    /* PA6 (MISO) floating input */
    gpio.GPIO_Pin  = GPIO_Pin_6;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    /* PB3 (RES) / PB4 (DC): push-pull outputs; PB5 (BUSY) pull-up input */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin   = EPD_RES_BIT | EPD_DC_BIT;
    GPIO_Init(GPIOB, &gpio);

    gpio.GPIO_Pin  = EPD_BUSY_BIT;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio);

    /* SPI1: master, 8-bit, CPOL=0 CPHA=0, MSB first, 9 MHz, software NSS */
    SPI_I2S_DeInit(SPI1);
    spi.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode              = SPI_Mode_Master;
    spi.SPI_DataSize          = SPI_DataSize_8b;
    spi.SPI_CPOL              = SPI_CPOL_Low;
    spi.SPI_CPHA              = SPI_CPHA_1Edge;
    spi.SPI_NSS               = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;   /* 144/16 = 9 MHz */
    spi.SPI_FirstBit          = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI1, &spi);

    /* DMA1 Channel 3 (SPI1_TX): memory -> peripheral, 8-bit, mem inc */
    DMA_DeInit(DMA1_Channel3);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&SPI1->DATAR;
    dma.DMA_MemoryBaseAddr     = 0;
    dma.DMA_DIR                = DMA_DIR_PeripheralDST;
    dma.DMA_BufferSize         = 0;
    dma.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode               = DMA_Mode_Normal;
    dma.DMA_Priority           = DMA_Priority_VeryHigh;
    dma.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel3, &dma);
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);

    SPI_Cmd(SPI1, ENABLE);

    /* Idle states */
    EPD_CS_HIGH();
    EPD_RES_HIGH();
    EPD_DC_HIGH();
}

/*********************************************************************
 * @fn      Epaper_Hw_Reset
 *
 * @brief   Single-pulse reset matching OKRA reference code:
 *          HIGH(100ms) -> LOW(20ms) -> HIGH(20ms)
 */
void Epaper_Hw_Reset(void)
{
    EPD_RES_HIGH();
    Delay_Ms(100);
    EPD_RES_LOW();
    Delay_Ms(20);
    EPD_RES_HIGH();
    Delay_Ms(20);
}

/*********************************************************************
 * @fn      Epaper_Hw_WaitBusy
 *
 * @brief   Wait while BUSY_N = LOW (busy). Timeout in ms.
 */
int Epaper_Hw_WaitBusy(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while (Epd_Is_Busy())
    {
        if (elapsed >= timeout_ms)
        {
            printf("[epd_hw] BUSY timeout (%lu ms), BUSY=0x%08lX\r\n",
                   timeout_ms, (uint32_t)EPD_BUSY_READ());
            return -1;
        }
        Delay_Ms(10);
        elapsed += 10;
    }
    return 0;
}

/*********************************************************************
 * @fn      Epaper_Hw_WriteCmd
 *
 * @brief   Send command byte: DC=LOW -> CS=LOW -> byte -> CS=HIGH -> DC=HIGH
 */
void Epaper_Hw_WriteCmd(uint8_t cmd)
{
    EPD_DC_LOW();
    EPD_CS_LOW();
    spi_tx_byte(cmd);
    EPD_CS_HIGH();
    EPD_DC_HIGH();
}

/*********************************************************************
 * @fn      Epaper_Hw_WriteData
 *
 * @brief   Send one data byte: DC=HIGH -> CS=LOW -> byte -> CS=HIGH
 */
void Epaper_Hw_WriteData(uint8_t data)
{
    EPD_DC_HIGH();
    EPD_CS_LOW();
    spi_tx_byte(data);
    EPD_CS_HIGH();
}

/*********************************************************************
 * @fn      Epaper_Hw_WriteDataBuf
 *
 * @brief   Send a buffer via DMA. CS held low for the entire transfer.
 */
void Epaper_Hw_WriteDataBuf(const uint8_t *buf, uint32_t len)
{
    EPD_DC_HIGH();
    EPD_CS_LOW();
    spi_tx_dma(buf, len);
    EPD_CS_HIGH();
}

/*********************************************************************
 * @fn      Epaper_Hw_WriteDataFill
 *
 * @brief   Send the same byte N times via DMA (fixed source address).
 *          Used for the all-zero Red channel and clear operations.
 */
void Epaper_Hw_WriteDataFill(uint8_t data, uint32_t len)
{
    static uint8_t s_fill_byte;
    if (len == 0) return;

    s_fill_byte = data;
    EPD_DC_HIGH();
    EPD_CS_LOW();

    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA1_Channel3->CFGR &= ~DMA_MemoryInc_Enable;   /* fixed source address */
    DMA1_Channel3->MADDR = (uint32_t)&s_fill_byte;
    DMA1_Channel3->CNTR  = len;
    DMA_ClearFlag(DMA1_FLAG_TC3);
    DMA_Cmd(DMA1_Channel3, ENABLE);
    while (DMA_GetFlagStatus(DMA1_FLAG_TC3) == RESET) { }
    DMA_Cmd(DMA1_Channel3, DISABLE);
    DMA1_Channel3->CFGR |= DMA_MemoryInc_Enable;    /* restore normal mode */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET) { }

    EPD_CS_HIGH();
}

/*********************************************************************
 * @fn      Epaper_Hw_ReadReg
 * @note   SPI read (full duplex, dummy bytes).  Flush stale RX first.
 */
void Epaper_Hw_ReadReg(uint8_t reg, uint8_t *buf, uint8_t len)
{
    /* Drain any stale RX data */
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == SET) {
        (void)SPI_I2S_ReceiveData(SPI1);
    }

    Epaper_Hw_WriteCmd(reg);

    EPD_DC_HIGH();
    EPD_CS_LOW();
    for (uint8_t i = 0; i < len; i++) {
        while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) { }
        SPI_I2S_SendData(SPI1, 0xFF);
        while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET) { }
        buf[i] = (uint8_t)SPI_I2S_ReceiveData(SPI1);
    }
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET) { }
    EPD_CS_HIGH();
}

#else /* !EPD_USE_HW_SPI — legacy GPIO bit-bang implementation */

/*=============================================================================
 *  Legacy software SPI (manufacturer reference timing)
 *===========================================================================*/

void Epaper_Hw_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_AFIO, ENABLE);

    /* Disable JTAG to free PB3/PB4 */
    AFIO->PCFR1 &= ~(uint32_t)AFIO_PCFR1_SWJ_CFG;
    AFIO->PCFR1 |= AFIO_PCFR1_SWJ_CFG_JTAGDISABLE;

    /* GPIOA outputs: CS(PA4), SCK(PA5), MOSI(PA7) */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin = EPD_CS_BIT | EPD_SCK_BIT | EPD_MOSI_BIT;
    GPIO_Init(GPIOA, &gpio);

    /* PA6 (MISO) floating input */
    gpio.GPIO_Pin  = GPIO_Pin_6;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    /* GPIOB outputs: RES(PB3), DC(PB4) */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin = EPD_RES_BIT | EPD_DC_BIT;
    GPIO_Init(GPIOB, &gpio);

    /* GPIOB input: BUSY(PB5) pull-up */
    gpio.GPIO_Pin  = EPD_BUSY_BIT;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio);

    /* Idle states (match manufacturer) */
    EPD_CS_HIGH();
    EPD_SCK_LOW();
    EPD_MOSI_LOW();
    EPD_RES_HIGH();
    EPD_DC_HIGH();
}

void Epaper_Hw_Reset(void)
{
    EPD_RES_HIGH();
    Delay_Ms(100);
    EPD_RES_LOW();
    Delay_Ms(20);
    EPD_RES_HIGH();
    Delay_Ms(20);
}

int Epaper_Hw_WaitBusy(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while (Epd_Is_Busy())
    {
        if (elapsed >= timeout_ms)
        {
            printf("[epd_hw] BUSY timeout (%lu ms), BUSY=0x%08lX\r\n",
                   timeout_ms, (uint32_t)EPD_BUSY_READ());
            return -1;
        }
        Delay_Ms(10);
        elapsed += 10;
    }
    return 0;
}

void Epaper_Hw_WriteCmd(uint8_t cmd)
{
    EPD_DC_LOW();     /* DC first (command mode) */
    EPD_CS_LOW();     /* CS second */
    EPD_SCK_LOW();    /* Ensure SCK starts LOW (CPOL=0) */

    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        if (cmd & 0x80)
            EPD_MOSI_HIGH();
        else
            EPD_MOSI_LOW();
        cmd <<= 1;

        EPD_SPI_DLY_DAT();   /* Data setup time >= 15ns */
        EPD_SCK_HIGH();
        EPD_SPI_DLY_SCK();   /* SCK high >= 35ns */
        EPD_SCK_LOW();
        EPD_SPI_DLY_SCK();   /* SCK low >= 35ns */
    }

    EPD_CS_HIGH();    /* CS high (latch command) */
    EPD_DC_HIGH();    /* DC back to data mode */
}

void Epaper_Hw_WriteData(uint8_t data)
{
    EPD_DC_HIGH();    /* DC first (data mode) */
    EPD_CS_LOW();     /* CS second */
    EPD_SCK_LOW();    /* Ensure SCK starts LOW */

    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        if (data & 0x80)
            EPD_MOSI_HIGH();
        else
            EPD_MOSI_LOW();
        data <<= 1;

        EPD_SPI_DLY_DAT();   /* Data setup time >= 15ns */
        EPD_SCK_HIGH();
        EPD_SPI_DLY_SCK();   /* SCK high >= 35ns */
        EPD_SCK_LOW();
        EPD_SPI_DLY_SCK();   /* SCK low >= 35ns */
    }

    EPD_CS_HIGH();
    EPD_DC_HIGH();    /* Keep DC high after data write */
}

void Epaper_Hw_WriteDataBuf(const uint8_t *buf, uint32_t len)
{
    uint32_t idx;
    uint8_t i, data;

    EPD_DC_HIGH();
    EPD_CS_LOW();
    EPD_SCK_LOW();

    for (idx = 0; idx < len; idx++)
    {
        data = buf[idx];
        for (i = 0; i < 8; i++)
        {
            if (data & 0x80)
                EPD_MOSI_HIGH();
            else
                EPD_MOSI_LOW();
            data <<= 1;

            EPD_SPI_DLY_DAT();   /* Data setup time >= 15ns */
            EPD_SCK_HIGH();
            EPD_SPI_DLY_SCK();   /* SCK high >= 35ns */
            EPD_SCK_LOW();
            EPD_SPI_DLY_SCK();   /* SCK low >= 35ns */
        }
    }

    EPD_CS_HIGH();
}

void Epaper_Hw_WriteDataFill(uint8_t data, uint32_t len)
{
    uint8_t b = data;
    while (len--) {
        Epaper_Hw_WriteDataBuf(&b, 1);
    }
}

void Epaper_Hw_ReadReg(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i, j, val;

    Epaper_Hw_WriteCmd(reg);

    EPD_DC_HIGH();
    EPD_CS_LOW();
    EPD_SCK_LOW();

    for (i = 0; i < len; i++)
    {
        val = 0;
        for (j = 0; j < 8; j++)
        {
            EPD_SCK_HIGH();
            EPD_SPI_DLY_SCK();   /* SCK high >= 35ns */
            val <<= 1;
            if (GPIOA->INDR & GPIO_Pin_6)
                val |= 0x01;
            EPD_SCK_LOW();
            EPD_SPI_DLY_SCK();   /* SCK low >= 35ns */
        }
        buf[i] = val;
    }
    EPD_CS_HIGH();
}

#endif /* EPD_USE_HW_SPI */
