#include "max30102.h"
#include <string.h>

max30102_ctx_t max30102_ctx;
volatile uint8_t g_max30102_int_flag = 0;

/* ============================================================================
 * Software I2C (PB14=SCL, PB15=SDA)
 * ============================================================================ */

#define I2C_DELAY_US    5

static void I2C_Delay(void)
{
    /* At 72MHz, each loop iteration is ~4 cycles (nop + loop overhead).
     * For 5μs: need 72 * 5 = 360 cycles ≈ 90 iterations. */
    uint16_t i;
    for (i = 0; i < 90; i++)
        __asm volatile ("nop");
}

static void SCL_High(void)
{
    GPIO_SetBits(MAX30102_I2C_SCL_PORT, MAX30102_I2C_SCL_PIN);
    I2C_Delay();
}

static void SCL_Low(void)
{
    GPIO_ResetBits(MAX30102_I2C_SCL_PORT, MAX30102_I2C_SCL_PIN);
    I2C_Delay();
}

static void SDA_High(void)
{
    GPIO_SetBits(MAX30102_I2C_SDA_PORT, MAX30102_I2C_SDA_PIN);
    I2C_Delay();
}

static void SDA_Low(void)
{
    GPIO_ResetBits(MAX30102_I2C_SDA_PORT, MAX30102_I2C_SDA_PIN);
    I2C_Delay();
}

static uint8_t SDA_Read(void)
{
    return GPIO_ReadInputDataBit(MAX30102_I2C_SDA_PORT, MAX30102_I2C_SDA_PIN);
}

static void I2C_Start(void)
{
    SDA_High();
    SCL_High();
    SDA_Low();
    SCL_Low();
}

static void I2C_Stop(void)
{
    SDA_Low();
    SCL_High();
    SDA_High();
}

static uint8_t I2C_WaitAck(void)
{
    uint8_t ack;

    SDA_High();
    SCL_High();
    ack = SDA_Read();
    SCL_Low();

    return ack ? 0 : 1;  /* 0=NACK, 1=ACK */
}

static void I2C_SendByte(uint8_t byte)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        if (byte & 0x80)
            SDA_High();
        else
            SDA_Low();
        SCL_High();
        SCL_Low();
        byte <<= 1;
    }
}

static uint8_t I2C_ReadByte(uint8_t ack)
{
    uint8_t i;
    uint8_t byte = 0;

    SDA_High();
    for (i = 0; i < 8; i++)
    {
        byte <<= 1;
        SCL_High();
        if (SDA_Read())
            byte |= 0x01;
        SCL_Low();
    }

    if (ack)
        SDA_Low();
    else
        SDA_High();
    SCL_High();
    SCL_Low();
    SDA_High();

    return byte;
}

/* ============================================================================
 * MAX30102 register access
 * ============================================================================ */

static uint8_t Max30102_ReadReg(uint8_t reg)
{
    uint8_t val;

    I2C_Start();
    I2C_SendByte(MAX30102_I2C_ADDR << 1);
    I2C_WaitAck();
    I2C_SendByte(reg);
    I2C_WaitAck();
    I2C_Start();
    I2C_SendByte((MAX30102_I2C_ADDR << 1) | 0x01);
    I2C_WaitAck();
    val = I2C_ReadByte(0);
    I2C_Stop();

    return val;
}

static void Max30102_WriteReg(uint8_t reg, uint8_t val)
{
    I2C_Start();
    I2C_SendByte(MAX30102_I2C_ADDR << 1);
    I2C_WaitAck();
    I2C_SendByte(reg);
    I2C_WaitAck();
    I2C_SendByte(val);
    I2C_WaitAck();
    I2C_Stop();
}

static void Max30102_ReadFIFO(uint32_t *red, uint32_t *ir)
{
    uint8_t buf[6];

    I2C_Start();
    I2C_SendByte(MAX30102_I2C_ADDR << 1);
    I2C_WaitAck();
    I2C_SendByte(MAX30102_REG_FIFO_DATA);
    I2C_WaitAck();
    I2C_Start();
    I2C_SendByte((MAX30102_I2C_ADDR << 1) | 0x01);
    I2C_WaitAck();

    buf[0] = I2C_ReadByte(1);
    buf[1] = I2C_ReadByte(1);
    buf[2] = I2C_ReadByte(1);
    buf[3] = I2C_ReadByte(1);
    buf[4] = I2C_ReadByte(1);
    buf[5] = I2C_ReadByte(0);
    I2C_Stop();

    /* Red LED: buf[0..2], 18-bit (top 3 bytes) */
    *red = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    *red &= 0x03FFFF;

    /* IR LED: buf[3..5], 18-bit */
    *ir = ((uint32_t)buf[3] << 16) | ((uint32_t)buf[4] << 8) | buf[5];
    *ir &= 0x03FFFF;
}

/* ============================================================================
 * GPIO and INT initialization
 * ============================================================================ */

static void Max30102_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* I2C: SCL output, SDA open-drain */
    gpio.GPIO_Pin   = MAX30102_I2C_SCL_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(MAX30102_I2C_SCL_PORT, &gpio);

    gpio.GPIO_Pin  = MAX30102_I2C_SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(MAX30102_I2C_SDA_PORT, &gpio);

    /* INT: input pull-up */
    gpio.GPIO_Pin  = MAX30102_INT_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(MAX30102_INT_PORT, &gpio);
}

static void Max30102_INT_Init(void)
{
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource13);

    exti.EXTI_Line    = MAX30102_INT_EXTI_LINE;
    exti.EXTI_Mode    = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel = MAX30102_INT_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void Max30102_Init(void)
{
    uint8_t part_id;

    memset(&max30102_ctx, 0, sizeof(max30102_ctx_t));
    max30102_ctx.monitor_interval = 5;  /* Default: 5 seconds */

    Max30102_GPIO_Init();

    /* Reset */
    Max30102_WriteReg(MAX30102_REG_MODE_CONFIG, 0x40);
    Delay_Ms(10);

    /* Verify I2C communication by reading PART_ID (should be 0x15 for MAX30102) */
    part_id = Max30102_ReadReg(MAX30102_REG_PART_ID);
    if (part_id != 0x15)
    {
        /* I2C communication failed, don't configure the sensor */
        max30102_ctx.state = HEALTH_STATE_IDLE;
        return;
    }

    /* Configure: SpO2 mode, 100Hz sample rate, 18-bit pulse width, ADC range=4096nA
     * Matches reference Nucleo_MAX30102 configuration */
    Max30102_WriteReg(MAX30102_REG_INTR_ENABLE_1,
                      MAX30102_INT_A_FULL_BIT | MAX30102_INT_PPG_RDY_BIT);
    Max30102_WriteReg(MAX30102_REG_INTR_ENABLE_2, 0x00);
    Max30102_WriteReg(MAX30102_REG_FIFO_WR_PTR, 0x00);
    Max30102_WriteReg(MAX30102_REG_OVF_COUNTER, 0x00);
    Max30102_WriteReg(MAX30102_REG_FIFO_RD_PTR, 0x00);
    Max30102_WriteReg(MAX30102_REG_FIFO_CONFIG, 0x1F);    /* SMP_AVE=0, ROLLOVER=1, A_FULL=15 */
    Max30102_WriteReg(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2);
    Max30102_WriteReg(MAX30102_REG_SPO2_CONFIG, 0x27);    /* ADC=4096nA, 100Hz, 18bit */
    Max30102_WriteReg(MAX30102_REG_LED1_PA, MAX30102_LED_PA_DEFAULT);  /* Red ~7mA */
    Max30102_WriteReg(MAX30102_REG_LED2_PA, MAX30102_LED_PA_DEFAULT);  /* IR ~7mA */
    Max30102_WriteReg(MAX30102_REG_PILOT_PA, 0x7F);       /* Pilot ~25mA */
    Max30102_WriteReg(MAX30102_REG_PROX_INT_THRESH, MAX30102_PROX_THRESH);

    /* INT pin setup */
    Max30102_INT_Init();
}

void Max30102_Shutdown(void)
{
    Max30102_WriteReg(MAX30102_REG_MODE_CONFIG, 0x80);
    max30102_ctx.state = HEALTH_STATE_IDLE;
}

void Max30102_StartMonitoring(void)
{
    /* Clear FIFO */
    Max30102_WriteReg(MAX30102_REG_FIFO_WR_PTR, 0x00);
    Max30102_WriteReg(MAX30102_REG_OVF_COUNTER, 0x00);
    Max30102_WriteReg(MAX30102_REG_FIFO_RD_PTR, 0x00);

    /* Reset state */
    max30102_ctx.buf_count = 0;
    max30102_ctx.buf_idx = 0;
    max30102_ctx.new_samples = 0;
    max30102_ctx.hr_smooth_idx = 0;
    max30102_ctx.spo2_smooth_idx = 0;
    max30102_ctx.heart_rate = 0;
    max30102_ctx.spo2 = 0;
    max30102_ctx.hrv = 0;
    max30102_ctx.hr_valid = 0;
    max30102_ctx.spo2_valid = 0;
    max30102_ctx.finger_off_count = 0;

    max30102_ctx.state = HEALTH_STATE_MONITORING;
}

void Max30102_StopMonitoring(void)
{
    max30102_ctx.state = HEALTH_STATE_IDLE;
}

void Max30102_SetInterval(uint16_t seconds)
{
    if (seconds == 0)
        seconds = 1;
    max30102_ctx.monitor_interval = seconds;
}

/* Read all available FIFO samples on interrupt */
void Max30102_ProcessInt(void)
{
    uint8_t intr_status;
    uint8_t wr_ptr, rd_ptr, num_samples;
    uint32_t red, ir;

    /* Read and clear interrupt status */
    intr_status = Max30102_ReadReg(MAX30102_REG_INTR_STATUS_1);
    Max30102_ReadReg(MAX30102_REG_INTR_STATUS_2);

    /* ---- IDLE state: only handle PROX_INT (finger detected) ---- */
    if (max30102_ctx.state == HEALTH_STATE_IDLE)
    {
        if (intr_status & MAX30102_INT_PROX_INT_BIT)
        {
            /* Finger detected: auto-start monitoring */
            Max30102_StartMonitoring();
        }
        return;
    }

    /* ---- MONITORING state: read FIFO data ---- */
    if (max30102_ctx.state != HEALTH_STATE_MONITORING)
        return;

    /* Read FIFO pointers */
    wr_ptr = Max30102_ReadReg(MAX30102_REG_FIFO_WR_PTR);
    rd_ptr = Max30102_ReadReg(MAX30102_REG_FIFO_RD_PTR);

    /* Calculate number of available samples */
    if (wr_ptr >= rd_ptr)
        num_samples = wr_ptr - rd_ptr;
    else
        num_samples = 32 - rd_ptr + wr_ptr;

    /* Read samples */
    while (num_samples > 0)
    {
        Max30102_ReadFIFO(&red, &ir);

        /* Store in circular buffer */
        max30102_ctx.ir_buf[max30102_ctx.buf_idx] = ir;
        max30102_ctx.red_buf[max30102_ctx.buf_idx] = red;
        max30102_ctx.buf_idx = (max30102_ctx.buf_idx + 1) % MAX30102_BUFFER_SIZE;

        if (max30102_ctx.buf_count < MAX30102_BUFFER_SIZE)
            max30102_ctx.buf_count++;

        max30102_ctx.new_samples++;

        /* Finger-off detection: count consecutive low-IR samples */
        if (ir < MAX30102_FINGER_OFF_IR_MIN)
            max30102_ctx.finger_off_count++;
        else
            max30102_ctx.finger_off_count = 0;

        max30102_ctx.last_ir = ir;

        num_samples--;
    }
}

/* Poll FIFO without relying on EXTI interrupt.
 * Called from main loop every ~10ms as a fallback/supplement to interrupt. */
void Max30102_PollFIFO(void)
{
    uint8_t wr_ptr, rd_ptr, num_samples;
    uint32_t red, ir;

    if (max30102_ctx.state != HEALTH_STATE_MONITORING)
        return;

    /* Read and clear interrupt status (required by MAX30102 before FIFO read) */
    Max30102_ReadReg(MAX30102_REG_INTR_STATUS_1);
    Max30102_ReadReg(MAX30102_REG_INTR_STATUS_2);

    /* Read FIFO pointers */
    wr_ptr = Max30102_ReadReg(MAX30102_REG_FIFO_WR_PTR);
    rd_ptr = Max30102_ReadReg(MAX30102_REG_FIFO_RD_PTR);

    if (wr_ptr == rd_ptr)
        return;  /* No new data */

    /* Calculate number of available samples */
    if (wr_ptr >= rd_ptr)
        num_samples = wr_ptr - rd_ptr;
    else
        num_samples = 32 - rd_ptr + wr_ptr;

    /* Limit to prevent blocking too long */
    if (num_samples > 16)
        num_samples = 16;

    /* Read samples */
    while (num_samples > 0)
    {
        Max30102_ReadFIFO(&red, &ir);

        max30102_ctx.ir_buf[max30102_ctx.buf_idx] = ir;
        max30102_ctx.red_buf[max30102_ctx.buf_idx] = red;
        max30102_ctx.buf_idx = (max30102_ctx.buf_idx + 1) % MAX30102_BUFFER_SIZE;

        if (max30102_ctx.buf_count < MAX30102_BUFFER_SIZE)
            max30102_ctx.buf_count++;

        max30102_ctx.new_samples++;

        if (ir < MAX30102_FINGER_OFF_IR_MIN)
            max30102_ctx.finger_off_count++;
        else
            max30102_ctx.finger_off_count = 0;

        max30102_ctx.last_ir = ir;

        num_samples--;
    }
}

/* ============================================================================
 * PPG Algorithm: Maxim MAXREFDES117 reference algorithm ported to C
 * Original: algorithm.cpp by Maxim Integrated Products, Inc.
 * ============================================================================ */

/* SpO2 lookup table: -45.060*ratioAverage^2 + 30.354*ratioAverage + 94.845 */
static const uint8_t uch_spo2_table[184] = {
    95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99,
    99, 99, 99, 99,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
   100,100,100,100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97,
    97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91,
    90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81,
    80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67,
    66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50,
    49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29,
    28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10,  9,  7,  6,  5,
     3,  2,  1
};

/* Hamming window coefficients (512 * hamming(5)) */
static const uint16_t auw_hamm[5] = { 41, 276, 512, 276, 41 };

/* Algorithm working buffers (static to avoid stack overflow)
 * an_x and an_y are reused from ir_linear/red_linear after copying.
 * an_dx is separate since it holds derivative data. */
static int32_t an_dx[MAX30102_BUFFER_SIZE];  /* Derivative */
static uint32_t ir_linear[MAX30102_BUFFER_SIZE];  /* Also used as an_x (int32_t cast) in algorithm */
static uint32_t red_linear[MAX30102_BUFFER_SIZE]; /* Also used as an_y (int32_t cast) in algorithm */

/* ---- Helper functions (from Maxim reference) ---- */

static void algo_sort_ascend(int32_t *pn_x, int32_t n_size)
{
    int32_t i, j, n_temp;
    for (i = 1; i < n_size; i++) {
        n_temp = pn_x[i];
        for (j = i; j > 0 && n_temp < pn_x[j-1]; j--)
            pn_x[j] = pn_x[j-1];
        pn_x[j] = n_temp;
    }
}

static void algo_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size)
{
    int32_t i, j, n_temp;
    for (i = 1; i < n_size; i++) {
        n_temp = pn_indx[i];
        for (j = i; j > 0 && pn_x[n_temp] > pn_x[pn_indx[j-1]]; j--)
            pn_indx[j] = pn_indx[j-1];
        pn_indx[j] = n_temp;
    }
}

static void algo_peaks_above_min_height(int32_t *pn_locs, int32_t *pn_npks,
                                         int32_t *pn_x, int32_t n_size,
                                         int32_t n_min_height)
{
    int32_t i = 1, n_width;
    *pn_npks = 0;

    while (i < n_size - 1) {
        if (pn_x[i] > n_min_height && pn_x[i] > pn_x[i-1]) {
            n_width = 1;
            while (i + n_width < n_size && pn_x[i] == pn_x[i + n_width])
                n_width++;
            if (pn_x[i] > pn_x[i + n_width] && (*pn_npks) < 15) {
                pn_locs[(*pn_npks)++] = i;
                i += n_width + 1;
            } else {
                i += n_width;
            }
        } else {
            i++;
        }
    }
}

static void algo_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks,
                                     int32_t *pn_x, int32_t n_min_distance)
{
    int32_t i, j, n_old_npks, n_dist;

    algo_sort_indices_descend(pn_x, pn_locs, *pn_npks);

    for (i = -1; i < *pn_npks; i++) {
        n_old_npks = *pn_npks;
        *pn_npks = i + 1;
        for (j = i + 1; j < n_old_npks; j++) {
            n_dist = pn_locs[j] - (i == -1 ? -1 : pn_locs[i]);
            if (n_dist > n_min_distance || n_dist < -n_min_distance)
                pn_locs[(*pn_npks)++] = pn_locs[j];
        }
    }

    algo_sort_ascend(pn_locs, *pn_npks);
}

static void algo_find_peaks(int32_t *pn_locs, int32_t *pn_npks,
                             int32_t *pn_x, int32_t n_size,
                             int32_t n_min_height, int32_t n_min_distance,
                             int32_t n_max_num)
{
    algo_peaks_above_min_height(pn_locs, pn_npks, pn_x, n_size, n_min_height);
    algo_remove_close_peaks(pn_locs, pn_npks, pn_x, n_min_distance);
    if (*pn_npks > n_max_num)
        *pn_npks = n_max_num;
}

/**
 * Maxim reference algorithm: heart rate and SpO2 calculation.
 * Ported from algorithm.cpp (MAXREFDES117#).
 * Uses ir_linear/red_linear as an_x/an_y working buffers directly.
 *
 * @param buf_len    Buffer length
 * @param[out] pn_spo2, pch_spo2_valid, pn_heart_rate, pch_hr_valid, pn_hrv
 */
static void maxim_heart_rate_and_oxygen_saturation(
    int32_t buf_len,
    int32_t *pn_spo2, int8_t *pch_spo2_valid,
    int32_t *pn_heart_rate, int8_t *pch_hr_valid,
    uint16_t *pn_hrv)
{
    /* an_x = (int32_t*)ir_linear, an_y = (int32_t*)red_linear
     * ir_linear/red_linear already contain the raw data */
    int32_t *an_x = (int32_t *)ir_linear;
    int32_t *an_y = (int32_t *)red_linear;

    uint32_t un_ir_mean;
    int32_t k, n_i_ratio_count;
    int32_t i, s, m, n_exact_ir_valley_locs_count, n_middle_idx;
    int32_t n_th1, n_npks, n_c_min;
    int32_t an_ir_valley_locs[15];
    int32_t an_exact_ir_valley_locs[15];
    int32_t an_dx_peak_locs[15];
    int32_t n_peak_interval_sum;

    int32_t n_y_ac, n_x_ac;
    int32_t n_spo2_calc;
    int32_t n_y_dc_max, n_x_dc_max;
    int32_t n_y_dc_max_idx, n_x_dc_max_idx;
    int32_t an_ratio[5], n_ratio_average;
    int32_t n_nume, n_denom;
    uint32_t un_only_once;

    /* ---- HR Calculation ----
     * Compute derivative directly from ir_linear without modifying it,
     * so the original data is preserved for SpO2 calculation. */

    /* Remove DC */
    un_ir_mean = 0;
    for (k = 0; k < buf_len; k++)
        un_ir_mean += ir_linear[k];
    un_ir_mean /= buf_len;

    /* Compute derivative: DC-remove + MA4 + diff + MA2, all in one pass.
     * This avoids modifying ir_linear (which an_x aliases). */
    {
        int32_t dx_len = buf_len - MAX30102_MA4_SIZE - 1;
        for (k = 0; k < dx_len; k++)
        {
            int32_t ma4_k   = ((int32_t)ir_linear[k]   - (int32_t)un_ir_mean)
                            + ((int32_t)ir_linear[k+1] - (int32_t)un_ir_mean)
                            + ((int32_t)ir_linear[k+2] - (int32_t)un_ir_mean)
                            + ((int32_t)ir_linear[k+3] - (int32_t)un_ir_mean);
            int32_t ma4_k1  = ((int32_t)ir_linear[k+1] - (int32_t)un_ir_mean)
                            + ((int32_t)ir_linear[k+2] - (int32_t)un_ir_mean)
                            + ((int32_t)ir_linear[k+3] - (int32_t)un_ir_mean)
                            + ((int32_t)ir_linear[k+4] - (int32_t)un_ir_mean);
            an_dx[k] = ma4_k1 / 4 - ma4_k / 4;
        }

        /* 2-point moving average on derivative */
        for (k = 0; k < dx_len - 1; k++)
            an_dx[k] = (an_dx[k] + an_dx[k+1]) / 2;
    }

    /* Hamming window - flip waveform to detect valley as peak */
    {
        int32_t hamm_len = buf_len - MAX30102_HAMMING_SIZE - MAX30102_MA4_SIZE - 2;
        for (i = 0; i < hamm_len; i++) {
            s = 0;
            for (k = i; k < i + MAX30102_HAMMING_SIZE; k++)
                s -= an_dx[k] * auw_hamm[k - i];
            an_dx[i] = s / 1146;
        }
    }

    /* Threshold calculation */
    n_th1 = 0;
    {
        int32_t hamm_len = buf_len - MAX30102_HAMMING_SIZE;
        for (k = 0; k < hamm_len; k++)
            n_th1 += (an_dx[k] > 0) ? an_dx[k] : (-an_dx[k]);
        n_th1 = n_th1 / hamm_len;
    }

    /* Find peaks (actually valleys in original signal) */
    algo_find_peaks(an_dx_peak_locs, &n_npks, an_dx,
                    buf_len - MAX30102_HAMMING_SIZE, n_th1, 8, 5);

    /* Calculate heart rate from peak intervals */
    n_peak_interval_sum = 0;
    if (n_npks >= 2) {
        for (k = 1; k < n_npks; k++)
            n_peak_interval_sum += (an_dx_peak_locs[k] - an_dx_peak_locs[k-1]);
        n_peak_interval_sum = n_peak_interval_sum / (n_npks - 1);
        *pn_heart_rate = (int32_t)(6000 / n_peak_interval_sum);
        *pch_hr_valid = 1;
    } else {
        *pn_heart_rate = -999;
        *pch_hr_valid = 0;
    }

    /* Valley locations in original signal */
    for (k = 0; k < n_npks; k++)
        an_ir_valley_locs[k] = an_dx_peak_locs[k] + MAX30102_HAMMING_SIZE / 2;

    /* ---- SpO2 Calculation ----
     * ir_linear and red_linear are still intact (not modified above).
     * an_x/an_y alias them, so we can use an_x/an_y directly. */

    /* Find precise min near each valley in the ORIGINAL IR signal */
    n_exact_ir_valley_locs_count = 0;
    for (k = 0; k < n_npks; k++) {
        un_only_once = 1;
        m = an_ir_valley_locs[k];
        n_c_min = 16777216;
        if (m + 5 < buf_len - MAX30102_HAMMING_SIZE && m - 5 > 0) {
            for (i = m - 5; i < m + 5; i++) {
                if (an_x[i] < n_c_min) {
                    if (un_only_once > 0)
                        un_only_once = 0;
                    n_c_min = an_x[i];
                    an_exact_ir_valley_locs[k] = i;
                }
            }
            if (un_only_once == 0)
                n_exact_ir_valley_locs_count++;
        }
    }

    if (n_exact_ir_valley_locs_count < 2) {
        *pn_spo2 = -999;
        *pch_spo2_valid = 0;
        *pn_hrv = 0;
        return;
    }

    /* 4-point moving average on raw signals (modifies an_x/an_y in place) */
    for (k = 0; k < buf_len - MAX30102_MA4_SIZE; k++) {
        an_x[k] = (an_x[k] + an_x[k+1] + an_x[k+2] + an_x[k+3]) / 4;
        an_y[k] = (an_y[k] + an_y[k+1] + an_y[k+2] + an_y[k+3]) / 4;
    }

    /* Calculate SpO2 ratio between valley pairs */
    n_ratio_average = 0;
    n_i_ratio_count = 0;
    for (k = 0; k < 5; k++) an_ratio[k] = 0;

    for (k = 0; k < n_exact_ir_valley_locs_count; k++) {
        if (an_exact_ir_valley_locs[k] > buf_len) {
            *pn_spo2 = -999;
            *pch_spo2_valid = 0;
            *pn_hrv = 0;
            return;
        }
    }

    for (k = 0; k < n_exact_ir_valley_locs_count - 1; k++) {
        n_y_dc_max = -16777216;
        n_x_dc_max = -16777216;
        if (an_exact_ir_valley_locs[k+1] - an_exact_ir_valley_locs[k] > 10) {
            for (i = an_exact_ir_valley_locs[k]; i < an_exact_ir_valley_locs[k+1]; i++) {
                if (an_x[i] > n_x_dc_max) { n_x_dc_max = an_x[i]; n_x_dc_max_idx = i; }
                if (an_y[i] > n_y_dc_max) { n_y_dc_max = an_y[i]; n_y_dc_max_idx = i; }
            }
            n_y_ac = (an_y[an_exact_ir_valley_locs[k+1]] - an_y[an_exact_ir_valley_locs[k]])
                     * (n_y_dc_max_idx - an_exact_ir_valley_locs[k]);
            n_y_ac = an_y[an_exact_ir_valley_locs[k]] + n_y_ac / (an_exact_ir_valley_locs[k+1] - an_exact_ir_valley_locs[k]);
            n_y_ac = an_y[n_y_dc_max_idx] - n_y_ac;

            n_x_ac = (an_x[an_exact_ir_valley_locs[k+1]] - an_x[an_exact_ir_valley_locs[k]])
                     * (n_x_dc_max_idx - an_exact_ir_valley_locs[k]);
            n_x_ac = an_x[an_exact_ir_valley_locs[k]] + n_x_ac / (an_exact_ir_valley_locs[k+1] - an_exact_ir_valley_locs[k]);
            n_x_ac = an_x[n_y_dc_max_idx] - n_x_ac;

            n_nume = (n_y_ac * n_x_dc_max) >> 7;
            n_denom = (n_x_ac * n_y_dc_max) >> 7;
            if (n_denom > 0 && n_i_ratio_count < 5 && n_nume != 0) {
                an_ratio[n_i_ratio_count] = (n_nume * 100) / n_denom;
                n_i_ratio_count++;
            }
        }
    }

    algo_sort_ascend(an_ratio, n_i_ratio_count);
    n_middle_idx = n_i_ratio_count / 2;

    if (n_middle_idx > 1)
        n_ratio_average = (an_ratio[n_middle_idx-1] + an_ratio[n_middle_idx]) / 2;
    else
        n_ratio_average = an_ratio[n_middle_idx];

    if (n_ratio_average > 2 && n_ratio_average < 184) {
        n_spo2_calc = uch_spo2_table[n_ratio_average];
        *pn_spo2 = n_spo2_calc;
        *pch_spo2_valid = 1;
    } else {
        *pn_spo2 = -999;
        *pch_spo2_valid = 0;
    }

    /* Compute HRV (SDNN) from peak intervals.
     * Need at least 3 peaks (2 RR intervals) for meaningful variance.
     * With only 2 peaks there is 1 RR interval and variance is always 0. */
    *pn_hrv = 0;
    if (n_npks >= 3) {
        uint32_t sum = 0, mean, variance = 0;
        int32_t rr_count = n_npks - 1;
        uint16_t rr[15];
        int32_t ri;

        for (ri = 1; ri < n_npks; ri++)
            rr[ri - 1] = (uint16_t)((an_dx_peak_locs[ri] - an_dx_peak_locs[ri - 1]) * 10);

        for (ri = 0; ri < rr_count; ri++)
            sum += rr[ri];
        mean = sum / rr_count;

        for (ri = 0; ri < rr_count; ri++) {
            int32_t diff = (int32_t)rr[ri] - (int32_t)mean;
            variance += (uint32_t)(diff * diff);
        }
        variance /= rr_count;

        /* Integer sqrt */
        {
            uint32_t root = 0, bit = 1 << 30;
            while (bit > variance) bit >>= 2;
            while (bit != 0) {
                if (variance >= root + bit) { variance -= root + bit; root = (root >> 1) + bit; }
                else { root >>= 1; }
                bit >>= 2;
            }
            *pn_hrv = (uint16_t)root;
        }
    }
}

/* Add value to smoothing buffer and return average */
static uint8_t SmoothValue(uint8_t *buf, uint8_t *idx, uint8_t window_size, uint8_t val)
{
    uint16_t sum = 0;
    uint8_t count = 0;
    uint8_t i;

    buf[*idx] = val;
    *idx = (*idx + 1) % window_size;

    for (i = 0; i < window_size; i++)
    {
        if (buf[i] > 0)
        {
            sum += buf[i];
            count++;
        }
    }

    return count > 0 ? (uint8_t)(sum / count) : 0;
}

void Max30102_ProcessAlgorithm(void)
{
    if (max30102_ctx.state != HEALTH_STATE_MONITORING)
        return;

    /* Finger-off detection */
    if (max30102_ctx.finger_off_count >= MAX30102_FINGER_OFF_COUNT)
    {
        Max30102_StopMonitoring();
        return;
    }

    /* Need full buffer (200 samples = 2 seconds) before running algorithm */
    if (max30102_ctx.buf_count < MAX30102_BUFFER_SIZE)
        return;

    /* Only run algorithm when enough new samples accumulated (every 50 samples = 500ms) */
    if (max30102_ctx.new_samples < MAX30102_ALGO_RUN_SAMPLES)
        return;

    max30102_ctx.new_samples = 0;

    /* Build linear buffers from circular buffer (using static buffers) */
    uint16_t start_idx = max30102_ctx.buf_idx;  /* Oldest sample */
    uint16_t i;

    for (i = 0; i < MAX30102_BUFFER_SIZE; i++)
    {
        uint16_t idx = (start_idx + i) % MAX30102_BUFFER_SIZE;
        ir_linear[i] = max30102_ctx.ir_buf[idx];
        red_linear[i] = max30102_ctx.red_buf[idx];
    }

    /* Run Maxim reference algorithm */
    int32_t n_hr, n_spo2;
    int8_t hr_valid, spo2_valid;
    uint16_t n_hrv;

    maxim_heart_rate_and_oxygen_saturation(
        MAX30102_BUFFER_SIZE,
        &n_spo2, &spo2_valid, &n_hr, &hr_valid, &n_hrv);

    /* Update results with validation and smoothing */
    if (hr_valid && n_hr >= MAX30102_MIN_VALID_HR && n_hr <= MAX30102_MAX_VALID_HR)
    {
        max30102_ctx.heart_rate = SmoothValue(
            max30102_ctx.hr_smooth, &max30102_ctx.hr_smooth_idx,
            MAX30102_HR_SMOOTH_WINDOW, (uint8_t)n_hr);
        max30102_ctx.hr_valid = 1;
    }
    else
    {
        max30102_ctx.hr_valid = 0;
    }

    if (spo2_valid && n_spo2 >= MAX30102_MIN_VALID_SPO2 && n_spo2 <= MAX30102_MAX_VALID_SPO2)
    {
        max30102_ctx.spo2 = SmoothValue(
            max30102_ctx.spo2_smooth, &max30102_ctx.spo2_smooth_idx,
            MAX30102_SPO2_SMOOTH_WINDOW, (uint8_t)n_spo2);
        max30102_ctx.spo2_valid = 1;
    }
    else
    {
        max30102_ctx.spo2_valid = 0;
    }

    max30102_ctx.hrv = n_hrv;
}

uint8_t Max30102_GetHeartRate(void)
{
    return max30102_ctx.heart_rate;
}

uint8_t Max30102_GetSpO2(void)
{
    return max30102_ctx.spo2;
}

uint16_t Max30102_GetHRV(void)
{
    return max30102_ctx.hrv;
}
