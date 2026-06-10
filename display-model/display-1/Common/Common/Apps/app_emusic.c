/********************************** (C) COPYRIGHT *******************************
* File Name          : app_emusic.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2026/06/09
* Description        : Electronic Music app (V1.0).
*                      Receives Keyboard-3 real-time music events from Core
*                      and renders 24 piano keys, 12 control buttons, and
*                      3 faders with partial dirty-region refresh.
*
* Layout (800x480):
*   Top: Title bar (40px)
*   Piano keys: two octaves C4~B5, 14 white + 10 black keys
*   Buttons: 2 rows x 6, labeled KEY1~KEY12
*   Faders: 3 vertical sliders with percentage labels
********************************************************************************/
#include "app_emusic.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 *  Layout Configuration (800x480)
 *
 *  Vertical centering: total content height ≈ 40(btn) + 8(gap) + 40(btn)
 *  + 16(gap) + 180(piano) = 284px. Available = 480 - 40(title) - 28(status)
 *  = 412px. Top padding = (412 - 284) / 2 ≈ 64px → offset = 40 + 64 = 104.
 *=============================================================================*/

/* Button section (above piano) */
#define BTN_AREA_Y          104
#define BTN_AREA_X          20
#define BTN_W               78
#define BTN_H               40
#define BTN_GAP_X           8
#define BTN_GAP_Y           8
#define BTN_COLS            6
#define BTN_ROWS            2

/* Piano section (below buttons) */
#define PIANO_Y             (BTN_AREA_Y + BTN_ROWS * BTN_H + (BTN_ROWS - 1) * BTN_GAP_Y + 16)
#define PIANO_H             180
#define PIANO_X             20
#define PIANO_W             540

/* Fader section (right side) */
#define FADER_AREA_X        590
#define FADER_AREA_Y        104
#define FADER_W             40
#define FADER_TRACK_H       260
#define FADER_GAP           30
#define FADER_KNOB_H        14

/* Start/Stop toggle button */
#define TOGGLE_BTN_X        (FADER_AREA_X)
#define TOGGLE_BTN_Y        (FADER_AREA_Y + FADER_TRACK_H + 24)
#define TOGGLE_BTN_W        (FADER_W * 3 + FADER_GAP * 2)
#define TOGGLE_BTN_H        44

/* Status bar */
#define STATUS_Y            (UI_SCREEN_HEIGHT - 28)
#define STATUS_H            28

/*=============================================================================
 *  Colors
 *=============================================================================*/

#define EMU_BG              UI_HEX(0x1A1A2E)
#define EMU_PANEL_BG        UI_HEX(0x16213E)
#define EMU_ACCENT          UI_HEX(0x7EC8C8)
#define EMU_TEXT            UI_HEX(0xE0E0E0)
#define EMU_TEXT_DIM        UI_HEX(0x808080)
#define EMU_WHITE_KEY       UI_HEX(0xF0F0F0)
#define EMU_WHITE_KEY_PRESSED UI_HEX(0x7EC8C8)
#define EMU_BLACK_KEY       UI_HEX(0x2C2C3E)
#define EMU_BLACK_KEY_PRESSED UI_HEX(0x5AAFAF)
#define EMU_BTN_BG         UI_HEX(0x16213E)
#define EMU_BTN_PRESSED    UI_HEX(0x0F3460)
#define EMU_FADER_TRACK    UI_HEX(0x0F3460)
#define EMU_FADER_FILL     UI_HEX(0x7EC8C8)
#define EMU_FADER_KNOB     UI_HEX(0xE0E0E0)
#define EMU_BORDER         UI_HEX(0x313244)
#define EMU_STATUS_BG      UI_HEX(0x181825)
#define EMU_STATUS_TEXT    UI_HEX(0x6C7086)
#define EMU_START_BG      UI_HEX(0x2E7D32)
#define EMU_START_BG_PRESSED UI_HEX(0x1B5E20)
#define EMU_STOP_BG       UI_HEX(0xC62828)
#define EMU_STOP_BG_PRESSED UI_HEX(0x8E0000)

/*=============================================================================
 *  Piano Key Layout
 *
 *  24 keys: C4~B5, two octaves.
 *  White keys: 1,3,5,6,8,10,12,13,15,17,18,20,22,24 (14 keys)
 *  Black keys: 2,4,7,9,11,14,16,19,21,23 (10 keys)
 *=============================================================================*/

#define WHITE_KEY_COUNT     14
#define BLACK_KEY_COUNT     10

/* White key dimensions */
#define WK_W                (PIANO_W / WHITE_KEY_COUNT)
#define WK_H                PIANO_H

/* Black key dimensions */
#define BK_W                (WK_W * 3 / 5)
#define BK_H                (PIANO_H * 3 / 5)

/* Key ID → is black key */
static const uint8_t s_key_is_black[25] = {
    0, /* 0 unused */
    0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, /* 1-12: C4-B4 */
    0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0  /* 13-24: C5-B5 */
};

/* White key ID list (in order from left to right) */
static const uint8_t s_white_keys[WHITE_KEY_COUNT] = {
    1, 3, 5, 6, 8, 10, 12, 13, 15, 17, 18, 20, 22, 24
};

/* Black key ID list (in order from left to right) */
static const uint8_t s_black_keys[BLACK_KEY_COUNT] = {
    2, 4, 7, 9, 11, 14, 16, 19, 21, 23
};

/* Black key position: which white key index it sits after.
 * In a standard piano octave, black keys sit between:
 *   C#: after white 0 (C)
 *   D#: after white 1 (D)
 *   F#: after white 3 (F)
 *   G#: after white 4 (G)
 *   A#: after white 5 (A)
 * Octave 1 (keys 2,4,7,9,11): after white indices 0,1,3,4,5
 * Octave 2 (keys 14,16,19,21,23): after white indices 7,8,10,11,12
 */
static const int8_t s_black_after_white[BLACK_KEY_COUNT] = {
    0, 1, 3, 4, 5,    /* first octave */
    7, 8, 10, 11, 12  /* second octave */
};

/* Note names for display */
static const char *s_note_names[25] = {
    "", "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4",
    "C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5"
};

/*=============================================================================
 *  State
 *=============================================================================*/

static ui_app_page_t s_app_emusic;

/* Current state from Keyboard-3 */
static uint8_t  s_key_bitmap[3];    /* 24-bit piano key bitmap */
static uint8_t  s_buttons[2];       /* 12-bit button bitmap */
static uint16_t s_fader[3];         /* 3 fader values (0~65535) */

/* Previous state for dirty detection */
static uint8_t  s_prev_key_bitmap[3];
static uint8_t  s_prev_buttons[2];
static uint16_t s_prev_fader[3];

/* Music running state (toggle) */
static bool s_music_running = false;

/* Status text */
static char s_status[48];

/* Widgets: back button + title + start/stop toggle */
static ui_button_t s_toggle_btn;
static ui_widget_t *s_emusic_widgets[3];

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void emusic_on_keys(const uint8_t *key_bitmap);
static void emusic_on_buttons(const uint8_t *buttons);
static void emusic_on_faders(uint16_t fl, uint16_t fm, uint16_t fr);

static uart_emusic_callbacks_t s_emusic_callbacks = {
    .on_music_keys    = emusic_on_keys,
    .on_music_buttons = emusic_on_buttons,
    .on_music_faders  = emusic_on_faders,
};

/*=============================================================================
 *  Helper: get key pressed state from bitmap
 *=============================================================================*/

static inline bool key_is_pressed(const uint8_t *bm, uint8_t key_id)
{
    /* key_id is 1-based, bit 0 = key_id 1 */
    uint8_t idx = key_id - 1;
    return (bm[idx >> 3] & (1u << (idx & 7))) != 0;
}

static inline bool button_is_pressed(const uint8_t *bm, uint8_t btn_idx)
{
    /* btn_idx is 0-based */
    return (bm[btn_idx >> 3] & (1u << (btn_idx & 7))) != 0;
}

/*=============================================================================
 *  Key Geometry Helpers
 *=============================================================================*/

/* Get white key rectangle by its index (0~13) */
static void get_white_key_rect(uint8_t wk_idx, ui_rect_t *r)
{
    r->x = PIANO_X + wk_idx * WK_W;
    r->y = PIANO_Y;
    r->w = WK_W - 1;
    r->h = WK_H;
}

/* Get black key rectangle by its index (0~9) */
static void get_black_key_rect(uint8_t bk_idx, ui_rect_t *r)
{
    int8_t after = s_black_after_white[bk_idx];
    r->x = PIANO_X + (after + 1) * WK_W - BK_W / 2 - 1;
    r->y = PIANO_Y;
    r->w = BK_W;
    r->h = BK_H;
}

/* Get button rectangle by index (0~11) */
static void get_button_rect(uint8_t btn_idx, ui_rect_t *r)
{
    uint8_t col = btn_idx % BTN_COLS;
    uint8_t row = btn_idx / BTN_COLS;
    r->x = BTN_AREA_X + col * (BTN_W + BTN_GAP_X);
    r->y = BTN_AREA_Y + row * (BTN_H + BTN_GAP_Y);
    r->w = BTN_W;
    r->h = BTN_H;
}

/* Get fader track rectangle by index (0~2) */
static void get_fader_track_rect(uint8_t f_idx, ui_rect_t *r)
{
    r->x = FADER_AREA_X + f_idx * (FADER_W + FADER_GAP);
    r->y = FADER_AREA_Y;
    r->w = FADER_W;
    r->h = FADER_TRACK_H;
}

/*=============================================================================
 *  Drawing: Piano Keys
 *=============================================================================*/

static void draw_white_key(uint8_t wk_idx, bool pressed)
{
    ui_rect_t r;
    get_white_key_rect(wk_idx, &r);
    ui_color_t color = pressed ? EMU_WHITE_KEY_PRESSED : EMU_WHITE_KEY;
    ui_draw_fill_rect(&r, color);
    ui_draw_rect_border(&r, EMU_BORDER, 1);

    /* Draw note name at bottom */
    uint8_t key_id = s_white_keys[wk_idx];
    const char *name = s_note_names[key_id];
    int16_t tw = ui_text_width(name, &font_montserrat_12);
    int16_t tx = r.x + (r.w - tw) / 2;
    ui_draw_text(tx, r.y + r.h - 18, name, &font_montserrat_12,
                 pressed ? UI_COLOR_WHITE : EMU_TEXT_DIM);
}

static void draw_black_key(uint8_t bk_idx, bool pressed)
{
    ui_rect_t r;
    get_black_key_rect(bk_idx, &r);
    ui_color_t color = pressed ? EMU_BLACK_KEY_PRESSED : EMU_BLACK_KEY;
    ui_draw_fill_round_rect(&r, 3, color);
}

static void draw_all_piano_keys(void)
{
    /* Draw white keys first */
    for (uint8_t i = 0; i < WHITE_KEY_COUNT; i++) {
        uint8_t key_id = s_white_keys[i];
        draw_white_key(i, key_is_pressed(s_key_bitmap, key_id));
    }
    /* Draw black keys on top */
    for (uint8_t i = 0; i < BLACK_KEY_COUNT; i++) {
        uint8_t key_id = s_black_keys[i];
        draw_black_key(i, key_is_pressed(s_key_bitmap, key_id));
    }
}

/* Invalidate only changed piano keys */
static void invalidate_changed_keys(const uint8_t *old_bm, const uint8_t *new_bm)
{
    /* Check white keys */
    for (uint8_t i = 0; i < WHITE_KEY_COUNT; i++) {
        uint8_t key_id = s_white_keys[i];
        if (key_is_pressed(old_bm, key_id) != key_is_pressed(new_bm, key_id)) {
            ui_rect_t r;
            get_white_key_rect(i, &r);
            ui_page_invalidate(&r);
        }
    }
    /* Check black keys */
    for (uint8_t i = 0; i < BLACK_KEY_COUNT; i++) {
        uint8_t key_id = s_black_keys[i];
        if (key_is_pressed(old_bm, key_id) != key_is_pressed(new_bm, key_id)) {
            ui_rect_t r;
            get_black_key_rect(i, &r);
            ui_page_invalidate(&r);
        }
    }
}

/*=============================================================================
 *  Drawing: Buttons
 *=============================================================================*/

static const char *s_btn_labels[12] = {
    "KEY1", "KEY2", "KEY3", "KEY4", "KEY5", "KEY6",
    "KEY7", "KEY8", "KEY9", "KEY10", "KEY11", "KEY12"
};

static void draw_button(uint8_t btn_idx, bool pressed)
{
    ui_rect_t r;
    get_button_rect(btn_idx, &r);
    ui_color_t bg = pressed ? EMU_BTN_PRESSED : EMU_BTN_BG;
    ui_draw_fill_round_rect(&r, 6, bg);
    ui_draw_round_rect_border(&r, 6, EMU_BORDER, 1);

    const char *label = s_btn_labels[btn_idx];
    int16_t tw = ui_text_width(label, &font_montserrat_12);
    int16_t tx = r.x + (r.w - tw) / 2;
    int16_t ty = r.y + (r.h - 12) / 2;
    ui_draw_text(tx, ty, label, &font_montserrat_12,
                 pressed ? EMU_ACCENT : EMU_TEXT_DIM);
}

static void draw_all_buttons(void)
{
    for (uint8_t i = 0; i < 12; i++) {
        draw_button(i, button_is_pressed(s_buttons, i));
    }
}

static void invalidate_changed_buttons(const uint8_t *old_bm, const uint8_t *new_bm)
{
    for (uint8_t i = 0; i < 12; i++) {
        if (button_is_pressed(old_bm, i) != button_is_pressed(new_bm, i)) {
            ui_rect_t r;
            get_button_rect(i, &r);
            ui_page_invalidate(&r);
        }
    }
}

/*=============================================================================
 *  Drawing: Faders
 *=============================================================================*/

static const char *s_fader_labels[3] = { "L", "M", "R" };

static void draw_fader(uint8_t f_idx, uint16_t value)
{
    ui_rect_t track;
    get_fader_track_rect(f_idx, &track);

    /* Track background */
    ui_draw_fill_round_rect(&track, 4, EMU_FADER_TRACK);

    /* Fill from bottom */
    uint8_t pct = (uint8_t)((uint32_t)value * 100 / 65535);
    int16_t fill_h = (int16_t)((uint32_t)track.h * pct / 100);
    if (fill_h > 0) {
        ui_rect_t fill = {
            track.x,
            track.y + track.h - fill_h,
            track.w,
            fill_h
        };
        ui_draw_fill_round_rect(&fill, 4, EMU_FADER_FILL);
    }

    /* Knob */
    int16_t knob_y = track.y + track.h - fill_h - FADER_KNOB_H / 2;
    if (knob_y < track.y) knob_y = track.y;
    if (knob_y + FADER_KNOB_H > track.y + track.h) knob_y = track.y + track.h - FADER_KNOB_H;
    ui_rect_t knob = { track.x - 4, knob_y, track.w + 8, FADER_KNOB_H };
    ui_draw_fill_round_rect(&knob, 4, EMU_FADER_KNOB);

    /* Label above */
    int16_t label_x = track.x + (track.w - ui_text_width(s_fader_labels[f_idx], &font_montserrat_12)) / 2;
    ui_draw_text(label_x, track.y - 18, s_fader_labels[f_idx],
                 &font_montserrat_12, EMU_TEXT);

    /* Percentage below */
    char pct_text[8];
    snprintf(pct_text, sizeof(pct_text), "%d%%", pct);
    int16_t pct_x = track.x + (track.w - ui_text_width(pct_text, &font_montserrat_12)) / 2;
    ui_draw_text(pct_x, track.y + track.h + 6, pct_text,
                 &font_montserrat_12, EMU_TEXT_DIM);
}

static void draw_all_faders(void)
{
    for (uint8_t i = 0; i < 3; i++) {
        draw_fader(i, s_fader[i]);
    }
}

static void invalidate_fader(uint8_t f_idx)
{
    ui_rect_t r;
    get_fader_track_rect(f_idx, &r);
    /* Expand to include label and percentage */
    r.y -= 20;
    r.h += 30;
    r.x -= 6;
    r.w += 12;
    ui_page_invalidate(&r);
}

static void invalidate_changed_faders(const uint16_t *old_f, const uint16_t *new_f)
{
    for (uint8_t i = 0; i < 3; i++) {
        /* Dead zone: only update if change > 1% */
        int diff = (int)new_f[i] - (int)old_f[i];
        if (diff < 0) diff = -diff;
        if (diff > 655) { /* ~1% of 65535 */
            invalidate_fader(i);
        }
    }
}

/*=============================================================================
 *  UART Callbacks (Keyboard-3 music events)
 *=============================================================================*/

static void emusic_on_keys(const uint8_t *key_bitmap)
{
    invalidate_changed_keys(s_key_bitmap, key_bitmap);
    memcpy(s_prev_key_bitmap, s_key_bitmap, 3);
    memcpy(s_key_bitmap, key_bitmap, 3);

    /* Count pressed keys for status */
    uint8_t count = 0;
    for (uint8_t i = 0; i < 24; i++) {
        if (key_is_pressed(s_key_bitmap, i + 1)) count++;
    }
    snprintf(s_status, sizeof(s_status), "Keys: %d pressed", count);
}

static void emusic_on_buttons(const uint8_t *buttons)
{
    invalidate_changed_buttons(s_buttons, buttons);
    memcpy(s_prev_buttons, s_buttons, 2);
    memcpy(s_buttons, buttons, 2);
}

static void emusic_on_faders(uint16_t fl, uint16_t fm, uint16_t fr)
{
    uint16_t new_f[3] = { fl, fm, fr };
    invalidate_changed_faders(s_fader, new_f);
    memcpy(s_prev_fader, s_fader, sizeof(s_fader));
    s_fader[0] = fl;
    s_fader[1] = fm;
    s_fader[2] = fr;
}

/*=============================================================================
 *  Start/Stop Toggle
 *=============================================================================*/

static void toggle_btn_on_click(ui_widget_t *w)
{
    (void)w;
    s_music_running = !s_music_running;

    if (s_music_running) {
        UART_SendCLI("music start");
        s_toggle_btn.text = "Stop";
        ui_button_set_colors(&s_toggle_btn, EMU_STOP_BG, EMU_STOP_BG_PRESSED, UI_COLOR_WHITE);
        strcpy(s_status, "Running...");
    } else {
        UART_SendCLI("music stop");
        s_toggle_btn.text = "Start";
        ui_button_set_colors(&s_toggle_btn, EMU_START_BG, EMU_START_BG_PRESSED, UI_COLOR_WHITE);
        strcpy(s_status, "Stopped");
    }

    /* Invalidate toggle button area */
    ui_rect_t r = { TOGGLE_BTN_X, TOGGLE_BTN_Y, TOGGLE_BTN_W, TOGGLE_BTN_H };
    ui_page_invalidate(&r);
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void emusic_page_enter(ui_page_t *page)
{
    (void)page;

    /* Reset state */
    memset(s_key_bitmap, 0, 3);
    memset(s_prev_key_bitmap, 0, 3);
    memset(s_buttons, 0, 2);
    memset(s_prev_buttons, 0, 2);
    memset(s_fader, 0, sizeof(s_fader));
    memset(s_prev_fader, 0, sizeof(s_prev_fader));
    s_music_running = false;
    strcpy(s_status, "Press Start to begin");

    /* Reset toggle button appearance */
    s_toggle_btn.text = "Start";
    ui_button_set_colors(&s_toggle_btn, EMU_START_BG, EMU_START_BG_PRESSED, UI_COLOR_WHITE);

    /* Register callbacks for Keyboard-3 events */
    UART_SetEMusicCallbacks(&s_emusic_callbacks);

    ui_page_invalidate_all();
}

static void emusic_page_exit(ui_page_t *page)
{
    (void)page;

    /* Stop music keyboard event reporting if running */
    if (s_music_running) {
        UART_SendCLI("music stop");
        s_music_running = false;
    }

    /* Unregister callbacks */
    UART_ClearEMusicCallbacks();
}

static void emusic_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;

    int16_t dirty_top = dirty->y;
    int16_t dirty_bot = dirty->y + dirty->h;
    int16_t dirty_left = dirty->x;
    int16_t dirty_right = dirty->x + dirty->w;

    /* Title bar background */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Main background */
    if (dirty_bot > APP_TITLE_BAR_H) {
        ui_rect_t bg = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                        UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
        ui_draw_fill_rect(&bg, EMU_BG);
    }

    /* Button section label */
    if (dirty_top < BTN_AREA_Y + 10 && dirty_bot > BTN_AREA_Y - 10) {
        ui_draw_text(BTN_AREA_X, BTN_AREA_Y - 12, "Controls",
                     &font_montserrat_12, EMU_TEXT_DIM);
    }

    /* Buttons */
    if (dirty_bot > BTN_AREA_Y && dirty_top < BTN_AREA_Y + 2 * (BTN_H + BTN_GAP_Y)) {
        draw_all_buttons();
    }

    /* Piano section label */
    if (dirty_top < PIANO_Y && dirty_bot > BTN_AREA_Y) {
        ui_draw_text(PIANO_X, PIANO_Y - 2, "Piano (C4-B5)",
                     &font_montserrat_12, EMU_TEXT_DIM);
    }

    /* Piano keys */
    if (dirty_bot > PIANO_Y && dirty_top < PIANO_Y + PIANO_H) {
        draw_all_piano_keys();
    }

    /* Fader section */
    if (dirty_right > FADER_AREA_X - 10) {
        /* Section label */
        if (dirty_top < FADER_AREA_Y + 10) {
            ui_draw_text(FADER_AREA_X, FADER_AREA_Y - 2, "Faders",
                         &font_montserrat_12, EMU_TEXT_DIM);
        }
        if (dirty_bot > FADER_AREA_Y && dirty_top < FADER_AREA_Y + FADER_TRACK_H + 30) {
            draw_all_faders();
        }
    }

    /* Status bar */
    if (dirty_bot > STATUS_Y) {
        ui_rect_t status_bg = {0, STATUS_Y, UI_SCREEN_WIDTH, STATUS_H};
        ui_draw_fill_rect(&status_bg, EMU_STATUS_BG);
        ui_draw_text(12, STATUS_Y + 8, s_status,
                     &font_montserrat_12, EMU_STATUS_TEXT);
    }
}

/*=============================================================================
 *  Initialization
 *=============================================================================*/

void app_emusic_init(void)
{
    ui_app_page_init(&s_app_emusic, "EMusic", 0x10E);

    /* Override page callbacks */
    ui_page_set_callbacks(&s_app_emusic.page,
                          emusic_page_enter, emusic_page_exit,
                          emusic_page_draw, NULL);

    /* Widgets: back button + title + start/stop toggle */
    s_emusic_widgets[0] = (ui_widget_t *)&s_app_emusic.btn_back;
    s_emusic_widgets[1] = (ui_widget_t *)&s_app_emusic.lbl_title;
    s_emusic_widgets[2] = (ui_widget_t *)&s_toggle_btn;
    ui_page_set_widgets(&s_app_emusic.page, s_emusic_widgets, 3);

    /* Initialize toggle button */
    {
        ui_rect_t btn_rect = { TOGGLE_BTN_X, TOGGLE_BTN_Y, TOGGLE_BTN_W, TOGGLE_BTN_H };
        ui_button_init(&s_toggle_btn, &btn_rect, "Start", &font_montserrat_16);
        ui_button_set_colors(&s_toggle_btn, EMU_START_BG, EMU_START_BG_PRESSED, UI_COLOR_WHITE);
        ui_button_set_radius(&s_toggle_btn, 8);
        ui_button_set_callback(&s_toggle_btn, toggle_btn_on_click);
    }

    /* Initialize state */
    memset(s_key_bitmap, 0, 3);
    memset(s_prev_key_bitmap, 0, 3);
    memset(s_buttons, 0, 2);
    memset(s_prev_buttons, 0, 2);
    memset(s_fader, 0, sizeof(s_fader));
    memset(s_prev_fader, 0, sizeof(s_prev_fader));
    strcpy(s_status, "Ready");

    ui_page_register(&s_app_emusic.page);
}

ui_page_t *app_emusic_get_page(void)
{
    return &s_app_emusic.page;
}
