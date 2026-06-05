/**
 * @file st7305.h
 * @brief ST7305 2.13 inch 全反射显示屏驱动库，适配 CH32V103 平台
 *        SPI1 接口：PA4-NSS, PA5-SCK, PA7-MOSI
 *        控制引脚：PA3-DC, PA2-RES
 *        分辨率 122x250，竖屏，全屏缓存模式
 *
 *        像素分布（竖屏，122*250）：
 *        x 需要偏移 10 pix (12-122%12=10)
 *        每个 unit = 3 bytes = 24 pixels (4 cols x 2 rows interleaved)
 *        全屏 = 11 units wide x 125 units tall = 4125 bytes
 */
#ifndef __ST7305_H__
#define __ST7305_H__

#include "ch32v10x.h"

#define ST7305_NEED_FULL_BUFFER

#define LCD_WIDTH                   122
#define LCD_HEIGHT                  250
#define LCD_X_OFFSET                10      /* x 偏移 10 pix */
#define LCD_UNIT_WIDTH              11      /* 122/12 + 1 = 11 */
#define LCD_UNIT_HEIGHT             125     /* 250/2 = 125 */

#ifdef ST7305_NEED_FULL_BUFFER
    #define FULL_BUFFER_LENGTH      4125    /* 11*3*125 = 4125 */
#endif

/* SPI1 pin definitions */
#define ST7305_SPI                  SPI1
#define ST7305_SPI_RCC              RCC_APB2Periph_SPI1

#define ST7305_NSS_PORT             GPIOA
#define ST7305_NSS_PIN              GPIO_Pin_4

#define ST7305_SCK_PORT             GPIOA
#define ST7305_SCK_PIN              GPIO_Pin_5

#define ST7305_MOSI_PORT            GPIOA
#define ST7305_MOSI_PIN             GPIO_Pin_7

#define ST7305_DC_PORT              GPIOA
#define ST7305_DC_PIN               GPIO_Pin_3

#define ST7305_RES_PORT             GPIOA
#define ST7305_RES_PIN              GPIO_Pin_2

/* DC levels */
#define ST7305_DC_DATA              1
#define ST7305_DC_CMD               0

typedef enum {
    LCD_CTRL_WRITE_CMD = 0,
    LCD_CTRL_WRITE_DATA,
    LCD_CTRL_DELAY = 0x25,
    LCD_CTRL_OVER = 0x68,
} LCD_CTRL_e;

typedef struct {
    uint8_t ctrl;           /* LCD_CTRL_e */
    union {
        uint16_t data_len;
        uint16_t delay_ms;
    };
    const uint8_t *data_buf;
} lcd_init_seq_stu;

typedef enum {
    color_mono_white = 0,
    color_mono_black = 1,
} color_mono_e;

struct st7305_stu {
    uint8_t is_initialized;
#ifdef ST7305_NEED_FULL_BUFFER
    uint8_t *full_buffer;
#endif
};

extern lcd_init_seq_stu st7305_init_table[];

#ifdef ST7305_NEED_FULL_BUFFER
int st7305_init(struct st7305_stu *lcd, uint8_t *full_buffer);
#else
int st7305_init(struct st7305_stu *lcd);
#endif

void st7305_clear(struct st7305_stu *lcd, uint8_t color);
void st7305_fill_unit(struct st7305_stu *lcd, uint16_t unit_x, uint16_t unit_y,
                      uint16_t unit_w, uint16_t unit_h, uint8_t color);
void st7305_draw_unit(struct st7305_stu *lcd, uint16_t unit_x, uint16_t unit_y,
                      uint8_t *buffer);

void st7305_write_cmd(struct st7305_stu *lcd, uint8_t cmd);
void st7305_write_data(struct st7305_stu *lcd, const uint8_t *data, uint16_t len);
void st7305_set_unit_window(struct st7305_stu *lcd, uint16_t unit_x0, uint16_t unit_y0,
                            uint16_t unit_x1, uint16_t unit_y1);

void st7305_power_high(struct st7305_stu *lcd);
void st7305_power_low(struct st7305_stu *lcd);

#ifdef ST7305_NEED_FULL_BUFFER
void st7305_buf_draw_pix(struct st7305_stu *lcd, uint16_t x, uint16_t y, uint8_t color);
void st7305_buf_refresh(struct st7305_stu *lcd);
#endif

#endif /* __ST7305_H__ */
