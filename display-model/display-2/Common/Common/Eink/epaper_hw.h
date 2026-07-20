/**
 * @file    epaper_hw.h
 * @brief   JD79686AB e-paper hardware abstraction.
 *
 * Pin assignment (CH32V307RCT6):
 *   PA4  – CS#   (chip select, GPIO)
 *   PA5  – SCK   (SPI1 clock, AF)
 *   PA7  – MOSI  (SPI1 data out, AF)
 *   PB3  – RES#  (hardware reset)
 *   PB4  – D/C#  (data/command)
 *   PB5  – BUSY_N (busy indicator, input)
 *
 * Transport is selected with EPD_USE_HW_SPI below:
 *   1 = hardware SPI1 @ 9 MHz + DMA1 Ch3 (default, fast block transfers)
 *   0 = legacy GPIO bit-bang (reference timing, kept as fallback)
 */
#ifndef __EPAPER_HW_H
#define __EPAPER_HW_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32v30x.h"
#include "debug.h"

/*=============================================================================
 *  Transport selection
 *===========================================================================*/
#define EPD_USE_HW_SPI      1

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
 *  SPI timing delay macros
 *
 *  CH32V307 @ 144MHz: 1 NOP ≈ 7ns
 *  JD79686AB requires: SCK high/low ≥ 35ns, data setup ≥ 15ns
 *  Use 8 NOPs (~56ns) for SCK, 4 NOPs (~28ns) for data setup
 *===========================================================================*/
#define EPD_SPI_DLY_SCK()  do { __asm__ volatile ("nop"); __asm__ volatile ("nop"); \
                                __asm__ volatile ("nop"); __asm__ volatile ("nop"); \
                                __asm__ volatile ("nop"); __asm__ volatile ("nop"); \
                                __asm__ volatile ("nop"); __asm__ volatile ("nop"); } while(0)
#define EPD_SPI_DLY_DAT()  do { __asm__ volatile ("nop"); __asm__ volatile ("nop"); \
                                __asm__ volatile ("nop"); __asm__ volatile ("nop"); } while(0)

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
 * @brief  Send the same byte N times (DC=high, CS held low).
 *         Hardware mode uses DMA with a fixed source address.
 */
void Epaper_Hw_WriteDataFill(uint8_t data, uint32_t len);

/**
 * @brief  Read N bytes from a register (DC=high after cmd, CS held low).
 *         Note: may not work on modules without MISO connection.
 */
void Epaper_Hw_ReadReg(uint8_t reg, uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __EPAPER_HW_H */
