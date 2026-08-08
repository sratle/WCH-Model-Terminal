/********************************** (C) COPYRIGHT *******************************
* File Name          : sound.h
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : Sound system — game BGM and sound effects via CLI
*                      passthrough (pure CLI scheme, no protocol changes).
*                      Same design as display-1, module key "0102".
*
*  Channel plan (Core CS43131 has 2 virtual channels):
*    ch0: game BGM   /BGM/BGM-01.wav          (explicit channel arg)
*    ch1: all SFX    /SOUND/SOUND-*.wav       (explicit channel arg)
*
*  Config gating (config.json display section, fetched via "config get"):
*    operationsound: 1/0 — gates GEACTION / SCACTION / SOUND-HIT
*    gamebgm:        1/0 — gates game BGM start
********************************************************************************/
#ifndef __SOUND_H
#define __SOUND_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Sound effect paths on CH378 storage (absolute).
 * BGM-01 is a FAT 8.3 short name -> stored uppercase (.WAV).
 * SOUND-* names exceed 8 chars -> long filename entries, case-sensitive
 * LFN match -> keep the exact lowercase .wav as stored on the card. */
#define SOUND_PATH_BGM        "/BGM/BGM-01.WAV"
#define SOUND_PATH_DIRACTION  "/SOUND/SOUND-GEACTION.wav"
#define SOUND_PATH_CLICK      "/SOUND/SOUND-SCACTION.wav"
#define SOUND_PATH_HIT        "/SOUND/SOUND-HIT.wav"

/* One-time init (registers nothing permanently; safe to call at UI init) */
void Sound_Init(void);

/* Fetch operationsound/gamebgm from Core config ("config get <display key>")
 * and update the local g_settings cache. Registers a temporary CLI callback;
 * call only when no app holds CLI callbacks (e.g. games page enter). */
void Sound_RefreshConfig(void);

/* Game BGM (ch0) — session semantics:
 * Call Sound_BGM_Start() from every games-section page on_enter (games grid
 * AND each game page). It is idempotent: if our BGM is already playing on
 * ch0 it just re-joins the session without restarting the track.
 * Call Sound_BGM_Leave() from every such page on_exit. The actual "stop 0"
 * is deferred to Sound_BGM_Poll() and is cancelled when another games page
 * is entered synchronously (page push/pop), so BGM keeps playing across
 * grid <-> game navigation and only stops when leaving the games section. */
void Sound_BGM_Start(void);
void Sound_BGM_Leave(void);

/* Call from the main loop: executes deferred BGM stop and re-plays the
 * track when it ended (Core has no loop mode). Self-throttled. */
void Sound_BGM_Poll(void);

/* Sound effects (ch1), all gated by g_settings.operation_sound.
 * Self-throttled (SOUND_SFX_MIN_INTERVAL_MS) to protect the UART/CLI path. */
void Sound_SFX_DirAction(void);  /* game directional input: SOUND-GEACTION */
void Sound_SFX_Click(void);      /* Button/Tab/Switch trigger: SOUND-SCACTION */
void Sound_SFX_Hit(void);        /* hit enemy / eat food / dig: SOUND-HIT */

#ifdef __cplusplus
}
#endif

#endif /* __SOUND_H */
