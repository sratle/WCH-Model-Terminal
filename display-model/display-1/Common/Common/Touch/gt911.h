#ifndef __GT911_H
#define __GT911_H

#include <stdbool.h>
#include <stdint.h>

#define GT911_I2C_ADDR          0x5D

#define GT911_REG_COMMAND       0x8040
#define GT911_REG_ESD_CHECK     0x8041
#define GT911_REG_PROXIMITY_EN  0x8042
#define GT911_REG_CONFIG_DATA   0x8047
#define GT911_REG_MAX_X         0x8048
#define GT911_REG_MAX_Y         0x804A
#define GT911_REG_MAX_TOUCH     0x804C
#define GT911_REG_MOD_SW1       0x804D
#define GT911_REG_MOD_SW2       0x804E
#define GT911_REG_SHAKE_CNT     0x804F
#define GT911_REG_X_THRESHOLD   0x8057
#define GT911_REG_CONFIG_FRESH  0x8100

#define GT911_REG_ID            0x8140
#define GT911_REG_FW_VER        0x8144
#define GT911_READ_X_RES        0x8146
#define GT911_READ_Y_RES        0x8148
#define GT911_READ_VENDOR_ID    0x814A
#define GT911_READ_COORD_ADDR   0x814E

#define GT911_POINT1_X_ADDR     0x8150
#define GT911_POINT1_Y_ADDR     0x8152

#define GT911_CMD_READ          0x00
#define GT911_CMD_DIFFVAL       0x01
#define GT911_CMD_SOFTRESET     0x02
#define GT911_CMD_BASEUPDATE    0x03
#define GT911_CMD_CALIBRATE     0x04
#define GT911_CMD_SCREEN_OFF    0x05

#define GT911_MAX_TOUCH_POINTS  5

typedef enum {
    GT911_OK = 0,
    GT911_Error = 1,
    GT911_NotResponse = 2
} GT911_Status_t;

typedef struct {
    uint16_t X_Resolution;
    uint16_t Y_Resolution;
    uint8_t Number_Of_Touch_Support;
    bool ReverseX;
    bool ReverseY;
    bool SwithX2Y;
    bool SoftwareNoiseReduction;
} GT911_Config_t;

typedef struct {
    uint16_t x;
    uint16_t y;
} GT911_TouchPoint_t;

GT911_Status_t GT911_Init(GT911_Config_t config);
GT911_Status_t GT911_ReadTouch(GT911_TouchPoint_t *points, uint8_t *touch_count);

#endif
