/********************************** (C) COPYRIGHT *******************************
* File Name          : ssd1963.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/20
* Description        : SSD1963 display controller driver API.
*                      8080 parallel interface via FMC Bank1 NORSRAM1.
********************************************************************************/
#ifndef __SSD1963_H
#define __SSD1963_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32h417.h"
#include <stdbool.h>

/*=============================================================================
 *  SSD1963 Command Definitions
 *=============================================================================*/
#define SSD1963_NOP                     0x00
#define SSD1963_SOFT_RESET              0x01
#define SSD1963_GET_POWER_MODE          0x0A
#define SSD1963_GET_ADDRESS_MODE        0x0B
#define SSD1963_GET_DISPLAY_MODE        0x0D
#define SSD1963_GET_TEAR_EFFECT         0x0E
#define SSD1963_ENTER_SLEEP_MODE        0x10
#define SSD1963_EXIT_SLEEP_MODE         0x11
#define SSD1963_ENTER_PARTIAL_MODE      0x12
#define SSD1963_ENTER_NORMAL_MODE       0x13
#define SSD1963_EXIT_INVERT_MODE        0x20
#define SSD1963_ENTER_INVERT_MODE       0x21
#define SSD1963_SET_GAMMA_CURVE         0x26
#define SSD1963_SET_DISPLAY_OFF         0x28
#define SSD1963_SET_DISPLAY_ON          0x29
#define SSD1963_SET_COLUMN_ADDRESS      0x2A
#define SSD1963_SET_PAGE_ADDRESS        0x2B
#define SSD1963_WRITE_MEMORY_START      0x2C
#define SSD1963_READ_MEMORY_START       0x2E
#define SSD1963_SET_PARTIAL_AREA        0x30
#define SSD1963_SET_SCROLL_AREA         0x33
#define SSD1963_SET_TEAR_OFF            0x34
#define SSD1963_SET_TEAR_ON             0x35
#define SSD1963_SET_ADDRESS_MODE        0x36
#define SSD1963_SET_SCROLL_START        0x37
#define SSD1963_EXIT_IDLE_MODE          0x38
#define SSD1963_ENTER_IDLE_MODE         0x39
#define SSD1963_SET_PIXEL_FORMAT        0x3A
#define SSD1963_WRITE_MEMORY_CONTINUE   0x3C
#define SSD1963_READ_MEMORY_CONTINUE    0x3E
#define SSD1963_SET_TEAR_SCANLINE       0x44
#define SSD1963_GET_SCANLINE            0x45
#define SSD1963_READ_DDB                0xA1
#define SSD1963_SET_LCD_MODE            0xB0
#define SSD1963_SET_HORI_PERIOD         0xB4
#define SSD1963_SET_VERT_PERIOD         0xB6
#define SSD1963_SET_GPIO_CONF           0xB8
#define SSD1963_SET_GPIO_VALUE          0xBA
#define SSD1963_SET_POST_PROC           0xBC
#define SSD1963_SET_PWM_CONF            0xBE
#define SSD1963_SET_DBC_CONF            0xD0
#define SSD1963_GET_DBC_CONF            0xD1
#define SSD1963_SET_DBC_TH              0xD4
#define SSD1963_GET_DBC_TH              0xD5
#define SSD1963_SET_PLL                 0xE0
#define SSD1963_SET_PLL_MN              0xE2
#define SSD1963_GET_PLL_STATUS          0xE4
#define SSD1963_SET_DEEP_SLEEP          0xE5
#define SSD1963_SET_LSHIFT_FREQ         0xE6
#define SSD1963_SET_PIXEL_DATA_INTERFACE 0xF0

/*=============================================================================
 *  Screen Dimensions
 *=============================================================================*/
#define SSD1963_WIDTH                   800
#define SSD1963_HEIGHT                  480

/*=============================================================================
 *  API Functions
 *=============================================================================*/

/* Low-level access */
void SSD1963_WriteCmd(uint16_t cmd);
void SSD1963_WriteData(uint16_t data);
uint16_t SSD1963_ReadData(void);
void SSD1963_WriteReg(uint16_t reg, uint16_t val);
uint16_t SSD1963_ReadReg(uint16_t reg);

/* Initialization & Verification */
void SSD1963_Init(void);
bool SSD1963_SelfTest(void);

/* Window / GRAM access */
void SSD1963_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void SSD1963_WriteData16(uint16_t data);
void SSD1963_WriteData16Fast(uint16_t data);
void SSD1963_WriteBuffer(const uint16_t *buf, uint32_t len);
void SSD1963_FillColor(uint32_t count, uint16_t color);
void SSD1963_Clear(uint16_t color);

/* Backlight control (SSD1963 internal PWM) */
void SSD1963_SetBacklight(uint8_t pwm);

/* VSYNC / Tearing Effect synchronization */
void SSD1963_TE_Init(void);       /* Configure PB8 as TE input */
bool SSD1963_TE_IsHigh(void);     /* Read TE pin state */
void SSD1963_WaitVSync(void);     /* Wait for next VSYNC (TE rising edge) */

#ifdef __cplusplus
}
#endif

#endif /* __SSD1963_H */
