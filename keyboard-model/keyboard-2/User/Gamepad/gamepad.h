/********************************** (C) COPYRIGHT *******************************
 * File Name          : gamepad.h
 * Description        : Gamepad input driver for Keyboard-2.
 *                      Handles 6 buttons, 3 toggle switches, 2 analog
 *                      joysticks (ADC), and 2 rotary encoders.
 *********************************************************************************/
#ifndef __GAMEPAD_H
#define __GAMEPAD_H

#include "ch32v10x.h"
#include <string.h>

/* ====================================================================
 * Hardware pin definitions
 * ==================================================================== */

/* Buttons (active low, internal pull-up) */
#define BUT1_PORT       GPIOB
#define BUT1_PIN        GPIO_Pin_10
#define BUT2_PORT       GPIOB
#define BUT2_PIN        GPIO_Pin_11
#define BUT3_PORT       GPIOB
#define BUT3_PIN        GPIO_Pin_12
#define BUT4_PORT       GPIOB
#define BUT4_PIN        GPIO_Pin_13
#define BUT5_PORT       GPIOB
#define BUT5_PIN        GPIO_Pin_14
#define BUT6_PORT       GPIOB
#define BUT6_PIN        GPIO_Pin_15

#define BUT_COUNT       6

/* Toggle switches (active low, internal pull-up) */
#define SW1_UP_PORT     GPIOB
#define SW1_UP_PIN      GPIO_Pin_0
#define SW1_DN_PORT     GPIOB
#define SW1_DN_PIN      GPIO_Pin_1

#define SW2_UP_PORT     GPIOA
#define SW2_UP_PIN      GPIO_Pin_13
#define SW2_DN_PORT     GPIOA
#define SW2_DN_PIN      GPIO_Pin_14

#define SW3_UP_PORT     GPIOB
#define SW3_UP_PIN      GPIO_Pin_4
#define SW3_DN_PORT     GPIOB
#define SW3_DN_PIN      GPIO_Pin_5

#define SW_COUNT        3

/* Rotary encoders (A/B quadrature + D push button) */
#define EC1_A_PORT      GPIOB
#define EC1_A_PIN       GPIO_Pin_6
#define EC1_B_PORT      GPIOB
#define EC1_B_PIN       GPIO_Pin_7
#define EC1_D_PORT      GPIOA
#define EC1_D_PIN       GPIO_Pin_11

#define EC2_A_PORT      GPIOB
#define EC2_A_PIN       GPIO_Pin_8
#define EC2_B_PORT      GPIOB
#define EC2_B_PIN       GPIO_Pin_9
#define EC2_D_PORT      GPIOA
#define EC2_D_PIN       GPIO_Pin_12

#define EC_COUNT        2

/* Joystick ADC channels */
#define ROC_ADC                 ADC1
#define ROC_ADC_CLK             RCC_APB2Periph_ADC1
#define ROC1_X_ADC_CHANNEL      ADC_Channel_4   /* PA4 */
#define ROC1_Y_ADC_CHANNEL      ADC_Channel_5   /* PA5 */
#define ROC2_X_ADC_CHANNEL      ADC_Channel_6   /* PA6 */
#define ROC2_Y_ADC_CHANNEL      ADC_Channel_7   /* PA7 */
#define ROC_AXES_COUNT          4

/* ====================================================================
 * Constants
 * ==================================================================== */

#define ADC_RESOLUTION          4096    /* 12-bit */
#define ADC_CENTER              2048
#define JOY_DEADZONE            200     /* ~5% of full range (ADC counts) */
#define JOY_CENTER_U8           128     /* uint8 center value */
#define JOY_DEADZONE_U8         20      /* 8-bit deadzone (±20 around center, applied after >>4) */
#define JOY_THRESHOLD_U8        20      /* threshold for WASD key mapping (uint8) */
#define JOY_CHANGE_THRESHOLD    10      /* min uint8 change to trigger report (prevents ADC jitter) */

#define DEBOUNCE_COUNT          3       /* debounce iterations (scan_period * count) */

/* Encoder step delta for mouse movement */
#define EC_MOUSE_STEP           5

/* Switch state encoding (2 bits per switch) */
#define SW_STATE_MID            0x00
#define SW_STATE_UP             0x01
#define SW_STATE_DOWN           0x02

/* ====================================================================
 * Gamepad state structure
 * ==================================================================== */

typedef struct {
    /* Joystick values (uint8, 0~255, center=128) */
    uint8_t roc1_x;
    uint8_t roc1_y;
    uint8_t roc2_x;
    uint8_t roc2_y;

    /* Button states (1=pressed, 0=released) */
    uint8_t button_pressed[BUT_COUNT];

    /* Switch states (SW_STATE_MID/UP/DOWN) */
    uint8_t switch_state[SW_COUNT];

    /* Encoder rotation delta (accumulated, read-and-clear) */
    int8_t  ec1_delta;
    int8_t  ec2_delta;

    /* Encoder button states (1=pressed, 0=released) */
    uint8_t ec_pressed[EC_COUNT];
} gamepad_state_t;

/* ====================================================================
 * Function declarations
 * ==================================================================== */

/* Initialize all gamepad hardware: GPIO, ADC, encoder state */
void Gamepad_Init(void);

/* Scan all inputs: buttons, switches, joysticks, encoders.
 * Should be called periodically (every scan tick, e.g. 5ms). */
void Gamepad_Scan(void);

/* Get current gamepad state (call after Gamepad_Scan) */
const gamepad_state_t *Gamepad_GetState(void);

/* Build CH9329 keyboard HID report from current state.
 * Maps ROC1→WASD, BUT3-6→HJKL, SW1-3→Shift/Ctrl/Alt.
 * report[0..7] = 8-byte HID keyboard report.
 * Returns number of active key slots used. */
uint8_t Gamepad_BuildKeyboardReport(uint8_t *report);

/* Build CH9329 relative mouse HID report from current state.
 * Maps ROC2→X/Y, BUT1/2→left/right, EC1→X, EC2→Y, EC1_D→middle.
 * mouse[0..3] = 4-byte HID mouse report. */
void Gamepad_BuildMouseReport(uint8_t *mouse);

/* Build Core protocol GAME_INPUT data (9 bytes).
 * data[0..8] = ROC1_X, ROC1_Y, ROC2_X, ROC2_Y, buttons, switches,
 *              EC1_delta, EC2_delta, encoder_buttons */
void Gamepad_BuildCoreData(uint8_t *data);

/* Check if gamepad state has changed since last CH9329/Core send */
uint8_t Gamepad_HasChanged(void);

/* Mark current state as "sent" (resets change flag) */
void Gamepad_MarkSent(void);

#endif /* __GAMEPAD_H */
