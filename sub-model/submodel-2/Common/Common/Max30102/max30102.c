#include "max30102.h"
#include <string.h>

max30102_ctx_t max30102_ctx;
volatile uint8_t g_max30102_int_flag = 0;

/* ============================================================================
 * Software I2C (PB14=SCL, PB15=SDA)
 * ============================================================================ */

#define I2C_DELAY_US    2

static void I2C_Delay(void)
{
    uint8_t i;
    for (i = 0; i < I2C_DELAY_US * 8; i++)
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
                      MAX30102_INT_A_FULL_BIT | MAX30102_INT_PPG_RDY_BIT | MAX30102_INT_PROX_INT_BIT);
    Max30102_WriteReg(MAX30102_REG_INTR_ENABLE_2, 0x00);
    Max30102_WriteReg(MAX30102_REG_FIFO_WR_PTR, 0x00);
    Max30102_WriteReg(MAX30102_REG_OVF_COUNTER, 0x00);
    Max30102_WriteReg(MAX30102_REG_FIFO_RD_PTR, 0x00);
    Max30102_WriteReg(MAX30102_REG_FIFO_CONFIG, 0x0F);    /* SMP_AVE=0, A_FULL=15 */
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
    max30102_ctx.rr_count = 0;
    max30102_ctx.hr_smooth_idx = 0;
    max30102_ctx.spo2_smooth_idx = 0;
    max30102_ctx.heart_rate = 0;
    max30102_ctx.spo2 = 0;
    max30102_ctx.hrv = 0;

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
        max30102_ctx.buf_idx = (max30102_ctx.buf_idx + 1) % MAX30102_PPG_BUF_SIZE;

        if (max30102_ctx.buf_count < MAX30102_PPG_BUF_SIZE)
            max30102_ctx.buf_count++;

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
        max30102_ctx.buf_idx = (max30102_ctx.buf_idx + 1) % MAX30102_PPG_BUF_SIZE;

        if (max30102_ctx.buf_count < MAX30102_PPG_BUF_SIZE)
            max30102_ctx.buf_count++;

        if (ir < MAX30102_FINGER_OFF_IR_MIN)
            max30102_ctx.finger_off_count++;
        else
            max30102_ctx.finger_off_count = 0;

        max30102_ctx.last_ir = ir;

        num_samples--;
    }
}

/* ============================================================================
 * PPG Algorithm: Peak detection, HR, SpO2, HRV
 * ============================================================================ */

/* Find peaks in IR PPG signal and compute heart rate */
static uint16_t FindPeaks(const uint32_t *buf, uint16_t count,
                          uint16_t *peak_indices, uint16_t max_peaks)
{
    uint16_t peak_count = 0;
    uint16_t i;
    uint32_t threshold;
    uint32_t max_val;
    int32_t prev_diff, curr_diff;

    if (count < 5)
        return 0;

    /* Compute signal threshold as 60% of max */
    max_val = 0;
    for (i = 0; i < count; i++)
    {
        if (buf[i] > max_val)
            max_val = buf[i];
    }
    threshold = max_val * 6 / 10;

    /* Detect peaks: sign change from positive to negative slope */
    prev_diff = 0;
    for (i = 1; i < count - 1 && peak_count < max_peaks; i++)
    {
        curr_diff = (int32_t)buf[i + 1] - (int32_t)buf[i];

        if (prev_diff > 0 && curr_diff <= 0 && buf[i] > threshold)
        {
            /* Ensure minimum distance between peaks (~25 samples at 100Hz = 250ms) */
            if (peak_count == 0 ||
                (i - peak_indices[peak_count - 1]) >= 25)
            {
                peak_indices[peak_count++] = i;
            }
        }

        prev_diff = curr_diff;
    }

    return peak_count;
}

/* Compute SpO2 using R = (AC_red/DC_red) / (AC_ir/DC_ir) */
static uint8_t ComputeSpO2(const uint32_t *red_buf, const uint32_t *ir_buf,
                           uint16_t count)
{
    uint32_t red_dc, ir_dc;
    uint32_t red_ac, ir_ac;
    uint32_t red_min, red_max, ir_min, ir_max;
    uint16_t i;
    float r_val, spo2;

    if (count < 10)
        return 0;

    /* Find AC/DC for red */
    red_min = red_max = red_buf[0];
    ir_min = ir_max = ir_buf[0];

    for (i = 1; i < count; i++)
    {
        if (red_buf[i] < red_min) red_min = red_buf[i];
        if (red_buf[i] > red_max) red_max = red_buf[i];
        if (ir_buf[i] < ir_min) ir_min = ir_buf[i];
        if (ir_buf[i] > ir_max) ir_max = ir_buf[i];
    }

    red_dc = (red_min + red_max) / 2;
    ir_dc = (ir_min + ir_max) / 2;
    red_ac = red_max - red_min;
    ir_ac = ir_max - ir_min;

    if (ir_dc == 0 || red_dc == 0 || ir_ac == 0)
        return 0;

    /* R = (AC_red/DC_red) / (AC_ir/DC_ir) */
    r_val = ((float)red_ac / (float)red_dc) / ((float)ir_ac / (float)ir_dc);

    /* Empirical formula: SpO2 = 110 - 25*R (simplified linear approximation) */
    spo2 = 110.0f - 25.0f * r_val;

    if (spo2 > 100.0f) spo2 = 100.0f;
    if (spo2 < 0.0f) spo2 = 0.0f;

    return (uint8_t)spo2;
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
    uint16_t peak_indices[32];
    uint16_t peak_count;
    uint16_t i;
    uint8_t raw_hr;
    uint8_t raw_spo2;
    uint16_t start_idx;
    uint16_t count;

    if (max30102_ctx.state != HEALTH_STATE_MONITORING)
        return;

    /* Finger-off detection: if IR signal has been low for consecutive samples,
     * finger is no longer on the sensor → auto-stop monitoring */
    if (max30102_ctx.finger_off_count >= MAX30102_FINGER_OFF_COUNT)
    {
        Max30102_StopMonitoring();
        return;
    }

    if (max30102_ctx.buf_count < 30)
        return;

    /* Use the most recent samples from circular buffer */
    count = max30102_ctx.buf_count;
    if (count > MAX30102_PPG_BUF_SIZE)
        count = MAX30102_PPG_BUF_SIZE;

    /* Build linear buffer from circular buffer for peak detection */
    uint32_t ir_linear[MAX30102_PPG_BUF_SIZE];
    uint32_t red_linear[MAX30102_PPG_BUF_SIZE];

    start_idx = (max30102_ctx.buf_idx + MAX30102_PPG_BUF_SIZE - count) % MAX30102_PPG_BUF_SIZE;

    for (i = 0; i < count; i++)
    {
        uint16_t idx = (start_idx + i) % MAX30102_PPG_BUF_SIZE;
        ir_linear[i] = max30102_ctx.ir_buf[idx];
        red_linear[i] = max30102_ctx.red_buf[idx];
    }

    /* Peak detection on IR signal */
    peak_count = FindPeaks(ir_linear, count, peak_indices, 32);

    if (peak_count < 2)
        return;

    /* Compute heart rate from peak intervals
     * Sample rate = 100Hz, so interval_samples / 100 = time in seconds
     * HR = 60 / (interval_samples / 100) = 6000 / interval_samples */
    {
        uint32_t total_interval = 0;
        uint8_t valid_intervals = 0;

        for (i = 1; i < peak_count; i++)
        {
            uint16_t interval = peak_indices[i] - peak_indices[i - 1];
            if (interval >= 25 && interval <= 150)  /* 40-240 BPM range */
            {
                total_interval += interval;
                valid_intervals++;

                /* Store RR intervals for HRV (convert samples to ms: interval * 10) */
                if (max30102_ctx.rr_count < 32)
                {
                    max30102_ctx.rr_intervals[max30102_ctx.rr_count++] = interval * 10;
                }
            }
        }

        if (valid_intervals == 0)
            return;

        raw_hr = (uint8_t)(6000UL / (total_interval / valid_intervals));
    }

    /* Compute SpO2 */
    raw_spo2 = ComputeSpO2(red_linear, ir_linear, count);

    /* Validate and smooth */
    if (raw_hr >= MAX30102_MIN_VALID_HR && raw_hr <= MAX30102_MAX_VALID_HR)
    {
        max30102_ctx.heart_rate = SmoothValue(
            max30102_ctx.hr_smooth, &max30102_ctx.hr_smooth_idx,
            MAX30102_HR_SMOOTH_WINDOW, raw_hr);
    }

    if (raw_spo2 >= MAX30102_MIN_VALID_SPO2 && raw_spo2 <= MAX30102_MAX_VALID_SPO2)
    {
        max30102_ctx.spo2 = SmoothValue(
            max30102_ctx.spo2_smooth, &max30102_ctx.spo2_smooth_idx,
            MAX30102_SPO2_SMOOTH_WINDOW, raw_spo2);
    }

    /* Compute HRV (SDNN) if enough RR intervals */
    if (max30102_ctx.rr_count >= MAX30102_HRV_MIN_RR_COUNT)
    {
        uint32_t sum = 0;
        uint32_t mean;
        uint32_t variance = 0;

        for (i = 0; i < max30102_ctx.rr_count; i++)
            sum += max30102_ctx.rr_intervals[i];

        mean = sum / max30102_ctx.rr_count;

        for (i = 0; i < max30102_ctx.rr_count; i++)
        {
            int32_t diff = (int32_t)max30102_ctx.rr_intervals[i] - (int32_t)mean;
            variance += (uint32_t)(diff * diff);
        }

        /* SDNN = sqrt(variance / count), integer approximation */
        variance /= max30102_ctx.rr_count;

        /* Integer sqrt */
        {
            uint32_t root = 0;
            uint32_t bit = 1 << 30;

            while (bit > variance)
                bit >>= 2;

            while (bit != 0)
            {
                if (variance >= root + bit)
                {
                    variance -= root + bit;
                    root = (root >> 1) + bit;
                }
                else
                {
                    root >>= 1;
                }
                bit >>= 2;
            }

            max30102_ctx.hrv = (uint16_t)root;
        }

        /* Reset RR buffer for next calculation */
        max30102_ctx.rr_count = 0;
    }
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
