/********************************** (C) COPYRIGHT *******************************
* File Name          : app_music.c
* Author             : LCD Model Team
* Version            : V3.0.0
* Date               : 2026/05/23
* Description        : Music Player app (V3.0 CLI passthrough).
*                      All controls via CLI: play/pause/resume/stop/vol.
*                      Reads music state from Core via g_disp_state.
********************************************************************************/
#include "app_music.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 *  Layout Configuration (800x480)
 *=============================================================================*/

#define DISC_SIZE           180
#define DISC_X              ((UI_SCREEN_WIDTH - DISC_SIZE) / 2)
#define DISC_Y              (APP_TITLE_BAR_H + 20)
#define INFO_Y              (DISC_Y + DISC_SIZE + 20)
#define INFO_H              50
#define PROG_Y              (INFO_Y + INFO_H + 10)
#define PROG_X              60
#define PROG_W              (UI_SCREEN_WIDTH - 120)
#define PROG_H              20
#define TIME_Y              (PROG_Y + PROG_H + 4)
#define CTRL_Y              (TIME_Y + 24)
#define CTRL_BTN_W          64
#define CTRL_BTN_H          48
#define CTRL_GAP            16
#define CTRL_TOTAL_W        (5 * CTRL_BTN_W + 4 * CTRL_GAP)
#define CTRL_X              ((UI_SCREEN_WIDTH - CTRL_TOTAL_W) / 2)
#define VOL_Y               (CTRL_Y + CTRL_BTN_H + 20)
#define VOL_X               80
#define VOL_W               (UI_SCREEN_WIDTH - 160)
#define VOL_H               24

/*=============================================================================
 *  Colors
 *=============================================================================*/

#define MUSIC_BG            UI_HEX(0x1A1A2E)
#define MUSIC_DISC_OUTER    UI_HEX(0x16213E)
#define MUSIC_DISC_INNER    UI_HEX(0x0F3460)
#define MUSIC_DISC_CENTER   UI_HEX(0x1A1A2E)
#define MUSIC_ACCENT        UI_HEX(0x7EC8C8)
#define MUSIC_TEXT          UI_HEX(0xE0E0E0)
#define MUSIC_TEXT_DIM      UI_HEX(0x808080)
#define MUSIC_CTRL_BG       UI_HEX(0x16213E)
#define MUSIC_VOL_BG        UI_HEX(0x16213E)
#define MUSIC_VOL_FILL      UI_HEX(0x7EC8C8)

/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_app_music;
static ui_button_t btn_play, btn_prev, btn_next, btn_stop, btn_mode;
static ui_widget_t s_vol_touch;
static ui_widget_t *s_music_widgets[8];
static char s_track_name[68];
static char s_time_pos[12];
static char s_time_dur[12];
static char s_vol_text[8];
static char s_status_text[32];
static uint8_t s_prev_state = 0xFF;
static uint8_t s_prev_vol = 0xFF;
static uint8_t s_current_mode = 0;

/* 本地位置估算：播放时每秒自增，收到 Core 状态更新时校准 */
static uint32_t s_local_pos_ms = 0;
static uint32_t s_last_tick_ms = 0;
static bool s_local_tracking = false;

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void format_time_ms(uint32_t ms, char *buf)
{
    uint32_t sec = ms / 1000;
    snprintf(buf, 12, "%02u:%02u", (unsigned)(sec / 60), (unsigned)(sec % 60));
}

static void music_update_texts(void)
{
    if (g_disp_state.music_track[0] != '\0') {
        strncpy(s_track_name, g_disp_state.music_track, sizeof(s_track_name) - 1);
        s_track_name[sizeof(s_track_name) - 1] = '\0';
    } else {
        strcpy(s_track_name, "No track");
    }
    /* 使用本地估算位置，若有 Core 更新则校准 */
    uint32_t display_pos = s_local_tracking ? s_local_pos_ms : g_disp_state.music_pos_ms;
    format_time_ms(display_pos, s_time_pos);
    format_time_ms(g_disp_state.music_dur_ms, s_time_dur);
    snprintf(s_vol_text, sizeof(s_vol_text), "%d%%", g_disp_state.music_volume);
    switch (g_disp_state.music_state) {
    case MUSIC_STATE_IDLE:    strcpy(s_status_text, "Idle"); break;
    case MUSIC_STATE_PLAYING: strcpy(s_status_text, "Playing"); break;
    case MUSIC_STATE_PAUSED:  strcpy(s_status_text, "Paused"); break;
    case MUSIC_STATE_STOPPED: strcpy(s_status_text, "Stopped"); break;
    default:                  strcpy(s_status_text, "---"); break;
    }
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

static void music_draw_disc(void)
{
    int16_t cx = DISC_X + DISC_SIZE / 2;
    int16_t cy = DISC_Y + DISC_SIZE / 2;
    int16_t r_outer = DISC_SIZE / 2;

    ui_draw_fill_circle(cx, cy, r_outer, MUSIC_DISC_OUTER);
    ui_draw_fill_circle(cx, cy, r_outer - 30, MUSIC_DISC_INNER);
    ui_draw_fill_circle(cx, cy, 20, MUSIC_DISC_CENTER);
    ui_draw_circle_border(cx, cy, r_outer - 5, MUSIC_ACCENT, 1);
    ui_draw_text(cx - 6, cy - 8, "*", &font_montserrat_16, MUSIC_ACCENT);
}

static void music_draw_info(void)
{
    ui_rect_t info_rect = {40, INFO_Y, UI_SCREEN_WIDTH - 80, INFO_H};
    ui_draw_text_in_rect(&info_rect, s_track_name, &font_montserrat_16, MUSIC_TEXT, 0x11);
    ui_rect_t status_rect = {40, INFO_Y + 26, UI_SCREEN_WIDTH - 80, 20};
    ui_draw_text_in_rect(&status_rect, s_status_text, &font_montserrat_12, MUSIC_TEXT_DIM, 0x11);
}

static void music_draw_progress(void)
{
    ui_rect_t prog_bg = {PROG_X, PROG_Y, PROG_W, PROG_H};
    ui_draw_fill_round_rect(&prog_bg, 4, MUSIC_VOL_BG);

    uint32_t dur = g_disp_state.music_dur_ms;
    uint32_t pos = s_local_tracking ? s_local_pos_ms : g_disp_state.music_pos_ms;
    if (dur > 0 && pos <= dur) {
        int16_t fill_w = (int16_t)((uint32_t)PROG_W * pos / dur);
        if (fill_w > 0) {
            ui_rect_t pf = {PROG_X, PROG_Y, fill_w, PROG_H};
            ui_draw_fill_round_rect(&pf, 4, MUSIC_VOL_FILL);
        }
    }

    ui_draw_text(PROG_X, TIME_Y, s_time_pos, &font_montserrat_12, MUSIC_TEXT_DIM);
    int16_t dw = ui_text_width(s_time_dur, &font_montserrat_12);
    ui_draw_text(PROG_X + PROG_W - dw, TIME_Y, s_time_dur, &font_montserrat_12, MUSIC_TEXT_DIM);
}

static void music_draw_volume(void)
{
    ui_rect_t vol_bg = {VOL_X, VOL_Y + 4, VOL_W, VOL_H - 8};
    ui_draw_fill_round_rect(&vol_bg, 4, MUSIC_VOL_BG);

    int16_t fill_w = (int16_t)((uint32_t)VOL_W * g_disp_state.music_volume / 100);
    if (fill_w > 0) {
        ui_rect_t vf = {VOL_X, VOL_Y + 4, fill_w, VOL_H - 8};
        ui_draw_fill_round_rect(&vf, 4, MUSIC_VOL_FILL);
    }

    ui_draw_text(VOL_X - 40, VOL_Y + 4, "Vol", &font_montserrat_12, MUSIC_TEXT_DIM);
    ui_draw_text(VOL_X + VOL_W + 8, VOL_Y + 4, s_vol_text, &font_montserrat_12, MUSIC_TEXT_DIM);
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void music_page_enter(ui_page_t *page)
{
    (void)page;
    s_prev_state = 0xFF;
    s_prev_vol = 0xFF;
    s_local_tracking = false;
    s_local_pos_ms = g_disp_state.music_pos_ms;
    s_last_tick_ms = 0;
    music_update_texts();
    ui_page_invalidate_all();
}

static void music_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page; (void)dirty;
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);
    ui_rect_t bg = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bg, MUSIC_BG);
    music_draw_disc();
    music_draw_info();
    music_draw_progress();
    music_draw_volume();
}

static void music_page_update(ui_page_t *page)
{
    (void)page;
    bool changed = false;

    /* 本地位置估算：播放中每 100ms 自增 */
    if (g_disp_state.music_state == MUSIC_STATE_PLAYING) {
        uint32_t now = ui_get_real_ms();
        if (s_last_tick_ms > 0 && now > s_last_tick_ms) {
            uint32_t dt = now - s_last_tick_ms;
            if (dt >= 100) {
                s_local_pos_ms += dt;
                s_last_tick_ms = now;
                /* 上限保护 */
                if (g_disp_state.music_dur_ms > 0 &&
                    s_local_pos_ms > g_disp_state.music_dur_ms)
                    s_local_pos_ms = g_disp_state.music_dur_ms;
                changed = true;
            }
        } else {
            s_last_tick_ms = now;
        }
        if (!s_local_tracking) {
            s_local_tracking = true;
            s_local_pos_ms = g_disp_state.music_pos_ms;
            s_last_tick_ms = ui_get_real_ms();
            changed = true;
        }
    } else {
        if (s_local_tracking) {
            s_local_tracking = false;
            s_local_pos_ms = g_disp_state.music_pos_ms;
            changed = true;
        }
        s_last_tick_ms = 0;
    }

    /* Core 状态变化时校准本地位置 */
    if (g_disp_state.music_state != s_prev_state) {
        s_prev_state = g_disp_state.music_state;
        s_local_pos_ms = g_disp_state.music_pos_ms;
        changed = true;
    }
    if (g_disp_state.music_volume != s_prev_vol) {
        s_prev_vol = g_disp_state.music_volume;
        changed = true;
    }
    if (changed) {
        music_update_texts();
        ui_page_invalidate_all();
    }
}

/*=============================================================================
 *  Control Button Callbacks
 *=============================================================================*/

static void btn_play_click(ui_widget_t *w)
{
    (void)w;
    if (g_disp_state.music_state == MUSIC_STATE_PLAYING)
        UART_SendMusicControl(MUSIC_CTRL_PAUSE, 0);
    else
        UART_SendMusicControl(MUSIC_CTRL_PLAY, 0);
}

static void btn_prev_click(ui_widget_t *w)
{
    (void)w;
    UART_SendCLI("prev");
}

static void btn_next_click(ui_widget_t *w)
{
    (void)w;
    UART_SendCLI("next");
}

static void btn_stop_click(ui_widget_t *w)
{
    (void)w;
    UART_SendMusicControl(MUSIC_CTRL_STOP, 0);
}

static void btn_mode_click(ui_widget_t *w)
{
    (void)w;
    s_current_mode = (s_current_mode + 1) % 3;
    /* Play mode (single/repeat/shuffle) is managed on Display side only */
    static const char *mode_names[] = { "Single", "Repeat", "Shuffle" };
    btn_mode.text = mode_names[s_current_mode];
    btn_mode.base.flags |= UI_WIDGET_FLAG_DIRTY;
}

static void vol_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type != UI_EVENT_CLICK && e->type != UI_EVENT_DOWN && e->type != UI_EVENT_MOVE)
        return;
    int16_t rel_x = e->pos.x - VOL_X;
    if (rel_x < 0) rel_x = 0;
    if (rel_x > VOL_W) rel_x = VOL_W;
    UART_SendVolumeControl(0x00, (uint8_t)((uint32_t)rel_x * 100 / VOL_W));
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void app_music_init(void)
{
    ui_app_page_init(&s_app_music, "Music", 0x100);
    int16_t bx = CTRL_X;

    ui_rect_t r_mode = {bx, CTRL_Y, CTRL_BTN_W, CTRL_BTN_H};
    ui_button_init(&btn_mode, &r_mode, "Mode", &font_montserrat_12);
    ui_button_set_callback(&btn_mode, btn_mode_click);
    ui_button_set_colors(&btn_mode, MUSIC_CTRL_BG, MUSIC_ACCENT, MUSIC_TEXT);
    btn_mode.radius = 12;

    bx += CTRL_BTN_W + CTRL_GAP;
    ui_rect_t r_prev = {bx, CTRL_Y, CTRL_BTN_W, CTRL_BTN_H};
    ui_button_init(&btn_prev, &r_prev, "<<", &font_montserrat_16);
    ui_button_set_callback(&btn_prev, btn_prev_click);
    ui_button_set_colors(&btn_prev, MUSIC_CTRL_BG, MUSIC_ACCENT, MUSIC_TEXT);
    btn_prev.radius = 12;

    bx += CTRL_BTN_W + CTRL_GAP;
    ui_rect_t r_play = {bx, CTRL_Y, CTRL_BTN_W, CTRL_BTN_H};
    ui_button_init(&btn_play, &r_play, ">/||", &font_montserrat_12);
    ui_button_set_callback(&btn_play, btn_play_click);
    ui_button_set_colors(&btn_play, MUSIC_ACCENT, UI_COLOR_SECONDARY, MUSIC_BG);
    btn_play.radius = 24;

    bx += CTRL_BTN_W + CTRL_GAP;
    ui_rect_t r_next = {bx, CTRL_Y, CTRL_BTN_W, CTRL_BTN_H};
    ui_button_init(&btn_next, &r_next, ">>", &font_montserrat_16);
    ui_button_set_callback(&btn_next, btn_next_click);
    ui_button_set_colors(&btn_next, MUSIC_CTRL_BG, MUSIC_ACCENT, MUSIC_TEXT);
    btn_next.radius = 12;

    bx += CTRL_BTN_W + CTRL_GAP;
    ui_rect_t r_stop = {bx, CTRL_Y, CTRL_BTN_W, CTRL_BTN_H};
    ui_button_init(&btn_stop, &r_stop, "Stop", &font_montserrat_12);
    ui_button_set_callback(&btn_stop, btn_stop_click);
    ui_button_set_colors(&btn_stop, MUSIC_CTRL_BG, MUSIC_ACCENT, MUSIC_TEXT);
    btn_stop.radius = 12;

    ui_rect_t vol_rect = {VOL_X, VOL_Y, VOL_W, VOL_H};
    ui_widget_init(&s_vol_touch, &vol_rect);
    s_vol_touch.bg_color = UI_COLOR_TRANSPARENT;
    s_vol_touch.event_cb = vol_touch_event;

    s_music_widgets[0] = (ui_widget_t *)&s_app_music.btn_back;
    s_music_widgets[1] = (ui_widget_t *)&s_app_music.lbl_title;
    s_music_widgets[2] = (ui_widget_t *)&btn_mode;
    s_music_widgets[3] = (ui_widget_t *)&btn_prev;
    s_music_widgets[4] = (ui_widget_t *)&btn_play;
    s_music_widgets[5] = (ui_widget_t *)&btn_next;
    s_music_widgets[6] = (ui_widget_t *)&btn_stop;
    s_music_widgets[7] = &s_vol_touch;

    ui_page_set_widgets(&s_app_music.page, s_music_widgets, 8);
    ui_page_set_callbacks(&s_app_music.page, music_page_enter, NULL, music_page_draw, NULL);
    ui_page_set_update_cb(&s_app_music.page, music_page_update);
    ui_page_register(&s_app_music.page);
    music_update_texts();
}

ui_page_t *app_music_get_page(void) { return &s_app_music.page; }
