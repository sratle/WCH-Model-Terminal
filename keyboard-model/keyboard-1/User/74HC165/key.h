/********************************** (C) COPYRIGHT *******************************
 * File Name          : key.h
 * Description        : Key scanning, debounce and event detection module.
 *                      Maps 74HC165 raw bits to logical key events.
 *********************************************************************************/
#ifndef __KEY_H
#define __KEY_H

#include "debug.h"
#include "74HC165.h"

/* ====================================================================
 * Key configuration
 * ==================================================================== */
#define KEY_COUNT           39      /* Total number of physical keys */
#define KEY_SCAN_PERIOD_MS  2       /* Timer scan period (2ms) */
#define KEY_DEBOUNCE_CNT    5       /* Stable count threshold (5 x 2ms = 10ms) */

/* ====================================================================
 * Key state definitions
 * ==================================================================== */
#define KEY_STATE_RELEASED  0
#define KEY_STATE_PRESSED   1

/* ====================================================================
 * Key event types
 * ==================================================================== */
#define KEY_EVT_NONE        0
#define KEY_EVT_PRESS       1
#define KEY_EVT_RELEASE     2

/* ====================================================================
 * Key index definitions (0-based, maps to keyboard-layout.json order)
 * Row 1: 0-9, Row 2: 10-19, Row 3: 20-29, Row 4: 30-38
 * ==================================================================== */
/* Row 1 */
#define KEY_Q       0
#define KEY_W       1
#define KEY_E       2
#define KEY_R       3
#define KEY_T       4
#define KEY_Y       5
#define KEY_U       6
#define KEY_I       7
#define KEY_O       8
#define KEY_BACK    9

/* Row 2 */
#define KEY_CAPS    10
#define KEY_A       11
#define KEY_S       12
#define KEY_D       13
#define KEY_F       14
#define KEY_G       15
#define KEY_H       16
#define KEY_J       17
#define KEY_P       18
#define KEY_ENTER   19

/* Row 3 */
#define KEY_SHIFT   20
#define KEY_Z       21
#define KEY_X       22
#define KEY_C       23
#define KEY_V       24
#define KEY_B       25
#define KEY_N       26
#define KEY_M       27
#define KEY_K       28
#define KEY_L       29

/* Row 4 */
#define KEY_CTRL    30
#define KEY_FN      31
#define KEY_TAB     32
#define KEY_SPACE   33
#define KEY_COMMA   34
#define KEY_DOT     35
#define KEY_SEMI    36
#define KEY_QUOTE   37
#define KEY_SLASH   38

/* ====================================================================
 * Key name table (declared in key.c)
 * ==================================================================== */
extern const char *key_names[KEY_COUNT];

/* ====================================================================
 * USB HID key code table (declared in key.c)
 * Maps key index (0-38) to USB HID Usage ID (Keyboard Page 0x07).
 * Modifier keys use their modifier bit value (0xE0-E7).
 * Fn key = 0x00 (no HID code, handled locally for layer switching).
 * ==================================================================== */
extern const uint8_t key_hid_codes[KEY_COUNT];

/* HID modifier bit masks (for byte 0 of HID keyboard report) */
#define HID_MOD_NONE        0x00
#define HID_MOD_L_CTRL      0x01
#define HID_MOD_L_SHIFT     0x02
#define HID_MOD_L_ALT       0x04
#define HID_MOD_L_GUI       0x08
#define HID_MOD_R_CTRL      0x10
#define HID_MOD_R_SHIFT     0x20
#define HID_MOD_R_ALT       0x40
#define HID_MOD_R_GUI       0x80

/* Modifier bit for each key index (0 = not a modifier).
 * Keys with key_hid_codes >= 0xE0 are modifiers. */
extern const uint8_t key_mod_bits[KEY_COUNT];

/* ====================================================================
 * Key event buffer (defined in key.c, used by main.c FlushEvents)
 * Events are buffered during scan and flushed via printf in idle time.
 * ==================================================================== */
#define EVT_BUF_SIZE    16

typedef struct {
    uint8_t key_idx;      /* key index 0-38 */
    uint8_t pressed;      /* 1=press, 0=release */
} KeyEvt;

extern KeyEvt    key_evt_buf[EVT_BUF_SIZE];
extern uint8_t   key_evt_count;

/* ====================================================================
 * Function declarations
 * ==================================================================== */

/**
 * @brief  Initialize key module state arrays.
 */
void Key_Init(void);

/**
 * @brief  Update raw key state from 74HC165 scan data.
 *         Called from timer ISR context.
 * @param  raw  Pointer to HC165 read buffer (5 bytes, 40 bits).
 */
void Key_Scan(const uint8_t *raw);

/**
 * @brief  Process debounced key states, detect press/release events.
 *         Buffers events into key_evt_buf[] (no printf).
 *         Called from main loop scan cycle.
 */
void Key_Process(void);

/**
 * @brief  Get current debounced state of a specific key.
 * @param  key_id  Key index (0 ~ KEY_COUNT-1).
 * @return KEY_STATE_PRESSED or KEY_STATE_RELEASED.
 */
uint8_t Key_GetState(uint8_t key_id);

#endif /* __KEY_H */