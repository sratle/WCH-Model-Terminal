#ifndef __VL53L0X_H__
#define __VL53L0X_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"

/* VL53L0X I2C address (7-bit, shifted left for 8-bit usage) */
#define VL53L0X_I2C_ADDR           0x29
#define VL53L0X_I2C_ADDR_8BIT     (VL53L0X_I2C_ADDR << 1)

/* VL53L0X GPIO pins */
#define VL53L0X_SCL_PORT           GPIOB
#define VL53L0X_SCL_PIN            GPIO_Pin_15
#define VL53L0X_SDA_PORT           GPIOB
#define VL53L0X_SDA_PIN            GPIO_Pin_14
#define VL53L0X_GPIO1_PORT         GPIOB
#define VL53L0X_GPIO1_PIN          GPIO_Pin_13
#define VL53L0X_XSHUT_PORT         GPIOB
#define VL53L0X_XSHUT_PIN          GPIO_Pin_12

/* VL53L0X register addresses */
#define VL53L0X_REG_IDENTIFICATION_MODEL_ID      0x00A0
#define VL53L0X_REG_IDENTIFICATION_REVISION_ID   0x00A1
#define VL53L0X_REG_SYSRANGE_START               0x0018
#define VL53L0X_REG_SYSTEM_INTERRUPT_CONFIG      0x000A
#define VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR       0x000B
#define VL53L0X_REG_RESULT_INTERRUPT_STATUS      0x0013
#define VL53L0X_REG_RESULT_RANGE_STATUS          0x0014

/* Expected model ID */
#define VL53L0X_MODEL_ID_EXPECTED    0xEEAA

/* Ranging states */
typedef enum {
    VL53L0X_STATE_IDLE = 0,
    VL53L0X_STATE_RANGING
} vl53l0x_state_t;

typedef struct {
    vl53l0x_state_t state;
    uint8_t  initialized;
    uint16_t last_distance_mm;
    uint8_t  data_ready;
} vl53l0x_ctx_t;

extern vl53l0x_ctx_t vl53l0x_ctx;

void VL53L0X_Init(void);
uint8_t VL53L0X_IsInitialized(void);
void VL53L0X_StartContinuous(void);
void VL53L0X_StopContinuous(void);
uint16_t VL53L0X_ReadDistance(void);
uint8_t VL53L0X_IsDataReady(void);
void VL53L0X_ClearInterrupt(void);

#ifdef __cplusplus
}
#endif

#endif
