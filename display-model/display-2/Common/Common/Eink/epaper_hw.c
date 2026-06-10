/**
 * @file    epaper_hw.c
 * @brief   JD79686AB hardware abstraction – software SPI.
 *
 * STRICTLY matches manufacturer STM32 SPI_Init.c timing:
 *   - EPD_WR_REG:  DC=LOW → CS=LOW → shift → CS=HIGH → DC=HIGH
 *   - EPD_WR_DATA: DC=HIGH → CS=LOW → shift → CS=HIGH
 *   - EPD_WR_Bus:  SCK=LOW before first bit
 */
#include "epaper_hw.h"

/*********************************************************************
 * @fn      Epaper_Hw_Init
 */
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

    printf("EPD: GPIO init done\r\n");
}

/*********************************************************************
 * @fn      Epaper_Hw_Reset
 *
 * @brief   Double-pulse reset matching Arduino OKRA0583BNF686F0 sample:
 *          HIGH(20ms) → LOW(30ms) → HIGH(30ms) → LOW(30ms) → HIGH(30ms)
 */
void Epaper_Hw_Reset(void)
{
    EPD_RES_HIGH();
    Delay_Ms(20);
    EPD_RES_LOW();
    Delay_Ms(30);
    EPD_RES_HIGH();
    Delay_Ms(30);
    EPD_RES_LOW();
    Delay_Ms(30);
    EPD_RES_HIGH();
    Delay_Ms(30);

    printf("EPD: Reset done, BUSY_N=%d RES=%d DC=%d CS=%d SCK=%d\r\n",
           EPD_BUSY_READ() ? 1 : 0,
           (GPIOB->OUTDR & EPD_RES_BIT) ? 1 : 0,
           (GPIOB->OUTDR & EPD_DC_BIT) ? 1 : 0,
           (GPIOA->OUTDR & EPD_CS_BIT) ? 1 : 0,
           (GPIOA->OUTDR & EPD_SCK_BIT) ? 1 : 0);
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
            printf("EPD: WaitBusy TIMEOUT %ums (raw=%d)\r\n",
                   (unsigned)elapsed, EPD_BUSY_READ() ? 1 : 0);
            return -1;
        }
        Delay_Ms(10);
        elapsed += 10;
    }
    if (elapsed > 0)
    {
        printf("EPD: WaitBusy OK %ums\r\n", (unsigned)elapsed);
    }
    return 0;
}

/*********************************************************************
 * @fn      Epaper_Hw_WriteCmd
 *
 * @brief   Send command byte. STRICTLY matches manufacturer EPD_WR_REG():
 *            DC=LOW → CS=LOW → shift(CS=LOW,SCK=LOW first) → CS=HIGH → DC=HIGH
 *
 *          KEY: DC must be set BEFORE CS goes low!
 */
void Epaper_Hw_WriteCmd(uint8_t cmd)
{
    EPD_DC_LOW();     /* DC first (command mode) */
    EPD_CS_LOW();     /* CS second */
    EPD_SCK_LOW();    /* Ensure SCK starts LOW (CPOL=0) */

    /* Shift 8 bits MSB first */
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        if (cmd & 0x80)
            EPD_MOSI_HIGH();
        else
            EPD_MOSI_LOW();
        cmd <<= 1;

        EPD_SPI_DLY_DAT();   /* Data setup time ≥ 15ns */
        EPD_SCK_HIGH();
        EPD_SPI_DLY_SCK();   /* SCK high ≥ 35ns */
        EPD_SCK_LOW();
        EPD_SPI_DLY_SCK();   /* SCK low ≥ 35ns */
    }

    EPD_CS_HIGH();    /* CS high (latch command) */
    EPD_DC_HIGH();    /* DC back to data mode */
}

/*********************************************************************
 * @fn      Epaper_Hw_WriteData
 *
 * @brief   Send data byte. Matches manufacturer EPD_WR_DATA8():
 *            DC=HIGH → CS=LOW → shift → CS=HIGH
 */
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

        EPD_SPI_DLY_DAT();   /* Data setup time ≥ 15ns */
        EPD_SCK_HIGH();
        EPD_SPI_DLY_SCK();   /* SCK high ≥ 35ns */
        EPD_SCK_LOW();
        EPD_SPI_DLY_SCK();   /* SCK low ≥ 35ns */
    }

    EPD_CS_HIGH();
}

/*********************************************************************
 * @fn      Epaper_Hw_WriteDataBuf
 *
 * @brief   Send buffer. CS held low for entire transfer.
 *          DC=HIGH → CS=LOW → [shift per byte] → CS=HIGH
 */
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

            EPD_SPI_DLY_DAT();   /* Data setup time ≥ 15ns */
            EPD_SCK_HIGH();
            EPD_SPI_DLY_SCK();   /* SCK high ≥ 35ns */
            EPD_SCK_LOW();
            EPD_SPI_DLY_SCK();   /* SCK low ≥ 35ns */
        }
    }

    EPD_CS_HIGH();
}

/*********************************************************************
 * @fn      Epaper_Hw_ReadReg
 * @note   SPI read – may not work on modules without MISO connection.
 */
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
            EPD_SPI_DLY_SCK();   /* SCK high ≥ 35ns */
            val <<= 1;
            if (GPIOA->INDR & GPIO_Pin_6)
                val |= 0x01;
            EPD_SCK_LOW();
            EPD_SPI_DLY_SCK();   /* SCK low ≥ 35ns */
        }
        buf[i] = val;
    }
    EPD_CS_HIGH();
}
