/********************************** (C) COPYRIGHT *******************************
* File Name          : app_music.c
* Author             : E-ink Model Team (ported from Display-1 V3.3)
* Version            : V1.0.0
* Date               : 2026/07/19
* Description        : Music Player app for E-ink (1bpp).
*                      CLI passthrough + local playlist, prev/next via local
*                      playlist, play mode managed locally, speaker toggle.
*
*                      E-ink adaptations:
*                        - 648x480 layout, white background (cheap refresh)
*                        - 1bpp palette: black strokes/fills on white
*                        - position tracking uses real dt (TIM2 time base)
*                        - playlist capped at 32 entries to save RAM
********************************************************************************/
#include "app_music.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include "../MiniUI/font/font_montserrat_12.h"
#include "../MiniUI/font/font_montserrat_16.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*=============================================================================
 *  Layout Configuration (648x480)
 *
 *  Left area (0..399):   Disc, track info, progress, controls, volume
 *  Right area (400..647): Playlist panel (scrollable)
 *=============================================================================*/

#define LEFT_W              400

/* Disc */
#define DISC_SIZE           140
#define DISC_X              ((LEFT_W - DISC_SIZE) / 2)
#define DISC_Y              (APP_TITLE_BAR_H + 12)

/* Track info */
#define INFO_Y              (DISC_Y + DISC_SIZE + 10)
#define INFO_H              44

/* Progress bar */
#define PROG_Y              (INFO_Y + INFO_H + 6)
#define PROG_X              32
#define PROG_W              (LEFT_W - 64)
#define PROG_H              14
#define TIME_Y              (PROG_Y + PROG_H + 2)

/* Control buttons */
#define CTRL_Y              (TIME_Y + 20)
#define CTRL_BTN_W          56
#define CTRL_BTN_H          40
#define CTRL_GAP            10
#define CTRL_TOTAL_W        (5 * CTRL_BTN_W + 4 * CTRL_GAP)
#define CTRL_X              ((LEFT_W - CTRL_TOTAL_W) / 2)

/* Volume */
#define VOL_Y               (CTRL_Y + CTRL_BTN_H + 12)
#define VOL_X               56
#define VOL_W               (LEFT_W - 112)
#define VOL_H               20

/* Speaker & Refresh row */
#define SPK_Y               (VOL_Y + VOL_H + 10)
#define SPK_BTN_W           84
#define SPK_BTN_H           34
#define SPK_BTN_GAP         16
#define SPK_BTN_X           ((LEFT_W - 2 * SPK_BTN_W - SPK_BTN_GAP) / 2)

/* Right-side playlist panel */
#define PL_X                LEFT_W
#define PL_Y                APP_TITLE_BAR_H
#define PL_W                (UI_SCREEN_WIDTH - LEFT_W)
#define PL_H                (UI_SCREEN_HEIGHT - APP_TITLE_BAR_H)
#define PL_ITEM_H           30
#define PL_HEADER_H         26
#define PL_VISIBLE          ((PL_H - PL_HEADER_H) / PL_ITEM_H)

/*=============================================================================
 *  Colors (1bpp)
 *=============================================================================*/

#define MUSIC_FG            UI_COLOR_BLACK
#define MUSIC_BG            UI_COLOR_WHITE

/*=============================================================================
 *  Playlist & Play Mode
 *=============================================================================*/

#define MUSIC_PLAYLIST_MAX  32      /* RAM discipline: 32*64 = 2 KB */
#define MUSIC_NAME_LEN      64

/* Play modes — managed locally, no Core command needed */
#define PLAY_MODE_SINGLE    0   /* Play one track, stop at end */
#define PLAY_MODE_REPEAT    1   /* Repeat all (loop playlist) */
#define PLAY_MODE_SINGLE_LOOP 2 /* Repeat current track */

typedef struct {
    char   names[MUSIC_PLAYLIST_MAX][MUSIC_NAME_LEN];
    int16_t count;
    int16_t current;      /* Index of currently playing track, -1 if none */
    uint8_t mode;         /* PLAY_MODE_SINGLE / REPEAT / SINGLE_LOOP */
    int16_t scroll;       /* Scroll offset for playlist */
} music_playlist_t;

static music_playlist_t s_playlist;

/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_app_music;
static ui_button_t btn_play, btn_prev, btn_next, btn_stop, btn_mode;
static ui_button_t btn_speaker, btn_refresh;
static ui_widget_t s_vol_touch;
static ui_widget_t s_pl_touch;  /* Playlist touch area */
static ui_widget_t *s_music_widgets[11];
static char s_track_name[68];
static char s_time_pos[12];
static char s_time_dur[12];
static char s_vol_text[8];
static char s_status_text[32];
static uint8_t s_prev_state = 0xFF;
static uint8_t s_prev_vol = 0xFF;
static uint32_t s_prev_dur = 0xFFFFFFFF;

/* Speaker state: true = external speaker ON, false = headphone only */
static bool s_speaker_on = false;

/* Local position estimation: advanced by real dt from the UI tick */
static uint32_t s_local_pos_ms = 0;
static bool s_local_tracking = false;

/* Track-end detection flag — set when position reaches duration */
static bool s_track_ended = false;

/* Flag: skip playst on enter because play command was just sent */
static bool s_skip_playst = false;

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void music_on_cli_complete(const char *output, uint16_t len, const char *tag);
static void music_ensure_current_visible(void);

static uart_cli_cb_t s_music_cb = {
    .on_cli_complete = music_on_cli_complete,
};

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void format_time_ms(uint32_t ms, char *buf)
{
    uint32_t sec = ms / 1000;
    snprintf(buf, 12, "%02u:%02u", (unsigned)(sec / 60), (unsigned)(sec % 60));
}

static bool music_has_prev(void)
{
    return s_playlist.count > 1 && s_playlist.current > 0;
}

static bool music_has_next(void)
{
    return s_playlist.count > 1 && s_playlist.current >= 0 &&
           s_playlist.current < s_playlist.count - 1;
}

/* Play a track from the playlist by index */
static void music_play_index(int16_t index)
{
    if (index < 0 || index >= s_playlist.count) return;
    s_playlist.current = index;
    s_track_ended = false;
    s_local_pos_ms = 0;
    s_local_tracking = true;

    strncpy(s_track_name, s_playlist.names[index], sizeof(s_track_name) - 1);
    s_track_name[sizeof(s_track_name) - 1] = '\0';
    strcpy(s_time_pos, "00:00");
    strcpy(s_status_text, "Playing");

    btn_play.text = "||";
    btn_play.base.flags |= UI_WIDGET_FLAG_DIRTY;

    music_ensure_current_visible();
    ui_page_invalidate_all();

    UART_SendPlayMusic(s_playlist.names[index]);
}

/* Called when current track ends (position reaches duration) */
static void music_on_track_end(void)
{
    s_track_ended = true;
    switch (s_playlist.mode) {
    case PLAY_MODE_SINGLE_LOOP:
        if (s_playlist.current >= 0)
            music_play_index(s_playlist.current);
        break;
    case PLAY_MODE_REPEAT:
        if (s_playlist.count > 0) {
            int16_t next = s_playlist.current + 1;
            if (next >= s_playlist.count) next = 0;
            music_play_index(next);
        }
        break;
    case PLAY_MODE_SINGLE:
    default:
        break;
    }
}

/* Ensure current track is visible in playlist scroll */
static void music_ensure_current_visible(void)
{
    if (s_playlist.current < 0) return;
    if (s_playlist.current < s_playlist.scroll)
        s_playlist.scroll = s_playlist.current;
    else if (s_playlist.current >= s_playlist.scroll + PL_VISIBLE)
        s_playlist.scroll = s_playlist.current - PL_VISIBLE + 1;
}

static void music_update_texts(void)
{
    if (s_playlist.current >= 0 && s_playlist.current < s_playlist.count) {
        strncpy(s_track_name, s_playlist.names[s_playlist.current], sizeof(s_track_name) - 1);
        s_track_name[sizeof(s_track_name) - 1] = '\0';
    } else if (g_disp_state.music_track[0] != '\0') {
        strncpy(s_track_name, g_disp_state.music_track, sizeof(s_track_name) - 1);
        s_track_name[sizeof(s_track_name) - 1] = '\0';
    } else {
        strcpy(s_track_name, "No track");
    }
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

    if (g_disp_state.music_state == MUSIC_STATE_PLAYING) {
        btn_play.text = "||";   /* pause icon while playing */
    } else {
        btn_play.text = ">";    /* play icon otherwise */
    }
    btn_play.base.flags |= UI_WIDGET_FLAG_DIRTY;

    /* 外放状态由 Core 二进制 MUSIC_STATUS 驱动同步 */
    s_speaker_on = (g_disp_state.music_speaker != 0);
    btn_speaker.text = s_speaker_on ? "Spk ON" : "Spk OFF";
    btn_speaker.base.flags |= UI_WIDGET_FLAG_DIRTY;

    switch (s_playlist.mode) {
    case PLAY_MODE_SINGLE:      btn_mode.text = "1"; break;
    case PLAY_MODE_REPEAT:      btn_mode.text = "Rep"; break;
    case PLAY_MODE_SINGLE_LOOP: btn_mode.text = "1R"; break;
    default:                    btn_mode.text = "1"; break;
    }
    btn_mode.base.flags |= UI_WIDGET_FLAG_DIRTY;
}

/*=============================================================================
 *  CLI Response Callback (music-specific)
 *=============================================================================*/

static void music_on_cli_complete(const char *output, uint16_t len, const char *tag)
{
    if (!output || len == 0) return;

    if (strcmp(UART_GetLastCLITag(), "playst") == 0) {
        const char *p;

        p = strstr(output, "State:");
        if (p) {
            p += 6;
            while (*p == ' ') p++;
            if (strncmp(p, "Playing", 7) == 0)      g_disp_state.music_state = MUSIC_STATE_PLAYING;
            else if (strncmp(p, "Paused", 6) == 0)   g_disp_state.music_state = MUSIC_STATE_PAUSED;
            else if (strncmp(p, "Stopped", 7) == 0)  g_disp_state.music_state = MUSIC_STATE_STOPPED;
            else                                     g_disp_state.music_state = MUSIC_STATE_IDLE;
        }

        p = strstr(output, "Track:");
        if (p) {
            p += 6;
            while (*p == ' ') p++;
            const char *eol = strchr(p, '\r');
            if (!eol) eol = strchr(p, '\n');
            uint16_t tlen = eol ? (uint16_t)(eol - p) : (uint16_t)strlen(p);
            if (tlen > sizeof(g_disp_state.music_track) - 1)
                tlen = sizeof(g_disp_state.music_track) - 1;
            if (tlen > 0 && !(tlen == 6 && strncmp(p, "(none)", 6) == 0)) {
                memcpy(g_disp_state.music_track, p, tlen);
                g_disp_state.music_track[tlen] = '\0';
            }
        }

        p = strstr(output, "Time:");
        if (p) {
            p += 5;
            while (*p == ' ') p++;
            unsigned long mm = 0, ss = 0;
            char *ep;
            mm = strtoul(p, &ep, 10);
            if (ep && *ep == ':') {
                ss = strtoul(ep + 1, NULL, 10);
            }
            g_disp_state.music_pos_ms = (uint32_t)(mm * 60 + ss) * 1000;
            s_local_pos_ms = g_disp_state.music_pos_ms;
            s_local_tracking = true;
        }

        p = strstr(output, "Volume:");
        if (p) {
            p += 7;
            while (*p == ' ') p++;
            g_disp_state.music_volume = (uint8_t)strtoul(p, NULL, 10);
        }

        p = strstr(output, "Speaker:");
        if (p) {
            const char *l_pos = strstr(p, "L=");
            const char *r_pos = strstr(p, "R=");
            bool l_on = false, r_on = false;
            if (l_pos && l_pos[2] == 'O' && l_pos[3] == 'N') l_on = true;
            if (r_pos && r_pos[2] == 'O' && r_pos[3] == 'N') r_on = true;
            /* 写入共享状态，由 music_update_texts 统一刷新按钮 */
            g_disp_state.music_speaker = (l_on || r_on) ? 1 : 0;
        }

        music_update_texts();
        ui_page_invalidate_all();
    }
}

/*=============================================================================
 *  Drawing (1bpp)
 *=============================================================================*/

static void music_draw_disc(void)
{
    int16_t cx = DISC_X + DISC_SIZE / 2;
    int16_t cy = DISC_Y + DISC_SIZE / 2;
    int16_t r_outer = DISC_SIZE / 2;

    /* Vinyl: black outer ring, white body, black center */
    ui_draw_fill_circle(cx, cy, r_outer, MUSIC_FG);
    ui_draw_fill_circle(cx, cy, r_outer - 6, MUSIC_BG);
    ui_draw_circle_border(cx, cy, r_outer - 24, MUSIC_FG, 1);
    ui_draw_circle_border(cx, cy, r_outer - 40, MUSIC_FG, 1);
    ui_draw_fill_circle(cx, cy, 16, MUSIC_FG);
    ui_draw_fill_circle(cx, cy, 5, MUSIC_BG);
}

static void music_draw_info(void)
{
    ui_rect_t info_rect = {16, INFO_Y, LEFT_W - 32, INFO_H};
    ui_draw_text_in_rect(&info_rect, s_track_name, &font_montserrat_16, MUSIC_FG, 1);
    ui_rect_t status_rect = {16, INFO_Y + 24, LEFT_W - 32, 20};
    ui_draw_text_in_rect(&status_rect, s_status_text, &font_montserrat_12, MUSIC_FG, 1);
}

static void music_draw_progress(void)
{
    ui_rect_t prog_bg = {PROG_X, PROG_Y, PROG_W, PROG_H};
    ui_draw_round_rect_border(&prog_bg, 4, MUSIC_FG, 1);

    uint32_t dur = g_disp_state.music_dur_ms;
    uint32_t pos = s_local_tracking ? s_local_pos_ms : g_disp_state.music_pos_ms;
    if (dur > 0 && pos <= dur) {
        int16_t fill_w = (int16_t)((uint32_t)(PROG_W - 2) * pos / dur);
        if (fill_w > 0) {
            ui_rect_t pf = {PROG_X + 1, PROG_Y + 1, fill_w, PROG_H - 2};
            ui_draw_fill_round_rect(&pf, 3, MUSIC_FG);
        }
    }

    ui_draw_text(PROG_X, TIME_Y, s_time_pos, &font_montserrat_12, MUSIC_FG);
    int16_t dw = ui_text_width(s_time_dur, &font_montserrat_12);
    ui_draw_text(PROG_X + PROG_W - dw, TIME_Y, s_time_dur, &font_montserrat_12, MUSIC_FG);
}

static void music_draw_volume(void)
{
    ui_rect_t vol_bg = {VOL_X, VOL_Y + 4, VOL_W, VOL_H - 8};
    ui_draw_round_rect_border(&vol_bg, 4, MUSIC_FG, 1);

    int16_t fill_w = (int16_t)((uint32_t)(VOL_W - 2) * g_disp_state.music_volume / 100);
    if (fill_w > 0) {
        ui_rect_t vf = {VOL_X + 1, VOL_Y + 5, fill_w, VOL_H - 10};
        ui_draw_fill_rect(&vf, MUSIC_FG);
    }

    ui_draw_text(VOL_X - 36, VOL_Y + 4, "Vol", &font_montserrat_12, MUSIC_FG);
    ui_draw_text(VOL_X + VOL_W + 6, VOL_Y + 4, s_vol_text, &font_montserrat_12, MUSIC_FG);
}

static void music_draw_speaker_row(void)
{
    ui_draw_text(SPK_BTN_X - 36, SPK_Y + 10, "Out", &font_montserrat_12, MUSIC_FG);
}

static void music_draw_playlist(void)
{
    if (s_playlist.count == 0) return;

    /* Panel background */
    ui_rect_t bg = {PL_X, PL_Y, PL_W, PL_H};
    ui_draw_fill_rect(&bg, MUSIC_BG);

    /* Left border line */
    ui_draw_vline(PL_X, PL_Y, PL_H, MUSIC_FG);

    /* Header */
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "Playlist (%d)", s_playlist.count);
    ui_draw_text(PL_X + 10, PL_Y + 6, hdr, &font_montserrat_12, MUSIC_FG);
    ui_draw_hline(PL_X, PL_Y + PL_HEADER_H, PL_W, MUSIC_FG);

    /* Items */
    for (int16_t i = 0; i < PL_VISIBLE && (i + s_playlist.scroll) < s_playlist.count; i++) {
        int16_t idx = i + s_playlist.scroll;
        int16_t iy = PL_Y + PL_HEADER_H + i * PL_ITEM_H;
        ui_rect_t item_rect = {PL_X + 4, iy + 1, PL_W - 8, PL_ITEM_H - 2};

        if (idx == s_playlist.current) {
            /* Current track: inverted (black fill, white text) */
            ui_draw_fill_round_rect(&item_rect, 4, MUSIC_FG);
            ui_draw_text(PL_X + 10, iy + 8, ">", &font_montserrat_12, MUSIC_BG);
            ui_draw_text(PL_X + 24, iy + 8, s_playlist.names[idx],
                         &font_montserrat_12, MUSIC_BG);
        } else {
            ui_draw_text(PL_X + 24, iy + 8, s_playlist.names[idx],
                         &font_montserrat_12, MUSIC_FG);
        }

        /* Separator line */
        ui_draw_hline(PL_X + 8, iy + PL_ITEM_H - 1, PL_W - 16, MUSIC_FG);
    }

    /* Scroll indicator */
    if (s_playlist.count > PL_VISIBLE) {
        int16_t scroll_area_y = PL_Y + PL_HEADER_H;
        int16_t scroll_area_h = PL_H - PL_HEADER_H;
        int16_t bar_h = scroll_area_h * PL_VISIBLE / s_playlist.count;
        if (bar_h < 20) bar_h = 20;
        int16_t bar_y = scroll_area_y + (scroll_area_h - bar_h) * s_playlist.scroll / (s_playlist.count - PL_VISIBLE);
        ui_rect_t scroll_bar = {PL_X + PL_W - 5, bar_y, 4, bar_h};
        ui_draw_fill_rect(&scroll_bar, MUSIC_FG);
    }
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void music_page_enter(ui_page_t *page)
{
    (void)page;
    s_prev_state = 0xFF;
    s_prev_vol = 0xFF;
    s_prev_dur = 0xFFFFFFFF;
    s_local_tracking = false;
    s_local_pos_ms = g_disp_state.music_pos_ms;
    s_track_ended = false;

    UART_SetCLICallbacks(&s_music_cb);

    /* Sync state from Core, unless a play command was just sent */
    if (s_skip_playst) {
        s_skip_playst = false;
    } else {
        UART_SendCLI("playst");
    }

    music_ensure_current_visible();
    music_update_texts();
    ui_page_invalidate_all();
}

static void music_page_exit(ui_page_t *page)
{
    (void)page;
    UART_ClearCLICallbacks();
}

static void music_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;
    (void)dirty;

    /* Title bar background */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Main background */
    ui_rect_t bg = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bg, MUSIC_BG);

    music_draw_disc();
    music_draw_info();
    music_draw_progress();
    music_draw_volume();
    music_draw_speaker_row();
    music_draw_playlist();
}

static void music_page_update(ui_page_t *page, uint32_t dt_ms)
{
    (void)page;

    /* --- Position tracking with real elapsed time --- */
    if (g_disp_state.music_state == MUSIC_STATE_PLAYING) {
        if (!s_local_tracking) {
            s_local_tracking = true;
            s_local_pos_ms = g_disp_state.music_pos_ms;
        }
        s_local_pos_ms += dt_ms;
        if (g_disp_state.music_dur_ms > 0 &&
            s_local_pos_ms > g_disp_state.music_dur_ms)
            s_local_pos_ms = g_disp_state.music_dur_ms;

        /* Detect track end */
        if (!s_track_ended && g_disp_state.music_dur_ms > 0 &&
            s_local_pos_ms >= g_disp_state.music_dur_ms) {
            music_on_track_end();
        }

        /* Refresh the progress area at ~1 Hz only (e-ink friendly) */
        static uint32_t s_prog_acc = 0;
        s_prog_acc += dt_ms;
        if (s_prog_acc >= 1000) {
            s_prog_acc = 0;
            music_update_texts();
            ui_rect_t prog_area = {PROG_X - 2, PROG_Y - 2,
                                   PROG_W + 4, (TIME_Y + 16) - PROG_Y + 4};
            ui_page_invalidate(&prog_area);
        }
    } else {
        if (s_local_tracking) {
            s_local_tracking = false;
            s_local_pos_ms = g_disp_state.music_pos_ms;
        }
    }

    /* State change: full redraw needed */
    if (g_disp_state.music_state != s_prev_state) {
        s_prev_state = g_disp_state.music_state;
        s_local_pos_ms = g_disp_state.music_pos_ms;
        music_update_texts();
        ui_page_invalidate_all();
        return;
    }

    /* Duration change (new track from Core): sync & full redraw */
    if (g_disp_state.music_dur_ms != s_prev_dur) {
        s_prev_dur = g_disp_state.music_dur_ms;
        s_local_pos_ms = g_disp_state.music_pos_ms;
        s_local_tracking = true;
        music_update_texts();
        ui_page_invalidate_all();
        return;
    }

    /* Volume change: only invalidate volume area */
    if (g_disp_state.music_volume != s_prev_vol) {
        s_prev_vol = g_disp_state.music_volume;
        music_update_texts();
        ui_rect_t vol_area = {VOL_X - 40, VOL_Y,
                              VOL_W + 60, VOL_H};
        ui_page_invalidate(&vol_area);
    }
}

/*=============================================================================
 *  Control Button Callbacks
 *=============================================================================*/

static void btn_play_click(ui_widget_t *w)
{
    (void)w;
    if (g_disp_state.music_state == MUSIC_STATE_PLAYING) {
        UART_SendMusicControl(MUSIC_CTRL_PAUSE, 0);
    } else if (g_disp_state.music_state == MUSIC_STATE_PAUSED) {
        UART_SendMusicControl(MUSIC_CTRL_PLAY, 0);  /* resume */
    } else {
        /* STOPPED or IDLE: play current track from playlist */
        if (s_playlist.current >= 0 && s_playlist.current < s_playlist.count)
            music_play_index(s_playlist.current);
    }
}

static void btn_prev_click(ui_widget_t *w)
{
    (void)w;
    if (music_has_prev())
        music_play_index(s_playlist.current - 1);
}

static void btn_next_click(ui_widget_t *w)
{
    (void)w;
    if (music_has_next())
        music_play_index(s_playlist.current + 1);
}

static void btn_stop_click(ui_widget_t *w)
{
    (void)w;
    UART_SendMusicControl(MUSIC_CTRL_STOP, 0);
}

static void btn_mode_click(ui_widget_t *w)
{
    (void)w;
    s_playlist.mode = (s_playlist.mode + 1) % 3;
    music_update_texts();
}

static void btn_speaker_click(ui_widget_t *w)
{
    (void)w;
    s_speaker_on = !s_speaker_on;
    g_disp_state.music_speaker = s_speaker_on ? 1 : 0;  /* 乐观更新，Core 推送会回正 */
    btn_speaker.text = s_speaker_on ? "Spk ON" : "Spk OFF";
    btn_speaker.base.flags |= UI_WIDGET_FLAG_DIRTY;
    UART_SendCLI(s_speaker_on ? "speaker on" : "speaker off");
}

static void btn_refresh_click(ui_widget_t *w)
{
    (void)w;
    UART_SendCLI("playst");
}

/* Volume bar: tap to set (touchpad generates taps, not drags) */
static void vol_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;

    if (e->type != UI_EVENT_CLICK && e->type != UI_EVENT_TOUCH_DOWN) return;

    int16_t rel_x = e->touch.x - VOL_X;
    if (rel_x < 0) rel_x = 0;
    if (rel_x > VOL_W) rel_x = VOL_W;
    UART_SendVolumeControl(0x00, (uint8_t)((uint32_t)rel_x * 100 / VOL_W));
}

/* Playlist touch: tap to select & play, swipe to scroll */
static void pl_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type == UI_EVENT_SWIPE_UP) {
        int16_t max_scroll = s_playlist.count - PL_VISIBLE;
        if (max_scroll < 0) max_scroll = 0;
        s_playlist.scroll += 2;
        if (s_playlist.scroll > max_scroll) s_playlist.scroll = max_scroll;
        ui_page_invalidate_all();
        return;
    }
    if (e->type == UI_EVENT_SWIPE_DOWN) {
        s_playlist.scroll -= 2;
        if (s_playlist.scroll < 0) s_playlist.scroll = 0;
        ui_page_invalidate_all();
        return;
    }
    /* Mouse wheel scrolling */
    if (e->type == UI_EVENT_MOVE && e->scroll_delta != 0) {
        int16_t max_scroll = s_playlist.count - PL_VISIBLE;
        if (max_scroll < 0) max_scroll = 0;
        s_playlist.scroll -= e->scroll_delta;
        if (s_playlist.scroll > max_scroll) s_playlist.scroll = max_scroll;
        if (s_playlist.scroll < 0) s_playlist.scroll = 0;
        ui_page_invalidate_all();
        return;
    }
    if (e->type == UI_EVENT_CLICK) {
        int16_t rel_y = e->touch.y - (PL_Y + PL_HEADER_H);
        if (rel_y < 0) return;
        int16_t idx = rel_y / PL_ITEM_H + s_playlist.scroll;
        if (idx >= 0 && idx < s_playlist.count) {
            music_play_index(idx);
        }
    }
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void app_music_init(void)
{
    ui_app_page_init(&s_app_music, "Music", 0x100);
    int16_t bx = CTRL_X;

    ui_rect_t r_mode = {bx, CTRL_Y, CTRL_BTN_W, CTRL_BTN_H};
    ui_button_init(&btn_mode, &r_mode, "1", &font_montserrat_12);
    ui_button_set_callback(&btn_mode, btn_mode_click);
    ui_button_set_colors(&btn_mode, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
    btn_mode.radius = 12;

    bx += CTRL_BTN_W + CTRL_GAP;
    ui_rect_t r_prev = {bx, CTRL_Y, CTRL_BTN_W, CTRL_BTN_H};
    ui_button_init(&btn_prev, &r_prev, "<<", &font_montserrat_16);
    ui_button_set_callback(&btn_prev, btn_prev_click);
    ui_button_set_colors(&btn_prev, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
    btn_prev.radius = 12;

    bx += CTRL_BTN_W + CTRL_GAP;
    ui_rect_t r_play = {bx, CTRL_Y, CTRL_BTN_W, CTRL_BTN_H};
    ui_button_init(&btn_play, &r_play, ">", &font_montserrat_16);
    ui_button_set_callback(&btn_play, btn_play_click);
    ui_button_set_colors(&btn_play, UI_COLOR_BLACK, UI_COLOR_WHITE, UI_COLOR_WHITE);
    btn_play.radius = 20;

    bx += CTRL_BTN_W + CTRL_GAP;
    ui_rect_t r_next = {bx, CTRL_Y, CTRL_BTN_W, CTRL_BTN_H};
    ui_button_init(&btn_next, &r_next, ">>", &font_montserrat_16);
    ui_button_set_callback(&btn_next, btn_next_click);
    ui_button_set_colors(&btn_next, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
    btn_next.radius = 12;

    bx += CTRL_BTN_W + CTRL_GAP;
    ui_rect_t r_stop = {bx, CTRL_Y, CTRL_BTN_W, CTRL_BTN_H};
    ui_button_init(&btn_stop, &r_stop, "Stop", &font_montserrat_12);
    ui_button_set_callback(&btn_stop, btn_stop_click);
    ui_button_set_colors(&btn_stop, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
    btn_stop.radius = 12;

    /* Speaker toggle button */
    ui_rect_t r_speaker = {SPK_BTN_X, SPK_Y, SPK_BTN_W, SPK_BTN_H};
    ui_button_init(&btn_speaker, &r_speaker, "Spk OFF", &font_montserrat_12);
    ui_button_set_callback(&btn_speaker, btn_speaker_click);
    ui_button_set_colors(&btn_speaker, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
    btn_speaker.radius = 12;

    /* Refresh button */
    ui_rect_t r_refresh = {SPK_BTN_X + SPK_BTN_W + SPK_BTN_GAP, SPK_Y, SPK_BTN_W, SPK_BTN_H};
    ui_button_init(&btn_refresh, &r_refresh, "Refresh", &font_montserrat_12);
    ui_button_set_callback(&btn_refresh, btn_refresh_click);
    ui_button_set_colors(&btn_refresh, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
    btn_refresh.radius = 12;

    ui_rect_t vol_rect = {VOL_X, VOL_Y, VOL_W, VOL_H};
    ui_widget_init(&s_vol_touch, &vol_rect);
    s_vol_touch.bg_color = UI_COLOR_TRANSPARENT;
    s_vol_touch.event_cb = vol_touch_event;

    /* Playlist touch area — covers entire right panel */
    ui_rect_t pl_rect = {PL_X, PL_Y, PL_W, PL_H};
    ui_widget_init(&s_pl_touch, &pl_rect);
    s_pl_touch.bg_color = UI_COLOR_TRANSPARENT;
    s_pl_touch.event_cb = pl_touch_event;

    s_music_widgets[0] = (ui_widget_t *)&s_app_music.btn_back;
    s_music_widgets[1] = (ui_widget_t *)&s_app_music.lbl_title;
    s_music_widgets[2] = (ui_widget_t *)&btn_mode;
    s_music_widgets[3] = (ui_widget_t *)&btn_prev;
    s_music_widgets[4] = (ui_widget_t *)&btn_play;
    s_music_widgets[5] = (ui_widget_t *)&btn_next;
    s_music_widgets[6] = (ui_widget_t *)&btn_stop;
    s_music_widgets[7] = &s_vol_touch;
    s_music_widgets[8] = &s_pl_touch;
    s_music_widgets[9] = (ui_widget_t *)&btn_speaker;
    s_music_widgets[10] = (ui_widget_t *)&btn_refresh;

    ui_page_set_widgets(&s_app_music.page, s_music_widgets, 11);
    ui_page_set_callbacks(&s_app_music.page, music_page_enter, music_page_exit, music_page_draw, NULL);
    ui_page_set_update_cb(&s_app_music.page, music_page_update);
    ui_page_register(&s_app_music.page);

    /* Initialize playlist */
    memset(&s_playlist, 0, sizeof(s_playlist));
    s_playlist.current = -1;
    s_playlist.mode = PLAY_MODE_SINGLE;

    music_update_texts();
}

ui_page_t *app_music_get_page(void) { return &s_app_music.page; }

void app_music_set_playlist(const char *names[], int16_t count, int16_t start_index)
{
    s_playlist.count = 0;
    s_playlist.current = -1;
    s_playlist.scroll = 0;

    for (int16_t i = 0; i < count && i < MUSIC_PLAYLIST_MAX; i++) {
        if (names[i]) {
            strncpy(s_playlist.names[i], names[i], MUSIC_NAME_LEN - 1);
            s_playlist.names[i][MUSIC_NAME_LEN - 1] = '\0';
            s_playlist.count++;
        }
    }

    if (start_index >= 0 && start_index < s_playlist.count) {
        s_playlist.current = start_index;
    }

    music_ensure_current_visible();

    /* Play command will be sent by the caller; skip playst on enter */
    s_skip_playst = true;
}
