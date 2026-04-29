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

#define AUDIO_BUFFER_SIZE   1024
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

typedef struct {
    uint8_t enable; // cs43131 enable
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

void CS43131_init(cs43131_t *cs43131);
void CS43131_I2C_Config(void);

void I2S1_DMA_DoubleBufferInit(void);

/* 音频播放核心 */
void Audio_PlayStart(const uint16_t *data, uint32_t length);
void Audio_PlayStop(void);
void Audio_FillBuffer(uint16_t *buffer);
void Audio_GenerateSineWave(uint16_t *buffer, uint32_t length);
void Audio_PlaySineStart(void);

/* 框架函数：后续实现 */
uint8_t Audio_ParseWAVHeader(uint8_t *buf, wav_info_t *info);
uint8_t Audio_PlayWAV(const char *filename);
void Audio_Process(void);
uint8_t Audio_IsPlaying(void);

/* 保留旧接口（空框架） */
void Audio_GenerateTriangleWave(uint16_t *buffer, uint32_t length, int16_t amplitude, uint32_t period);
void Audio_PlayTriangleWave(int16_t amplitude, uint32_t period);

void DMA1_Channel1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

#ifdef __cplusplus
}
#endif

#endif
