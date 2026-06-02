/********************************** (C) COPYRIGHT *******************************
 * File Name          : key.c
 * Description        : Key scanning, debounce and event detection module.
 *                      Maps 74HC165 raw bits (active LOW) to key press/release.
 *********************************************************************************/
#include "key.h"

/* ====================================================================
 * Key name table (matches keyboard-layout.json, left-to-right, top-to-bottom)
 * ==================================================================== */
const char *key_names[KEY_COUNT] = {
    /* Row 1: 0-9 */
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "Back",
    /* Row 2: 10-19 */
    "Caps", "A", "S", "D", "F", "G", "H", "J", "P", "Enter",
    /* Row 3: 20-29 */
    "Shift", "Z", "X", "C", "V", "B", "N", "M", "K", "L",
    /* Row 4: 30-38 */
    "Ctrl", "Fn", "Super", "Space", ",", ".", ";", "'", "/"
};

/* ====================================================================
 * USB HID key code table (Keyboard/Keypad Page 0x07)
 * Modifier keys: 0xE0=L_Ctrl, 0xE1=L_Shift, 0xE3=L_GUI
 * Fn key: 0x00 (no standard HID code, used for local layer switching)
 * Back: mapped to Escape (0x29)
 * ==================================================================== */
const uint8_t key_hid_codes[KEY_COUNT] = {
    /* Row 1: Q W E R T Y U I O Back(Esc) */
    0x14, 0x1A, 0x08, 0x15, 0x17, 0x1C, 0x18, 0x0C, 0x12, 0x29,
    /* Row 2: Caps A S D F G H J P Enter */
    0x39, 0x04, 0x16, 0x07, 0x09, 0x0A, 0x0B, 0x0D, 0x13, 0x28,
    /* Row 3: Shift(L) Z X C V B N M K L */
    0xE1, 0x1D, 0x1B, 0x06, 0x19, 0x05, 0x11, 0x10, 0x0E, 0x0F,
    /* Row 4: Ctrl(L) Fn(none) Super(L_GUI) Space , . ; ' / */
    0xE0, 0x00, 0xE3, 0x2C, 0x36, 0x37, 0x33, 0x34, 0x38
};

/* ====================================================================
 * Modifier bit for each key (maps to HID report byte 0 bitmask)
 * 0x00 = not a modifier key
 * ==================================================================== */
const uint8_t key_mod_bits[KEY_COUNT] = {
    /* Row 1: Q W E R T Y U I O Back */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* Row 2: Caps A S D F G H J P Enter */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* Row 3: Shift(L) Z X C V B N M K L */
    HID_MOD_L_SHIFT, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* Row 4: Ctrl(L) Fn Super(L_GUI) Space , . ; ' / */
    HID_MOD_L_CTRL, 0x00, HID_MOD_L_GUI, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* ====================================================================
 * Module state (static)
 * ==================================================================== */
static uint8_t key_state[KEY_COUNT];         /* Debounced state: 0=released, 1=pressed */
static uint8_t debounce_cnt[KEY_COUNT];      /* Consecutive same-state counter */

/* ====================================================================
 * Event buffer (shared with main.c via key.h extern)
 * ==================================================================== */
KeyEvt    key_evt_buf[EVT_BUF_SIZE];
uint8_t   key_evt_count = 0;

/*********************************************************************
 * @fn      Key_Init
 *
 * @brief   Initialize key state arrays. All keys start as released.
 *
 * @return  none
 *********************************************************************/
void Key_Init(void)
{
    uint8_t i;

    for (i = 0; i < KEY_COUNT; i++)
    {
        key_state[i] = KEY_STATE_RELEASED;
        debounce_cnt[i] = 0;
    }
}

/*********************************************************************
 * @fn      Key_Scan
 *
 * @brief   Update debounce counters from raw 74HC165 data.
 *          Called from timer ISR context (every KEY_SCAN_PERIOD_MS).
 *
 * @param   raw  HC165 read buffer (5 bytes).
 *               raw[byte] bit position = key index.
 *               0 = pressed (active LOW), 1 = released.
 *
 * @return  none
 *********************************************************************/
void Key_Scan(const uint8_t *raw)
{
    uint8_t i;
    uint8_t byte_idx;
    uint8_t bit_idx;
    uint8_t is_pressed;

    for (i = 0; i < KEY_COUNT; i++)
    {
        byte_idx = i >> 3;          /* i / 8: which chip (0-4) */
        bit_idx  = i & 0x07;        /* bit position: buf[n] bit j = chip n's D(7-j) */

        /* HC165_Read() inverts bits: buf=1 means pressed, buf=0 means released */
        is_pressed = (raw[byte_idx] & (1 << bit_idx)) ? 1 : 0;

        if (is_pressed)
        {
            if (debounce_cnt[i] < KEY_DEBOUNCE_CNT)
            {
                debounce_cnt[i]++;
            }
        }
        else
        {
            if (debounce_cnt[i] > 0)
            {
                debounce_cnt[i]--;
            }
        }

        /* Debounce threshold reached: update stable state */
        if (debounce_cnt[i] >= KEY_DEBOUNCE_CNT)
        {
            key_state[i] = KEY_STATE_PRESSED;
        }
        else if (debounce_cnt[i] == 0)
        {
            key_state[i] = KEY_STATE_RELEASED;
        }
        /* else: in transition, keep previous state */
    }
}

/*********************************************************************
 * @fn      Key_Process
 *
 * @brief   Compare current debounced state with last known state,
 *          buffer press/release events for deferred printf.
 *          Called from main loop scan cycle (no blocking I/O).
 *
 * @return  none
 *********************************************************************/
void Key_Process(void)
{
    static uint8_t last_state[KEY_COUNT] = {0};
    uint8_t i;

    for (i = 0; i < KEY_COUNT; i++)
    {
        if (key_state[i] != last_state[i])
        {
            /* Buffer event for deferred printf (no blocking in scan cycle) */
            if (key_evt_count < EVT_BUF_SIZE)
            {
                key_evt_buf[key_evt_count].key_idx  = i;
                key_evt_buf[key_evt_count].pressed  = key_state[i];
                key_evt_count++;
            }
            last_state[i] = key_state[i];
        }
    }
}

/*********************************************************************
 * @fn      Key_GetState
 *
 * @brief   Get debounced state of a specific key.
 *
 * @param   key_id  Key index (0 ~ KEY_COUNT-1).
 *
 * @return  KEY_STATE_PRESSED or KEY_STATE_RELEASED.
 *********************************************************************/
uint8_t Key_GetState(uint8_t key_id)
{
    if (key_id >= KEY_COUNT)
        return KEY_STATE_RELEASED;
    return key_state[key_id];
}