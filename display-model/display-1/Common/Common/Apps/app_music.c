/********************************** (C) COPYRIGHT *******************************
* File Name          : app_music.c
* Author             : LCD Model Team
* Version            : V3.3.0
* Date               : 2026/06/02
* Description        : Music Player app (V3.3 CLI passthrough + local playlist).
*                      Playlist built from file browser's wav list.
*                      Prev/next via local playlist, play <name> to Core.
*                      Play mode (single/repeat/sequential) managed locally.
*                      Separate play/pause icons for clear state indication.
*                      Registers own UART callbacks to avoid file app interference.
*                      V3.2: Right-side playlist panel, allow exit while playing,
*                            send playst on re-enter to sync state.
*                      V3.3: Speaker toggle (on/off via CLI), refresh button to
*                            send playst and sync speaker state.
********************************************************************************/
#include "app_music.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include "../MiniUI/miniui_anim.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*=============================================================================
 *  Layout Configuration (800x480)
 *
 *  Left half (0..499):  Disc, track info, progress, controls, volume
 *  Right half (500..799): Playlist panel (scrollable)
 *=============================================================================*/

#define LEFT_W              500

/* Disc */
#define DISC_SIZE           160
#define DISC_X              ((LEFT_W - DISC_SIZE) / 2)
#define DISC_Y              (APP_TITLE_BAR_H + 16)

/* Track info */
#define INFO_Y              (DISC_Y + DISC_SIZE + 12)
#define INFO_H              44

/* Progress bar */
#define PROG_Y              (INFO_Y + INFO_H + 8)
#define PROG_X              40
#define PROG_W              (LEFT_W - 80)
#define PROG_H              16
#define TIME_Y              (PROG_Y + PROG_H + 2)

/* Control buttons */
#define CTRL_Y              (TIME_Y + 22)
#define CTRL_BTN_W          56
#define CTRL_BTN_H          44
#define CTRL_GAP            12
#define CTRL_TOTAL_W        (5 * CTRL_BTN_W + 4 * CTRL_GAP)
#define CTRL_X              ((LEFT_W - CTRL_TOTAL_W) / 2)

/* Volume */
#define VOL_Y               (CTRL_Y + CTRL_BTN_H + 16)
#define VOL_X               60
#define VOL_W               (LEFT_W - 120)
#define VOL_H               20

/* Speaker & Refresh row */
#define SPK_Y               (VOL_Y + VOL_H + 12)
#define SPK_BTN_W           80
#define SPK_BTN_H           36
#define SPK_BTN_GAP         16
#define SPK_BTN_X           ((LEFT_W - 2 * SPK_BTN_W - SPK_BTN_GAP) / 2)

/* Right-side playlist panel */
#define PL_X                LEFT_W
#define PL_Y                APP_TITLE_BAR_H
#define PL_W                (UI_SCREEN_WIDTH - LEFT_W)
#define PL_H                (UI_SCREEN_HEIGHT - APP_TITLE_BAR_H)
#define PL_ITEM_H           32
#define PL_HEADER_H         28
#define PL_VISIBLE          ((PL_H - PL_HEADER_H) / PL_ITEM_H)

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
#define MUSIC_PL_BG         UI_HEX(0x16213E)
#define MUSIC_PL_SEL        UI_HEX(0x0F3460)
#define MUSIC_PL_CUR        UI_HEX(0x7EC8C8)
#define MUSIC_PL_BORDER     UI_HEX(0x0F3460)
#define MUSIC_PL_SCROLL     UI_HEX(0x3A506B)

/*=============================================================================
 *  Playlist & Play Mode
 *=============================================================================*/

#define MUSIC_PLAYLIST_MAX  64
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
    int16_t scroll;       /* Scroll offset for playlist (goal, in items) */
} music_playlist_t;

static music_playlist_t s_playlist;

/* Smooth (animated, pixel-level) playlist scrolling */
static ui_scroller_t s_pl_scroller;

/* Scroller position update: redraw just the playlist panel */
static void music_pl_scroll_moved(int32_t value, void *user_data)
{
    (void)value; (void)user_data;
    ui_rect_t r = {PL_X, PL_Y, PL_W, PL_H};
    ui_page_invalidate(&r);
}

/* Set playlist scroll (in items), clamped; animates the pixel position */
static void pl_scroll_set(int16_t items, bool animate)
{
    int16_t max_items = s_playlist.count - PL_VISIBLE;
    if (max_items < 0) max_items = 0;
    if (items > max_items) items = max_items;
    if (items < 0) items = 0;
    s_playlist.scroll = items;
    if (animate) {
        ui_scroller_set_goal(&s_pl_scroller, (int32_t)items * PL_ITEM_H);
    } else {
        ui_scroller_jump(&s_pl_scroller, (int32_t)items * PL_ITEM_H);
    }
}

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

/* Speaker state: true = external speaker ON, false = headphone only (default) */
static bool s_speaker_on = false;

/* Local position estimation: frame-based counter (25fps = 40ms/frame) */
static uint32_t s_local_pos_ms = 0;
static bool s_local_tracking = false;
#define MUSIC_FRAME_MS  40  /* 25fps */

/* Track-end detection flag — set when position reaches duration */
static bool s_track_ended = false;

/* Flag: skip playst on enter because play command was just sent from file browser */
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

    /* Immediately update displayed track name from local playlist,
     * don't wait for async MUSIC_STATUS from Core */
    strncpy(s_track_name, s_playlist.names[index], sizeof(s_track_name) - 1);
    s_track_name[sizeof(s_track_name) - 1] = '\0';
    strcpy(s_time_pos, "00:00");
    strcpy(s_status_text, "Playing");

    /* Update play/pause button to show pause icon */
    btn_play.text = "||";
    btn_play.base.flags |= UI_WIDGET_FLAG_DIRTY;

    music_ensure_current_visible();
    ui_page_invalidate_all();

    UART_SendPlayMusic(s_playlist.names[index]);
}

/* Called when current track ends (detected by position reaching duration) */
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
        pl_scroll_set(s_playlist.current, true);
    else if (s_playlist.current >= s_playlist.scroll + PL_VISIBLE)
        pl_scroll_set(s_playlist.current - PL_VISIBLE + 1, true);
}

static void music_update_texts(void)
{
    /* Prefer local playlist name for immediate display when switching tracks;
     * fall back to g_disp_state.music_track when no local selection */
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

    /* Update play/pause button text based on state */
    if (g_disp_state.music_state == MUSIC_STATE_PLAYING) {
        btn_play.text = "||";   /* Show pause icon when playing */
    } else {
        btn_play.text = ">";    /* Show play icon when not playing */
    }
    btn_play.base.flags |= UI_WIDGET_FLAG_DIRTY;

    /* 外放状态由 Core 二进制 MUSIC_STATUS 驱动同步 */
    s_speaker_on = (g_disp_state.music_speaker != 0);
    btn_speaker.text = s_speaker_on ? "Spk ON" : "Spk OFF";
    btn_speaker.base.flags |= UI_WIDGET_FLAG_DIRTY;

    /* Update mode button text */
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
 *  Drawing
 *=============================================================================*/

static void music_draw_disc(void)
{
    int16_t cx = DISC_X + DISC_SIZE / 2;
    int16_t cy = DISC_Y + DISC_SIZE / 2;
    int16_t r_outer = DISC_SIZE / 2;

    ui_draw_fill_circle(cx, cy, r_outer, MUSIC_DISC_OUTER);
    ui_draw_fill_circle(cx, cy, r_outer - 25, MUSIC_DISC_INNER);
    ui_draw_fill_circle(cx, cy, 18, MUSIC_DISC_CENTER);
    ui_draw_circle_border(cx, cy, r_outer - 5, MUSIC_ACCENT, 1);
    ui_draw_text(cx - 6, cy - 8, "*", &font_montserrat_16, MUSIC_ACCENT);
}

static void music_draw_info(void)
{
    ui_rect_t info_rect = {20, INFO_Y, LEFT_W - 40, INFO_H};
    ui_draw_text_in_rect(&info_rect, s_track_name, &font_montserrat_16, MUSIC_TEXT, 0x11);
    ui_rect_t status_rect = {20, INFO_Y + 24, LEFT_W - 40, 20};
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

    ui_draw_text(VOL_X - 36, VOL_Y + 4, "Vol", &font_montserrat_12, MUSIC_TEXT_DIM);
    ui_draw_text(VOL_X + VOL_W + 6, VOL_Y + 4, s_vol_text, &font_montserrat_12, MUSIC_TEXT_DIM);
}

static void music_draw_speaker_row(void)
{
    ui_draw_text(SPK_BTN_X - 36, SPK_Y + 10, "Out", &font_montserrat_12, MUSIC_TEXT_DIM);
}

static void music_draw_playlist(void)
{
    if (s_playlist.count == 0) return;

    /* Panel background */
    ui_rect_t bg = {PL_X, PL_Y, PL_W, PL_H};
    ui_draw_fill_rect(&bg, MUSIC_PL_BG);

    /* Left border line */
    ui_draw_vline(PL_X, PL_Y, PL_H, MUSIC_PL_BORDER);

    /* Header */
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "Playlist (%d)", s_playlist.count);
    ui_draw_text(PL_X + 10, PL_Y + 6, hdr, &font_montserrat_12, MUSIC_TEXT_DIM);
    ui_draw_hline(PL_X, PL_Y + PL_HEADER_H, PL_W, MUSIC_PL_BORDER);

    /* Items — pixel-level smooth scroll, clipped to the items viewport */
    ui_rect_t items_view = {PL_X, PL_Y + PL_HEADER_H, PL_W, PL_H - PL_HEADER_H};
    int32_t pos = s_pl_scroller.pos;
    int16_t first = (int16_t)(pos / PL_ITEM_H);
    int16_t off   = (int16_t)(pos % PL_ITEM_H);

    ui_render_push_target(&items_view);
    for (int16_t i = 0; i <= PL_VISIBLE && (first + i) < s_playlist.count; i++) {
        int16_t idx = first + i;
        int16_t iy = PL_Y + PL_HEADER_H - off + i * PL_ITEM_H;
        ui_rect_t item_rect = {PL_X + 4, iy + 1, PL_W - 8, PL_ITEM_H - 2};

        if (idx == s_playlist.current) {
            ui_draw_fill_round_rect(&item_rect, 4, MUSIC_PL_SEL);
            /* Playing indicator */
            ui_draw_text(PL_X + 10, iy + 8, ">", &font_montserrat_12, MUSIC_PL_CUR);
            ui_draw_text(PL_X + 24, iy + 8, s_playlist.names[idx],
                         &font_montserrat_12, MUSIC_PL_CUR);
        } else {
            ui_draw_text(PL_X + 24, iy + 8, s_playlist.names[idx],
                         &font_montserrat_12, MUSIC_TEXT_DIM);
        }

        /* Separator line */
        ui_draw_hline(PL_X + 8, iy + PL_ITEM_H - 1, PL_W - 16, MUSIC_PL_BORDER);
    }
    ui_render_pop_target();

    /* Scroll indicator (position follows the animated pixel offset) */
    if (s_playlist.count > PL_VISIBLE) {
        int16_t scroll_area_y = PL_Y + PL_HEADER_H;
        int16_t scroll_area_h = PL_H - PL_HEADER_H;
        int16_t bar_h = scroll_area_h * PL_VISIBLE / s_playlist.count;
        if (bar_h < 20) bar_h = 20;
        int32_t max_px = (int32_t)(s_playlist.count - PL_VISIBLE) * PL_ITEM_H;
        int16_t bar_y = scroll_area_y;
        if (max_px > 0) {
            bar_y += (int16_t)((int32_t)(scroll_area_h - bar_h) * pos / max_px);
        }
        ui_rect_t scroll_bg = {PL_X + PL_W - 5, scroll_area_y, 5, scroll_area_h};
        ui_draw_fill_rect(&scroll_bg, UI_HEX(0x0D1B2A));
        ui_rect_t scroll_bar = {PL_X + PL_W - 5, bar_y, 5, bar_h};
        ui_draw_fill_rect(&scroll_bar, MUSIC_PL_SCROLL);
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

    /* Register music callbacks so CLI responses don't go to file app */
    UART_SetCLICallbacks(&s_music_cb);

    /* Send playst to sync music state from Core, but skip if a play
     * command was just sent from file browser (it would overwrite the
     * pending play request). */
    if (s_skip_playst) {
        s_skip_playst = false;
    } else {
        UART_SendCLI("playst");
    }

    /* Ensure current track is visible */
    music_ensure_current_visible();

    music_update_texts();
    ui_page_invalidate_all();
}

static void music_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;

    /* Title bar background */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Main background */
    ui_rect_t bg = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bg, MUSIC_BG);

    /* Always draw all sections — drawing primitives are auto-clipped to dirty */
    music_draw_disc();
    music_draw_info();
    music_draw_progress();
    music_draw_volume();
    music_draw_speaker_row();
    music_draw_playlist();
}

static void music_page_update(ui_page_t *page)
{
    (void)page;

    /* --- Frame-based position counter (25fps = 40ms per frame) --- */
    if (g_disp_state.music_state == MUSIC_STATE_PLAYING) {
        if (!s_local_tracking) {
            s_local_tracking = true;
            s_local_pos_ms = g_disp_state.music_pos_ms;
        }
        s_local_pos_ms += MUSIC_FRAME_MS;
        if (g_disp_state.music_dur_ms > 0 &&
            s_local_pos_ms > g_disp_state.music_dur_ms)
            s_local_pos_ms = g_disp_state.music_dur_ms;

        /* Detect track end */
        if (!s_track_ended && g_disp_state.music_dur_ms > 0 &&
            s_local_pos_ms >= g_disp_state.music_dur_ms) {
            music_on_track_end();
        }

        /* Only invalidate progress bar area (time text + bar) */
        music_update_texts();
        ui_rect_t prog_area = {PROG_X - 2, PROG_Y - 2,
                               PROG_W + 4, (TIME_Y + 16) - PROG_Y + 4};
        ui_page_invalidate(&prog_area);
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

    /* Duration change (new track started from Core): sync position & full redraw */
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

static bool s_vol_dragging = false;

static void vol_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;

    /* Only adjust volume on DOWN (start drag) or MOVE while dragging */
    if (e->type == UI_EVENT_DOWN) {
        s_vol_dragging = true;
    } else if (e->type == UI_EVENT_UP || e->type == UI_EVENT_CLICK) {
        s_vol_dragging = false;
    } else if (e->type == UI_EVENT_PRESS_CANCEL) {
        s_vol_dragging = false;
        return;
    }

    if (!s_vol_dragging) return;

    int16_t rel_x = e->pos.x - VOL_X;
    if (rel_x < 0) rel_x = 0;
    if (rel_x > VOL_W) rel_x = VOL_W;
    UART_SendVolumeControl(0x00, (uint8_t)((uint32_t)rel_x * 100 / VOL_W));
}

/* Playlist touch: tap to select & play, swipe to scroll */
static void pl_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type == UI_EVENT_SWIPE_UP) {
        pl_scroll_set(s_playlist.scroll + 2, true);
        return;
    }
    if (e->type == UI_EVENT_SWIPE_DOWN) {
        pl_scroll_set(s_playlist.scroll - 2, true);
        return;
    }
    /* Mouse wheel scrolling (smooth) */
    if (e->type == UI_EVENT_MOVE && e->scroll_delta != 0) {
        pl_scroll_set(s_playlist.scroll - e->scroll_delta, true);
        return;
    }
    if (e->type == UI_EVENT_CLICK) {
        /* Determine which item was tapped (uses the animated pixel offset
         * so the hit matches what is on screen) */
        int16_t rel_y = e->pos.y - (PL_Y + PL_HEADER_H);
        if (rel_y < 0) return;
        int16_t idx = (int16_t)((rel_y + s_pl_scroller.pos) / PL_ITEM_H);
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
    ui_button_init(&btn_play, &r_play, ">", &font_montserrat_16);
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

    /* Speaker toggle button */
    ui_rect_t r_speaker = {SPK_BTN_X, SPK_Y, SPK_BTN_W, SPK_BTN_H};
    ui_button_init(&btn_speaker, &r_speaker, "Spk OFF", &font_montserrat_12);
    ui_button_set_callback(&btn_speaker, btn_speaker_click);
    ui_button_set_colors(&btn_speaker, MUSIC_CTRL_BG, MUSIC_ACCENT, MUSIC_TEXT);
    btn_speaker.radius = 12;

    /* Refresh button */
    ui_rect_t r_refresh = {SPK_BTN_X + SPK_BTN_W + SPK_BTN_GAP, SPK_Y, SPK_BTN_W, SPK_BTN_H};
    ui_button_init(&btn_refresh, &r_refresh, "Refresh", &font_montserrat_12);
    ui_button_set_callback(&btn_refresh, btn_refresh_click);
    ui_button_set_colors(&btn_refresh, MUSIC_CTRL_BG, MUSIC_ACCENT, MUSIC_TEXT);
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
    ui_page_set_callbacks(&s_app_music.page, music_page_enter, NULL, music_page_draw, NULL);
    ui_page_set_update_cb(&s_app_music.page, music_page_update);
    ui_page_register(&s_app_music.page);

    /* Initialize playlist */
    memset(&s_playlist, 0, sizeof(s_playlist));
    s_playlist.current = -1;
    s_playlist.mode = PLAY_MODE_SINGLE;
    ui_scroller_init(&s_pl_scroller, 0, music_pl_scroll_moved, NULL);

    music_update_texts();
}

ui_page_t *app_music_get_page(void) { return &s_app_music.page; }

void app_music_set_playlist(const char *names[], int16_t count, int16_t start_index)
{
    s_playlist.count = 0;
    s_playlist.current = -1;
    pl_scroll_set(0, false);

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

    /* Scroll to show the selected track */
    music_ensure_current_visible();

    /* Mark that play command will be sent by caller, so music_page_enter
     * should skip playst to avoid overwriting the pending play request. */
    s_skip_playst = true;
}
