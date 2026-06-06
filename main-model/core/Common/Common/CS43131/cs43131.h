/********************************** (C) COPYRIGHT *******************************
 * File Name          : cs43131.h
 * Author             : 
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : CS43131 DAC driver header.
 *******************************************************************************/
#ifndef __CS43131_H__
#define __CS43131_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"
#include "ch32h417_gpio.h"

#define AUDIO_BUFFER_SIZE   2048
#define SPI2_TX_DMA_REQUEST 65
#define CS43131_I2C_ADDR    0x30

/* CS43131 GPIO definitions NOTE: SPI2==I2S1*/
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

typedef struct {
    uint8_t enable;               /* cs43131 enable */
    uint8_t volume;               /* 音量 0~100 */
    volatile uint32_t samples_played; /* 播放进度计数 (声道样本数) */
    char track_name[64];          /* 当前曲目名称 */
}cs43131_t;

/* 播放模式 */
typedef enum {
    AUDIO_MODE_NONE = 0,
    AUDIO_MODE_MEMORY,   /* 播放内存中的音频数据 */
    AUDIO_MODE_SINE,     /* 正弦波测试 */
    AUDIO_MODE_WAV       /* WAV 流式播放 */
} audio_mode_t;

/* 播放状态 */
typedef enum {
    AUDIO_STATE_IDLE = 0,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PAUSED,
    AUDIO_STATE_STOPPED
} audio_state_t;

/* WAV 文件信息 */
typedef struct {
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint16_t num_channels;
    uint32_t data_offset;
    uint32_t data_size;
} wav_info_t;

/* Speaker 通道选择 */
typedef enum {
    SPEAKER_CHANNEL_LEFT  = 0x01,
    SPEAKER_CHANNEL_RIGHT = 0x02,
    SPEAKER_CHANNEL_BOTH  = 0x03
} speaker_channel_t;

void Speaker_Init(void);
void Speaker_On(speaker_channel_t channel);
void Speaker_Off(speaker_channel_t channel);
speaker_channel_t Speaker_GetState(void);

void CS43131_init(cs43131_t *cs43131);
void CS43131_I2C_Config(void);

void I2S1_DMA_DoubleBufferInit(void);

/* 音频播放核心 */
void Audio_PlayStart(const uint16_t *data, uint32_t length);
void Audio_PlayStop(void);
void Audio_FillBuffer(uint16_t *buffer, uint32_t length);
void Audio_GenerateSineWave(uint16_t *buffer, uint32_t length);
void Audio_PlaySineStart(void);

/* 音量控制 */
void Audio_SetVolume(uint8_t vol);
uint8_t Audio_GetVolume(void);

/* 暂停/恢复 */
void Audio_Pause(void);
void Audio_Resume(void);

/* 播放状态查询 */
audio_state_t Audio_GetState(void);
uint8_t Audio_IsPlaying(void);

/* CH378 占用查询（WAV 流式播放期间 CH378 不可用于其他操作） */
uint8_t Audio_IsStreaming(void);

/* 音乐状态脏标记：状态/音量/曲目变化时置位，供 Display 同步使用 */
uint8_t Audio_IsStatusDirty(void);
void Audio_ClearStatusDirty(void);

/* 播放进度与曲目信息 */
uint32_t Audio_GetPlayTime_ms(void);
uint32_t Audio_GetDuration_ms(void);
const char* Audio_GetCurrentTrackName(void);
void Audio_SetCurrentTrack(const char *name);

/* 框架函数：后续实现 */
uint8_t Audio_ParseWAVHeader(uint8_t *buf, wav_info_t *info);
void Audio_PlayWAV_Start(wav_info_t *info);
void Audio_Process(void);

/* 跨核同步：将音频状态写入共享内存供 V3F 读取 */
void Audio_SyncToSharedMemory(void);

void DMA1_Channel1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

#ifdef __cplusplus
}
#endif

#endif
