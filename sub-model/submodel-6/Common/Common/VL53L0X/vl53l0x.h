#ifndef __VL53L0X_H__
#define __VL53L0X_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"

/* Debug switch: 1 = enable printf diagnostics + VL53L0X_DumpDiagnostics()
 * (requires USART_Printf_Init in main.c). 0 = production build. */
#define VL53L0X_DEBUG_EN            0

/* VL53L0X I2C address (7-bit, shifted left for 8-bit usage) */
#define VL53L0X_I2C_ADDR           0x29
#define VL53L0X_I2C_ADDR_8BIT     (VL53L0X_I2C_ADDR << 1)

/* VL53L0X GPIO pins (matched to actual wiring: PB14->SCL, PB15->SDA) */
#define VL53L0X_SCL_PORT           GPIOB
#define VL53L0X_SCL_PIN            GPIO_Pin_14
#define VL53L0X_SDA_PORT           GPIOB
#define VL53L0X_SDA_PIN            GPIO_Pin_15
#define VL53L0X_GPIO1_PORT         GPIOB
#define VL53L0X_GPIO1_PIN          GPIO_Pin_13
#define VL53L0X_XSHUT_PORT         GPIOB
#define VL53L0X_XSHUT_PIN          GPIO_Pin_12

/* VL53L0X register addresses (8-bit index, per datasheet Figure 15) */
#define VL53L0X_REG_SYSRANGE_START                 0x00
#define VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG         0x01
#define VL53L0X_REG_SYSTEM_INTERRUPT_CONFIG        0x0A
#define VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR         0x0B
#define VL53L0X_REG_RESULT_INTERRUPT_STATUS        0x13
#define VL53L0X_REG_RESULT_RANGE_STATUS            0x14
#define VL53L0X_REG_RESULT_RANGE_VALUE             (VL53L0X_REG_RESULT_RANGE_STATUS + 10) /* 0x1E */
#define VL53L0X_REG_SIGNAL_RATE_LIMIT              0x44   /* 16-bit, 9.7 fixed point MCPS */
#define VL53L0X_REG_MSRC_CONFIG_CONTROL            0x60
#define VL53L0X_REG_IDENTIFICATION_MODEL_ID        0xC0   /* = 0xEE */
#define VL53L0X_REG_IDENTIFICATION_REVISION_ID     0xC2   /* = 0x10 */

/* Datasheet Table 4 reference values (fresh reset, validate I2C interface) */
#define VL53L0X_MODEL_ID_EXPECTED      0xEE   /* reg 0xC0 */
#define VL53L0X_MODEL_ID2_EXPECTED     0xAA   /* reg 0xC1 */
#define VL53L0X_REVISION_ID_EXPECTED   0x10   /* reg 0xC2 */

/* Invalid distance marker returned by VL53L0X_ReadDistance() */
#define VL53L0X_DISTANCE_INVALID       0xFFFF

/* Ranging states */
typedef enum {
    VL53L0X_STATE_IDLE = 0,
    VL53L0X_STATE_RANGING
} vl53l0x_state_t;

typedef struct {
    vl53l0x_state_t state;
    uint8_t  initialized;
    uint8_t  stop_variable;    /* NVM stop variable, saved during DataInit */
    uint8_t  data_ready;
    uint8_t  i2c_error_count;  /* cumulative I2C NACK/error counter (debug) */
    uint8_t  last_range_status;/* raw RESULT_RANGE_STATUS of last read (debug) */
    uint16_t last_distance_mm;
} vl53l0x_ctx_t;

extern vl53l0x_ctx_t vl53l0x_ctx;

void VL53L0X_Init(void);
uint8_t VL53L0X_IsInitialized(void);
void VL53L0X_StartContinuous(void);
void VL53L0X_StopContinuous(void);
uint16_t VL53L0X_ReadDistance(void);
uint8_t VL53L0X_IsDataReady(void);
void VL53L0X_ClearInterrupt(void);

/* DEBUG: hardware bring-up diagnostics (I2C recovery / scan / pin toggle).
 * Only available when VL53L0X_DEBUG_EN = 1. */
#if VL53L0X_DEBUG_EN
void VL53L0X_DumpDiagnostics(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
