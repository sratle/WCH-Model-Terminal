/********************************** (C) COPYRIGHT *******************************
* File Name          : ssd1963.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : SSD1963 display controller driver header.
*                      800x480 RGB888 LCD, MCU 16-bit RGB565 input via FMC 8080.
********************************************************************************/
#ifndef __SSD1963_H
#define __SSD1963_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32h417.h"
#include "../FMC/fmc_driver.h"

/*=============================================================================
 *  Screen Parameters
 *=============================================================================*/
#define LCD_WIDTH               800
#define LCD_HEIGHT              480
#define LCD_PIXELS              (LCD_WIDTH * LCD_HEIGHT)

/*=============================================================================
 *  SSD1963 Register Commands
 *=============================================================================*/

/* System commands */
#define SSD1963_NOP             0x00
#define SSD1963_SOFT_RESET      0x01
#define SSD1963_GET_POWER_MODE  0x0A
#define SSD1963_GET_ADDRESS_MODE 0x0B
#define SSD1963_GET_DISPLAY_MODE 0x0D
#define SSD1963_GET_TEAR_EFFECT_STATUS 0x0E
#define SSD1963_ENTER_SLEEP_MODE 0x10
#define SSD1963_EXIT_SLEEP_MODE 0x11
#define SSD1963_ENTER_PARTIAL_MODE 0x12
#define SSD1963_ENTER_NORMAL_MODE 0x13
#define SSD1963_EXIT_INVERT_MODE 0x20
#define SSD1963_ENTER_INVERT_MODE 0x21
#define SSD1963_SET_DISPLAY_OFF 0x28
#define SSD1963_SET_DISPLAY_ON  0x29
#define SSD1963_SET_COLUMN_ADDRESS 0x2A
#define SSD1963_SET_PAGE_ADDRESS 0x2B
#define SSD1963_WRITE_MEMORY_START 0x2C
#define SSD1963_READ_MEMORY_START 0x2E
#define SSD1963_SET_PARTIAL_AREA 0x30
#define SSD1963_SET_SCROLL_AREA 0x33
#define SSD1963_SET_TEAR_OFF    0x34
#define SSD1963_SET_TEAR_ON     0x35
#define SSD1963_SET_ADDRESS_MODE 0x36
#define SSD1963_SET_SCROLL_START 0x37
#define SSD1963_EXIT_IDLE_MODE  0x38
#define SSD1963_ENTER_IDLE_MODE 0x39
#define SSD1963_SET_PIXEL_DATA_INTERFACE 0xF0

/* LCD driver AC control */
#define SSD1963_SET_LCD_MODE    0xB0
#define SSD1963_SET_HORI_PERIOD 0xB4
#define SSD1963_SET_VERT_PERIOD 0xB6
#define SSD1963_SET_GPIO_CONF   0xB8
#define SSD1963_SET_GPIO_VALUE  0xBA
#define SSD1963_SET_POST_PROC   0xBC
#define SSD1963_SET_PWM_CONF    0xBE
#define SSD1963_SET_LCD_GEN0    0xC0
#define SSD1963_SET_LCD_GEN1    0xC1
#define SSD1963_SET_LCD_GEN2    0xC2
#define SSD1963_SET_LCD_GEN3    0xC3
#define SSD1963_SET_GPIO0_ROP   0xC4
#define SSD1963_SET_GPIO1_ROP   0xC5
#define SSD1963_SET_GPIO2_ROP   0xC6
#define SSD1963_SET_GPIO3_ROP   0xC7
#define SSD1963_SET_DBC_CONF    0xD0

/* PLL and clock */
#define SSD1963_SET_PLL         0xE0
#define SSD1963_SET_PLL_MN      0xE2
#define SSD1963_GET_PLL_STATUS  0xE4
#define SSD1963_SET_DEEP_SLEEP  0xE5
#define SSD1963_SET_LSHIFT_FREQ 0xE6

/*=============================================================================
 *  GPIO Pin Definitions (SSD1963 Hardware Reset)
 *=============================================================================*/
#define SSD1963_RST_PORT        GPIOA
#define SSD1963_RST_PIN         GPIO_Pin_0

/*=============================================================================
 *  Function Prototypes
 *=============================================================================*/

/**
 * @brief  Initialize SSD1963 display controller.
 *         Performs hardware reset, PLL configuration, LCD timing setup,
 *         and enables display output.
 * @param  None
 * @return None
 */
void SSD1963_Init(void);

/**
 * @brief  Hardware reset SSD1963 via RST pin (PA0).
 * @param  None
 * @return None
 */
void SSD1963_HardReset(void);

/**
 * @brief  Software reset SSD1963.
 * @param  None
 * @return None
 */
void SSD1963_SoftReset(void);

/**
 * @brief  Write a command to SSD1963.
 * @param  cmd: 8-bit command code
 * @return None
 */
static inline void SSD1963_WriteCmd(uint8_t cmd)
{
    SSD1963_CMD_ADDR = cmd;
}

/**
 * @brief  Write an 8-bit data parameter to SSD1963.
 * @param  data: 8-bit data value
 * @return None
 */
static inline void SSD1963_WriteData(uint8_t data)
{
    SSD1963_DATA_ADDR = data;
}

/**
 * @brief  Write a 16-bit data (RGB565 pixel) to SSD1963 GRAM.
 * @param  data: 16-bit RGB565 color value
 * @return None
 */
static inline void SSD1963_WriteData16(uint16_t data)
{
    SSD1963_DATA_ADDR = data;
}

/**
 * @brief  Write multiple 16-bit pixels to SSD1963 GRAM.
 * @param  buf: pointer to RGB565 pixel buffer
 * @param  len: number of pixels to write
 * @return None
 */
void SSD1963_WriteBuffer(const uint16_t *buf, uint32_t len);

/**
 * @brief  Set display window (GRAM write area).
 * @param  x0: start column (0 ~ LCD_WIDTH-1)
 * @param  y0: start page/row (0 ~ LCD_HEIGHT-1)
 * @param  x1: end column (0 ~ LCD_WIDTH-1)
 * @param  y1: end page/row (0 ~ LCD_HEIGHT-1)
 * @return None
 */
void SSD1963_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief  Fill entire screen with a single RGB565 color.
 * @param  color: RGB565 color value
 * @return None
 */
void SSD1963_Clear(uint16_t color);

/**
 * @brief  Draw a single pixel at specified coordinates.
 * @param  x: column coordinate (0 ~ LCD_WIDTH-1)
 * @param  y: row coordinate (0 ~ LCD_HEIGHT-1)
 * @param  color: RGB565 color value
 * @return None
 */
void SSD1963_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief  Configure SSD1963 internal PWM for backlight control.
 * @param  duty: PWM duty cycle 0~255 (0=off, 255=100%)
 * @return None
 */
void SSD1963_SetBacklight(uint8_t duty);

#ifdef __cplusplus
}
#endif

#endif /* __SSD1963_H */
