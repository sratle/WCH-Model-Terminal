/********************************** (C) COPYRIGHT *******************************
* File Name          : sound.c
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : Sound system implementation (display-2, module key 0102).
********************************************************************************/
#include "sound.h"
#include "../settings.h"
#include "../UART/uart_module.h"
#include "../MiniUI/miniui.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*=============================================================================
 *  Tunables
 *=============================================================================*/

#define SOUND_SFX_MIN_INTERVAL_MS   30     /* min gap between SFX CLI sends (per type) */
#define SOUND_BGM_RESTART_MS        3000   /* min gap between BGM (re)starts */
#define SOUND_CONFIG_KEY            "0102" /* display-2 = E-ink subtype 02 */

/*=============================================================================
 *  State
 *=============================================================================*/

static uint32_t s_last_dir_ms = 0;       /* per-type throttle timers: a     */
static uint32_t s_last_click_ms = 0;     /* direction input / widget click  */
static uint32_t s_last_hit_ms = 0;       /* must not suppress each other    */
static bool     s_bgm_active = false;    /* BGM should be playing (games open) */
static uint32_t s_bgm_start_ms = 0;      /* last BGM start attempt timestamp */

/*=============================================================================
 *  SFX common
 *=============================================================================*/

static void sfx_play(const char *path, uint32_t *last_ms)
{
    if (!g_settings.operation_sound) return;

    uint32_t now = ui_get_real_ms();
    if ((uint32_t)(now - *last_ms) < SOUND_SFX_MIN_INTERVAL_MS) return;
    *last_ms = now;

    char cmd[80];
    snprintf(cmd, sizeof(cmd), "play %s 1", path);
    UART_SendCLI(cmd);
}

void Sound_SFX_DirAction(void)
{
    sfx_play(SOUND_PATH_DIRACTION, &s_last_dir_ms);
}

void Sound_SFX_Click(void)
{
    sfx_play(SOUND_PATH_CLICK, &s_last_click_ms);
}

void Sound_SFX_Hit(void)
{
    sfx_play(SOUND_PATH_HIT, &s_last_hit_ms);
}

/*=============================================================================
 *  BGM (ch0) — session semantics
 *=============================================================================*/

static bool s_bgm_leave_pending = false;   /* deferred stop requested */

static bool bgm_is_ours_playing(void)
{
    /* MUSIC_STATUS cache: our BGM track name is the path we sent */
    if (g_disp_state.music_state != MUSIC_STATE_PLAYING &&
        g_disp_state.music_state != MUSIC_STATE_PAUSED) {
        return false;
    }
    return strcmp(g_disp_state.music_track, SOUND_PATH_BGM) == 0;
}

static void bgm_send_play(void)
{
    s_bgm_start_ms = ui_get_real_ms();
    UART_SendCLI("play " SOUND_PATH_BGM " 0");
}

void Sound_BGM_Start(void)
{
    if (!g_settings.game_bgm) {
        s_bgm_active = false;
        return;
    }
    s_bgm_leave_pending = false;

    /* Re-joining an ongoing session (grid <-> game navigation) */
    if (bgm_is_ours_playing()) {
        s_bgm_active = true;
        return;
    }

    /* A start was attempted very recently; the MUSIC_STATUS cache lags
     * behind, so assume it is taking effect — join the session without
     * re-sending play (prevents track restart on fast grid->game entry). */
    uint32_t now = ui_get_real_ms();
    if ((uint32_t)(now - s_bgm_start_ms) < SOUND_BGM_RESTART_MS) {
        s_bgm_active = true;
        return;
    }

    s_bgm_active = true;
    /* Clear any leftover playback (e.g. Music app) before taking ch0 */
    UART_SendCLI("stop");
    bgm_send_play();
}

void Sound_BGM_Leave(void)
{
    /* Deferred: cancelled if another games page is entered synchronously */
    s_bgm_active = false;
    s_bgm_leave_pending = true;
}

void Sound_BGM_Poll(void)
{
    /* Deferred stop: only fires when no games page re-activated in time */
    if (s_bgm_leave_pending && !s_bgm_active) {
        s_bgm_leave_pending = false;
        UART_SendCLI("stop 0");
        return;
    }

    if (!s_bgm_active) return;
    if (g_disp_state.music_state == MUSIC_STATE_PLAYING ||
        g_disp_state.music_state == MUSIC_STATE_PAUSED) {
        return;
    }
    /* IDLE/STOPPED: track ended (or start failed) — restart, throttled so a
     * missing BGM file cannot flood the CLI path. */
    uint32_t now = ui_get_real_ms();
    if ((uint32_t)(now - s_bgm_start_ms) < SOUND_BGM_RESTART_MS) return;
    bgm_send_play();
}

/*=============================================================================
 *  Config fetch (operationsound / gamebgm)
 *=============================================================================*/

static void sound_on_cli_complete(const char *buf, uint16_t len, const char *tag)
{
    /* Called via UART_SetSystemCLIObserver: permanent and non-exclusive,
     * receives EVERY CLI response — must only react to the config keys we
     * care about and never touch the app callback slot. */
    (void)tag;

    if (buf) {
        /* Parse "key:value" lines (same format as the settings page) */
        char parse_buf[256];
        uint16_t copy_len = (len < sizeof(parse_buf) - 1) ? len : (uint16_t)(sizeof(parse_buf) - 1);
        memcpy(parse_buf, buf, copy_len);
        parse_buf[copy_len] = '\0';

        char *line = parse_buf;
        while (line && *line) {
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            char *cr = strchr(line, '\r');
            if (cr) *cr = '\0';

            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                int val = atoi(colon + 1);
                if (strcmp(line, "operationsound") == 0) {
                    g_settings.operation_sound = (val != 0);
                } else if (strcmp(line, "gamebgm") == 0) {
                    g_settings.game_bgm = (val != 0);
                }
            }

            if (nl) line = nl + 1;
            else break;
        }

        /* Reconcile BGM with the freshly fetched switch, but ONLY while a
         * games session is active — the config response may arrive after the
         * user already left the game, and starting BGM here would then loop
         * forever outside games (s_bgm_active stays true). */
        if (s_bgm_active) {
            if (g_settings.game_bgm) {
                Sound_BGM_Start();
            } else {
                Sound_BGM_Leave();
            }
        }
    }
}

void Sound_RefreshConfig(void)
{
    /* Send only — the response reaches sound_on_cli_complete via the
     * permanent system observer, no callback-slot registration needed. */
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "config get %s", SOUND_CONFIG_KEY);
    UART_SendCLI(cmd);
}

void Sound_Init(void)
{
    UART_SetSystemCLIObserver(sound_on_cli_complete);
}
