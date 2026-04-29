/********************************** (C) COPYRIGHT *******************************
 * File Name          : cs43131.c
 * Author             : 
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : CS43131 DAC driver implementation.
 *******************************************************************************/
#include "cs43131.h"
#include "./I2c_soft/i2c_soft.h"
#include "ch32h417_dma.h"
#include "ch32h417_spi.h"
#include "ch32h417_rcc.h"
#include "debug.h"

#include "CH378/CH378.h"
#include "hardware.h"
#include <string.h>

extern cs43131_t CS43131_g;

static uint16_t audio_buffer_A[AUDIO_BUFFER_SIZE];
static uint16_t audio_buffer_B[AUDIO_BUFFER_SIZE];

static const uint16_t *song_data = NULL;
static volatile uint32_t song_data_length = 0;
static volatile uint32_t audio_data_offset = 0;

/* 播放模式与状态 */
static audio_mode_t audio_mode = AUDIO_MODE_NONE;
static audio_state_t audio_state = AUDIO_STATE_IDLE;



/* 1/4 周期正弦查找表 (0~pi/2)，振幅 8000，65 点 */
static const int16_t sine_quarter[65] = {
    0, 196, 393, 589, 784, 979, 1174, 1368, 1561, 1753, 1944, 2134, 2322, 2509, 2695, 2879,
    3061, 3242, 3420, 3597, 3771, 3943, 4113, 4280, 4445, 4606, 4766, 4922, 5075, 5225, 5372, 5516,
    5657, 5794, 5928, 6058, 6184, 6307, 6426, 6541, 6652, 6759, 6862, 6961, 7055, 7146, 7232, 7314,
    7391, 7464, 7532, 7596, 7656, 7710, 7760, 7806, 7846, 7882, 7913, 7940, 7961, 7978, 7990, 7998,
    8000
};

static uint32_t sine_phase = 0;       /* 8-bit 相位累加器 0~255 */
static const uint32_t sine_inc = 6;   /* 相位增量，约 1033 Hz @ 44.1kHz */

/* WAV 播放上下文 */
static wav_info_t wav_info;
static uint8_t wav_file_opened = 0;
static uint8_t wav_eof = 0;

/* Ring Buffer (32KB) */
#define AUDIO_RB_SIZE  (32 * 1024)
static uint8_t audio_ringbuf[AUDIO_RB_SIZE];
static volatile uint32_t rb_write_pos = 0;
static volatile uint32_t rb_read_pos = 0;

/* Ring Buffer 辅助函数 */
static uint32_t rb_used(void)
{
    return (rb_write_pos + AUDIO_RB_SIZE - rb_read_pos) % AUDIO_RB_SIZE;
}

static uint32_t rb_free(void)
{
    return AUDIO_RB_SIZE - rb_used() - 1;
}

static void rb_write_data(uint8_t *data, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        audio_ringbuf[rb_write_pos] = data[i];
        rb_write_pos = (rb_write_pos + 1) % AUDIO_RB_SIZE;
    }
}

static void rb_read_to_buffer(uint16_t *dst, uint32_t sample_count)
{
    uint32_t i;
    for (i = 0; i < sample_count; i++) {
        uint8_t lo = audio_ringbuf[rb_read_pos];
        rb_read_pos = (rb_read_pos + 1) % AUDIO_RB_SIZE;
        uint8_t hi = audio_ringbuf[rb_read_pos];
        rb_read_pos = (rb_read_pos + 1) % AUDIO_RB_SIZE;
        dst[i] = (uint16_t)(lo | (hi << 8));
    }
}

static uint8_t CS43131_I2C_WriteReg(uint32_t addr, uint8_t dat)
{
    uint8_t ret = SoftI2C_WriteReg(CS43131_I2C_ADDR, addr, dat);
    if (ret)
    {
        printf("I2C_WR_ERR: 0x%06X=0x%02X\r\n", addr, dat);
    }
    return ret;
}

static uint8_t CS43131_I2C_ReadReg(uint32_t addr)
{
    uint8_t dat = 0;
    uint8_t ret = SoftI2C_ReadReg(CS43131_I2C_ADDR, addr, &dat);
    if (ret)
    {
        printf("I2C_RD_ERR: 0x%06X\r\n", addr);
        return 0xFF;
    }
    return dat;
}

/*********************************************************************
 * @fn      CS43131_I2C_Config
 *
 * @brief   Configure CS43131 via software I2C.
 *
 * @return  none
 *********************************************************************/
void CS43131_I2C_Config(void)
{
    uint8_t tmp;

    printf("CS43131_I2C_Config\r\n");

    /* RST low -> high, delay 10ms */
    GPIO_ResetBits(CS43131_RST_GPIO_PORT, CS43131_RST_PIN);
    Delay_Ms(1);
    GPIO_SetBits(CS43131_RST_GPIO_PORT, CS43131_RST_PIN);
    Delay_Ms(50);

    printf("CS43131_I2C_Config: RST low -> high, delay 10ms\r\n");

    /* 一、复位后基础初始化配置：临时使用 RCO */
    CS43131_I2C_WriteReg(0x010006, 0x06); /* MCLK_SRC_SEL=RCO */

    CS43131_I2C_WriteReg(0x020052, 0x04); /* XTAL_IBIAS=12.5uA (100) */

    /* 二、启动外部 22.5792 MHz 晶振并等待稳定 */
    CS43131_I2C_WriteReg(0x020000, 0xF6); /* Power up XTAL only */
    Delay_Ms(100);                         /* 等待晶振起振并稳定 */

    tmp = CS43131_I2C_ReadReg(0x0F0000);
    printf("CS43131_I2C_Config: XTAL powered, status=0x%02X\r\n", tmp);

    /* 三、系统时钟源切换为外部晶振（不使用 PLL）
     * 22.5792 MHz XTAL 经 divide-by-2 得到 11.2896 MHz = 256*44.1kHz，
     * 直接作为 44.1kHz 音频的 MCLK，无需 PLL 转换。
     */
    CS43131_I2C_WriteReg(0x040004, 0x00); /* CLKOUT source = XTAL */
    CS43131_I2C_WriteReg(0x020000, 0xF4); /* Power up XTAL | CLKOUT (no PLL) */
    CS43131_I2C_WriteReg(0x010006, 0x04); /* MCLK_SRC_SEL=XTAL */
    Delay_Ms(10);                          /* 等待时钟切换稳定 */

    printf("CS43131_I2C_Config: clock source switched to XTAL 22.5792MHz\r\n");

    /* 五、ASP I2S Master接口配置（44.1KHz、16bit） */
    CS43131_I2C_WriteReg(0x01000B, 0x01); /* ASP_SPRATE = 44.1KHz */
    CS43131_I2C_WriteReg(0x01000C, 0x02); /* ASP_SPSIZE = 16bit */
    CS43131_I2C_WriteReg(0x040010, 0x01); /* ASP_N_LSB = 1 */
    CS43131_I2C_WriteReg(0x040011, 0x00); /* ASP_N_MSB = 0 */
    CS43131_I2C_WriteReg(0x040012, 0x10); /* ASP_M_LSB = 16 */
    CS43131_I2C_WriteReg(0x040013, 0x00); /* ASP_M_MSB = 0 */
    CS43131_I2C_WriteReg(0x040014, 0x0F); /* ASP_LCHI_LSB = 15 (16 SCLK) */
    CS43131_I2C_WriteReg(0x040015, 0x00); /* ASP_LCHI_MSB = 0 */
    CS43131_I2C_WriteReg(0x040016, 0x1F); /* ASP_LCPR_LSB = 31 (32 SCLK) */
    CS43131_I2C_WriteReg(0x040017, 0x00); /* ASP_LCPR_MSB = 0 */
    CS43131_I2C_WriteReg(0x040018, 0x1C); /* ASP Master Mode | ASP SCLK Inverted */
    CS43131_I2C_WriteReg(0x040019, 0x0A); /* 50/50 duty | FSD=1 | STP=0 */
    CS43131_I2C_WriteReg(0x050000, 0x00); /* ASP_RX_CH1 = 0 */
    CS43131_I2C_WriteReg(0x050001, 0x00); /* ASP_RX_CH2 = 0 */
    CS43131_I2C_WriteReg(0x05000A, 0x05); /* CH1: LRCK低电平采样, 16bit, 使能 */
    CS43131_I2C_WriteReg(0x05000B, 0x0D); /* CH2: LRCK高电平采样, 16bit, 使能 */
    CS43131_I2C_WriteReg(0x01000D, 0x02); /* XSP_3ST=1(default), ASP_3ST=0, 使能时钟输出 */

    printf("CS43131_I2C_Config:ASP I2S Master interface config\r\n");

    /* 六、PCM音频路径配置 */
    CS43131_I2C_WriteReg(0x090000, 0x02); /* 高通滤波开启, 快速滚降 */
    Audio_SetVolume(60);                  /* 默认音量 50% */
    CS43131_I2C_WriteReg(0x090003, 0xEC); /* 软斜坡, 自动静音, 双声道同步 */
    CS43131_I2C_WriteReg(0x090004, 0x00); /* 关闭反转/交换/复制 */

    printf("CS43131_I2C_Config:PCM audio path config\r\n");

    /* 七、耳机输出硬件配置 */
    CS43131_I2C_WriteReg(0x0B0000, 0x1E); /* Class H, 高压模式, 自适应电源 */
    CS43131_I2C_WriteReg(0x080000, 0x30); /* 1.732Vrms满量程输出 */
    CS43131_I2C_WriteReg(0x0D0000, 0x04); /* 关闭耳机插入检测 */

    printf("CS43131_I2C_Config:headphone output hardware config\r\n");

    /* 八、Pop-free设置 */
    CS43131_I2C_WriteReg(0x010010, 0x99); /* Pop-free setting */
    CS43131_I2C_WriteReg(0x080032, 0x20);

    /* 九、最终模块使能（XTAL 直接驱动，不经过 PLL） */
    CS43131_I2C_WriteReg(0x020000, 0xB4); /* Power up ASP | XTAL | CLKOUT */
    CS43131_I2C_WriteReg(0x020000, 0xA4); /* Power up HP | ASP | XTAL | CLKOUT */
    Delay_Ms(22);                         /* 功放软启动完成 */

    /* 恢复Pop-free设置 */
    CS43131_I2C_WriteReg(0x010010, 0x00); /* Pop-free setting restore */
    CS43131_I2C_WriteReg(0x080032, 0x00);

    /* 取消静音 */
    CS43131_I2C_WriteReg(0x090003, 0xEC); /* Disable mute */
}

/**
 * 正式板使用I2S1,I2C2
 * 正式板PB12-WS,PB13-CK,PB15-SDO
 * PB4-OE,PB3-RST
 * PC0-SCL,PC1-SDA
 * CS43131 I2S1 (CH32H417 以 SPI2 复用为 I2S1)
 */
void CS43131_init(cs43131_t *cs43131)
{
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2S_InitTypeDef I2S_InitStructure = {0};

    /* Enable clocks */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOA | RCC_HB2Periph_GPIOB | RCC_HB2Periph_AFIO, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI2, ENABLE);

    /* I2S1 GPIO: PB12-WS(AF5), PB13-CK(AF5), PB15-SDO(AF5) */
    GPIO_PinAFConfig(I2S_WS_GPIO_PORT, GPIO_PinSource12, I2S_WS_AF);
    GPIO_PinAFConfig(I2S_CK_GPIO_PORT, GPIO_PinSource13, I2S_CK_AF);
    GPIO_PinAFConfig(I2S_SDO_GPIO_PORT, GPIO_PinSource15, I2S_SDO_AF);

    GPIO_InitStructure.GPIO_Pin = I2S_WS_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(I2S_WS_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = I2S_CK_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(I2S_CK_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = I2S_SDO_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(I2S_SDO_GPIO_PORT, &GPIO_InitStructure);

    /* OE & RST as output */
    GPIO_InitStructure.GPIO_Pin = CS43131_OE_PIN | CS43131_RST_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(CS43131_OE_GPIO_PORT, &GPIO_InitStructure);

    /* OE pull up, RST pull down (initial low) */
    GPIO_SetBits(CS43131_OE_GPIO_PORT, CS43131_OE_PIN);
    GPIO_ResetBits(CS43131_RST_GPIO_PORT, CS43131_RST_PIN);

    /* Software I2C init */
    SoftI2C_Init();

    /* CS43131 I2C configuration */
    CS43131_I2C_Config();

    /* I2S1 (SPI2) init: slave tx, Philips, 16b, 44.1kHz, CPOL low, MCLK disable */
    I2S_StructInit (&I2S_InitStructure);
    I2S_InitStructure.I2S_Mode = I2S_Mode_SlaveTx;
    I2S_InitStructure.I2S_Standard = I2S_Standard_Phillips;
    I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_16b;
    I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
    I2S_InitStructure.I2S_AudioFreq = I2S_AudioFreq_44k;
    I2S_InitStructure.I2S_CPOL = I2S_CPOL_Low;
    I2S_Init (SPI2, &I2S_InitStructure);

    /* I2S1 DMA double buffer init */
    I2S1_DMA_DoubleBufferInit();

    I2S_Cmd (SPI2, ENABLE);

    cs43131->enable = 1;
}

void I2S1_DMA_DoubleBufferInit(void)
{
    DMA_InitTypeDef DMA_InitStructure = {0};

    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);
    DMA_DeInit(DMA1_Channel1);

    DMA_StructInit(&DMA_InitStructure);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DATAR;
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)audio_buffer_A;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = AUDIO_BUFFER_SIZE;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;

    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    DMA_MuxChannelConfig(DMA_MuxChannel1, SPI2_TX_DMA_REQUEST);

    DMA_ITConfig(DMA1_Channel1, DMA_IT_HT | DMA_IT_TC, ENABLE);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);
    DMA_Cmd(DMA1_Channel1, ENABLE);
}

/*********************************************************************
 * @fn      sine_lookup
 *
 * @brief   Lookup sine value from quarter-table using symmetry.
 *
 * @param   phase - 8-bit phase 0~255
 *
 * @return  16-bit signed sample
 *********************************************************************/
static int16_t sine_lookup(uint8_t phase)
{
    uint8_t idx;
    int16_t val;

    if (phase <= 64) {
        idx = phase;
        val = sine_quarter[idx];
    } else if (phase <= 128) {
        idx = 128 - phase;
        val = sine_quarter[idx];
    } else if (phase <= 192) {
        idx = phase - 128;
        val = -sine_quarter[idx];
    } else {
        idx = 256 - phase;
        val = -sine_quarter[idx];
    }
    return val;
}

/*********************************************************************
 * @fn      Audio_GenerateSineWave
 *
 * @brief   Generate 1kHz sine wave into buffer (mono -> stereo).
 *
 * @param   buffer - pointer to buffer to fill.
 * @param   length - number of samples to generate.
 *
 * @return  none
 *********************************************************************/
void Audio_GenerateSineWave(uint16_t *buffer, uint32_t length)
{
    uint32_t i;
    for (i = 0; i < length; i++) {
        int16_t sample = sine_lookup((uint8_t)sine_phase);
        buffer[i] = (uint16_t)sample;
        sine_phase += sine_inc;
    }
}

/*********************************************************************
 * @fn      Audio_FillBuffer
 *
 * @brief   Fill audio buffer with next samples from song data.
 *
 * @param   buffer - pointer to buffer to fill.
 *
 * @return  none
 *********************************************************************/
void Audio_FillBuffer(uint16_t *buffer, uint32_t length)
{
    uint32_t i;
    uint32_t remaining;
    uint32_t copy_count;

    if (audio_state != AUDIO_STATE_PLAYING || song_data == NULL) {
        memset(buffer, 0, length * sizeof(uint16_t));
        return;
    }

    remaining = song_data_length - audio_data_offset;
    copy_count = (remaining < length) ? remaining : length;

    for (i = 0; i < copy_count; i++) {
        buffer[i] = song_data[audio_data_offset + i];
    }
    for (; i < length; i++) {
        buffer[i] = 0;
    }

    audio_data_offset += copy_count;
    if (audio_data_offset >= song_data_length) {
        audio_state = AUDIO_STATE_IDLE;
        audio_mode = AUDIO_MODE_NONE;
        song_data = NULL;
    }
}

/*********************************************************************
 * @fn      Audio_PlayStart
 *
 * @brief   Start playing audio from given data buffer.
 *
 * @param   data   - pointer to 16bit audio samples.
 * @param   length - total number of samples.
 *
 * @return  none
 *********************************************************************/
void Audio_PlayStart(const uint16_t *data, uint32_t length)
{
    song_data = data;
    song_data_length = length;
    audio_data_offset = 0;
    audio_mode = AUDIO_MODE_MEMORY;
    audio_state = AUDIO_STATE_PLAYING;

    /* 预填充两个 buffer，避免启动噪声 */
    Audio_FillBuffer(audio_buffer_A, AUDIO_BUFFER_SIZE);
}

/*********************************************************************
 * @fn      Audio_PlayStop
 *
 * @brief   Stop audio playback.
 *
 * @return  none
 *********************************************************************/
void Audio_PlayStop(void)
{
    audio_mode = AUDIO_MODE_NONE;
    audio_state = AUDIO_STATE_STOPPED;
    song_data = NULL;
    wav_eof = 0;
    CS43131_g.samples_played = 0;
    CS43131_g.track_name[0] = '\0';
    if (wav_file_opened) {
        CH378FileClose(0);
        wav_file_opened = 0;
    }
}

/*********************************************************************
 * @fn      Audio_PlaySineStart
 *
 * @brief   Start playing 1kHz sine wave for hardware test.
 *
 * @return  none
 *********************************************************************/
void Audio_PlaySineStart(void)
{
    sine_phase = 0;
    audio_mode = AUDIO_MODE_SINE;
    audio_state = AUDIO_STATE_PLAYING;

    Audio_GenerateSineWave(audio_buffer_A, AUDIO_BUFFER_SIZE);

    printf("Audio_PlaySineStart: 1kHz sine wave started\r\n");
}

/*********************************************************************
 * @fn      Audio_IsPlaying
 *
 * @brief   Check if audio is currently playing.
 *
 * @return  1 if playing, 0 otherwise
 *********************************************************************/
/*********************************************************************
 * @fn      Audio_SetVolume
 *
 * @brief   Set playback volume (0~100).
 *
 * @param   vol - 0=mute, 100=max
 *
 * @return  none
 *********************************************************************/
void Audio_SetVolume(uint8_t vol)
{
    if (vol > 100) vol = 100;
    CS43131_g.volume = vol;
    /* CS43131 PCM Volume: 0x20=0dB(max), ~0xC0=mute.
     * Linear map: 100 → 0x20, 0 → 0xC0 */
    uint8_t reg_val = (uint8_t)(((100 - vol) * 160) / 100) + 32;
    CS43131_I2C_WriteReg(0x090001, reg_val); /* Volume B */
    CS43131_I2C_WriteReg(0x090002, reg_val); /* Volume A */
}

/*********************************************************************
 * @fn      Audio_GetVolume
 *
 * @brief   Get current volume.
 *
 * @return  0~100
 *********************************************************************/
uint8_t Audio_GetVolume(void)
{
    return CS43131_g.volume;
}

/*********************************************************************
 * @fn      Audio_Pause
 *
 * @brief   Pause playback.
 *
 * @return  none
 *********************************************************************/
void Audio_Pause(void)
{
    if (audio_state == AUDIO_STATE_PLAYING) {
        audio_state = AUDIO_STATE_PAUSED;
    }
}

/*********************************************************************
 * @fn      Audio_Resume
 *
 * @brief   Resume playback.
 *
 * @return  none
 *********************************************************************/
void Audio_Resume(void)
{
    if (audio_state == AUDIO_STATE_PAUSED) {
        audio_state = AUDIO_STATE_PLAYING;
    }
}

/*********************************************************************
 * @fn      Audio_GetPlayTime_ms
 *
 * @brief   Get current playback time in milliseconds.
 *
 * @return  time in ms
 *********************************************************************/
uint32_t Audio_GetPlayTime_ms(void)
{
    /* audio_samples_played is in channel-samples.
     * Stereo 44.1kHz = 88200 channel-samples/sec.
     * ms = samples * 1000 / 88200 */
    return (CS43131_g.samples_played * 1000ULL) / 88200;
}

/*********************************************************************
 * @fn      Audio_GetCurrentTrackName
 *
 * @brief   Get current track name.
 *
 * @return  pointer to track name string, or empty string if none
 *********************************************************************/
const char* Audio_GetCurrentTrackName(void)
{
    return CS43131_g.track_name;
}

/*********************************************************************
 * @fn      Audio_SetCurrentTrack
 *
 * @brief   Set current track name.
 *
 * @param   name - track name string
 *
 * @return  none
 *********************************************************************/
void Audio_SetCurrentTrack(const char *name)
{
    if (name) {
        strncpy(CS43131_g.track_name, name, sizeof(CS43131_g.track_name) - 1);
        CS43131_g.track_name[sizeof(CS43131_g.track_name) - 1] = '\0';
    } else {
        CS43131_g.track_name[0] = '\0';
    }
}

uint8_t Audio_IsPlaying(void)
{
    return (audio_state == AUDIO_STATE_PLAYING);
}

/*********************************************************************
 * @fn      Audio_ParseWAVHeader
 *
 * @brief   Parse standard WAV file header.
 *
 * @param   buf   - pointer to header data (at least 44 bytes).
 * @param   info  - pointer to wav_info_t to fill.
 *
 * @return  0 on success, non-zero on error
 *********************************************************************/
uint8_t Audio_ParseWAVHeader(uint8_t *buf, wav_info_t *info)
{
    if (buf[0] != 'R' || buf[1] != 'I' || buf[2] != 'F' || buf[3] != 'F') {
        printf("WAV: not a RIFF file\r\n");
        return 1;
    }
    if (buf[8] != 'W' || buf[9] != 'A' || buf[10] != 'V' || buf[11] != 'E') {
        printf("WAV: not a WAVE file\r\n");
        return 2;
    }

    /* 标准 44 字节头解析 */
    info->num_channels   = buf[22] | (buf[23] << 8);
    info->sample_rate    = buf[24] | (buf[25] << 8) | (buf[26] << 16) | (buf[27] << 24);
    info->bits_per_sample = buf[34] | (buf[35] << 8);
    info->data_offset    = 44;
    info->data_size      = buf[40] | (buf[41] << 8) | (buf[42] << 16) | (buf[43] << 24);

    if (info->sample_rate != 44100 || info->bits_per_sample != 16 || info->num_channels != 2) {
        printf("WAV: unsupported format (SR=%lu, Bits=%u, Ch=%u)\r\n",
               info->sample_rate, info->bits_per_sample, info->num_channels);
        return 3;
    }
    return 0;
}

/*********************************************************************
 * @fn      Audio_PlayWAV_Start
 *
 * @brief   Start streaming WAV playback. File must already be opened
 *          and positioned at data chunk by caller (CLI).
 *
 * @param   info - parsed WAV header info.
 *
 * @return  none
 *********************************************************************/
void Audio_PlayWAV_Start(wav_info_t *info)
{
    uint8_t temp_buf[4096];
    uint16_t real_len;
    uint8_t status;

    Audio_PlayStop();
    rb_write_pos = 0;
    rb_read_pos = 0;
    wav_eof = 0;
    memcpy(&wav_info, info, sizeof(wav_info_t));
    wav_file_opened = 1; /* 文件已由 CLI 打开并定位到 data_offset */

    /* 预填充 ring buffer 到约 16KB */
    while (rb_used() < 16384) {
        status = CH378ByteRead(temp_buf, sizeof(temp_buf), &real_len);
        if (status != ERR_SUCCESS || real_len == 0) {
            wav_eof = 1;
            break;
        }
        rb_write_data(temp_buf, real_len);
        if (real_len < sizeof(temp_buf)) {
            wav_eof = 1;
            break;
        }
    }

    if (wav_eof) {
        CH378FileClose(0);
        wav_file_opened = 0;
    }

    audio_mode = AUDIO_MODE_WAV;
    audio_state = AUDIO_STATE_PLAYING;
    printf("Audio_PlayWAV_Start: streaming started, rb_used=%lu\r\n", rb_used());
}

/*********************************************************************
 * @fn      Audio_Process
 *
 * @brief   Main-loop polling for audio streaming.
 *          Must be called periodically (e.g. every 1~10ms in main loop).
 *
 * @return  none
 *********************************************************************/
void Audio_Process(void)
{
    uint8_t status;
    uint16_t real_len;
    uint8_t temp_buf[4096];

    if (audio_mode != AUDIO_MODE_WAV) return;
    if (!wav_file_opened) return;
    if (wav_eof) return;

    /* 每次尽可能多地读取，直到 ring buffer 空闲不足或文件结束 */
    while (rb_free() >= sizeof(temp_buf)) {
        status = CH378ByteRead(temp_buf, sizeof(temp_buf), &real_len);
        if (status != ERR_SUCCESS || real_len == 0) {
            wav_eof = 1;
            CH378FileClose(0);
            wav_file_opened = 0;
            break;
        }
        rb_write_data(temp_buf, real_len);
        if (real_len < sizeof(temp_buf)) {
            wav_eof = 1;
            CH378FileClose(0);
            wav_file_opened = 0;
            break;
        }
    }
}

/*********************************************************************
 * @fn      DMA1_Channel1_IRQHandler
 *
 * @brief   DMA1 Channel1 interrupt handler for I2S1 TX double buffer.
 *
 * @return  none
 *********************************************************************/
#define AUDIO_HALF_SIZE (AUDIO_BUFFER_SIZE / 2)

static void fill_audio_half(uint16_t *buf)
{
    /* 暂停状态：填静音，保持进度不增加 */
    if (audio_state == AUDIO_STATE_PAUSED) {
        memset(buf, 0, AUDIO_HALF_SIZE * sizeof(uint16_t));
        return;
    }

    if (audio_mode == AUDIO_MODE_MEMORY) {
        Audio_FillBuffer(buf, AUDIO_HALF_SIZE);
    } else if (audio_mode == AUDIO_MODE_SINE) {
        Audio_GenerateSineWave(buf, AUDIO_HALF_SIZE);
    } else if (audio_mode == AUDIO_MODE_WAV) {
        if (rb_used() >= AUDIO_HALF_SIZE * sizeof(uint16_t)) {
            rb_read_to_buffer(buf, AUDIO_HALF_SIZE);
        } else {
            memset(buf, 0, AUDIO_HALF_SIZE * sizeof(uint16_t));
            if (wav_eof && rb_used() == 0) {
                audio_state = AUDIO_STATE_IDLE;
                audio_mode = AUDIO_MODE_NONE;
                printf("Audio: playback finished\r\n");
            }
        }
    } else {
        memset(buf, 0, AUDIO_HALF_SIZE * sizeof(uint16_t));
    }
}

static void update_playback_progress(void)
{
    if (audio_state != AUDIO_STATE_PLAYING) return;

    if (audio_mode == AUDIO_MODE_WAV) {
        /* 只有实际从 ring buffer 读出数据时才计进度 */
        if (rb_used() >= AUDIO_HALF_SIZE * sizeof(uint16_t)) {
            CS43131_g.samples_played += AUDIO_HALF_SIZE;
        }
    } else if (audio_mode != AUDIO_MODE_NONE) {
        CS43131_g.samples_played += AUDIO_HALF_SIZE;
    }
}

void DMA1_Channel1_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1, DMA1_IT_HT1) == SET)
    {
        DMA_ClearITPendingBit(DMA1, DMA1_IT_HT1);
        /* DMA 正在传后半部分，安全地填前半部分 */
        fill_audio_half(audio_buffer_A);
        update_playback_progress();
    }

    if (DMA_GetITStatus(DMA1, DMA1_IT_TC1) == SET)
    {
        DMA_ClearITPendingBit(DMA1, DMA1_IT_TC1);
        /* DMA 刚回到开头，安全地填后半部分 */
        fill_audio_half(audio_buffer_A + AUDIO_HALF_SIZE);
        update_playback_progress();
    }
}
