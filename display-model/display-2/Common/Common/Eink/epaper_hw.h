/**
 * @file    epaper_hw.h
 * @brief   JD79686AB e-paper hardware abstraction – software SPI + GPIO.
 *
 * Pin assignment (CH32V307RCT6):
 *   PA4  – CS#   (chip select)
 *   PA5  – SCK   (SPI clock)
 *   PA7  – MOSI  (SPI data out)
 *   PB3  – RES#  (hardware reset)
 *   PB4  – D/C#  (data/command)
 *   PB5  – BUSY_N (busy indicator, input)
 *
 * Uses direct register manipulation (BSHR/BCR/OUTDR) for maximum speed,
 * following the soft_8080 approach. No SPI peripheral needed.
 */
#ifndef __EPAPER_HW_H
#define __EPAPER_HW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32v30x.h"
#include "debug.h"

/*=============================================================================
 *  Pin bit masks
 *===========================================================================*/
#define EPD_CS_BIT    GPIO_Pin_4   /* PA4 */
#define EPD_SCK_BIT   GPIO_Pin_5   /* PA5 */
#define EPD_MOSI_BIT  GPIO_Pin_7   /* PA7 */
#define EPD_RES_BIT   GPIO_Pin_3   /* PB3 */
#define EPD_DC_BIT    GPIO_Pin_4   /* PB4 */
#define EPD_BUSY_BIT  GPIO_Pin_5   /* PB5 */

/*=============================================================================
 *  Atomic pin control macros (BSHR = set, BCR = clear)
 *===========================================================================*/
#define EPD_CS_LOW()   do { GPIOA->BCR  = EPD_CS_BIT;   } while(0)
#define EPD_CS_HIGH()  do { GPIOA->BSHR = EPD_CS_BIT;   } while(0)
#define EPD_SCK_LOW()  do { GPIOA->BCR  = EPD_SCK_BIT;  } while(0)
#define EPD_SCK_HIGH() do { GPIOA->BSHR = EPD_SCK_BIT;  } while(0)
#define EPD_MOSI_LOW() do { GPIOA->BCR  = EPD_MOSI_BIT; } while(0)
#define EPD_MOSI_HIGH()do { GPIOA->BSHR = EPD_MOSI_BIT; } while(0)
#define EPD_DC_LOW()   do { GPIOB->BCR  = EPD_DC_BIT;   } while(0)
#define EPD_DC_HIGH()  do { GPIOB->BSHR = EPD_DC_BIT;   } while(0)
#define EPD_RES_LOW()  do { GPIOB->BCR  = EPD_RES_BIT;   } while(0)
#define EPD_RES_HIGH() do { GPIOB->BSHR = EPD_RES_BIT;   } while(0)

#define EPD_BUSY_READ() (GPIOB->INDR & EPD_BUSY_BIT)

/*=============================================================================
 *  Software SPI: shift one byte MSB-first (CPOL=0, CPHA=0)
 *===========================================================================*/
static inline void EPD_SPI_WriteByte(uint8_t data)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        if (data & 0x80)
            EPD_MOSI_HIGH();
        else
            EPD_MOSI_LOW();
        data <<= 1;

        EPD_SCK_HIGH();
        EPD_SCK_LOW();
    }
}

/*=============================================================================
 *  Inline pin helpers
 *===========================================================================*/
static inline uint8_t Epd_Is_Busy(void)
{
    /* BUSY_N: LOW(0) = IC busy, HIGH(1) = IC idle */
    return (EPD_BUSY_READ() == 0) ? 1 : 0;
}

/* ---- API ---- */

/**
 * @brief  Initialise GPIO pins for software SPI + control signals.
 */
void Epaper_Hw_Init(void);

/**
 * @brief  Hardware reset (double pulse, matching reference code).
 */
void Epaper_Hw_Reset(void);

/**
 * @brief  Block until BUSY_N goes HIGH (idle) or timeout.
 * @return 0 on success, -1 on timeout.
 */
int Epaper_Hw_WaitBusy(uint32_t timeout_ms);

/**
 * @brief  Send one command byte (DC=low, CS toggled).
 */
void Epaper_Hw_WriteCmd(uint8_t cmd);

/**
 * @brief  Send one data byte (DC=high, CS toggled).
 */
void Epaper_Hw_WriteData(uint8_t data);

/**
 * @brief  Send a data buffer (DC=high, CS held low for entire transfer).
 */
void Epaper_Hw_WriteDataBuf(const uint8_t *buf, uint32_t len);

/**
 * @brief  Read N bytes from a register (DC=high after cmd, CS held low).
 *         Note: may not work on modules without MISO connection.
 */
void Epaper_Hw_ReadReg(uint8_t reg, uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __EPAPER_HW_H */
