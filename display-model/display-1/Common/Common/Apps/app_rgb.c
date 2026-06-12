/********************************** (C) COPYRIGHT *******************************
* File Name          : app_rgb.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2026/06/07
* Description        : RGB lighting control app (V2.0).
*                      Mode selection (Solid/Breathing/Marquee/Custom).
*                      R/G/B color sliders with live preview square.
*                      Brightness and speed sliders.
*                      Apply, Load Custom, Status buttons.
*                      All commands via CLI passthrough to Core.
********************************************************************************/
#include "app_rgb.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*=============================================================================
 *  Layout Configuration (800x480)
 *=============================================================================*/

#define RGB_LEFT_W          400
#define RGB_RIGHT_X         RGB_LEFT_W

/* Mode buttons */
#define RGB_MODE_Y          (APP_TITLE_BAR_H + 10)
#define RGB_MODE_BTN_W      170
#define RGB_MODE_BTN_H      34
#define RGB_MODE_GAP        10

/* Color preview */
#define RGB_PREVIEW_X       30
#define RGB_PREVIEW_Y       (RGB_MODE_Y + 2 * (RGB_MODE_BTN_H + RGB_MODE_GAP) + 10)
#define RGB_PREVIEW_SIZE    100

/* Color sliders */
#define RGB_SLIDER_X        160
#define RGB_SLIDER_W        210
#define RGB_SLIDER_H        20
#define RGB_SLIDER_GAP      10
#define RGB_R_Y             (RGB_PREVIEW_Y + 10)
#define RGB_G_Y             (RGB_R_Y + RGB_SLIDER_H + RGB_SLIDER_GAP + 16)
#define RGB_B_Y             (RGB_G_Y + RGB_SLIDER_H + RGB_SLIDER_GAP + 16)
#define RGB_LABEL_X         (RGB_SLIDER_X - 20)

/* Right panel */
#define RGB_BRIGHT_Y        (APP_TITLE_BAR_H + 10)
#define RGB_SPEED_Y         (RGB_BRIGHT_Y + RGB_SLIDER_H + RGB_SLIDER_GAP + 30)
#define RGB_RIGHT_SLIDER_X  (RGB_RIGHT_X + 80)
#define RGB_RIGHT_SLIDER_W  280

/* Action buttons */
#define RGB_ACT_Y           (RGB_SPEED_Y + RGB_SLIDER_H + 30)
#define RGB_ACT_BTN_W       110
#define RGB_ACT_BTN_H       36
#define RGB_ACT_GAP         10

/* Preset colors row */
#define RGB_PRESET_Y        (RGB_ACT_Y + RGB_ACT_BTN_H + 24)
#define RGB_PRESET_SIZE     36
#define RGB_PRESET_GAP      8

/* Info area */
#define RGB_INFO_Y          (RGB_PRESET_Y + RGB_PRESET_SIZE + 20)

/* Status bar */
#define RGB_STATUS_Y        (UI_SCREEN_HEIGHT - 30)
#define RGB_STATUS_H        30

/*=============================================================================
 *  Colors
 *=============================================================================*/

#define RGB_BG              UI_HEX(0x1A1A2E)
#define RGB_PANEL_BG        UI_HEX(0x16213E)
#define RGB_ACCENT          UI_HEX(0x7EC8C8)
#define RGB_TEXT            UI_HEX(0xE0E0E0)
#define RGB_TEXT_DIM        UI_HEX(0x808080)
#define RGB_BTN_BG          UI_HEX(0x16213E)
#define RGB_BTN_ACTIVE      UI_HEX(0x0F3460)
#define RGB_SLIDER_BG       UI_HEX(0x0F3460)
#define RGB_STATUS_BG       UI_HEX(0x181825)
#define RGB_STATUS_TEXT     UI_HEX(0x6C7086)
#define RGB_BORDER          UI_HEX(0x313244)

/* Mode names */
static const char *s_mode_names[] = {"Solid", "Breathing", "Marquee", "Custom"};
/* Mode protocol values: 1=Solid, 2=Breathing, 3=Marquee, 0=Custom */
static const uint8_t s_mode_values[] = {1, 2, 3, 0};
#define RGB_NUM_MODES   4

/* Preset colors for quick selection */
static const uint8_t s_presets[][3] = {
    {255, 0, 0},     /* Red */
    {0, 255, 0},     /* Green */
    {0, 0, 255},     /* Blue */
    {255, 255, 0},   /* Yellow */
    {0, 255, 255},   /* Cyan */
    {255, 0, 255},   /* Magenta */
    {255, 128, 0},   /* Orange */
    {255, 255, 255}, /* White */
};
#define RGB_NUM_PRESETS (sizeof(s_presets) / sizeof(s_presets[0]))

/*=============================================================================
 *  State
 *=============================================================================*/

static ui_app_page_t s_app_rgb;

static uint8_t s_mode;        /* 0=Custom, 1=Solid, 2=Breathing, 3=Marquee */
static uint8_t s_r, s_g, s_b;
static uint8_t s_brightness;
static uint8_t s_speed;

/* CLI response via global buffer (on_cli_complete) */

/* Status message */
static char s_status[80];
/* Info text (from rgb status) */
static char s_info[256];

/* Widgets */
static ui_button_t  btn_mode[RGB_NUM_MODES];
static ui_slider_t  s_slider_r, s_slider_g, s_slider_b;
static ui_slider_t  s_slider_bright, s_slider_speed;
static ui_button_t  btn_apply, btn_custom, btn_status;
static ui_widget_t  s_preset_btns[8];
static ui_widget_t *s_rgb_widgets[2 + RGB_NUM_MODES + 5 + 3 + 8];
/* = 2 + 4 + 5 + 3 + 8 = 22 (back_btn + title + app widgets) */

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void rgb_on_cli_complete(const char *buf, uint16_t len, const char *tag);
static void rgb_send_apply(void);
static void rgb_set_status(const char *msg);
static void rgb_invalidate_preview(void);
static void rgb_invalidate_mode_btns(void);
static void rgb_invalidate_status(void);
static void rgb_invalidate_info(void);

static uart_cli_cb_t s_rgb_cb = {
    .on_cli_complete = rgb_on_cli_complete,
};

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void rgb_set_status(const char *msg)
{
    strncpy(s_status, msg, sizeof(s_status) - 1);
    s_status[sizeof(s_status) - 1] = '\0';
    rgb_invalidate_status();
}

static void rgb_invalidate_preview(void)
{
    ui_rect_t r = {RGB_PREVIEW_X, RGB_PREVIEW_Y, RGB_PREVIEW_SIZE, RGB_PREVIEW_SIZE};
    ui_page_invalidate(&r);
}

static void rgb_invalidate_mode_btns(void)
{
    for (int i = 0; i < RGB_NUM_MODES; i++)
        ui_widget_invalidate((ui_widget_t *)&btn_mode[i]);
}

static void rgb_invalidate_status(void)
{
    ui_rect_t r = {0, RGB_STATUS_Y, UI_SCREEN_WIDTH, RGB_STATUS_H};
    ui_page_invalidate(&r);
}

static void rgb_invalidate_info(void)
{
    ui_rect_t r = {RGB_RIGHT_X, RGB_INFO_Y, UI_SCREEN_WIDTH - RGB_RIGHT_X, 80};
    ui_page_invalidate(&r);
}

static ui_color_t rgb_to_565(uint8_t r, uint8_t g, uint8_t b)
{
    return UI_RGB(r, g, b);
}

static void rgb_update_preview(void)
{
    rgb_invalidate_preview();
}

/* rgb_mode_to_index: reserved for future use */
#if 0
static int rgb_mode_to_index(uint8_t mode)
{
    for (int i = 0; i < RGB_NUM_MODES; i++) {
        if (s_mode_values[i] == mode) return i;
    }
    return 0;
}
#endif

/*=============================================================================
 *  CLI Response
 *=============================================================================*/

static void rgb_on_cli_complete(const char *buf, uint16_t len, const char *tag)
{
    (void)tag;

    /* Check if this is an rgb-related response */
    if (strstr(buf, "rgb:") != NULL || strstr(buf, "mode=") != NULL) {
        if (strstr(buf, "mode=") != NULL) {
            int mode = 0, r = 0, g = 0, b = 0, bright = 0, speed = 0;
            if (sscanf(buf, "rgb: mode=%d R=%d G=%d B=%d bright=%d speed=%d",
                       &mode, &r, &g, &b, &bright, &speed) >= 4) {
                s_mode = (uint8_t)mode;
                s_r = (uint8_t)r;
                s_g = (uint8_t)g;
                s_b = (uint8_t)b;
                s_brightness = (uint8_t)bright;
                s_speed = (uint8_t)speed;

                ui_slider_set_value(&s_slider_r, s_r);
                ui_slider_set_value(&s_slider_g, s_g);
                ui_slider_set_value(&s_slider_b, s_b);
                ui_slider_set_value(&s_slider_bright, s_brightness);
                ui_slider_set_value(&s_slider_speed, s_speed);

                rgb_update_preview();
                rgb_invalidate_mode_btns();
            }

            uint16_t ilen = len;
            if (ilen > 255) ilen = 255;
            memcpy(s_info, buf, ilen);
            s_info[ilen] = '\0';
            for (uint16_t i = 0; i < ilen; i++) {
                if (s_info[i] == '\r' || s_info[i] == '\n')
                    s_info[i] = ' ';
            }
            rgb_invalidate_info();
        }

        char short_msg[78];
        uint16_t slen = len;
        if (slen > 77) slen = 77;
        memcpy(short_msg, buf, slen);
        short_msg[slen] = '\0';
        for (uint16_t i = 0; i < slen; i++) {
            if (short_msg[i] == '\r' || short_msg[i] == '\n')
                short_msg[i] = ' ';
        }
        rgb_set_status(short_msg);
    }
}

/*=============================================================================
 *  Mode Button Callbacks
 *=============================================================================*/

static void mode_btn_click(ui_widget_t *w)
{
    int idx = (int)(intptr_t)w->user_data;
    s_mode = s_mode_values[idx];
    rgb_invalidate_mode_btns();

    /* If custom mode, offer to load from json */
    if (s_mode == 0) {
        rgb_set_status("Custom mode selected - use Load Custom to play animation");
    } else {
        char msg[40];
        snprintf(msg, sizeof(msg), "Mode: %s", s_mode_names[idx]);
        rgb_set_status(msg);
    }
}

/*=============================================================================
 *  Slider Callbacks
 *=============================================================================*/

static void slider_r_change(ui_widget_t *w, int16_t value)
{
    (void)w;
    s_r = (uint8_t)value;
    rgb_update_preview();
}

static void slider_g_change(ui_widget_t *w, int16_t value)
{
    (void)w;
    s_g = (uint8_t)value;
    rgb_update_preview();
}

static void slider_b_change(ui_widget_t *w, int16_t value)
{
    (void)w;
    s_b = (uint8_t)value;
    rgb_update_preview();
}

static void slider_bright_change(ui_widget_t *w, int16_t value)
{
    (void)w;
    s_brightness = (uint8_t)value;
}

static void slider_speed_change(ui_widget_t *w, int16_t value)
{
    (void)w;
    s_speed = (uint8_t)value;
}

/*=============================================================================
 *  Action Button Callbacks
 *=============================================================================*/

static void rgb_send_apply(void)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "rgb mode %d %d %d %d %d %d",
             s_mode, s_r, s_g, s_b, s_brightness, s_speed);
    UART_SendCLI(cmd);
    rgb_set_status("Applied");
}

static void btn_apply_click(ui_widget_t *w)
{
    (void)w;
    rgb_send_apply();
}

static void btn_custom_click(ui_widget_t *w)
{
    (void)w;
    UART_SendCLI("rgb refresh json");
    rgb_set_status("Loading custom animation from rgb.json...");
}

static void btn_status_click(ui_widget_t *w)
{
    (void)w;
    UART_SendCLI("rgb status");
    rgb_set_status("Querying RGB status...");
}

/*=============================================================================
 *  Preset Color Buttons
 *=============================================================================*/

static void preset_touch_event(ui_widget_t *w, ui_event_t *e)
{
    if (e->type == UI_EVENT_CLICK || e->type == UI_EVENT_DOWN) {
        int idx = (int)(intptr_t)w->user_data;
        if (idx >= 0 && idx < (int)RGB_NUM_PRESETS) {
            s_r = s_presets[idx][0];
            s_g = s_presets[idx][1];
            s_b = s_presets[idx][2];

            ui_slider_set_value(&s_slider_r, s_r);
            ui_slider_set_value(&s_slider_g, s_g);
            ui_slider_set_value(&s_slider_b, s_b);

            rgb_update_preview();
            /* Invalidate sliders to show updated values */
            ui_widget_invalidate((ui_widget_t *)&s_slider_r);
            ui_widget_invalidate((ui_widget_t *)&s_slider_g);
            ui_widget_invalidate((ui_widget_t *)&s_slider_b);

            char msg[40];
            snprintf(msg, sizeof(msg), "Color: %d, %d, %d", s_r, s_g, s_b);
            rgb_set_status(msg);
        }
    }
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void rgb_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetCLICallbacks(&s_rgb_cb);
    /* Query current state */
    UART_SendCLI("rgb status");
    rgb_set_status("Querying RGB status...");
}

static void rgb_page_exit(ui_page_t *page)
{
    (void)page;
    UART_ClearCLICallbacks();
}

static void rgb_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;

    int16_t dirty_top = dirty->y;
    int16_t dirty_bot = dirty->y + dirty->h;
    int16_t dirty_left = dirty->x;
    int16_t dirty_right = dirty->x + dirty->w;

    /* Title bar background */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Full background */
    if (dirty_bot > APP_TITLE_BAR_H) {
        ui_rect_t bg = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
        ui_draw_fill_rect(&bg, RGB_BG);
    }

    /* Left panel area */
    if (dirty_left < RGB_LEFT_W && dirty_bot > APP_TITLE_BAR_H) {
        /* Section labels */
        if (dirty_top < RGB_MODE_Y + 10) {
            ui_draw_text(12, RGB_MODE_Y - 2, "Mode", &font_montserrat_12, RGB_TEXT_DIM);
        }

        if (dirty_top < RGB_PREVIEW_Y + 20) {
            ui_draw_text(12, RGB_PREVIEW_Y - 14, "Color Preview", &font_montserrat_12, RGB_TEXT_DIM);
        }

        /* Color preview square */
        if (dirty_top < RGB_PREVIEW_Y + RGB_PREVIEW_SIZE && dirty_bot > RGB_PREVIEW_Y) {
            ui_rect_t preview = {RGB_PREVIEW_X, RGB_PREVIEW_Y, RGB_PREVIEW_SIZE, RGB_PREVIEW_SIZE};
            ui_color_t pc = rgb_to_565(s_r, s_g, s_b);
            ui_draw_fill_rect(&preview, pc);
            ui_draw_rect_border(&preview, RGB_BORDER, 2);

            /* RGB value text below preview */
            char rgb_text[32];
            snprintf(rgb_text, sizeof(rgb_text), "R:%d  G:%d  B:%d", s_r, s_g, s_b);
            ui_draw_text(RGB_PREVIEW_X, RGB_PREVIEW_Y + RGB_PREVIEW_SIZE + 6,
                         rgb_text, &font_montserrat_12, RGB_TEXT);
        }

        /* Slider labels */
        if (dirty_top < RGB_B_Y + RGB_SLIDER_H + 20) {
            ui_draw_text(RGB_LABEL_X, RGB_R_Y + 2, "R", &font_montserrat_12, UI_HEX(0xFF6666));
            ui_draw_text(RGB_LABEL_X, RGB_G_Y + 2, "G", &font_montserrat_12, UI_HEX(0x66FF66));
            ui_draw_text(RGB_LABEL_X, RGB_B_Y + 2, "B", &font_montserrat_12, UI_HEX(0x6666FF));
        }

        /* Mode button active highlights */
        if (dirty_top < RGB_MODE_Y + 2 * (RGB_MODE_BTN_H + RGB_MODE_GAP)) {
            for (int i = 0; i < RGB_NUM_MODES; i++) {
                if (s_mode_values[i] == s_mode) {
                    ui_rect_t r = btn_mode[i].base.rect;
                    ui_draw_rect_border(&r, RGB_ACCENT, 2);
                }
            }
        }
    }

    /* Right panel */
    if (dirty_right > RGB_RIGHT_X && dirty_bot > APP_TITLE_BAR_H) {
        /* Section labels */
        if (dirty_top < RGB_BRIGHT_Y + 20) {
            ui_draw_text(RGB_RIGHT_X + 12, RGB_BRIGHT_Y - 2, "Brightness",
                         &font_montserrat_12, RGB_TEXT_DIM);
        }
        if (dirty_top < RGB_SPEED_Y + 20) {
            ui_draw_text(RGB_RIGHT_X + 12, RGB_SPEED_Y - 14, "Speed",
                         &font_montserrat_12, RGB_TEXT_DIM);
        }
        if (dirty_top < RGB_PRESET_Y + 10) {
            ui_draw_text(RGB_RIGHT_X + 12, RGB_PRESET_Y - 14, "Quick Colors",
                         &font_montserrat_12, RGB_TEXT_DIM);
        }

        /* Preset color swatch borders */
        if (dirty_top < RGB_PRESET_Y + RGB_PRESET_SIZE && dirty_bot > RGB_PRESET_Y) {
            for (int i = 0; i < (int)RGB_NUM_PRESETS; i++) {
                ui_rect_t r = s_preset_btns[i].rect;
                ui_draw_rect_border(&r, RGB_BORDER, 1);
            }
        }

        /* Info text area */
        if (dirty_top < RGB_INFO_Y + 80 && dirty_bot > RGB_INFO_Y) {
            ui_rect_t info_bg = {RGB_RIGHT_X + 8, RGB_INFO_Y,
                                 UI_SCREEN_WIDTH - RGB_RIGHT_X - 16, 72};
            ui_draw_fill_rect(&info_bg, RGB_PANEL_BG);
            ui_draw_rect_border(&info_bg, RGB_BORDER, 1);

            /* Draw info text (word-wrap within box) */
            if (s_info[0] != '\0') {
                int16_t ty = RGB_INFO_Y + 6;
                const char *p = s_info;
                int max_chars = (UI_SCREEN_WIDTH - RGB_RIGHT_X - 32) / 8;
                while (*p && ty < RGB_INFO_Y + 66) {
                    char line[64];
                    int llen = 0;
                    while (*p && llen < max_chars && *p != '\n') {
                        line[llen++] = *p++;
                    }
                    if (*p == '\n') p++;
                    line[llen] = '\0';
                    ui_draw_text(RGB_RIGHT_X + 14, ty, line, &font_montserrat_12, RGB_TEXT_DIM);
                    ty += 14;
                }
            } else {
                ui_draw_text(RGB_RIGHT_X + 14, RGB_INFO_Y + 6,
                             "Press Status to query", &font_montserrat_12, RGB_TEXT_DIM);
            }
        }
    }

    /* Status bar */
    if (dirty_bot > RGB_STATUS_Y) {
        ui_rect_t status_bg = {0, RGB_STATUS_Y, UI_SCREEN_WIDTH, RGB_STATUS_H};
        ui_draw_fill_rect(&status_bg, RGB_STATUS_BG);
        ui_draw_text(12, RGB_STATUS_Y + 8, s_status, &font_montserrat_12, RGB_STATUS_TEXT);
    }
}

/*=============================================================================
 *  Initialization
 *=============================================================================*/

void app_rgb_init(void)
{
    ui_app_page_init(&s_app_rgb, "RGB", 0x10B);

    /* Initialize state */
    s_mode = 1;       /* Solid */
    s_r = 0; s_g = 128; s_b = 255;
    s_brightness = 200;
    s_speed = 128;
    s_info[0] = '\0';
    strcpy(s_status, "Ready");

    int widx = 0;

    /* Back button and title (from ui_app_page_t) */
    s_rgb_widgets[widx++] = (ui_widget_t *)&s_app_rgb.btn_back;
    s_rgb_widgets[widx++] = (ui_widget_t *)&s_app_rgb.lbl_title;

    /* Mode buttons (2x2 grid) */
    {
        int16_t mx = 12;
        for (int i = 0; i < RGB_NUM_MODES; i++) {
            int16_t col = i % 2;
            int16_t row = i / 2;
            ui_rect_t r = {mx + col * (RGB_MODE_BTN_W + RGB_MODE_GAP),
                           RGB_MODE_Y + row * (RGB_MODE_BTN_H + RGB_MODE_GAP),
                           RGB_MODE_BTN_W, RGB_MODE_BTN_H};
            ui_button_init(&btn_mode[i], &r, s_mode_names[i], &font_montserrat_12);
            ui_button_set_callback(&btn_mode[i], mode_btn_click);
            ui_button_set_colors(&btn_mode[i], RGB_BTN_BG, RGB_BTN_ACTIVE, RGB_TEXT);
            btn_mode[i].radius = 8;
            btn_mode[i].base.user_data = (void *)(intptr_t)i;
            s_rgb_widgets[widx++] = (ui_widget_t *)&btn_mode[i];
        }
    }

    /* R/G/B sliders */
    {
        ui_rect_t rr = {RGB_SLIDER_X, RGB_R_Y, RGB_SLIDER_W, RGB_SLIDER_H};
        ui_slider_init(&s_slider_r, &rr, 0, 255, s_r);
        ui_slider_set_callback(&s_slider_r, slider_r_change);
        s_rgb_widgets[widx++] = (ui_widget_t *)&s_slider_r;

        ui_rect_t rg = {RGB_SLIDER_X, RGB_G_Y, RGB_SLIDER_W, RGB_SLIDER_H};
        ui_slider_init(&s_slider_g, &rg, 0, 255, s_g);
        ui_slider_set_callback(&s_slider_g, slider_g_change);
        s_rgb_widgets[widx++] = (ui_widget_t *)&s_slider_g;

        ui_rect_t rb = {RGB_SLIDER_X, RGB_B_Y, RGB_SLIDER_W, RGB_SLIDER_H};
        ui_slider_init(&s_slider_b, &rb, 0, 255, s_b);
        ui_slider_set_callback(&s_slider_b, slider_b_change);
        s_rgb_widgets[widx++] = (ui_widget_t *)&s_slider_b;
    }

    /* Brightness slider */
    {
        ui_rect_t r = {RGB_RIGHT_SLIDER_X, RGB_BRIGHT_Y, RGB_RIGHT_SLIDER_W, RGB_SLIDER_H};
        ui_slider_init(&s_slider_bright, &r, 0, 255, s_brightness);
        ui_slider_set_callback(&s_slider_bright, slider_bright_change);
        s_rgb_widgets[widx++] = (ui_widget_t *)&s_slider_bright;
    }

    /* Speed slider */
    {
        ui_rect_t r = {RGB_RIGHT_SLIDER_X, RGB_SPEED_Y, RGB_RIGHT_SLIDER_W, RGB_SLIDER_H};
        ui_slider_init(&s_slider_speed, &r, 0, 255, s_speed);
        ui_slider_set_callback(&s_slider_speed, slider_speed_change);
        s_rgb_widgets[widx++] = (ui_widget_t *)&s_slider_speed;
    }

    /* Action buttons */
    {
        int16_t bx = RGB_RIGHT_X + 12;
        ui_rect_t r1 = {bx, RGB_ACT_Y, RGB_ACT_BTN_W, RGB_ACT_BTN_H};
        ui_button_init(&btn_apply, &r1, "Apply", &font_montserrat_12);
        ui_button_set_callback(&btn_apply, btn_apply_click);
        ui_button_set_colors(&btn_apply, RGB_ACCENT, UI_HEX(0x5BA8A8), UI_COLOR_WHITE);
        btn_apply.radius = 8;
        s_rgb_widgets[widx++] = (ui_widget_t *)&btn_apply;

        bx += RGB_ACT_BTN_W + RGB_ACT_GAP;
        ui_rect_t r2 = {bx, RGB_ACT_Y, RGB_ACT_BTN_W, RGB_ACT_BTN_H};
        ui_button_init(&btn_custom, &r2, "Load Custom", &font_montserrat_12);
        ui_button_set_callback(&btn_custom, btn_custom_click);
        ui_button_set_colors(&btn_custom, RGB_BTN_BG, RGB_BTN_ACTIVE, RGB_TEXT);
        btn_custom.radius = 8;
        s_rgb_widgets[widx++] = (ui_widget_t *)&btn_custom;

        bx += RGB_ACT_BTN_W + RGB_ACT_GAP;
        ui_rect_t r3 = {bx, RGB_ACT_Y, RGB_ACT_BTN_W, RGB_ACT_BTN_H};
        ui_button_init(&btn_status, &r3, "Status", &font_montserrat_12);
        ui_button_set_callback(&btn_status, btn_status_click);
        ui_button_set_colors(&btn_status, RGB_BTN_BG, RGB_BTN_ACTIVE, RGB_TEXT);
        btn_status.radius = 8;
        s_rgb_widgets[widx++] = (ui_widget_t *)&btn_status;
    }

    /* Preset color buttons */
    {
        int16_t px = RGB_RIGHT_X + 12;
        for (int i = 0; i < (int)RGB_NUM_PRESETS; i++) {
            ui_rect_t r = {px + i * (RGB_PRESET_SIZE + RGB_PRESET_GAP),
                           RGB_PRESET_Y, RGB_PRESET_SIZE, RGB_PRESET_SIZE};
            ui_widget_init(&s_preset_btns[i], &r);
            s_preset_btns[i].bg_color = rgb_to_565(s_presets[i][0], s_presets[i][1], s_presets[i][2]);
            s_preset_btns[i].event_cb = preset_touch_event;
            s_preset_btns[i].user_data = (void *)(intptr_t)i;
            s_preset_btns[i].flags |= UI_WIDGET_FLAG_OPAQUE;
            s_rgb_widgets[widx++] = &s_preset_btns[i];
        }
    }

    ui_page_set_widgets(&s_app_rgb.page, s_rgb_widgets, (uint16_t)widx);
    ui_page_set_callbacks(&s_app_rgb.page, rgb_page_enter, rgb_page_exit, rgb_page_draw, NULL);
    ui_page_register(&s_app_rgb.page);
}

ui_page_t *app_rgb_get_page(void)
{
    return &s_app_rgb.page;
}
