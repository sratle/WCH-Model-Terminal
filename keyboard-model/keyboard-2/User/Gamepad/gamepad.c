/********************************** (C) COPYRIGHT *******************************
 * File Name          : gamepad.c
 * Description        : Gamepad input driver implementation for Keyboard-2.
 *                      Handles 6 buttons, 3 toggle switches, 2 analog
 *                      joysticks (ADC), and 2 rotary encoders.
 *                      Provides CH9329 HID report builders (keyboard+mouse)
 *                      and Core protocol data builder.
 *********************************************************************************/
#include "gamepad.h"
#include "../CH9329/ch9329.h"

/* ====================================================================
 * Internal state
 * ==================================================================== */

static gamepad_state_t g_state;
static gamepad_state_t g_prev_sent;

/* Debounce counters for buttons */
static uint8_t btn_dbc[BUT_COUNT];

/* Encoder previous raw pin levels (0=LOW, 1=HIGH) for edge detection */
static uint8_t ec1_prev_a, ec1_prev_b;
static uint8_t ec2_prev_a, ec2_prev_b;

/* Change flag */
static uint8_t g_changed = 0;

/* ====================================================================
 * Rotary encoder edge-detection decoder
 *
 * Standard incremental encoder has two quadrature channels A & B.
 * Only one channel changes at any given time (Gray code).
 *
 * CW  (A leads B): 11 -> 01 -> 00 -> 10 -> 11  (raw pin levels)
 * CCW (B leads A): 11 -> 10 -> 00 -> 01 -> 11
 *
 * Edge rules (using raw GPIO readings, HIGH=1/LOW=0):
 *   On A edge: A != B  -> CW  (+1)
 *             A == B  -> CCW (-1)
 *   On B edge: A == B  -> CW  (+1)
 *             A != B  -> CCW (-1)
 * ==================================================================== */

/* Process one encoder, returns delta (-1, 0, or +1) */
static int8_t Encoder_Decode(uint8_t curr_a, uint8_t curr_b,
                             uint8_t *prev_a, uint8_t *prev_b)
{
    int8_t delta = 0;

    /* A channel edge */
    if (curr_a != *prev_a)
    {
        if (curr_a != curr_b)
            delta++;   /* CW */
        else
            delta--;   /* CCW */
    }

    /* B channel edge (independent, gives full 4x resolution) */
    if (curr_b != *prev_b)
    {
        if (curr_a == curr_b)
            delta++;   /* CW */
        else
            delta--;   /* CCW */
    }

    *prev_a = curr_a;
    *prev_b = curr_b;
    return delta;
}

/* ====================================================================
 * GPIO initialization
 * ==================================================================== */

static void Gamepad_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_AFIO, ENABLE);

    /* PA13/PA14 are default SWD pins; must remap to use as GPIO */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);

    /* --- Buttons: PB10~PB15, input pull-up --- */
    gpio.GPIO_Pin  = BUT1_PIN | BUT2_PIN | BUT3_PIN |
                     BUT4_PIN | BUT5_PIN | BUT6_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio);

    /* --- Switch SW1: PB0, PB1 --- */
    gpio.GPIO_Pin  = SW1_UP_PIN | SW1_DN_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio);

    /* --- Switch SW2: PA13, PA14 (remapped from SWD) --- */
    gpio.GPIO_Pin  = SW2_UP_PIN | SW2_DN_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &gpio);

    /* --- Switch SW3: PB4, PB5 --- */
    gpio.GPIO_Pin  = SW3_UP_PIN | SW3_DN_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio);

    /* --- Encoder 1: PB6(A), PB7(B), PA11(D) --- */
    /* Encoder C pin is VCC, so A/B/D are pull-down, active HIGH */
    gpio.GPIO_Pin  = EC1_A_PIN | EC1_B_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOB, &gpio);

    gpio.GPIO_Pin  = EC1_D_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOA, &gpio);

    /* --- Encoder 2: PB8(A), PB9(B), PA12(D) --- */
    gpio.GPIO_Pin  = EC2_A_PIN | EC2_B_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOB, &gpio);

    gpio.GPIO_Pin  = EC2_D_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOA, &gpio);

    /* --- Joystick ADC pins: PA4~PA7, analog input --- */
    gpio.GPIO_Pin  = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &gpio);
}

/* ====================================================================
 * ADC initialization
 * ==================================================================== */

static void Gamepad_ADC_Init(void)
{
    ADC_InitTypeDef adc;

    RCC_APB2PeriphClockCmd(ROC_ADC_CLK, ENABLE);

    RCC_ADCCLKConfig(RCC_PCLK2_Div6);   /* 72MHz/6 = 12MHz ADC clock */

    ADC_DeInit(ROC_ADC);
    ADC_StructInit(&adc);

    adc.ADC_Mode               = ADC_Mode_Independent;
    adc.ADC_ScanConvMode       = DISABLE;
    adc.ADC_ContinuousConvMode = DISABLE;
    adc.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    adc.ADC_DataAlign          = ADC_DataAlign_Right;
    adc.ADC_NbrOfChannel       = 1;
    ADC_Init(ROC_ADC, &adc);

    ADC_Cmd(ROC_ADC, ENABLE);

    /* ADC calibration */
    ADC_ResetCalibration(ROC_ADC);
    while (ADC_GetResetCalibrationStatus(ROC_ADC));
    ADC_StartCalibration(ROC_ADC);
    while (ADC_GetCalibrationStatus(ROC_ADC));
}

/* ====================================================================
 * ADC single-channel read
 * ==================================================================== */

static uint16_t Gamepad_ADC_Read(uint8_t channel)
{
    ADC_RegularChannelConfig(ROC_ADC, channel, 1, ADC_SampleTime_55Cycles5);
    ADC_SoftwareStartConvCmd(ROC_ADC, ENABLE);
    while (!ADC_GetFlagStatus(ROC_ADC, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ROC_ADC);
}

/* ====================================================================
 * Read a single GPIO pin (returns 1 if pin is low = active)
 * ==================================================================== */

static uint8_t GPIO_IsActive(GPIO_TypeDef *port, uint16_t pin)
{
    return (GPIO_ReadInputDataBit(port, pin) == Bit_RESET) ? 1 : 0;
}

/* ====================================================================
 * Public API implementation
 * ==================================================================== */

void Gamepad_Init(void)
{
    memset(&g_state, 0, sizeof(g_state));
    memset(&g_prev_sent, 0, sizeof(g_prev_sent));
    memset(btn_dbc, 0, sizeof(btn_dbc));

    Gamepad_GPIO_Init();
    Gamepad_ADC_Init();

    /* Initialize encoder previous raw pin levels */
    ec1_prev_a = GPIO_ReadInputDataBit(EC1_A_PORT, EC1_A_PIN);
    ec1_prev_b = GPIO_ReadInputDataBit(EC1_B_PORT, EC1_B_PIN);
    ec2_prev_a = GPIO_ReadInputDataBit(EC2_A_PORT, EC2_A_PIN);
    ec2_prev_b = GPIO_ReadInputDataBit(EC2_B_PORT, EC2_B_PIN);
}

void Gamepad_Scan(void)
{
    uint8_t i;
    uint8_t raw;
    int16_t adc_val;
    int16_t centered;

    /* --- Button scan with debounce --- */
    static const struct {
        GPIO_TypeDef *port;
        uint16_t pin;
    } btn_pins[BUT_COUNT] = {
        { BUT1_PORT, BUT1_PIN },
        { BUT2_PORT, BUT2_PIN },
        { BUT3_PORT, BUT3_PIN },
        { BUT4_PORT, BUT4_PIN },
        { BUT5_PORT, BUT5_PIN },
        { BUT6_PORT, BUT6_PIN },
    };

    for (i = 0; i < BUT_COUNT; i++)
    {
        raw = GPIO_IsActive(btn_pins[i].port, btn_pins[i].pin);

        if (raw != g_state.button_pressed[i])
        {
            btn_dbc[i]++;
            if (btn_dbc[i] >= DEBOUNCE_COUNT)
            {
                g_state.button_pressed[i] = raw;
                btn_dbc[i] = 0;
                g_changed = 1;
            }
        }
        else
        {
            btn_dbc[i] = 0;
        }
    }

    /* --- Switch scan --- */
    {
        uint8_t sw_up, sw_dn;

        /* SW1 */
        sw_up = GPIO_IsActive(SW1_UP_PORT, SW1_UP_PIN);
        sw_dn = GPIO_IsActive(SW1_DN_PORT, SW1_DN_PIN);
        {
            uint8_t new_state;
            if (sw_up && !sw_dn)      new_state = SW_STATE_UP;
            else if (!sw_up && sw_dn) new_state = SW_STATE_DOWN;
            else                      new_state = SW_STATE_MID;
            if (new_state != g_state.switch_state[0])
            {
                g_state.switch_state[0] = new_state;
                g_changed = 1;
            }
        }

        /* SW2 */
        sw_up = GPIO_IsActive(SW2_UP_PORT, SW2_UP_PIN);
        sw_dn = GPIO_IsActive(SW2_DN_PORT, SW2_DN_PIN);
        {
            uint8_t new_state;
            if (sw_up && !sw_dn)      new_state = SW_STATE_UP;
            else if (!sw_up && sw_dn) new_state = SW_STATE_DOWN;
            else                      new_state = SW_STATE_MID;
            if (new_state != g_state.switch_state[1])
            {
                g_state.switch_state[1] = new_state;
                g_changed = 1;
            }
        }

        /* SW3 */
        sw_up = GPIO_IsActive(SW3_UP_PORT, SW3_UP_PIN);
        sw_dn = GPIO_IsActive(SW3_DN_PORT, SW3_DN_PIN);
        {
            uint8_t new_state;
            if (sw_up && !sw_dn)      new_state = SW_STATE_UP;
            else if (!sw_up && sw_dn) new_state = SW_STATE_DOWN;
            else                      new_state = SW_STATE_MID;
            if (new_state != g_state.switch_state[2])
            {
                g_state.switch_state[2] = new_state;
                g_changed = 1;
            }
        }
    }

    /* --- Joystick ADC scan (uint8: 0-255, center=128, 8-bit deadzone) ---
     * Deadzone applied AFTER >>4 conversion (not on 12-bit ADC value)
     * to avoid discontinuity at the deadzone boundary. */
    adc_val = (int16_t)Gamepad_ADC_Read(ROC1_X_ADC_CHANNEL);
    { uint8_t v = (uint8_t)(adc_val >> 4);
      if (v > JOY_CENTER_U8 - JOY_DEADZONE_U8 && v < JOY_CENTER_U8 + JOY_DEADZONE_U8)
          v = JOY_CENTER_U8;
      if (v != g_state.roc1_x) { g_state.roc1_x = v; g_changed = 1; } }

    adc_val = (int16_t)Gamepad_ADC_Read(ROC1_Y_ADC_CHANNEL);
    { uint8_t v = (uint8_t)(adc_val >> 4);
      if (v > JOY_CENTER_U8 - JOY_DEADZONE_U8 && v < JOY_CENTER_U8 + JOY_DEADZONE_U8)
          v = JOY_CENTER_U8;
      if (v != g_state.roc1_y) { g_state.roc1_y = v; g_changed = 1; } }

    adc_val = (int16_t)Gamepad_ADC_Read(ROC2_X_ADC_CHANNEL);
    { uint8_t v = (uint8_t)(adc_val >> 4);
      if (v > JOY_CENTER_U8 - JOY_DEADZONE_U8 && v < JOY_CENTER_U8 + JOY_DEADZONE_U8)
          v = JOY_CENTER_U8;
      if (v != g_state.roc2_x) { g_state.roc2_x = v; g_changed = 1; } }

    adc_val = (int16_t)Gamepad_ADC_Read(ROC2_Y_ADC_CHANNEL);
    { uint8_t v = (uint8_t)(adc_val >> 4);
      if (v > JOY_CENTER_U8 - JOY_DEADZONE_U8 && v < JOY_CENTER_U8 + JOY_DEADZONE_U8)
          v = JOY_CENTER_U8;
      if (v != g_state.roc2_y) { g_state.roc2_y = v; g_changed = 1; } }

    /* --- Encoder edge-detection decode --- */
    {
        uint8_t curr_a, curr_b;
        int8_t  delta;

        /* EC1 */
        curr_a = GPIO_ReadInputDataBit(EC1_A_PORT, EC1_A_PIN);
        curr_b = GPIO_ReadInputDataBit(EC1_B_PORT, EC1_B_PIN);
        delta  = Encoder_Decode(curr_a, curr_b, &ec1_prev_a, &ec1_prev_b);
        if (delta != 0)
        {
            g_state.ec1_delta += delta;
            g_changed = 1;
        }

        /* EC2 */
        curr_a = GPIO_ReadInputDataBit(EC2_A_PORT, EC2_A_PIN);
        curr_b = GPIO_ReadInputDataBit(EC2_B_PORT, EC2_B_PIN);
        delta  = Encoder_Decode(curr_a, curr_b, &ec2_prev_a, &ec2_prev_b);
        if (delta != 0)
        {
            g_state.ec2_delta += delta;
            g_changed = 1;
        }
    }

    /* --- Encoder button scan (active HIGH, pull-down) --- */
    raw = GPIO_ReadInputDataBit(EC1_D_PORT, EC1_D_PIN);  /* 1=pressed, 0=released */
    if (raw != g_state.ec_pressed[0])
    {
        g_state.ec_pressed[0] = raw;
        g_changed = 1;
    }
    raw = GPIO_ReadInputDataBit(EC2_D_PORT, EC2_D_PIN);
    if (raw != g_state.ec_pressed[1])
    {
        g_state.ec_pressed[1] = raw;
        g_changed = 1;
    }
}

const gamepad_state_t *Gamepad_GetState(void)
{
    return &g_state;
}

uint8_t Gamepad_BuildKeyboardReport(uint8_t *report)
{
    uint8_t slot = 2;  /* start at key slot 0 (index 2 in report) */

    /* Clear report */
    memset(report, 0, CH9329_KB_DATA_LEN);

    /* --- Modifier keys from switches --- */
    /* SW1 UP → Left Shift, SW2 UP → Left Ctrl, SW3 UP → Left Alt */
    if (g_state.switch_state[0] == SW_STATE_UP) report[0] |= HID_MOD_LSHIFT;
    if (g_state.switch_state[1] == SW_STATE_UP) report[0] |= HID_MOD_LCTRL;
    if (g_state.switch_state[2] == SW_STATE_UP) report[0] |= HID_MOD_LALT;

    /* --- ROC1 → WASD (uint8: center=128, threshold=± 20) --- */
    if (g_state.roc1_x > JOY_CENTER_U8 + JOY_THRESHOLD_U8)       report[slot++] = HID_KEY_D;  /* Right */
    else if (g_state.roc1_x < JOY_CENTER_U8 - JOY_THRESHOLD_U8)  report[slot++] = HID_KEY_A;  /* Left */
    if (g_state.roc1_y > JOY_CENTER_U8 + JOY_THRESHOLD_U8)       report[slot++] = HID_KEY_S;  /* Down */
    else if (g_state.roc1_y < JOY_CENTER_U8 - JOY_THRESHOLD_U8)  report[slot++] = HID_KEY_W;  /* Up */

    /* --- BUT3~6 → HJKL --- */
    if (g_state.button_pressed[2] && slot < CH9329_KB_DATA_LEN)
        report[slot++] = HID_KEY_H;
    if (g_state.button_pressed[3] && slot < CH9329_KB_DATA_LEN)
        report[slot++] = HID_KEY_J;
    if (g_state.button_pressed[4] && slot < CH9329_KB_DATA_LEN)
        report[slot++] = HID_KEY_K;
    if (g_state.button_pressed[5] && slot < CH9329_KB_DATA_LEN)
        report[slot++] = HID_KEY_L;

    return (slot - 2);
}

void Gamepad_BuildMouseReport(uint8_t *mouse)
{
    int16_t mx, my;

    /* --- Buttons --- */
    /* BUT1 → left, BUT2 → right, EC1_D → middle */
    mouse[0] = 0;
    if (g_state.button_pressed[0]) mouse[0] |= HID_MS_BTN_LEFT;
    if (g_state.button_pressed[1]) mouse[0] |= HID_MS_BTN_RIGHT;
    if (g_state.ec_pressed[0])     mouse[0] |= HID_MS_BTN_MIDDLE;

    /* --- X: ROC2_X (centered + scaled) + EC1 delta --- */
    mx = (int16_t)g_state.roc2_x - JOY_CENTER_U8;
    mx = mx / 8;
    mx += (int16_t)g_state.ec1_delta * EC_MOUSE_STEP;
    if (mx > 127)  mx = 127;
    if (mx < -128) mx = -128;
    mouse[1] = (uint8_t)(int8_t)mx;

    /* --- Y: ROC2_Y (centered + scaled) + EC2 delta --- */
    my = (int16_t)g_state.roc2_y - JOY_CENTER_U8;
    my = my / 8;
    my += (int16_t)g_state.ec2_delta * EC_MOUSE_STEP;
    if (my > 127)  my = 127;
    if (my < -128) my = -128;
    mouse[2] = (uint8_t)(int8_t)my;

    /* --- Wheel: unused --- */
    mouse[3] = 0;
}

void Gamepad_BuildCoreData(uint8_t *data)
{
    /* DATA[0~3]: Joystick values */
    data[0] = (uint8_t)g_state.roc1_x;
    data[1] = (uint8_t)g_state.roc1_y;
    data[2] = (uint8_t)g_state.roc2_x;
    data[3] = (uint8_t)g_state.roc2_y;

    /* DATA[4]: Button bitmap (bit0~5 = BUT1~6) */
    data[4] = 0;
    if (g_state.button_pressed[0]) data[4] |= (1 << 0);
    if (g_state.button_pressed[1]) data[4] |= (1 << 1);
    if (g_state.button_pressed[2]) data[4] |= (1 << 2);
    if (g_state.button_pressed[3]) data[4] |= (1 << 3);
    if (g_state.button_pressed[4]) data[4] |= (1 << 4);
    if (g_state.button_pressed[5]) data[4] |= (1 << 5);

    /* DATA[5]: Switch states (2 bits each) */
    data[5] = (g_state.switch_state[0] & 0x03) |
              ((g_state.switch_state[1] & 0x03) << 2) |
              ((g_state.switch_state[2] & 0x03) << 4);

    /* DATA[6~7]: Encoder deltas */
    data[6] = (uint8_t)g_state.ec1_delta;
    data[7] = (uint8_t)g_state.ec2_delta;

    /* DATA[8]: Encoder button bitmap */
    data[8] = 0;
    if (g_state.ec_pressed[0]) data[8] |= (1 << 0);
    if (g_state.ec_pressed[1]) data[8] |= (1 << 1);
}

uint8_t Gamepad_HasChanged(void)
{
    return g_changed;
}

void Gamepad_MarkSent(void)
{
    /* Clear encoder deltas after being consumed */
    g_state.ec1_delta = 0;
    g_state.ec2_delta = 0;

    /* Save sent state and clear change flag */
    g_prev_sent = g_state;
    g_changed = 0;
}
