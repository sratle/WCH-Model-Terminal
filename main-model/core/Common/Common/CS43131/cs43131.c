/********************************** (C) COPYRIGHT *******************************
 * File Name          : cs43131.c
 * Author             : 
 * Version            : V2.0.0
 * Date               : 2025/06/09
 * Description        : CS43131 DAC + multi-channel audio engine + effects.
 *******************************************************************************/
#include "cs43131.h"
#include "./I2c_soft/i2c_soft.h"
#include "ch32h417_dma.h"
#include "ch32h417_spi.h"
#include "ch32h417_rcc.h"
#include "debug.h"
#include "CH378/CH378.h"
#include <string.h>

extern cs43131_t CS43131_g;

#define AUDIO_HALF_SIZE (AUDIO_BUFFER_SIZE / 2)

/* ======================================================================== */
/*  Speaker (FM8002A)                                                        */
/* ======================================================================== */

void Speaker_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOD, ENABLE);
    GPIO_InitStructure.GPIO_Pin = SPEAKER_LEFT_SHUTDOWN_PIN | SPEAKER_RIGHT_SHUTDOWN_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_SetBits(SPEAKER_LEFT_SHUTDOWN_GPIO_PORT, SPEAKER_LEFT_SHUTDOWN_PIN);
    GPIO_SetBits(SPEAKER_RIGHT_SHUTDOWN_GPIO_PORT, SPEAKER_RIGHT_SHUTDOWN_PIN);
    printf("Speaker_Init: FM8002A shutdown pins initialized (muted)\r\n");
}

void Speaker_On(speaker_channel_t channel)
{
    if (channel & SPEAKER_CHANNEL_LEFT)
        GPIO_ResetBits(SPEAKER_LEFT_SHUTDOWN_GPIO_PORT, SPEAKER_LEFT_SHUTDOWN_PIN);
    if (channel & SPEAKER_CHANNEL_RIGHT)
        GPIO_ResetBits(SPEAKER_RIGHT_SHUTDOWN_GPIO_PORT, SPEAKER_RIGHT_SHUTDOWN_PIN);
}

void Speaker_Off(speaker_channel_t channel)
{
    if (channel & SPEAKER_CHANNEL_LEFT)
        GPIO_SetBits(SPEAKER_LEFT_SHUTDOWN_GPIO_PORT, SPEAKER_LEFT_SHUTDOWN_PIN);
    if (channel & SPEAKER_CHANNEL_RIGHT)
        GPIO_SetBits(SPEAKER_RIGHT_SHUTDOWN_GPIO_PORT, SPEAKER_RIGHT_SHUTDOWN_PIN);
}

speaker_channel_t Speaker_GetState(void)
{
    speaker_channel_t state = 0;
    if (GPIO_ReadOutputDataBit(SPEAKER_LEFT_SHUTDOWN_GPIO_PORT,
                               SPEAKER_LEFT_SHUTDOWN_PIN) == Bit_RESET)
        state |= SPEAKER_CHANNEL_LEFT;
    if (GPIO_ReadOutputDataBit(SPEAKER_RIGHT_SHUTDOWN_GPIO_PORT,
                               SPEAKER_RIGHT_SHUTDOWN_PIN) == Bit_RESET)
        state |= SPEAKER_CHANNEL_RIGHT;
    return state;
}

/* ======================================================================== */
/*  CS43131 I2C Config                                                       */
/* ======================================================================== */

static uint8_t CS43131_I2C_WriteReg(uint32_t addr, uint8_t dat)
{
    uint8_t ret = SoftI2C_WriteReg(CS43131_I2C_ADDR, addr, dat);
    if (ret) printf("I2C_WR_ERR: 0x%06X=0x%02X\r\n", addr, dat);
    return ret;
}

static uint8_t CS43131_I2C_ReadReg(uint32_t addr)
{
    uint8_t dat = 0;
    uint8_t ret = SoftI2C_ReadReg(CS43131_I2C_ADDR, addr, &dat);
    if (ret) { printf("I2C_RD_ERR: 0x%06X\r\n", addr); return 0xFF; }
    return dat;
}

void CS43131_I2C_Config(void)
{
    uint8_t tmp;
    printf("CS43131_I2C_Config\r\n");
    CS43131_I2C_WriteReg(0x010006, 0x06);
    CS43131_I2C_WriteReg(0x020052, 0x04);
    CS43131_I2C_WriteReg(0x020000, 0xF6);
    Delay_Ms(100);
    tmp = CS43131_I2C_ReadReg(0x0F0000);
    printf("CS43131_I2C_Config: XTAL powered, status=0x%02X\r\n", tmp);
    CS43131_I2C_WriteReg(0x040004, 0x00);
    CS43131_I2C_WriteReg(0x020000, 0xF4);
    CS43131_I2C_WriteReg(0x010006, 0x04);
    Delay_Ms(10);
    printf("CS43131_I2C_Config: clock source switched to XTAL 22.5792MHz\r\n");
    CS43131_I2C_WriteReg(0x01000B, 0x01);
    CS43131_I2C_WriteReg(0x01000C, 0x02);
    CS43131_I2C_WriteReg(0x040010, 0x01);
    CS43131_I2C_WriteReg(0x040011, 0x00);
    CS43131_I2C_WriteReg(0x040012, 0x10);
    CS43131_I2C_WriteReg(0x040013, 0x00);
    CS43131_I2C_WriteReg(0x040014, 0x0F);
    CS43131_I2C_WriteReg(0x040015, 0x00);
    CS43131_I2C_WriteReg(0x040016, 0x1F);
    CS43131_I2C_WriteReg(0x040017, 0x00);
    CS43131_I2C_WriteReg(0x040018, 0x1C);
    CS43131_I2C_WriteReg(0x040019, 0x0A);
    CS43131_I2C_WriteReg(0x050000, 0x00);
    CS43131_I2C_WriteReg(0x050001, 0x00);
    CS43131_I2C_WriteReg(0x05000A, 0x05);
    CS43131_I2C_WriteReg(0x05000B, 0x0D);
    CS43131_I2C_WriteReg(0x01000D, 0x02);
    printf("CS43131_I2C_Config: ASP I2S Master interface config\r\n");
    CS43131_I2C_WriteReg(0x090000, 0x02);
    Audio_SetVolume(60);
    CS43131_I2C_WriteReg(0x090003, 0xEC);
    CS43131_I2C_WriteReg(0x090004, 0x00);
    printf("CS43131_I2C_Config: PCM audio path config\r\n");
    CS43131_I2C_WriteReg(0x0B0000, 0x1E);
    CS43131_I2C_WriteReg(0x080000, 0x30);
    CS43131_I2C_WriteReg(0x0D0000, 0x04);
    printf("CS43131_I2C_Config: headphone output hardware config\r\n");
    CS43131_I2C_WriteReg(0x010010, 0x99);
    CS43131_I2C_WriteReg(0x080032, 0x20);
    CS43131_I2C_WriteReg(0x020000, 0xB4);
    CS43131_I2C_WriteReg(0x020000, 0xA4);
    Delay_Ms(22);
    CS43131_I2C_WriteReg(0x010010, 0x00);
    CS43131_I2C_WriteReg(0x080032, 0x00);
    CS43131_I2C_WriteReg(0x090003, 0xEC);
}

/* ======================================================================== */
/*  CS43131 + I2S + DMA Init                                                 */
/* ======================================================================== */

void CS43131_init(cs43131_t *cs43131)
{
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2S_InitTypeDef I2S_InitStructure = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOA | RCC_HB2Periph_GPIOB | RCC_HB2Periph_AFIO, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI2, ENABLE);

    GPIO_PinAFConfig(I2S_WS_GPIO_PORT, GPIO_PinSource12, I2S_WS_AF);
    GPIO_PinAFConfig(I2S_CK_GPIO_PORT, GPIO_PinSource13, I2S_CK_AF);
    GPIO_PinAFConfig(I2S_SDO_GPIO_PORT, GPIO_PinSource15, I2S_SDO_AF);

    GPIO_InitStructure.GPIO_Pin = I2S_WS_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(I2S_WS_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = I2S_CK_PIN;
    GPIO_Init(I2S_CK_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = I2S_SDO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(I2S_SDO_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = CS43131_OE_PIN | CS43131_RST_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(CS43131_OE_GPIO_PORT, &GPIO_InitStructure);
    GPIO_SetBits(CS43131_OE_GPIO_PORT, CS43131_OE_PIN);
    GPIO_ResetBits(CS43131_RST_GPIO_PORT, CS43131_RST_PIN);

    SoftI2C_Init();
    CS43131_I2C_Config();

    I2S_StructInit(&I2S_InitStructure);
    I2S_InitStructure.I2S_Mode = I2S_Mode_SlaveTx;
    I2S_InitStructure.I2S_Standard = I2S_Standard_Phillips;
    I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_16b;
    I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
    I2S_InitStructure.I2S_AudioFreq = I2S_AudioFreq_44k;
    I2S_InitStructure.I2S_CPOL = I2S_CPOL_Low;
    I2S_Init(SPI2, &I2S_InitStructure);

    I2S1_DMA_DoubleBufferInit();
    I2S_Cmd(SPI2, ENABLE);
    Speaker_Init();
    cs43131->enable = 1;
}

void I2S1_DMA_DoubleBufferInit(void)
{
    DMA_InitTypeDef DMA_InitStructure = {0};
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);
    DMA_DeInit(DMA1_Channel1);
    DMA_StructInit(&DMA_InitStructure);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DATAR;
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)CS43131_g.dma_buf;
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

/* ======================================================================== */
/*  Per-Channel Ring Buffer Helpers                                          */
/* ======================================================================== */

static uint32_t ch_rb_used(audio_channel_t *ch)
{
    return (ch->rb_write_pos + AUDIO_CH_RB_SIZE - ch->rb_read_pos) % AUDIO_CH_RB_SIZE;
}

static uint32_t ch_rb_free(audio_channel_t *ch)
{
    return AUDIO_CH_RB_SIZE - ch_rb_used(ch) - 1;
}

static void ch_rb_write(audio_channel_t *ch, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        ch->rb[ch->rb_write_pos] = data[i];
        ch->rb_write_pos = (ch->rb_write_pos + 1) % AUDIO_CH_RB_SIZE;
    }
}

/* ======================================================================== */
/*  Biquad Filter (Q13 fixed-point)                                         */
/* ======================================================================== */

#define BQ_SHIFT 13
#define BQ_UNITY (1 << BQ_SHIFT)  /* 8192 */

static inline int16_t biquad_process(biquad_t *bq, int16_t x)
{
    int32_t acc;
    int32_t x0 = x;
    acc  = bq->b0 * x0;
    acc += bq->b1 * bq->x1;
    acc += bq->b2 * bq->x2;
    acc -= bq->a1 * bq->y1;
    acc -= bq->a2 * bq->y2;
    acc >>= BQ_SHIFT;
    bq->x2 = bq->x1; bq->x1 = x0;
    bq->y2 = bq->y1;
    if (acc > 32767) acc = 32767;
    if (acc < -32768) acc = -32768;
    bq->y1 = (int16_t)acc;
    return (int16_t)acc;
}

/* ======================================================================== */
/*  EQ: Pre-computed biquad coefficients (Q13)                              */
/*  Bass=100Hz, Mid=1kHz, Treble=10kHz @ 44.1kHz, Q≈0.707                  */
/* ======================================================================== */

/* Target coefficients at +6dB gain, stored as (coeff_Q13, a_sign_for_a1a2) */
/* Bass 100Hz low-shelf +6dB: b0=8931 b1=-17051 b2=8512 a1=-15640 a2=6823 */
/* Mid  1kHz peaking  +6dB:   b0=11121 b1=-12858 b2=6313 a1=-12858 a2=4666 */
/* Treble 10kHz high-shelf+6dB: b0=14762 b1=-21541 b2=9143 a1=-18751 a2=7566 */

static const int16_t eq_boost_b[3][3] = {
    { 8931, -17051,  8512},  /* bass  b0,b1,b2 */
    {11121, -12858,  6313},  /* mid   b0,b1,b2 */
    {14762, -21541,  9143},  /* treble b0,b1,b2 */
};
static const int16_t eq_boost_a[3][2] = {
    {-15640,  6823},  /* bass  a1,a2 */
    {-12858,  4666},  /* mid   a1,a2 */
    {-18751,  7566},  /* treble a1,a2 */
};

void Audio_EQ_UpdateCoeffs(void)
{
    int band;
    int16_t gains[3];
    gains[0] = CS43131_g.eq.bass_gain_db;
    gains[1] = CS43131_g.eq.mid_gain_db;
    gains[2] = CS43131_g.eq.treble_gain_db;

    for (band = 0; band < 3; band++) {
        int16_t g = gains[band];
        biquad_t *bq = &CS43131_g.eq.bands[band];
        if (g > 12) g = 12;
        if (g < -12) g = -12;
        if (g == 0) {
            bq->b0 = BQ_UNITY; bq->b1 = 0; bq->b2 = 0;
            bq->a1 = 0; bq->a2 = 0;
        } else {
            int16_t abs_g = (g > 0) ? g : -g;
            /* lerp: coeff = UNITY + abs_g/6 * (target - UNITY) */
            bq->b0 = BQ_UNITY + (int32_t)abs_g * (eq_boost_b[band][0] - BQ_UNITY) / 6;
            bq->b1 = (int32_t)abs_g * eq_boost_b[band][1] / 6;
            bq->b2 = (int32_t)abs_g * eq_boost_b[band][2] / 6;
            bq->a1 = (int32_t)abs_g * eq_boost_a[band][0] / 6;
            bq->a2 = (int32_t)abs_g * eq_boost_a[band][1] / 6;
            /* For cut (negative gain), negate all target coefficients */
            if (g < 0) {
                bq->b0 = 2 * BQ_UNITY - bq->b0;  /* = UNITY - (bq->b0 - UNITY) */
                bq->b1 = -bq->b1;
                bq->b2 = -bq->b2;
                bq->a1 = -bq->a1;
                bq->a2 = -bq->a2;
            }
        }
    }
}

/* ======================================================================== */
/*  Compressor                                                               */
/* ======================================================================== */

static int32_t comp_abs_db(int16_t sample)
{
    /* Rough dB estimate: 20*log10(|sample|/32768) ≈ 6*(bitpos-15) */
    uint16_t abs_s = (sample < 0) ? (uint16_t)(-sample) : (uint16_t)sample;
    if (abs_s == 0) return -96;
    int bit = 0;
    uint16_t v = abs_s;
    while (v) { bit++; v >>= 1; }
    return (int32_t)(bit - 15) * 6;
}

static void compressor_process(audio_compressor_t *comp, int16_t *buf, uint32_t len)
{
    uint32_t i;
    int32_t attack_coeff = 983;   /* ~0.12 in Q13, fast attack */
    int32_t release_coeff = 131;  /* ~0.016 in Q13, slow release */
    int32_t threshold = comp->threshold_db;
    int32_t ratio = comp->ratio;

    for (i = 0; i < len; i++) {
        int32_t in_db = comp_abs_db(buf[i]);
        int32_t gain_db = 0;

        /* Envelope follower */
        if (in_db > comp->env)
            comp->env += ((in_db - comp->env) * attack_coeff) >> BQ_SHIFT;
        else
            comp->env += ((in_db - comp->env) * release_coeff) >> BQ_SHIFT;

        /* Gain reduction */
        if (comp->env > threshold && ratio > 1) {
            gain_db = -((comp->env - threshold) * (ratio - 1)) / ratio;
        }

        /* Apply gain reduction + makeup in one step (approximate) */
        {
            int32_t total_db = gain_db + comp->makeup_db;
            /* Convert dB to linear: approximate 2^(db/6) via shift */
            int32_t samp = buf[i];
            if (total_db < 0) {
                int32_t atten = (-total_db);
                if (atten > 48) atten = 48;
                samp = samp >> (atten / 6);  /* rough -6dB per halving */
            } else if (total_db > 0) {
                int32_t boost = total_db;
                if (boost > 24) boost = 24;
                samp = samp * (1 + boost / 6);
            }
            if (samp > 32767) samp = 32767;
            if (samp < -32768) samp = -32768;
            buf[i] = (int16_t)samp;
        }
    }
}

/* ======================================================================== */
/*  Echo / Delay                                                             */
/* ======================================================================== */

static void echo_process(audio_echo_t *echo, int16_t *buf, uint32_t len)
{
    uint32_t i;
    int32_t fb = (int32_t)echo->feedback;   /* 0~200 → scale /256 */
    int32_t wet = (int32_t)echo->mix;       /* 0~100 → scale /100 */
    uint16_t dlen = echo->delay_len;
    uint16_t dpos = echo->delay_pos;

    if (dlen == 0) return;

    for (i = 0; i < len; i += 2) {
        /* Read delayed samples (stereo interleaved) */
        uint16_t rd_idx = (dpos * 2);
        int16_t del_l = echo->delay_line[rd_idx];
        int16_t del_r = echo->delay_line[rd_idx + 1];

        /* Mix dry + wet */
        int32_t out_l = (int32_t)buf[i]     + (del_l * wet) / 100;
        int32_t out_r = (int32_t)buf[i + 1] + (del_r * wet) / 100;
        if (out_l > 32767) out_l = 32767;
        if (out_l < -32768) out_l = -32768;
        if (out_r > 32767) out_r = 32767;
        if (out_r < -32768) out_r = -32768;
        buf[i]     = (int16_t)out_l;
        buf[i + 1] = (int16_t)out_r;

        /* Write to delay line: input + feedback */
        int32_t wr_l = (int32_t)buf[i]     + (del_l * fb) / 256;
        int32_t wr_r = (int32_t)buf[i + 1] + (del_r * fb) / 256;
        if (wr_l > 32767) wr_l = 32767;
        if (wr_l < -32768) wr_l = -32768;
        if (wr_r > 32767) wr_r = 32767;
        if (wr_r < -32768) wr_r = -32768;
        echo->delay_line[rd_idx]     = (int16_t)wr_l;
        echo->delay_line[rd_idx + 1] = (int16_t)wr_r;

        dpos = (dpos + 1) % dlen;
    }
    echo->delay_pos = dpos;
}

/* ======================================================================== */
/*  Effects chain: apply all enabled effects to a buffer                     */
/* ======================================================================== */

static void effects_chain_process(int16_t *buf, uint32_t len)
{
    if (!CS43131_g.fx_master_enable) return;  /* 总开关关闭，旁路所有效果器 */

    if (CS43131_g.eq.enable) {
        uint32_t i;
        for (i = 0; i < len; i++) {
            int16_t s = buf[i];
            s = biquad_process(&CS43131_g.eq.bands[0], s);
            s = biquad_process(&CS43131_g.eq.bands[1], s);
            s = biquad_process(&CS43131_g.eq.bands[2], s);
            buf[i] = s;
        }
    }
    if (CS43131_g.compressor.enable) {
        compressor_process(&CS43131_g.compressor, buf, len);
    }
    if (CS43131_g.echo.enable) {
        echo_process(&CS43131_g.echo, buf, len);
    }
}

/* ======================================================================== */
/*  Channel Management                                                       */
/* ======================================================================== */

uint8_t Audio_ChannelStart(uint8_t ch, const char *path, const wav_info_t *info)
{
    audio_channel_t *c;
    if (ch >= AUDIO_MAX_CHANNELS || !path || !info) return 1;
    c = &CS43131_g.channels[ch];

    /* Stop channel if already active */
    if (c->active) Audio_ChannelStop(ch);

    /* Initialize channel */
    memset(c, 0, sizeof(audio_channel_t));
    strncpy(c->path, path, sizeof(c->path) - 1);
    memcpy(&c->wav_info, info, sizeof(wav_info_t));
    c->read_offset = info->data_offset;
    c->volume = 100;
    c->pan = 128;
    c->state = AUDIO_STATE_PLAYING;
    c->active = 1;
    c->eof = 0;
    c->rb_write_pos = 0;
    c->rb_read_pos = 0;

    CS43131_g.active_channel_count++;
    CS43131_g.audio_state = AUDIO_STATE_PLAYING;
    CS43131_g.current_channel = ch;
    CS43131_g.status_dirty = 1;

    printf("Audio: ch%d start '%s' (%luHz %dbit %dch)\r\n",
           ch, path,
           (unsigned long)info->sample_rate,
           info->bits_per_sample, info->num_channels);
    return 0;
}

void Audio_ChannelStop(uint8_t ch)
{
    audio_channel_t *c;
    if (ch >= AUDIO_MAX_CHANNELS) return;
    c = &CS43131_g.channels[ch];
    if (!c->active) return;

    /* Close CH378 file if open */
    if (c->file_open) {
        CH378FileClose(0);
        c->file_open = 0;
    }

    c->active = 0;
    c->state = AUDIO_STATE_STOPPED;
    c->eof = 0;
    c->rb_write_pos = 0;
    c->rb_read_pos = 0;
    c->samples_played = 0;
    c->track_name[0] = '\0';

    if (CS43131_g.active_channel_count > 0)
        CS43131_g.active_channel_count--;
    if (CS43131_g.active_channel_count == 0)
        CS43131_g.audio_state = AUDIO_STATE_IDLE;
    CS43131_g.status_dirty = 1;
}

void Audio_StopAll(void)
{
    uint8_t i;
    for (i = 0; i < AUDIO_MAX_CHANNELS; i++)
        Audio_ChannelStop(i);
}

void Audio_ChannelPause(uint8_t ch)
{
    if (ch >= AUDIO_MAX_CHANNELS) return;
    if (CS43131_g.channels[ch].state == AUDIO_STATE_PLAYING) {
        CS43131_g.channels[ch].state = AUDIO_STATE_PAUSED;
        CS43131_g.status_dirty = 1;
    }
}

void Audio_ChannelResume(uint8_t ch)
{
    if (ch >= AUDIO_MAX_CHANNELS) return;
    if (CS43131_g.channels[ch].state == AUDIO_STATE_PAUSED) {
        CS43131_g.channels[ch].state = AUDIO_STATE_PLAYING;
        CS43131_g.status_dirty = 1;
    }
}

void Audio_ChannelSetVolume(uint8_t ch, uint8_t vol)
{
    if (ch >= AUDIO_MAX_CHANNELS) return;
    if (vol > 100) vol = 100;
    CS43131_g.channels[ch].volume = vol;
    CS43131_g.status_dirty = 1;
}

void Audio_ChannelSetPan(uint8_t ch, uint8_t pan)
{
    if (ch >= AUDIO_MAX_CHANNELS) return;
    CS43131_g.channels[ch].pan = pan;
}

audio_state_t Audio_ChannelGetState(uint8_t ch)
{
    if (ch >= AUDIO_MAX_CHANNELS) return AUDIO_STATE_IDLE;
    return CS43131_g.channels[ch].state;
}

uint32_t Audio_ChannelGetPlayTime_ms(uint8_t ch)
{
    if (ch >= AUDIO_MAX_CHANNELS) return 0;
    return (CS43131_g.channels[ch].samples_played * 1000ULL) / 88200;
}

uint32_t Audio_ChannelGetDuration_ms(uint8_t ch)
{
    audio_channel_t *c;
    uint32_t byte_rate;
    if (ch >= AUDIO_MAX_CHANNELS) return 0;
    c = &CS43131_g.channels[ch];
    if (c->wav_info.data_size == 0) return 0;
    byte_rate = c->wav_info.sample_rate * c->wav_info.num_channels *
                c->wav_info.bits_per_sample / 8;
    if (byte_rate == 0) return 0;
    return (c->wav_info.data_size * 1000ULL) / byte_rate;
}

const char* Audio_ChannelGetTrackName(uint8_t ch)
{
    if (ch >= AUDIO_MAX_CHANNELS) return "";
    return CS43131_g.channels[ch].track_name;
}

/* ======================================================================== */
/*  Master Controls                                                          */
/* ======================================================================== */

void Audio_SetVolume(uint8_t vol)
{
    if (vol > 100) vol = 100;
    CS43131_g.volume = vol;
    uint8_t reg_val = (uint8_t)(((100 - vol) * 160) / 100) + 32;
    CS43131_I2C_WriteReg(0x090001, reg_val);
    CS43131_I2C_WriteReg(0x090002, reg_val);
    CS43131_g.status_dirty = 1;
}

uint8_t Audio_GetVolume(void) { return CS43131_g.volume; }

void Audio_Pause(void)
{
    uint8_t i;
    for (i = 0; i < AUDIO_MAX_CHANNELS; i++)
        if (CS43131_g.channels[i].active)
            Audio_ChannelPause(i);
    CS43131_g.audio_state = AUDIO_STATE_PAUSED;
    CS43131_g.status_dirty = 1;
}

void Audio_Resume(void)
{
    uint8_t i;
    for (i = 0; i < AUDIO_MAX_CHANNELS; i++)
        if (CS43131_g.channels[i].active)
            Audio_ChannelResume(i);
    if (CS43131_g.active_channel_count > 0)
        CS43131_g.audio_state = AUDIO_STATE_PLAYING;
    CS43131_g.status_dirty = 1;
}

audio_state_t Audio_GetState(void) { return CS43131_g.audio_state; }

uint8_t Audio_IsPlaying(void)
{
    return (CS43131_g.audio_state == AUDIO_STATE_PLAYING);
}

uint8_t Audio_IsStatusDirty(void) { return CS43131_g.status_dirty; }
void Audio_ClearStatusDirty(void) { CS43131_g.status_dirty = 0; }

/* ======================================================================== */
/*  CH378 Sharing                                                            */
/* ======================================================================== */

void Audio_CH378_Lock(void)   { CS43131_g.ch378_locked = 1; }
void Audio_CH378_Unlock(void) { CS43131_g.ch378_locked = 0; }

uint8_t Audio_IsStreaming(void)
{
    uint8_t i;
    for (i = 0; i < AUDIO_MAX_CHANNELS; i++)
        if (CS43131_g.channels[i].active && !CS43131_g.channels[i].eof)
            return 1;
    return 0;
}

/* ======================================================================== */
/*  Effects Control API                                                      */
/* ======================================================================== */

void Audio_FX_MasterEnable(uint8_t en)
{
    CS43131_g.fx_master_enable = en ? 1 : 0;
    if (!en) {
        /* 关闭时禁用所有子效果器，清空状态 */
        CS43131_g.eq.enable = 0;
        CS43131_g.compressor.enable = 0;
        if (CS43131_g.echo.enable) {
            memset(CS43131_g.echo.delay_line, 0, sizeof(CS43131_g.echo.delay_line));
            CS43131_g.echo.enable = 0;
        }
    }
}

uint8_t Audio_FX_IsMasterEnabled(void) { return CS43131_g.fx_master_enable; }

void Audio_EQ_SetBass(int16_t db)   { CS43131_g.eq.bass_gain_db = db; Audio_EQ_UpdateCoeffs(); }
void Audio_EQ_SetMid(int16_t db)    { CS43131_g.eq.mid_gain_db = db; Audio_EQ_UpdateCoeffs(); }
void Audio_EQ_SetTreble(int16_t db) { CS43131_g.eq.treble_gain_db = db; Audio_EQ_UpdateCoeffs(); }
void Audio_EQ_Enable(uint8_t en)    { CS43131_g.eq.enable = en ? 1 : 0; }

void Audio_Comp_SetParams(int16_t threshold_db, int16_t ratio, int16_t makeup_db)
{
    CS43131_g.compressor.threshold_db = threshold_db;
    CS43131_g.compressor.ratio = ratio;
    CS43131_g.compressor.makeup_db = makeup_db;
}
void Audio_Comp_Enable(uint8_t en) { CS43131_g.compressor.enable = en ? 1 : 0; }

void Audio_Echo_SetParams(uint16_t delay_ms, uint8_t feedback, uint8_t mix)
{
    if (delay_ms > 100) delay_ms = 100;
    if (delay_ms < 10) delay_ms = 10;
    CS43131_g.echo.delay_ms = delay_ms;
    CS43131_g.echo.feedback = feedback;
    CS43131_g.echo.mix = mix;
    CS43131_g.echo.delay_len = (uint16_t)((uint32_t)delay_ms * 44100 / 1000);
    if (CS43131_g.echo.delay_len > ECHO_DELAY_SAMPLES)
        CS43131_g.echo.delay_len = ECHO_DELAY_SAMPLES;
}
void Audio_Echo_Enable(uint8_t en)
{
    CS43131_g.echo.enable = en ? 1 : 0;
    if (en) {
        memset(CS43131_g.echo.delay_line, 0, sizeof(CS43131_g.echo.delay_line));
        CS43131_g.echo.delay_pos = 0;
    }
}

/* ======================================================================== */
/*  WAV Header Parser                                                        */
/* ======================================================================== */

uint8_t Audio_ParseWAVHeader(uint8_t *buf, wav_info_t *info)
{
    uint32_t pos, chunk_size;
    uint8_t found_fmt = 0, found_data = 0;

    if (buf[0] != 'R' || buf[1] != 'I' || buf[2] != 'F' || buf[3] != 'F') {
        printf("WAV: not a RIFF file\r\n"); return 1;
    }
    if (buf[8] != 'W' || buf[9] != 'A' || buf[10] != 'V' || buf[11] != 'E') {
        printf("WAV: not a WAVE file\r\n"); return 2;
    }
    pos = 12;
    while (pos + 8 <= 512) {
        chunk_size = buf[pos+4] | (buf[pos+5]<<8) | (buf[pos+6]<<16) | (buf[pos+7]<<24);
        if (buf[pos]=='f' && buf[pos+1]=='m' && buf[pos+2]=='t' && buf[pos+3]==' ') {
            if (chunk_size >= 16 && pos + 24 <= 512) {
                info->num_channels    = buf[pos+10] | (buf[pos+11]<<8);
                info->sample_rate     = buf[pos+12] | (buf[pos+13]<<8) |
                                        (buf[pos+14]<<16) | (buf[pos+15]<<24);
                info->bits_per_sample = buf[pos+22] | (buf[pos+23]<<8);
                found_fmt = 1;
            }
        } else if (buf[pos]=='d' && buf[pos+1]=='a' && buf[pos+2]=='t' && buf[pos+3]=='a') {
            info->data_offset = pos + 8;
            info->data_size   = chunk_size;
            found_data = 1;
        }
        pos += 8 + ((chunk_size + 1) & ~1U);
    }
    if (!found_fmt) { printf("WAV: fmt chunk not found\r\n"); return 3; }
    if (!found_data) { printf("WAV: data chunk not found\r\n"); return 4; }
    if (info->sample_rate != 44100 || info->bits_per_sample != 16 || info->num_channels != 2) {
        printf("WAV: unsupported (SR=%lu Bits=%u Ch=%u)\r\n",
               info->sample_rate, info->bits_per_sample, info->num_channels);
        return 5;
    }
    return 0;
}

/* ======================================================================== */
/*  Audio_Process: Main Loop — CH378 Streaming (sequential read)             */
/* ======================================================================== */

static uint8_t stream_channel_idx = 0; /* round-robin index */

/* Low watermark: only read when buffer drops below this.
 * 16KB = ~93ms of audio at 44.1kHz/16bit/stereo. */
#define AUDIO_CH_LOW_WATERMARK (AUDIO_CH_RB_SIZE / 2)

/* Helper: close CH378 file for a channel if open */
static void channel_file_close(audio_channel_t *ch)
{
    if (ch->file_open) {
        CH378FileClose(0);
        ch->file_open = 0;
    }
}

/* Helper: open CH378 file for a channel and seek to read_offset */
static uint8_t channel_file_open(audio_channel_t *ch)
{
    uint8_t status;

    if (ch->file_open) return ERR_SUCCESS; /* already open */

    status = CH378FileOpen((uint8_t *)ch->path);
    if (status != ERR_SUCCESS) return status;

    status = CH378ByteLocate(ch->read_offset);
    if (status != ERR_SUCCESS) {
        CH378FileClose(0);
        return status;
    }
    ch->file_open = 1;
    return ERR_SUCCESS;
}

void Audio_Process(void)
{
    audio_channel_t *ch;
    uint8_t status;
    uint16_t real_len;
    uint8_t temp_buf[AUDIO_CH_READ_BLOCK];
    uint8_t attempts;

    /* Quick exit if nothing to stream */
    if (CS43131_g.active_channel_count == 0) return;

    /* If CH378 is locked by another module, close all open files and skip */
    if (CS43131_g.ch378_locked) {
        for (attempts = 0; attempts < AUDIO_MAX_CHANNELS; attempts++) {
            channel_file_close(&CS43131_g.channels[attempts]);
        }
        return;
    }

    /* Find next active, non-EOF channel that needs data (round-robin) */
    for (attempts = 0; attempts < AUDIO_MAX_CHANNELS; attempts++) {
        if (stream_channel_idx >= AUDIO_MAX_CHANNELS)
            stream_channel_idx = 0;
        ch = &CS43131_g.channels[stream_channel_idx];
        stream_channel_idx++;

        if (!ch->active || ch->eof) {
            /* Channel finished — close its file if still open */
            channel_file_close(ch);
            continue;
        }

        /* Only read when buffer drops below low watermark */
        if (ch_rb_used(ch) > AUDIO_CH_LOW_WATERMARK) continue;

        /* Also check there's enough free space for a full block */
        if (ch_rb_free(ch) < AUDIO_CH_READ_BLOCK) continue;

        /* Ensure file is open (reopen + seek if was closed by lock or other channel) */
        /* CH378 has only one file handle: close other channels first */
        {
            uint8_t j;
            for (j = 0; j < AUDIO_MAX_CHANNELS; j++) {
                if (j != (uint8_t)(ch - CS43131_g.channels))
                    channel_file_close(&CS43131_g.channels[j]);
            }
        }
        status = channel_file_open(ch);
        if (status != ERR_SUCCESS) {
            printf("Audio: ch%d open failed (0x%02X)\r\n",
                   (int)(ch - CS43131_g.channels), status);
            ch->eof = 1;
            continue;
        }

        /* Sequential read — no seek needed, CH378 continues from last position */
        status = CH378ByteRead(temp_buf, AUDIO_CH_READ_BLOCK, &real_len);

        if (status != ERR_SUCCESS || real_len == 0) {
            ch->eof = 1;
            channel_file_close(ch);
            continue;
        }

        ch_rb_write(ch, temp_buf, real_len);
        ch->read_offset += real_len;

        if (real_len < AUDIO_CH_READ_BLOCK) {
            ch->eof = 1;
            channel_file_close(ch);
        }
        break; /* Only serve one channel per call */
    }
}

/* ======================================================================== */
/*  DMA ISR: Mixer + Effects → DMA Buffer                                   */
/* ======================================================================== */

static void mixer_fill_half(uint16_t *out, uint32_t count)
{
    int16_t *buf = (int16_t *)out;
    uint32_t i, ch_idx;
    audio_channel_t *ch;
    int32_t vol, pan_l, pan_r;
    int32_t l, r;

    /* Step 1: Mix all active channels */
    for (i = 0; i < count; i += 2) {
        l = 0; r = 0;
        for (ch_idx = 0; ch_idx < AUDIO_MAX_CHANNELS; ch_idx++) {
            ch = &CS43131_g.channels[ch_idx];
            if (!ch->active || ch->state != AUDIO_STATE_PLAYING) continue;
            if (ch_rb_used(ch) < 4) continue; /* need 4 bytes for stereo */

            /* Read L and R samples */
            uint8_t lo_l = ch->rb[ch->rb_read_pos];
            ch->rb_read_pos = (ch->rb_read_pos + 1) % AUDIO_CH_RB_SIZE;
            uint8_t hi_l = ch->rb[ch->rb_read_pos];
            ch->rb_read_pos = (ch->rb_read_pos + 1) % AUDIO_CH_RB_SIZE;
            uint8_t lo_r = ch->rb[ch->rb_read_pos];
            ch->rb_read_pos = (ch->rb_read_pos + 1) % AUDIO_CH_RB_SIZE;
            uint8_t hi_r = ch->rb[ch->rb_read_pos];
            ch->rb_read_pos = (ch->rb_read_pos + 1) % AUDIO_CH_RB_SIZE;

            int16_t sl = (int16_t)(lo_l | (hi_l << 8));
            int16_t sr = (int16_t)(lo_r | (hi_r << 8));

            vol = (int32_t)ch->volume;
            /* pan: 0=full left, 128=center, 255=full right */
            pan_l = (255 - ch->pan);
            pan_r = ch->pan;

            l += (sl * vol * pan_l) >> 15;
            r += (sr * vol * pan_r) >> 15;

            ch->samples_played += 2;
        }
        /* Clip */
        if (l > 32767) l = 32767; if (l < -32768) l = -32768;
        if (r > 32767) r = 32767; if (r < -32768) r = -32768;
        buf[i]     = (int16_t)l;
        buf[i + 1] = (int16_t)r;
    }

    /* Step 2: Apply effects chain */
    effects_chain_process(buf, count);

    /* Step 3: Convert back to unsigned 16-bit for I2S */
    for (i = 0; i < count; i++) {
        out[i] = (uint16_t)buf[i];
    }

    /* Check for playback completion */
    {
        uint8_t any_active = 0;
        for (ch_idx = 0; ch_idx < AUDIO_MAX_CHANNELS; ch_idx++) {
            ch = &CS43131_g.channels[ch_idx];
            if (ch->active && ch->state == AUDIO_STATE_PLAYING) {
                if (ch->eof && ch_rb_used(ch) < 4) {
                    ch->state = AUDIO_STATE_IDLE;
                    printf("Audio: ch%d finished\r\n", ch_idx);
                } else {
                    any_active = 1;
                }
            }
            if (ch->active && ch->state == AUDIO_STATE_PLAYING) any_active = 1;
        }
        if (!any_active && CS43131_g.active_channel_count > 0) {
            /* All channels finished */
            uint8_t all_done = 1;
            for (ch_idx = 0; ch_idx < AUDIO_MAX_CHANNELS; ch_idx++) {
                if (CS43131_g.channels[ch_idx].active &&
                    CS43131_g.channels[ch_idx].state == AUDIO_STATE_PLAYING) {
                    all_done = 0;
                    break;
                }
            }
            if (all_done) {
                CS43131_g.audio_state = AUDIO_STATE_IDLE;
                CS43131_g.status_dirty = 1;
            }
        }
    }
}

void DMA1_Channel1_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1, DMA1_IT_HT1) == SET)
    {
        DMA_ClearITPendingBit(DMA1, DMA1_IT_HT1);
        mixer_fill_half(CS43131_g.dma_buf, AUDIO_HALF_SIZE);
    }
    if (DMA_GetITStatus(DMA1, DMA1_IT_TC1) == SET)
    {
        DMA_ClearITPendingBit(DMA1, DMA1_IT_TC1);
        mixer_fill_half(CS43131_g.dma_buf + AUDIO_HALF_SIZE, AUDIO_HALF_SIZE);
    }
}

/* ======================================================================== */
/*  Legacy Compatibility Wrappers                                            */
/* ======================================================================== */

void Audio_PlayWAV_Start(wav_info_t *info)
{
    /* Legacy: start on channel 0. File must NOT be left open by caller. */
    Audio_StopAll();
    Audio_ChannelStart(0, "", info);
}

void Audio_PlayStop(void)
{
    Audio_StopAll();
}

void Audio_SetCurrentTrack(const char *name)
{
    if (name) {
        strncpy(CS43131_g.channels[0].track_name, name,
                sizeof(CS43131_g.channels[0].track_name) - 1);
        CS43131_g.channels[0].track_name[sizeof(CS43131_g.channels[0].track_name) - 1] = '\0';
    } else {
        CS43131_g.channels[0].track_name[0] = '\0';
    }
    CS43131_g.status_dirty = 1;
}

const char* Audio_GetCurrentTrackName(void)
{
    return CS43131_g.channels[0].track_name;
}

uint32_t Audio_GetPlayTime_ms(void)
{
    return Audio_ChannelGetPlayTime_ms(0);
}

uint32_t Audio_GetDuration_ms(void)
{
    return Audio_ChannelGetDuration_ms(0);
}
