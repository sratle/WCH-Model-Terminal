#include "fader.h"

/* ======================== ADC Channel Mapping ======================== */

/*
 * Fader index → ADC channel:
 *   0 (FADER_L) → PA3 → ADC_Channel_3
 *   1 (FADER_M) → PA1 → ADC_Channel_1
 *   2 (FADER_R) → PA2 → ADC_Channel_2
 */
static const uint8_t fader_adc_ch[FADER_COUNT] = {
    ADC_Channel_3,
    ADC_Channel_1,
    ADC_Channel_2
};

/* ======================== Filter State ======================== */

static uint16_t filter_buf[FADER_COUNT][FADER_FILTER_SIZE];
static uint8_t  filter_idx;
static uint16_t fader_value[FADER_COUNT];   /* filtered 12-bit value */
static uint16_t fader_output[FADER_COUNT];  /* scaled 16-bit output */
static uint8_t  fader_changed;

/* ======================== Internal ======================== */

static uint16_t Fader_ReadAdc(uint8_t channel)
{
    /* Configure regular channel: rank 1, sample time 239.5 cycles */
    ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_239Cycles5);

    /* Start software conversion */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    /* Wait for conversion complete */
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC))
        ;

    return ADC_GetConversionValue(ADC1);
}

/* ======================== Public API ======================== */

void Fader_Init(void)
{
    GPIO_InitTypeDef  gpio;
    ADC_InitTypeDef   adc;
    uint8_t i, j;

    /* Enable clocks */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);

    /* Set ADC clock divider to 6 (72MHz / 6 = 12MHz, max 14MHz) */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* Configure PA1, PA2, PA3 as analog input */
    gpio.GPIO_Pin   = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    gpio.GPIO_Mode  = GPIO_Mode_AIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);

    /* ADC1 configuration */
    ADC_DeInit(ADC1);
    adc.ADC_Mode               = ADC_Mode_Independent;
    adc.ADC_ScanConvMode       = DISABLE;
    adc.ADC_ContinuousConvMode = DISABLE;
    adc.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    adc.ADC_DataAlign          = ADC_DataAlign_Right;
    adc.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &adc);

    ADC_Cmd(ADC1, ENABLE);

    /* ADC calibration (required after enable) */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1))
        ;
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1))
        ;

    /* Clear filter state */
    filter_idx = 0;
    fader_changed = 0;

    for (i = 0; i < FADER_COUNT; i++)
    {
        fader_value[i]  = 0;
        fader_output[i] = 0;
        for (j = 0; j < FADER_FILTER_SIZE; j++)
        {
            filter_buf[i][j] = 0;
        }
    }

    /* Initial fill: read each channel to populate filter buffer */
    for (j = 0; j < FADER_FILTER_SIZE; j++)
    {
        for (i = 0; i < FADER_COUNT; i++)
        {
            filter_buf[i][j] = Fader_ReadAdc(fader_adc_ch[i]);
        }
    }
}

void Fader_Scan(void)
{
    uint8_t i;
    uint16_t raw, sum;
    int32_t diff;

    fader_changed = 0;

    /* Read all channels into current filter slot */
    for (i = 0; i < FADER_COUNT; i++)
    {
        raw = Fader_ReadAdc(fader_adc_ch[i]);
        filter_buf[i][filter_idx] = raw;
    }

    /* Advance circular buffer index */
    filter_idx++;
    if (filter_idx >= FADER_FILTER_SIZE)
        filter_idx = 0;

    /* Compute moving average and detect changes */
    for (i = 0; i < FADER_COUNT; i++)
    {
        uint8_t j;

        sum = 0;
        for (j = 0; j < FADER_FILTER_SIZE; j++)
        {
            sum += filter_buf[i][j];
        }
        raw = sum / FADER_FILTER_SIZE;

        /* Detect significant change */
        diff = (int32_t)raw - (int32_t)fader_value[i];
        if (diff > FADER_CHANGE_THRESHOLD || diff < -FADER_CHANGE_THRESHOLD)
        {
            fader_changed = 1;
        }

        fader_value[i] = raw;

        /* Scale 12-bit (0~4095) to 16-bit (0~65535) */
        fader_output[i] = (uint16_t)((uint32_t)raw << 4);
    }
}

void Fader_GetValues(uint16_t values[FADER_COUNT])
{
    values[0] = fader_output[0];
    values[1] = fader_output[1];
    values[2] = fader_output[2];
}

uint8_t Fader_HasChanged(void)
{
    return fader_changed;
}
