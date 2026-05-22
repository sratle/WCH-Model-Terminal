#include "gt911.h"
#include "ch32h417.h"
#include "debug.h"
#include "../I2C_soft/i2c_soft.h"
#include <string.h>

#define CTP_RST_PIN     GPIO_Pin_0
#define CTP_RST_PORT    GPIOC
#define CTP_INT_PIN     GPIO_Pin_1
#define CTP_INT_PORT    GPIOC

#define GT911_ADDR_LOW      0x5D
#define GT911_ADDR_HIGH     0x14

static uint8_t gt911_addr = GT911_ADDR_LOW;

static uint8_t GT911_Config[] = {
    0x81, 0x00, 0x04, 0x58, 0x02, 0x0A, 0x0C, 0x20, 0x01, 0x08, 0x28, 0x05, 0x50,
    0x3C, 0x0F, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x89, 0x2A, 0x0B, 0x2D, 0x2B, 0x0F, 0x0A, 0x00, 0x00, 0x01, 0xA9, 0x03,
    0x2D, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21,
    0x59, 0x94, 0xC5, 0x02, 0x07, 0x00, 0x00, 0x04, 0x93, 0x24, 0x00, 0x7D, 0x2C,
    0x00, 0x6B, 0x36, 0x00, 0x5D, 0x42, 0x00, 0x53, 0x50, 0x00, 0x53, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A,
    0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x04, 0x06, 0x08, 0x0A, 0x0F, 0x10, 0x12, 0x16, 0x18, 0x1C, 0x1D, 0x1E,
    0x1F, 0x20, 0x21, 0x22, 0x24, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xD6, 0x01
};

static GT911_Status_t gt911_i2c_write(uint8_t dev_addr, uint16_t reg_addr, uint8_t *data, uint16_t len)
{
    SoftI2C_Start();
    SoftI2C_SendByte(dev_addr << 1);
    if (SoftI2C_WaitAck()) { SoftI2C_Stop(); return GT911_Error; }

    SoftI2C_SendByte((reg_addr >> 8) & 0xFF);
    if (SoftI2C_WaitAck()) { SoftI2C_Stop(); return GT911_Error; }

    SoftI2C_SendByte(reg_addr & 0xFF);
    if (SoftI2C_WaitAck()) { SoftI2C_Stop(); return GT911_Error; }

    for (uint16_t i = 0; i < len; i++) {
        SoftI2C_SendByte(data[i]);
        if (SoftI2C_WaitAck()) { SoftI2C_Stop(); return GT911_Error; }
    }

    SoftI2C_Stop();
    return GT911_OK;
}

static GT911_Status_t gt911_i2c_read(uint8_t dev_addr, uint16_t reg_addr, uint8_t *data, uint16_t len)
{
    SoftI2C_Start();
    SoftI2C_SendByte(dev_addr << 1);
    if (SoftI2C_WaitAck()) { SoftI2C_Stop(); return GT911_Error; }

    SoftI2C_SendByte((reg_addr >> 8) & 0xFF);
    if (SoftI2C_WaitAck()) { SoftI2C_Stop(); return GT911_Error; }

    SoftI2C_SendByte(reg_addr & 0xFF);
    if (SoftI2C_WaitAck()) { SoftI2C_Stop(); return GT911_Error; }

    SoftI2C_Start();
    SoftI2C_SendByte((dev_addr << 1) | 0x01);
    if (SoftI2C_WaitAck()) { SoftI2C_Stop(); return GT911_Error; }

    for (uint16_t i = 0; i < len; i++) {
        data[i] = SoftI2C_ReadByte(i < len - 1);
    }

    SoftI2C_Stop();
    return GT911_OK;
}

static void gt911_rst_write(bool high)
{
    if (high) GPIO_SetBits(CTP_RST_PORT, CTP_RST_PIN);
    else GPIO_ResetBits(CTP_RST_PORT, CTP_RST_PIN);
}

static void gt911_int_write(bool high)
{
    if (high) GPIO_SetBits(CTP_INT_PORT, CTP_INT_PIN);
    else GPIO_ResetBits(CTP_INT_PORT, CTP_INT_PIN);
}

static void gt911_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = CTP_RST_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(CTP_RST_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = CTP_INT_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(CTP_INT_PORT, &GPIO_InitStructure);

    gt911_rst_write(true);
    gt911_int_write(true);
}

static void gt911_int_input(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = CTP_INT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(CTP_INT_PORT, &GPIO_InitStructure);
}

static void gt911_int_output(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = CTP_INT_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(CTP_INT_PORT, &GPIO_InitStructure);
}

static void gt911_reset_for_addr_5d(void)
{
    gt911_int_output();
    gt911_int_write(false);
    Delay_Ms(1);
    gt911_rst_write(false);
    Delay_Ms(20);
    gt911_rst_write(true);
    Delay_Ms(6);
    gt911_int_input();
    Delay_Ms(100);
}

static void gt911_reset_for_addr_14(void)
{
    gt911_int_output();
    gt911_int_write(true);
    Delay_Ms(1);
    gt911_rst_write(false);
    Delay_Ms(20);
    gt911_rst_write(true);
    Delay_Ms(6);
    gt911_int_input();
    Delay_Ms(100);
}

static void gt911_calculate_checksum(void)
{
    GT911_Config[184] = 0;
    for (uint8_t i = 0; i < 184; i++) {
        GT911_Config[184] += GT911_Config[i];
    }
    GT911_Config[184] = (~GT911_Config[184]) + 1;
}

static GT911_Status_t gt911_set_command(uint8_t command)
{
    return gt911_i2c_write(gt911_addr, GT911_REG_COMMAND, &command, 1);
}

static GT911_Status_t gt911_get_product_id(uint32_t *id)
{
    uint8_t buf[4];
    GT911_Status_t result = gt911_i2c_read(gt911_addr, GT911_REG_ID, buf, 4);
    if (result != GT911_OK) return result;
    memcpy(id, buf, 4);
    return GT911_OK;
}

static GT911_Status_t gt911_send_config(void)
{
    gt911_calculate_checksum();
    return gt911_i2c_write(gt911_addr, GT911_REG_CONFIG_DATA, GT911_Config, sizeof(GT911_Config));
}

static GT911_Status_t gt911_get_status(uint8_t *status)
{
    return gt911_i2c_read(gt911_addr, GT911_READ_COORD_ADDR, status, 1);
}

static GT911_Status_t gt911_set_status(uint8_t status)
{
    return gt911_i2c_write(gt911_addr, GT911_READ_COORD_ADDR, &status, 1);
}

static GT911_Status_t gt911_detect_addr(void)
{
    uint32_t product_id = 0;
    GT911_Status_t result;

    gt911_addr = GT911_ADDR_LOW;
    result = gt911_get_product_id(&product_id);
    if (result == GT911_OK && product_id != 0) {
        printf("[GT911] found at 0x5D, ID=0x%08lX\r\n", product_id);
        return GT911_OK;
    }

    gt911_addr = GT911_ADDR_HIGH;
    result = gt911_get_product_id(&product_id);
    if (result == GT911_OK && product_id != 0) {
        printf("[GT911] found at 0x14, ID=0x%08lX\r\n", product_id);
        return GT911_OK;
    }

    gt911_addr = GT911_ADDR_LOW;
    return GT911_NotResponse;
}

GT911_Status_t GT911_Init(GT911_Config_t config)
{
    GT911_Config[1] = config.X_Resolution & 0x00FF;
    GT911_Config[2] = (config.X_Resolution >> 8) & 0x00FF;
    GT911_Config[3] = config.Y_Resolution & 0x00FF;
    GT911_Config[4] = (config.Y_Resolution >> 8) & 0x00FF;
    GT911_Config[5] = config.Number_Of_Touch_Support;
    GT911_Config[6] = 0;
    GT911_Config[6] |= config.ReverseY << 7;
    GT911_Config[6] |= config.ReverseX << 6;
    GT911_Config[6] |= config.SwithX2Y << 3;
    GT911_Config[6] |= config.SoftwareNoiseReduction << 2;

    gt911_gpio_init();
    SoftI2C_Init();

    gt911_reset_for_addr_5d();
    Delay_Ms(200);

    GT911_Status_t result = gt911_detect_addr();
    if (result != GT911_OK) {
        gt911_reset_for_addr_14();
        Delay_Ms(200);
        result = gt911_detect_addr();
        if (result != GT911_OK) {
            printf("[GT911] Init FAILED\r\n");
            return result;
        }
    }

    gt911_send_config();
    gt911_set_command(GT911_CMD_READ);

    printf("[GT911] Init OK (0x%02X)\r\n", gt911_addr);
    return GT911_OK;
}

GT911_Status_t GT911_ReadTouch(GT911_TouchPoint_t *points, uint8_t *touch_count)
{
    uint8_t status;
    GT911_Status_t result = gt911_get_status(&status);
    if (result != GT911_OK) return result;

    if ((status & 0x80) != 0) {
        *touch_count = status & 0x0F;
        if (*touch_count > GT911_MAX_TOUCH_POINTS) {
            *touch_count = GT911_MAX_TOUCH_POINTS;
        }
        if (*touch_count != 0) {
            for (uint8_t i = 0; i < *touch_count; i++) {
                /* Each point: track_id(1) + x(2) + y(2) + size(2) + reserved(1) = 8 bytes */
                /* Base address for point i: 0x814F + i*8 for track_id */
                uint16_t base_addr = 0x814F + (i * 8);
                uint8_t buf[5];
                result = gt911_i2c_read(gt911_addr, base_addr, buf, 5);
                if (result != GT911_OK) return result;
                points[i].track_id = buf[0];
                points[i].x = (uint16_t)buf[1] | ((uint16_t)buf[2] << 8);
                points[i].y = (uint16_t)buf[3] | ((uint16_t)buf[4] << 8);
            }
        }
        gt911_set_status(0);
    } else {
        *touch_count = 0;
    }

    return GT911_OK;
}
