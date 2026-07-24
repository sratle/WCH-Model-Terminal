/********************************** (C) COPYRIGHT *******************************
 * File Name          : cs43131.h
 * Author             : 
 * Version            : V2.0.0
 * Date               : 2025/06/09
 * Description        : CS43131 DAC driver + multi-channel audio engine + effects.
 *******************************************************************************/
#ifndef __CS43131_H__
#define __CS43131_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"
#include "ch32h417_gpio.h"

/* ======================================================================== */
/*  Hardware Constants                                                       */
/* ======================================================================== */

#define AUDIO_BUFFER_SIZE   2048
#define SPI2_TX_DMA_REQUEST 65
#define CS43131_I2C_ADDR    0x30

/* CS43131 GPIO definitions NOTE: SPI2==I2S1 */
#define CS43131_RST_GPIO_PORT   GPIOB
#define CS43131_RST_PIN         GPIO_Pin_3
#define CS43131_OE_GPIO_PORT    GPIOB
#define CS43131_OE_PIN          GPIO_Pin_4

/* I2S GPIO definitions I2S1 */
#define I2S_WS_GPIO_PORT        GPIOB
#define I2S_WS_PIN              GPIO_Pin_12
#define I2S_WS_AF               GPIO_AF5
#define I2S_CK_GPIO_PORT        GPIOB
#define I2S_CK_PIN              GPIO_Pin_13
#define I2S_CK_AF               GPIO_AF5
#define I2S_SDO_GPIO_PORT       GPIOB
#define I2S_SDO_PIN             GPIO_Pin_15
#define I2S_SDO_AF              GPIO_AF5

/* Speaker (FM8002A) SHUTDOWN GPIO definitions */
#define SPEAKER_RIGHT_SHUTDOWN_GPIO_PORT   GPIOD
#define SPEAKER_RIGHT_SHUTDOWN_PIN         GPIO_Pin_10
#define SPEAKER_LEFT_SHUTDOWN_GPIO_PORT    GPIOD
#define SPEAKER_LEFT_SHUTDOWN_PIN          GPIO_Pin_11

/* ======================================================================== */
/*  Audio Engine Constants                                                   */
/* ======================================================================== */

#define AUDIO_MAX_CHANNELS  2
#define AUDIO_CH_RB_SIZE    (64 * 1024)     /* 64KB ring buffer per channel (~370ms @ 44.1kHz/16bit/stereo) */
#define AUDIO_RB_SIZE       (32 * 1024)     /* 32KB main mix ring buffer (legacy) */
#define AUDIO_CH_READ_BLOCK (8 * 1024)      /* 8KB per CH378 read burst (~46ms of audio, reduces open/close overhead) */

/* ======================================================================== */
/*  Enums                                                                    */
/* ======================================================================== */

/* 播放状态 */
typedef enum {
    AUDIO_STATE_IDLE = 0,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PAUSED,
    AUDIO_STATE_STOPPED
} audio_state_t;

/* ======================================================================== */
/*  WAV Header                                                               */
/* ======================================================================== */

typedef struct {
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint16_t num_channels;
    uint32_t data_offset;
    uint32_t data_size;
} wav_info_t;

/* ======================================================================== */
/*  Effects: Biquad Filter (Q20 fixed-point)                                 */
/* ======================================================================== */

typedef struct {
    int32_t b0, b1, b2, a1, a2;    /* Q20 coefficients */
    int32_t x1, x2, y1, y2;        /* state variables */
} biquad_t;

/* ======================================================================== */
/*  Effects: 3-Band EQ                                                       */
/* ======================================================================== */

typedef struct {
    uint8_t  enable;
    int16_t  bass_gain_db;      /* -12 ~ +12 dB, 100 Hz */
    int16_t  mid_gain_db;       /* -12 ~ +12 dB, 1 kHz  */
    int16_t  treble_gain_db;    /* -12 ~ +12 dB, 10 kHz */
    biquad_t bands[3];          /* bass, mid, treble — LEFT channel state  */
    biquad_t bands_r[3];        /* bass, mid, treble — RIGHT channel state */
} audio_eq_t;

/* ======================================================================== */
/*  Effects: Compressor                                                      */
/* ======================================================================== */

typedef struct {
    uint8_t  enable;
    int16_t  threshold_db;      /* -40 ~ 0 dB */
    int16_t  ratio;             /* 1~10 (e.g. 4 = 4:1) */
    int16_t  makeup_db;         /* 0 ~ 24 dB */
    /* internal fixed-point state (computed in Audio_Comp_SetParams/Enable) */
    int32_t  gain_q15;          /* smoothed applied gain (Q15, 32768 = 1.0) */
    int32_t  thresh_amp;        /* threshold as amplitude (Q15, 0..32768)   */
    int32_t  makeup_lin;        /* makeup gain (Q15)                        */
    int32_t  k_q15;             /* (ratio-1)/ratio in Q15                   */
} audio_compressor_t;

/* ======================================================================== */
/*  Effects: Echo / Delay                                                    */
/* ======================================================================== */

#define ECHO_DELAY_SAMPLES  4410    /* 100ms @ 44.1kHz */

typedef struct {
    uint8_t  enable;
    uint16_t delay_ms;          /* 10 ~ 100 ms */
    uint8_t  feedback;          /* 0 ~ 200 (0~78%) */
    uint8_t  mix;               /* 0 ~ 100 (dry/wet %) */
    /* internal */
    int16_t  delay_line[ECHO_DELAY_SAMPLES * 2]; /* stereo interleaved */
    uint16_t delay_len;         /* actual delay in samples */
    uint16_t delay_pos;
    int32_t  lp_l, lp_r;        /* one-pole LPF state in the feedback path (damping) */
} audio_echo_t;

/* ======================================================================== */
/*  Audio Channel                                                            */
/* ======================================================================== */

typedef struct {
    /* Ring buffer */
    uint8_t  rb[AUDIO_CH_RB_SIZE];
    volatile uint32_t rb_write_pos;
    volatile uint32_t rb_read_pos;

    /* Channel state */
    uint8_t  active;
    uint8_t  eof;
    audio_state_t state;
    uint32_t read_offset;       /* next byte offset in file to read */
    uint8_t  file_open;         /* 1 = CH378 file currently open for sequential reads */

    /* File info */
    char     path[260];
    wav_info_t wav_info;

    /* Per-channel controls */
    uint8_t  volume;            /* 0~100 */
    uint8_t  pan;               /* 0=left, 128=center, 255=right */

    /* Progress */
    volatile uint32_t samples_played;

    /* Track name */
    char     track_name[64];
} audio_channel_t;

/* ======================================================================== */
/*  Speaker Channel Select                                                   */
/* ======================================================================== */

typedef enum {
    SPEAKER_CHANNEL_LEFT  = 0x01,
    SPEAKER_CHANNEL_RIGHT = 0x02,
    SPEAKER_CHANNEL_BOTH  = 0x03
} speaker_channel_t;

/* ======================================================================== */
/*  CS43131 Global State                                                     */
/* ======================================================================== */

typedef struct {
    uint8_t enable;                     /* cs43131 enable */
    uint8_t volume;                     /* master volume 0~100 */
    audio_state_t audio_state;          /* overall playback state */

    /* Multi-channel engine */
    audio_channel_t channels[AUDIO_MAX_CHANNELS];
    uint8_t  active_channel_count;
    volatile uint8_t ch378_locked;      /* 1 = CH378 in use by other module, skip audio reads */

    /* DMA output buffer (mixed output) */
    uint16_t dma_buf[AUDIO_BUFFER_SIZE];
    volatile uint32_t dma_write_half;   /* which half ISR is filling: 0=first, 1=second */

    /* Effects chain (globally configurable) */
    uint8_t          fx_master_enable;  /* 总开关: 0=bypass all, 1=apply enabled effects */
    audio_eq_t         eq;
    audio_compressor_t compressor;
    audio_echo_t       echo;

    /* Status */
    volatile uint8_t   status_dirty;    /* dirty flag for Display sync */
    uint8_t            current_channel; /* last-started channel index */
} cs43131_t;

/* ======================================================================== */
/*  Speaker API                                                              */
/* ======================================================================== */

void Speaker_Init(void);
void Speaker_On(speaker_channel_t channel);
void Speaker_Off(speaker_channel_t channel);
speaker_channel_t Speaker_GetState(void);

/* ======================================================================== */
/*  CS43131 Hardware Init                                                    */
/* ======================================================================== */

void CS43131_init(cs43131_t *cs43131);
void CS43131_I2C_Config(void);
void I2S1_DMA_DoubleBufferInit(void);

/* ======================================================================== */
/*  Audio Engine: Channel Management                                         */
/* ======================================================================== */

/**
 * Start streaming a WAV file on the given channel.
 * Caller must have already parsed the WAV header.
 * The engine takes ownership of CH378 reads from data_offset onward.
 * File must NOT be left open by caller.
 *
 * @param ch        channel index (0 ~ AUDIO_MAX_CHANNELS-1)
 * @param path      file path on CH378 (will be copied)
 * @param info      parsed WAV header info
 * @return          0 on success, non-zero on error
 */
uint8_t Audio_ChannelStart(uint8_t ch, const char *path, const wav_info_t *info);

/** Stop a specific channel */
void Audio_ChannelStop(uint8_t ch);

/** Stop all channels */
void Audio_StopAll(void);

/** Pause/resume a specific channel */
void Audio_ChannelPause(uint8_t ch);
void Audio_ChannelResume(uint8_t ch);

/**
 * Seek a currently-playing channel to an absolute position (milliseconds).
 * Used for progress-bar dragging. The channel must be active and
 * PLAYING/PAUSED, and ms must be strictly less than the track duration.
 * Resets the channel ring buffer and read offset; the audio engine re-opens
 * the file and refills from the new position on the next Audio_Process() pass.
 *
 * @param ch  channel index (0 ~ AUDIO_MAX_CHANNELS-1)
 * @param ms  absolute playback position in milliseconds
 * @return    0 = success;
 *            1 = invalid channel index;
 *            2 = channel not currently playing;
 *            3 = position out of range / unknown duration
 */
uint8_t Audio_ChannelSeek_ms(uint8_t ch, uint32_t ms);

/** Per-channel volume and pan */
void Audio_ChannelSetVolume(uint8_t ch, uint8_t vol);
void Audio_ChannelSetPan(uint8_t ch, uint8_t pan);

/** Query channel state */
audio_state_t Audio_ChannelGetState(uint8_t ch);
uint32_t Audio_ChannelGetPlayTime_ms(uint8_t ch);
uint32_t Audio_ChannelGetDuration_ms(uint8_t ch);
const char* Audio_ChannelGetTrackName(uint8_t ch);

/* ======================================================================== */
/*  Audio Engine: Master Controls                                            */
/* ======================================================================== */

/** Master volume 0~100 */
void Audio_SetVolume(uint8_t vol);
uint8_t Audio_GetVolume(void);

/** Master pause/resume (all channels) */
void Audio_Pause(void);
void Audio_Resume(void);

/** Global playback state */
audio_state_t Audio_GetState(void);
uint8_t Audio_IsPlaying(void);

/** Status dirty flag (for Display sync) */
uint8_t Audio_IsStatusDirty(void);
void Audio_ClearStatusDirty(void);

/* ======================================================================== */
/*  Audio Engine: CH378 Sharing                                              */
/* ======================================================================== */

/**
 * Lock CH378: audio engine will skip reads until unlocked.
 * Call before any CH378 file operation from CLI/Config/etc.
 */
void Audio_CH378_Lock(void);

/**
 * Unlock CH378: audio engine resumes reads.
 * Call after CH378 file operation completes.
 */
void Audio_CH378_Unlock(void);

/** Query if any channel is actively streaming (needs CH378 reads) */
uint8_t Audio_IsStreaming(void);

/**
 * Pre-fill all active audio channel ring buffers to high watermark.
 * Call before Audio_CH378_Lock() to maximize buffer headroom
 * during CH378 file operations (CLI/Config/etc.).
 * Reads from CH378, so must be called BEFORE Lock.
 */
void Audio_PreFill(void);

/* ======================================================================== */
/*  Audio Engine: Effects Control                                            */
/* ======================================================================== */

/** Effects master switch: 0=bypass all effects, 1=apply enabled effects */
void Audio_FX_MasterEnable(uint8_t en);
uint8_t Audio_FX_IsMasterEnabled(void);

/** Apply the built-in "electronic piano enhancement" preset (EQ+Comp+Echo). */
void Audio_FX_ApplyMusicPreset(void);

/** EQ: set gains in dB (-12~+12), then recompute coefficients */
void Audio_EQ_SetBass(int16_t gain_db);
void Audio_EQ_SetMid(int16_t gain_db);
void Audio_EQ_SetTreble(int16_t gain_db);
void Audio_EQ_Enable(uint8_t en);
void Audio_EQ_UpdateCoeffs(void);

/** Compressor */
void Audio_Comp_SetParams(int16_t threshold_db, int16_t ratio, int16_t makeup_db);
void Audio_Comp_Enable(uint8_t en);

/** Echo */
void Audio_Echo_SetParams(uint16_t delay_ms, uint8_t feedback, uint8_t mix);
void Audio_Echo_Enable(uint8_t en);

/* ======================================================================== */
/*  WAV Parsing (utility)                                                    */
/* ======================================================================== */

uint8_t Audio_ParseWAVHeader(uint8_t *buf, wav_info_t *info);

/* ======================================================================== */
/*  Main Loop & DMA                                                          */
/* ======================================================================== */

/** Main loop polling: refills channel buffers from CH378, processes effects */
void Audio_Process(void);

/** DMA interrupt handler */
void DMA1_Channel1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/* ======================================================================== */
/*  Legacy Compatibility (backward-compat wrappers)                          */
/* ======================================================================== */

/**
 * Legacy: start single-channel WAV playback on channel 0.
 * Equivalent to Audio_ChannelStart(0, ...) but expects file to already
 * be open and positioned at data_offset (old behavior).
 * NOTE: File will be closed by the engine; caller should NOT close it.
 */
void Audio_PlayWAV_Start(wav_info_t *info);

/** Legacy: stop all playback (alias for Audio_StopAll) */
void Audio_PlayStop(void);

/** Legacy: set current track name on channel 0 */
void Audio_SetCurrentTrack(const char *name);

/** Legacy: get current track name from channel 0 */
const char* Audio_GetCurrentTrackName(void);

/** Legacy: get play time from channel 0 */
uint32_t Audio_GetPlayTime_ms(void);

/** Legacy: get duration from channel 0 */
uint32_t Audio_GetDuration_ms(void);

#ifdef __cplusplus
}
#endif

#endif
