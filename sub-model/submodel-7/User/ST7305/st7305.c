/**
 * @file st7305.c
 * @brief ST7305 驱动实现，直接驱动 CH32V103 SPI1 硬件
 */
#include "st7305.h"
#include "debug.h"

/*=============================================================================
 *  Hardware low-level functions (static)
 *=============================================================================*/

static void ST7305_WriteDC(uint8_t level)
{
    if (level)
        GPIO_SetBits(ST7305_DC_PORT, ST7305_DC_PIN);
    else
        GPIO_ResetBits(ST7305_DC_PORT, ST7305_DC_PIN);
}

static void ST7305_WriteCS(uint8_t level)
{
    if (level)
        GPIO_SetBits(ST7305_NSS_PORT, ST7305_NSS_PIN);
    else
        GPIO_ResetBits(ST7305_NSS_PORT, ST7305_NSS_PIN);
}

static void ST7305_WriteRST(uint8_t level)
{
    if (level)
        GPIO_SetBits(ST7305_RES_PORT, ST7305_RES_PIN);
    else
        GPIO_ResetBits(ST7305_RES_PORT, ST7305_RES_PIN);
}

static void ST7305_SPI_Transmit(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++)
    {
        while (SPI_I2S_GetFlagStatus(ST7305_SPI, SPI_I2S_FLAG_TXE) == RESET)
            ;
        SPI_I2S_SendData(ST7305_SPI, buf[i]);
    }
    /* Wait for transmission complete */
    while (SPI_I2S_GetFlagStatus(ST7305_SPI, SPI_I2S_FLAG_BSY) == SET)
        ;
}

static void ST7305_PinInit(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | ST7305_SPI_RCC, ENABLE);

    /* SPI1_SCK (PA5) - AF Push-Pull */
    gpio.GPIO_Pin   = ST7305_SCK_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(ST7305_SCK_PORT, &gpio);

    /* SPI1_MOSI (PA7) - AF Push-Pull */
    gpio.GPIO_Pin  = ST7305_MOSI_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(ST7305_MOSI_PORT, &gpio);

    /* SPI1_NSS (PA4) - Output Push-Pull (software managed) */
    gpio.GPIO_Pin  = ST7305_NSS_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(ST7305_NSS_PORT, &gpio);

    /* DC (PA3) - Output Push-Pull */
    gpio.GPIO_Pin  = ST7305_DC_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(ST7305_DC_PORT, &gpio);

    /* RES (PA2) - Output Push-Pull */
    gpio.GPIO_Pin  = ST7305_RES_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(ST7305_RES_PORT, &gpio);

    /* SPI1 configuration */
    {
        SPI_InitTypeDef spi;
        SPI_StructInit(&spi);
        spi.SPI_Direction         = SPI_Direction_1Line_Tx;
        spi.SPI_Mode              = SPI_Mode_Master;
        spi.SPI_DataSize          = SPI_DataSize_8b;
        spi.SPI_CPOL              = SPI_CPOL_Low;
        spi.SPI_CPHA              = SPI_CPHA_1Edge;
        spi.SPI_NSS               = SPI_NSS_Soft;
        spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4; /* 72/4 = 18MHz */
        spi.SPI_FirstBit          = SPI_FirstBit_MSB;
        SPI_Init(ST7305_SPI, &spi);
        SPI_Cmd(ST7305_SPI, ENABLE);
    }
}

/*=============================================================================
 *  Init sequence table (optimized for this specific panel)
 *=============================================================================*/

lcd_init_seq_stu st7305_init_table[] = {
    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xD6}},       /* NVM Load Control */
    {LCD_CTRL_WRITE_DATA, 2, (const uint8_t []){0x17, 0x02}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xD1}},       /* Booster Enable */
    {LCD_CTRL_WRITE_DATA, 1, (const uint8_t []){0x01}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xC0}},       /* Gate Voltage Setting */
    {LCD_CTRL_WRITE_DATA, 2, (const uint8_t []){0x12, 0x0A}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xC1}},       /* VSHP Setting */
    {LCD_CTRL_WRITE_DATA, 4, (const uint8_t []){0x3C, 0x3E, 0x3C, 0x3C}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xC2}},       /* VSLP Setting */
    {LCD_CTRL_WRITE_DATA, 4, (const uint8_t []){0x23, 0x21, 0x23, 0x23}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xC4}},       /* VSHN Setting */
    {LCD_CTRL_WRITE_DATA, 4, (const uint8_t []){0x5A, 0x5C, 0x5A, 0x5A}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xC5}},       /* VSLN Setting */
    {LCD_CTRL_WRITE_DATA, 4, (const uint8_t []){0x37, 0x35, 0x37, 0x37}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xD8}},       /* Oscillator off */
    {LCD_CTRL_WRITE_DATA, 2, (const uint8_t []){0x26, 0xE9}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xB2}},       /* Frame Rate Control */
    {LCD_CTRL_WRITE_DATA, 1, (const uint8_t []){0x05}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xB3}},       /* EQ Control HPM */
    {LCD_CTRL_WRITE_DATA, 10, (const uint8_t []){0xE5, 0xF6, 0x17, 0x77, 0x77,
                                                   0x77, 0x77, 0x77, 0x77, 0x71}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xB4}},       /* EQ Control LPM */
    {LCD_CTRL_WRITE_DATA, 8, (const uint8_t []){0x05, 0x46, 0x77, 0x77,
                                                  0x77, 0x77, 0x76, 0x45}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0x62}},
    {LCD_CTRL_WRITE_DATA, 3, (const uint8_t []){0x32, 0x03, 0x1F}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xB7}},       /* Source EQ Enable */
    {LCD_CTRL_WRITE_DATA, 1, (const uint8_t []){0x13}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xB0}},       /* Gate Line Setting */
    {LCD_CTRL_WRITE_DATA, 1, (const uint8_t []){0x3F}},      /* 252 line */

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0x11}},       /* Sleep out */
    {LCD_CTRL_DELAY, 120},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xC9}},       /* Source Voltage Select */
    {LCD_CTRL_WRITE_DATA, 1, (const uint8_t []){0x00}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0x36}},       /* Memory Data Access Control */
    {LCD_CTRL_WRITE_DATA, 1, (const uint8_t []){0x48}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0x3A}},       /* Data Format Select */
    {LCD_CTRL_WRITE_DATA, 1, (const uint8_t []){0x11}},      /* 3write for 24bit */

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xB9}},       /* Gamma Mode: Mono */
    {LCD_CTRL_WRITE_DATA, 1, (const uint8_t []){0x20}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xB8}},       /* Panel Setting */
    {LCD_CTRL_WRITE_DATA, 1, (const uint8_t []){0x29}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0x2A}},       /* Column Address */
    {LCD_CTRL_WRITE_DATA, 2, (const uint8_t []){0x19, 0x23}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0x2B}},       /* Row Address */
    {LCD_CTRL_WRITE_DATA, 2, (const uint8_t []){0x00, 0x7C}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0x35}},       /* TE */
    {LCD_CTRL_WRITE_DATA, 1, (const uint8_t []){0x00}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xD0}},       /* Auto power down */
    {LCD_CTRL_WRITE_DATA, 1, (const uint8_t []){0xFF}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0x39}},       /* Low power mode */
    {LCD_CTRL_DELAY, 20},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0xBB}},
    {LCD_CTRL_WRITE_DATA, 1, (const uint8_t []){0x4F}},

    {LCD_CTRL_WRITE_CMD, 1, (const uint8_t []){0x29}},       /* DISPLAY ON */

    {LCD_CTRL_OVER, 0, NULL},
};

/*=============================================================================
 *  Core driver functions
 *=============================================================================*/

void st7305_write_cmd(struct st7305_stu *lcd, uint8_t cmd)
{
    (void)lcd;
    ST7305_WriteDC(ST7305_DC_CMD);
    ST7305_SPI_Transmit(&cmd, 1);
}

void st7305_write_data(struct st7305_stu *lcd, const uint8_t *data, uint16_t len)
{
    (void)lcd;
    ST7305_WriteDC(ST7305_DC_DATA);
    ST7305_SPI_Transmit(data, len);
}

#ifdef ST7305_NEED_FULL_BUFFER
int st7305_init(struct st7305_stu *lcd, uint8_t *full_buffer)
#else
int st7305_init(struct st7305_stu *lcd)
#endif
{
    if (lcd == NULL)
        return -1;

#ifdef ST7305_NEED_FULL_BUFFER
    if (full_buffer == NULL)
        return -1;
    lcd->full_buffer = full_buffer;
#endif

    lcd->is_initialized = 0;

    /* Initialize hardware */
    ST7305_PinInit();

    ST7305_WriteCS(1);

    /* Hardware reset */
    ST7305_WriteRST(1);
    Delay_Ms(10);
    ST7305_WriteRST(0);
    Delay_Ms(10);
    ST7305_WriteRST(1);
    Delay_Ms(10);

    ST7305_WriteCS(0);

    /* Execute init sequence */
    for (uint16_t i = 0; ; i++)
    {
        switch (st7305_init_table[i].ctrl)
        {
            case LCD_CTRL_WRITE_CMD:
                st7305_write_cmd(lcd, st7305_init_table[i].data_buf[0]);
                break;

            case LCD_CTRL_WRITE_DATA:
                st7305_write_data(lcd, st7305_init_table[i].data_buf,
                                  st7305_init_table[i].data_len);
                break;

            case LCD_CTRL_DELAY:
                Delay_Ms(st7305_init_table[i].delay_ms);
                break;

            case LCD_CTRL_OVER:
                lcd->is_initialized = 1;
                return 0;

            default:
                return -2;
        }
    }

    return -3;
}

void st7305_set_unit_window(struct st7305_stu *lcd, uint16_t unit_x0, uint16_t unit_y0,
                            uint16_t unit_x1, uint16_t unit_y1)
{
    uint8_t temp[2];

    if (unit_x0 > LCD_UNIT_WIDTH || unit_x1 > LCD_UNIT_WIDTH ||
        unit_y0 > LCD_UNIT_HEIGHT || unit_y1 > LCD_UNIT_HEIGHT)
        return;

    /* Column address (reversed for this panel orientation) */
    st7305_write_cmd(lcd, 0x2A);
    temp[0] = LCD_UNIT_WIDTH - 1 - unit_x1 + 0x19;
    temp[1] = LCD_UNIT_WIDTH - 1 - unit_x0 + 0x19;
    st7305_write_data(lcd, temp, 2);

    /* Row address */
    st7305_write_cmd(lcd, 0x2B);
    temp[0] = unit_y0;
    temp[1] = unit_y1;
    st7305_write_data(lcd, temp, 2);

    /* Write memory start */
    st7305_write_cmd(lcd, 0x2C);
}

void st7305_fill_unit(struct st7305_stu *lcd, uint16_t unit_x, uint16_t unit_y,
                      uint16_t unit_w, uint16_t unit_h, uint8_t color)
{
    uint8_t unit_data_buf[3];

    st7305_set_unit_window(lcd, unit_x, unit_y,
                           unit_x + unit_w - 1, unit_y + unit_h - 1);

    if (color)
    {
        unit_data_buf[0] = 0xFF;
        unit_data_buf[1] = 0xFF;
        unit_data_buf[2] = 0xFF;
    }
    else
    {
        unit_data_buf[0] = 0x00;
        unit_data_buf[1] = 0x00;
        unit_data_buf[2] = 0x00;
    }

    for (uint8_t i = 0; i < unit_w; i++)
    {
        for (uint8_t j = 0; j < unit_h; j++)
        {
            st7305_write_data(lcd, unit_data_buf, 3);
        }
    }
}

void st7305_draw_unit(struct st7305_stu *lcd, uint16_t unit_x, uint16_t unit_y,
                      uint8_t *buffer)
{
    st7305_set_unit_window(lcd, unit_x, unit_y, unit_x, unit_y);
    st7305_write_data(lcd, buffer, 3);
}

void st7305_clear(struct st7305_stu *lcd, uint8_t color)
{
    st7305_fill_unit(lcd, 0, 0, LCD_UNIT_WIDTH, LCD_UNIT_HEIGHT, color);
}

void st7305_power_high(struct st7305_stu *lcd)
{
    st7305_write_cmd(lcd, 0x38);
}

void st7305_power_low(struct st7305_stu *lcd)
{
    st7305_write_cmd(lcd, 0x39);
}

/*=============================================================================
 *  Full buffer functions
 *=============================================================================*/

#ifdef ST7305_NEED_FULL_BUFFER

void st7305_buf_draw_pix(struct st7305_stu *lcd, uint16_t x, uint16_t y, uint8_t color)
{
    uint16_t unit_x, unit_y, byte_index;
    uint8_t line_bit_4, bit_position;

    if (x >= LCD_WIDTH || y >= LCD_HEIGHT)
        return;

    x += LCD_X_OFFSET;

    unit_x = x >> 2;
    unit_y = y >> 1;
    byte_index = unit_y * LCD_UNIT_WIDTH * 3 + unit_x;

    line_bit_4 = x % 4;
    bit_position = 7 - (line_bit_4 * 2 + (y % 2));

    if (color != 0)
        lcd->full_buffer[byte_index] |= (1 << bit_position);
    else
        lcd->full_buffer[byte_index] &= ~(1 << bit_position);
}

void st7305_buf_refresh(struct st7305_stu *lcd)
{
    st7305_set_unit_window(lcd, 0, 0, LCD_UNIT_WIDTH - 1, LCD_UNIT_HEIGHT - 1);
    st7305_write_data(lcd, lcd->full_buffer, FULL_BUFFER_LENGTH);
}

#endif /* ST7305_NEED_FULL_BUFFER */
