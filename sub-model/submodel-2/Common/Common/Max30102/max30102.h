#ifndef __MAX30102_H__
#define __MAX30102_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"

/* I2C GPIO */
#define MAX30102_I2C_SCL_PORT   GPIOB
#define MAX30102_I2C_SCL_PIN    GPIO_Pin_14
#define MAX30102_I2C_SDA_PORT   GPIOB
#define MAX30102_I2C_SDA_PIN    GPIO_Pin_15

/* INT GPIO */
#define MAX30102_INT_PORT       GPIOB
#define MAX30102_INT_PIN        GPIO_Pin_13
#define MAX30102_INT_IRQn       EXTI15_10_IRQn
#define MAX30102_INT_EXTI_LINE  EXTI_Line13

/* I2C address (7-bit, AD0=GND) */
#define MAX30102_I2C_ADDR       0x57

/* Register map */
#define MAX30102_REG_INTR_STATUS_1      0x00
#define MAX30102_REG_INTR_STATUS_2      0x01
#define MAX30102_REG_INTR_ENABLE_1      0x02
#define MAX30102_REG_INTR_ENABLE_2      0x03
#define MAX30102_REG_FIFO_WR_PTR        0x04
#define MAX30102_REG_OVF_COUNTER        0x05
#define MAX30102_REG_FIFO_RD_PTR        0x06
#define MAX30102_REG_FIFO_DATA          0x07
#define MAX30102_REG_FIFO_CONFIG        0x08
#define MAX30102_REG_MODE_CONFIG        0x09
#define MAX30102_REG_SPO2_CONFIG        0x0A
#define MAX30102_REG_LED1_PA            0x0C
#define MAX30102_REG_LED2_PA            0x0D
#define MAX30102_REG_PILOT_PA           0x10
#define MAX30102_REG_MULTI_LED_CTRL1    0x11
#define MAX30102_REG_MULTI_LED_CTRL2    0x12
#define MAX30102_REG_TEMP_INTR          0x1F
#define MAX30102_REG_TEMP_FRAC          0x20
#define MAX30102_REG_TEMP_CONFIG        0x21
#define MAX30102_REG_PROX_INT_THRESH    0x30
#define MAX30102_REG_REV_ID             0xFE
#define MAX30102_REG_PART_ID            0xFF

/* Interrupt bits (INTR_STATUS_1 / INTR_ENABLE_1) */
#define MAX30102_INT_A_FULL_BIT         (1 << 7)
#define MAX30102_INT_PPG_RDY_BIT        (1 << 6)
#define MAX30102_INT_ALC_OVF_BIT        (1 << 5)
#define MAX30102_INT_PROX_INT_BIT       (1 << 4)

/* Mode config values */
#define MAX30102_MODE_HR_ONLY           0x02
#define MAX30102_MODE_SPO2              0x03

/* SPO2 config: sample rate */
#define MAX30102_SR_50HZ                (0x00 << 2)
#define MAX30102_SR_100HZ               (0x01 << 2)
#define MAX30102_SR_200HZ               (0x02 << 2)
#define MAX30102_SR_400HZ               (0x03 << 2)
#define MAX30102_SR_800HZ               (0x04 << 2)
#define MAX30102_SR_1000HZ              (0x05 << 2)
#define MAX30102_SR_1600HZ              (0x06 << 2)
#define MAX30102_SR_3200HZ              (0x07 << 2)

/* SPO2 config: LED pulse width */
#define MAX30102_PW_15BIT               (0x00 << 0)  /* 15-bit, 69us */
#define MAX30102_PW_16BIT               (0x01 << 0)  /* 16-bit, 118us */
#define MAX30102_PW_17BIT               (0x02 << 0)  /* 17-bit, 215us */
#define MAX30102_PW_18BIT               (0x03 << 0)  /* 18-bit, 411us */

/* FIFO config */
#define MAX30102_FIFO_A_FULL_4          (4 << 0)
#define MAX30102_FIFO_A_FULL_8          (8 << 0)
#define MAX30102_FIFO_A_FULL_16         (16 << 0)

/* LED pulse amplitude (0x00=0mA, 0xFF=50mA) */
#define MAX30102_LED_PA_DEFAULT         0x24   /* ~7.2mA */
#define MAX30102_LED_PA_LOW             0x1F   /* ~6.2mA */
#define MAX30102_LED_PA_HIGH            0x3F   /* ~12.6mA */

/* Proximity detection */
#define MAX30102_PROX_THRESH            0x14   /* IR threshold for finger detection */
#define MAX30102_FINGER_OFF_IR_MIN      5000   /* IR below this = no finger */
#define MAX30102_FINGER_OFF_COUNT       50     /* Consecutive low-IR samples = finger off */

/* Algorithm parameters (adapted from Maxim MAXREFDES117 reference)
 * Buffer size set to 200 (2 seconds at 100Hz) for CH32V103 20KB RAM limit.
 * Reference uses 500 (5 seconds) but that requires ~18KB RAM just for buffers.
 * 200 samples gives 2-3 heartbeat cycles at 60-100 BPM, enough for the algorithm. */
#define MAX30102_FS                     100    /* Sample rate (Hz) */
#define MAX30102_BUFFER_SIZE            200    /* 200 samples = 2 seconds */
#define MAX30102_MA4_SIZE               4      /* 4-point moving average */
#define MAX30102_HAMMING_SIZE           5      /* Hamming window size */
#define MAX30102_ALGO_RUN_SAMPLES       50     /* Run algorithm every 50 new samples (500ms) */

/* PPG algorithm parameters (legacy, kept for smoothing) */
#define MAX30102_HR_SMOOTH_WINDOW       5      /* Heart rate smoothing window */
#define MAX30102_SPO2_SMOOTH_WINDOW     5      /* SpO2 smoothing window */
#define MAX30102_MIN_VALID_HR           40     /* Minimum valid heart rate */
#define MAX30102_MAX_VALID_HR           200    /* Maximum valid heart rate */
#define MAX30102_MIN_VALID_SPO2         70     /* Minimum valid SpO2 */
#define MAX30102_MAX_VALID_SPO2         100    /* Maximum valid SpO2 */
#define MAX30102_HRV_MIN_RR_COUNT       5      /* Minimum RR intervals for HRV */

/* Health monitor state */
typedef enum {
    HEALTH_STATE_IDLE = 0,       /* Waiting for finger (PROX_INT only) */
    HEALTH_STATE_MONITORING      /* Finger detected, collecting & reporting */
} health_state_t;

/* MAX30102 context */
typedef struct {
    health_state_t state;

    /* PPG raw data buffer (100 samples = 1 second at 100Hz) */
    uint32_t ir_buf[MAX30102_BUFFER_SIZE];
    uint32_t red_buf[MAX30102_BUFFER_SIZE];
    uint16_t buf_count;         /* Number of samples in buffer */
    uint16_t buf_idx;           /* Write index (circular) */
    uint16_t new_samples;       /* Count of new samples since last algorithm run */

    /* Computed results */
    uint8_t  heart_rate;        /* BPM, smoothed */
    uint8_t  spo2;              /* SpO2 percentage, smoothed */
    uint16_t hrv;               /* HRV (SDNN) in ms */
    uint8_t  hr_valid;          /* 1 if HR is valid */
    uint8_t  spo2_valid;        /* 1 if SpO2 is valid */

    /* Smoothing buffers */
    uint8_t  hr_smooth[MAX30102_HR_SMOOTH_WINDOW];
    uint8_t  spo2_smooth[MAX30102_SPO2_SMOOTH_WINDOW];
    uint8_t  hr_smooth_idx;
    uint8_t  spo2_smooth_idx;

    /* Monitor interval (seconds) */
    uint16_t monitor_interval;

    /* Finger-off detection */
    uint8_t  finger_off_count;

    /* Diagnostic: last IR sample value */
    uint32_t last_ir;

    /* INT flag */
    volatile uint8_t int_flag;
} max30102_ctx_t;

void Max30102_Init(void);
void Max30102_Shutdown(void);
void Max30102_StartMonitoring(void);
void Max30102_StopMonitoring(void);
void Max30102_SetInterval(uint16_t seconds);
void Max30102_ProcessInt(void);
void Max30102_PollFIFO(void);
void Max30102_ProcessAlgorithm(void);

uint8_t Max30102_GetHeartRate(void);
uint8_t Max30102_GetSpO2(void);
uint16_t Max30102_GetHRV(void);

extern max30102_ctx_t max30102_ctx;
extern volatile uint8_t g_max30102_int_flag;

#ifdef __cplusplus
}
#endif

#endif
