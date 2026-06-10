/**
 * @file    epaper_hw.c
 * @brief   JD79686AB hardware abstraction – software SPI via direct register
 *          manipulation (BSHR/BCR). No SPI peripheral needed.
 *
 * Inspired by soft_8080.c approach: atomic BSHR/BCR for single-pin ops.
 */
#include "epaper_hw.h"

/*********************************************************************
 * @fn      Epaper_Hw_Init
 *
 * @brief   Init all GPIO pins for software SPI + control.
 *          No SPI peripheral, no AFIO remap needed (PB3/PB4 freed
 *          by disabling JTAG via AFIO->PCFR1).
 */
void Epaper_Hw_Init(void)
{
    GPIO_InitTypeDef gpio;

    /* Enable GPIO clocks + AFIO (for JTAG disable) */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_AFIO, ENABLE);

    /* Disable JTAG to free PB3 (JTDO) and PB4 (NJTRST).
     * SWJ_CFG bits [26:24] are write-only. */
    AFIO->PCFR1 &= ~(uint32_t)AFIO_PCFR1_SWJ_CFG;
    AFIO->PCFR1 |= AFIO_PCFR1_SWJ_CFG_JTAGDISABLE;

    /* ---- GPIOA: CS(PA4), SCK(PA5), MOSI(PA7) as output ---- */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;

    gpio.GPIO_Pin = EPD_CS_BIT | EPD_SCK_BIT | EPD_MOSI_BIT;
    GPIO_Init(GPIOA, &gpio);

    /* PA6 (MISO) as floating input for potential register reads */
    gpio.GPIO_Pin  = GPIO_Pin_6;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    /* Idle states */
    EPD_CS_HIGH();
    EPD_SCK_LOW();     /* CPOL=0: clock idles low */
    EPD_MOSI_LOW();

    /* ---- GPIOB: RES(PB3), DC(PB4) as output ---- */
    gpio.GPIO_Pin = EPD_RES_BIT | EPD_DC_BIT;
    GPIO_Init(GPIOB, &gpio);

    EPD_RES_HIGH();    /* RES# idle high */
    EPD_DC_HIGH();     /* DC# idle high (data mode) */

    /* ---- GPIOB: BUSY(PB5) as input with pull-up ---- */
    gpio.GPIO_Pin  = EPD_BUSY_BIT;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio);
}

/*********************************************************************
 * @fn      Epaper_Hw_Reset
 *
 * @brief   Single-pulse hardware reset (matching manufacturer STM32 example).
 *          delay(100ms) → RES LOW(20ms) → RES HIGH(20ms) → WaitBusy
 */
void Epaper_Hw_Reset(void)
{
    Delay_Ms(100);
    EPD_RES_LOW();
    Delay_Ms(20);
    EPD_RES_HIGH();
    Delay_Ms(20);

    printf("EPD: Reset done, BUSY_N raw = %d (0=busy, 1=idle)\r\n",
           EPD_BUSY_READ() ? 1 : 0);
}

/*********************************************************************
 * @fn      Epaper_Hw_WaitBusy
 */
int Epaper_Hw_WaitBusy(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while (Epd_Is_Busy())
    {
        if (elapsed >= timeout_ms)
        {
            printf("EPD: WaitBusy TIMEOUT after %ums (raw=%d)\r\n",
                   (unsigned)elapsed, EPD_BUSY_READ() ? 1 : 0);
            return -1;
        }
        Delay_Ms(10);
        elapsed += 10;
    }
    if (elapsed > 0)
    {
        printf("EPD: WaitBusy OK after %ums\r\n", (unsigned)elapsed);
    }
    return 0;
}

/*********************************************************************
 * @fn      Epaper_Hw_WriteCmd
 *
 * @brief   Send one command byte. DC=low during transfer.
 *          Reference: CS low → DC low → shift byte → CS high → DC high.
 */
void Epaper_Hw_WriteCmd(uint8_t cmd)
{
    EPD_CS_LOW();
    EPD_DC_LOW();
    EPD_SPI_WriteByte(cmd);
    EPD_CS_HIGH();
    EPD_DC_HIGH();
}

/*********************************************************************
 * @fn      Epaper_Hw_WriteData
 *
 * @brief   Send one data byte. DC=high during transfer.
 */
void Epaper_Hw_WriteData(uint8_t data)
{
    EPD_CS_LOW();
    EPD_SPI_WriteByte(data);
    EPD_CS_HIGH();
}

/*********************************************************************
 * @fn      Epaper_Hw_WriteDataBuf
 *
 * @brief   Send a buffer of data bytes. CS held low for entire transfer
 *          (matching reference EPD_IO_WriteDataBytes pattern).
 */
void Epaper_Hw_WriteDataBuf(const uint8_t *buf, uint32_t len)
{
    uint32_t i;
    EPD_CS_LOW();
    for (i = 0; i < len; i++)
    {
        EPD_SPI_WriteByte(buf[i]);
    }
    EPD_CS_HIGH();
}

/*********************************************************************
 * @fn      Epaper_Hw_ReadReg
 *
 * @brief   Read N bytes from a register via software SPI.
 *          Phase 1: Send reg address (DC=low).
 *          Phase 2: Clock in data (DC=high, CS held low).
 *
 * @note    Requires MISO (PA6) connected. Many e-paper modules
 *          do NOT have MISO – reads will return 0xFF.
 */
void Epaper_Hw_ReadReg(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i, j, val;

    /* Phase 1: Send register address */
    Epaper_Hw_WriteCmd(reg);

    /* Phase 2: Read data (DC=high, CS held low) */
    EPD_CS_LOW();
    /* DC already high from WriteCmd */
    for (i = 0; i < len; i++)
    {
        val = 0;
        for (j = 0; j < 8; j++)
        {
            EPD_SCK_HIGH();
            val <<= 1;
            /* Read MISO (PA6) – may be floating if not connected */
            if (GPIOA->INDR & GPIO_Pin_6)
                val |= 0x01;
            EPD_SCK_LOW();
        }
        buf[i] = val;
    }
    EPD_CS_HIGH();
}
