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
    CS43131_g.status_dirty = 1;   /* 外放状态变化，触发向 Display 推送 */
}

void Speaker_Off(speaker_channel_t channel)
{
    if (channel & SPEAKER_CHANNEL_LEFT)
        GPIO_SetBits(SPEAKER_LEFT_SHUTDOWN_GPIO_PORT, SPEAKER_LEFT_SHUTDOWN_PIN);
    if (channel & SPEAKER_CHANNEL_RIGHT)
        GPIO_SetBits(SPEAKER_RIGHT_SHUTDOWN_GPIO_PORT, SPEAKER_RIGHT_SHUTDOWN_PIN);
    CS43131_g.status_dirty = 1;   /* 外放状态变化，触发向 Display 推送 */
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
/*  Biquad Filter (Q20 fixed-point, int64 accumulator)                       */
/* ======================================================================== */

#define BQ_SHIFT 20
#define BQ_UNITY (1 << BQ_SHIFT)  /* 1048576 (Q20) — high precision needed for the
                                   * 100Hz bass band whose poles sit close to z=1 */

static inline int32_t biquad_process(biquad_t *bq, int32_t x)
{
    int64_t acc;
    acc  = (int64_t)bq->b0 * x;
    acc += (int64_t)bq->b1 * bq->x1;
    acc += (int64_t)bq->b2 * bq->x2;
    acc -= (int64_t)bq->a1 * bq->y1;
    acc -= (int64_t)bq->a2 * bq->y2;
    acc >>= BQ_SHIFT;
    bq->x2 = bq->x1; bq->x1 = x;
    bq->y2 = bq->y1; bq->y1 = (int32_t)acc;
    /* No inter-stage clamp: the caller clamps once after the full cascade,
     * preserving headroom/precision (state is kept in int32). */
    return (int32_t)acc;
}

/* ======================================================================== */
/*  EQ: runtime biquad design from a gain LUT (fixed-point, no soft-float)   */
/*  3 peaking bands: Bass=100Hz, Mid=1kHz, Treble=10kHz @44.1kHz, Q=0.707    */
/*                                                                           */
/*  RBJ peaking coefficients are affine in A=10^(g/40) and 1/A given a fixed */
/*  w0/Q, so only A (and 1/A) depend on gain. We keep cos(w0) and            */
/*  alpha=sin(w0)/(2Q) as per-band constants (Q15) and look A up in a table. */
/* ======================================================================== */

/* Per-band constants in Q15: cos(w0) and alpha = sin(w0)/(2*Q), Q=0.707 */
static const int32_t eq_cos_w0[3] = { 32765, 32436,  4769 }; /* bass, mid, treble */
static const int32_t eq_alpha[3]  = {   330,  3290, 22924 };

/* A = 10^(gain/40) in Q15 for gain = -12..+12 dB (index 0..24).
 * 1/A for gain index i equals A at index (24 - i) (symmetry of 10^x). */
static const int32_t eq_A_lut[25] = {
    16423, 17396, 18427, 19519, 20676, 21901, 23199, 24573, 26030,
    27572, 29205, 30936, 32768, 34710, 36767, 38945, 41253, 43697,
    46286, 49029, 51934, 55011, 58271, 61723, 65379
};

static void eq_design_band(int band, int16_t gain_db)
{
    biquad_t *bl = &CS43131_g.eq.bands[band];
    biquad_t *br = &CS43131_g.eq.bands_r[band];
    int32_t idx, A, invA, alA, alInvA;
    int32_t b0, b1, b2, a0, a2;
    int32_t B0, B1, B2, A2;

    if (gain_db > 12)  gain_db = 12;
    if (gain_db < -12) gain_db = -12;

    if (gain_db == 0) {
        bl->b0 = br->b0 = BQ_UNITY;
        bl->b1 = br->b1 = 0; bl->b2 = br->b2 = 0;
        bl->a1 = br->a1 = 0; bl->a2 = br->a2 = 0;
        return;
    }

    idx  = gain_db + 12;
    A    = eq_A_lut[idx];
    invA = eq_A_lut[24 - idx];

    /* peaking: b0=1+alpha*A, b2=1-alpha*A, a0=1+alpha/A, a2=1-alpha/A,
     *          b1 = a1 = -2*cos(w0)  (all pre-normalization, Q15) */
    alA    = (int32_t)(((int64_t)eq_alpha[band] * A)    >> 15);
    alInvA = (int32_t)(((int64_t)eq_alpha[band] * invA) >> 15);

    b0 = 32768 + alA;
    b2 = 32768 - alA;
    a0 = 32768 + alInvA;          /* always >= 32768, safe divisor */
    a2 = 32768 - alInvA;
    b1 = -(eq_cos_w0[band] << 1); /* -2*cos(w0), shared by numerator and a1 */

    /* normalize by a0 and store in Q20; same coeffs feed both L and R banks
     * (each channel keeps its own filter state to avoid L/R cross-talk). */
    B0 = (int32_t)(((int64_t)b0 * BQ_UNITY) / a0);
    B1 = (int32_t)(((int64_t)b1 * BQ_UNITY) / a0);
    B2 = (int32_t)(((int64_t)b2 * BQ_UNITY) / a0);
    A2 = (int32_t)(((int64_t)a2 * BQ_UNITY) / a0);

    bl->b0 = br->b0 = B0;
    bl->b1 = br->b1 = B1;
    bl->b2 = br->b2 = B2;
    bl->a1 = br->a1 = B1;   /* a1 numerator equals b1 (-2*cos) */
    bl->a2 = br->a2 = A2;
}

void Audio_EQ_UpdateCoeffs(void)
{
    eq_design_band(0, CS43131_g.eq.bass_gain_db);
    eq_design_band(1, CS43131_g.eq.mid_gain_db);
    eq_design_band(2, CS43131_g.eq.treble_gain_db);
}

/* ======================================================================== */
/*  Compressor (feed-forward, continuous Q15 gain, smoothed envelope)        */
/* ======================================================================== */

/* 10^(-dB/20) in Q15 for dB = 0..40 (attenuation, used for threshold amp). */
static const int32_t comp_atten_lut[41] = {
    32768, 29205, 26029, 23198, 20675, 18426, 16423, 14640, 13048, 11629,
    10362,  9235,  8232,  7336,  6538,  5827,  5194,  4629,  4126,  3677,
     3277,  2921,  2603,  2320,  2068,  1843,  1642,  1464,  1305,  1163,
     1036,   923,   823,   734,   654,   583,   519,   463,   413,   368,
      328
};

/* 10^(dB/20) in Q15 for dB = 0..24 (makeup gain, >= unity). */
static const int32_t comp_makeup_lut[25] = {
     32768,  36767,  41253,  46286,  51934,  58271,  65379,  73357,  82310,
     92353, 103631, 116271, 130456, 146373, 164229, 184267, 206755, 232000,
    260305, 292062, 327680, 367641, 412553, 462946, 519357
};

static void compressor_process(audio_compressor_t *comp, int16_t *buf, uint32_t len)
{
    const int32_t attack_q15  = 6553;   /* ~0.2  : fast gain reduction  */
    const int32_t release_q15 = 328;    /* ~0.01 : slow recovery        */
    uint32_t i;
    int32_t thresh = comp->thresh_amp;
    int32_t makeup = comp->makeup_lin;
    int32_t k      = comp->k_q15;
    int32_t gain   = comp->gain_q15;

    if (makeup <= 0) makeup = 32768;    /* defensive: unity if never set */
    if (gain   <= 0) gain   = makeup;

    /* Stereo-linked: one gain derived from max(|L|,|R|), applied to both. */
    for (i = 0; i + 1 < len; i += 2) {
        int32_t l = buf[i];
        int32_t r = buf[i + 1];
        int32_t al = (l < 0) ? -l : l;
        int32_t ar = (r < 0) ? -r : r;
        int32_t peak = (al > ar) ? al : ar;
        int32_t inst_g, target, coeff, ol, orr;

        if (thresh > 0 && peak > thresh) {
            /* soft compression: g = 1 - k*(1 - thresh/peak), k=(ratio-1)/ratio */
            int32_t ratio_te = (int32_t)(((int64_t)thresh << 15) / peak); /* Q15 <=1 */
            inst_g = 32768 - (int32_t)(((int64_t)k * (32768 - ratio_te)) >> 15);
        } else {
            inst_g = 32768;
        }
        target = (int32_t)(((int64_t)inst_g * makeup) >> 15);

        coeff = (target < gain) ? attack_q15 : release_q15;
        gain += (int32_t)(((int64_t)coeff * (target - gain)) >> 15);
        if (gain < 0) gain = 0;

        ol  = (int32_t)(((int64_t)l * gain) >> 15);
        orr = (int32_t)(((int64_t)r * gain) >> 15);
        if (ol  > 32767) ol  = 32767; if (ol  < -32768) ol  = -32768;
        if (orr > 32767) orr = 32767; if (orr < -32768) orr = -32768;
        buf[i]     = (int16_t)ol;
        buf[i + 1] = (int16_t)orr;
    }
    comp->gain_q15 = gain;
}

/* ======================================================================== */
/*  Echo / Delay                                                             */
/* ======================================================================== */

static void echo_process(audio_echo_t *echo, int16_t *buf, uint32_t len)
{
    uint32_t i;
    uint16_t dlen = echo->delay_len;
    uint16_t dpos = echo->delay_pos;
    int32_t  fb   = (int32_t)echo->feedback * 128;  /* 0..200 -> Q15 (~0..0.78) */
    int32_t  wet  = (int32_t)echo->mix * 328;       /* 0..100 -> Q15 (~0..1.0)  */
    int32_t  dry;
    const int32_t damp = 9830;                      /* ~0.3 one-pole LPF (feedback damping) */
    int32_t  lp_l = echo->lp_l, lp_r = echo->lp_r;

    if (dlen == 0) return;
    if (wet > 32768) wet = 32768;
    dry = 32768 - wet;

    for (i = 0; i + 1 < len; i += 2) {
        uint16_t rd_idx = (uint16_t)(dpos * 2);
        int32_t del_l = echo->delay_line[rd_idx];
        int32_t del_r = echo->delay_line[rd_idx + 1];
        int32_t dry_l = buf[i];
        int32_t dry_r = buf[i + 1];
        int32_t out_l, out_r, wr_l, wr_r;

        /* dry/wet cross-fade (keeps overall level bounded) */
        out_l = (int32_t)(((int64_t)dry_l * dry + (int64_t)del_l * wet) >> 15);
        out_r = (int32_t)(((int64_t)dry_r * dry + (int64_t)del_r * wet) >> 15);
        if (out_l > 32767) out_l = 32767; if (out_l < -32768) out_l = -32768;
        if (out_r > 32767) out_r = 32767; if (out_r < -32768) out_r = -32768;
        buf[i]     = (int16_t)out_l;
        buf[i + 1] = (int16_t)out_r;

        /* feedback: low-pass the delayed signal (damping) then add DRY input */
        lp_l += (int32_t)(((int64_t)damp * (del_l - lp_l)) >> 15);
        lp_r += (int32_t)(((int64_t)damp * (del_r - lp_r)) >> 15);
        wr_l = dry_l + (int32_t)(((int64_t)fb * lp_l) >> 15);
        wr_r = dry_r + (int32_t)(((int64_t)fb * lp_r) >> 15);
        if (wr_l > 32767) wr_l = 32767; if (wr_l < -32768) wr_l = -32768;
        if (wr_r > 32767) wr_r = 32767; if (wr_r < -32768) wr_r = -32768;
        echo->delay_line[rd_idx]     = (int16_t)wr_l;
        echo->delay_line[rd_idx + 1] = (int16_t)wr_r;

        dpos++;
        if (dpos >= dlen) dpos = 0;
    }
    echo->delay_pos = dpos;
    echo->lp_l = lp_l;
    echo->lp_r = lp_r;
}

/* ======================================================================== */
/*  Effects chain: apply all enabled effects to a buffer                     */
/* ======================================================================== */

static void effects_chain_process(int16_t *buf, uint32_t len)
{
    if (!CS43131_g.fx_master_enable) return;  /* 总开关关闭，旁路所有效果器 */

    if (CS43131_g.eq.enable) {
        uint32_t i;
        /* Stereo: L and R each run through their own 3-band state bank so the
         * bands act at the true 44.1kHz rate with no L/R cross-contamination. */
        for (i = 0; i + 1 < len; i += 2) {
            int32_t sl = buf[i];
            int32_t sr = buf[i + 1];
            sl = biquad_process(&CS43131_g.eq.bands[0], sl);
            sl = biquad_process(&CS43131_g.eq.bands[1], sl);
            sl = biquad_process(&CS43131_g.eq.bands[2], sl);
            sr = biquad_process(&CS43131_g.eq.bands_r[0], sr);
            sr = biquad_process(&CS43131_g.eq.bands_r[1], sr);
            sr = biquad_process(&CS43131_g.eq.bands_r[2], sr);
            if (sl > 32767)  sl = 32767;  if (sl < -32768) sl = -32768;
            if (sr > 32767)  sr = 32767;  if (sr < -32768) sr = -32768;
            buf[i]     = (int16_t)sl;
            buf[i + 1] = (int16_t)sr;
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

void Audio_FX_ApplyMusicPreset(void)
{
    /* 电子琴增强预设：适度低频暖度 + 高频亮度 + 轻压缩 + 可控空间感。
     * Faders 后续会实时覆盖 bass/echo/comp；treble 常驻提供明亮感。 */
    Audio_EQ_SetBass(3);
    Audio_EQ_SetMid(0);
    Audio_EQ_SetTreble(4);
    Audio_EQ_Enable(1);

    Audio_Comp_SetParams(-18, 3, 4);   /* threshold -18dB, 3:1, +4dB makeup */
    Audio_Comp_Enable(1);

    Audio_Echo_SetParams(90, 60, 22);  /* 90ms, ~23% feedback, 22% wet */
    Audio_Echo_Enable(1);
}

void Audio_EQ_SetBass(int16_t db)   { CS43131_g.eq.bass_gain_db = db; Audio_EQ_UpdateCoeffs(); }
void Audio_EQ_SetMid(int16_t db)    { CS43131_g.eq.mid_gain_db = db; Audio_EQ_UpdateCoeffs(); }
void Audio_EQ_SetTreble(int16_t db) { CS43131_g.eq.treble_gain_db = db; Audio_EQ_UpdateCoeffs(); }
void Audio_EQ_Enable(uint8_t en)    { CS43131_g.eq.enable = en ? 1 : 0; }

void Audio_Comp_SetParams(int16_t threshold_db, int16_t ratio, int16_t makeup_db)
{
    if (threshold_db > 0)   threshold_db = 0;
    if (threshold_db < -40) threshold_db = -40;
    if (ratio < 1)  ratio = 1;
    if (ratio > 10) ratio = 10;
    if (makeup_db < 0)  makeup_db = 0;
    if (makeup_db > 24) makeup_db = 24;

    CS43131_g.compressor.threshold_db = threshold_db;
    CS43131_g.compressor.ratio        = ratio;
    CS43131_g.compressor.makeup_db    = makeup_db;
    CS43131_g.compressor.thresh_amp   = comp_atten_lut[-threshold_db];
    CS43131_g.compressor.makeup_lin   = comp_makeup_lut[makeup_db];
    CS43131_g.compressor.k_q15        = (int32_t)(ratio - 1) * 32768 / ratio;
}
void Audio_Comp_Enable(uint8_t en)
{
    CS43131_g.compressor.enable = en ? 1 : 0;
    if (en) {
        /* start at the makeup gain so enabling doesn't jump */
        int32_t m = CS43131_g.compressor.makeup_lin;
        CS43131_g.compressor.gain_q15 = (m > 0) ? m : 32768;
    }
}

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
        CS43131_g.echo.lp_l = 0;
        CS43131_g.echo.lp_r = 0;
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
            /* data chunk size is the (possibly huge) audio payload; we already
             * have the offset, so stop scanning to avoid pos overflow/wrap. */
            break;
        }
        {
            uint32_t adv = 8 + ((chunk_size + 1) & ~1U);
            /* guard against malformed chunk_size causing wrap / stall */
            if (adv < 8 || pos + adv <= pos)
                break;
            pos += adv;
        }
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
/*  Audio_Process: Main Loop — CH378 Streaming (fill-until-high-watermark)   */
/* ======================================================================== */

static uint8_t stream_channel_idx = 0; /* round-robin index */

/* Shared CH378 read staging buffer. Kept in .bss instead of on the (2KB) stack;
 * Audio_Process / Audio_PreFill run only in main-loop context, are never
 * re-entrant and never called from an ISR, so a single shared buffer is safe. */
static uint8_t audio_ch378_buf[AUDIO_CH_READ_BLOCK];

/* Low watermark: trigger read when buffer drops below this (~93ms). */
#define AUDIO_CH_LOW_WATERMARK  (AUDIO_CH_RB_SIZE / 2)
/* High watermark: keep reading until buffer reaches this (~140ms).
 * The gap between low and high watermarks reduces channel switches:
 * at 176KB/s, filling from 16KB to 24KB takes ~45ms, during which
 * no seek is needed for this channel. */
#define AUDIO_CH_HIGH_WATERMARK (AUDIO_CH_RB_SIZE * 3 / 4)

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

/* Pre-fill all active channels to high watermark before locking CH378.
 * Uses the same channel_file_open/channel_file_close helpers as Audio_Process.
 * Must be called BEFORE Audio_CH378_Lock(). */
void Audio_PreFill(void)
{
    uint8_t ch_idx;
    uint16_t real_len;
    uint8_t status;

    for (ch_idx = 0; ch_idx < AUDIO_MAX_CHANNELS; ch_idx++) {
        audio_channel_t *ch = &CS43131_g.channels[ch_idx];
        if (!ch->active || ch->eof) continue;

        /* Close other channels' files (CH378 single-handle) */
        {
            uint8_t j;
            for (j = 0; j < AUDIO_MAX_CHANNELS; j++) {
                if (j != ch_idx)
                    channel_file_close(&CS43131_g.channels[j]);
            }
        }

        /* Open file and seek to current read offset */
        status = channel_file_open(ch);
        if (status != ERR_SUCCESS) {
            ch->eof = 1;
            continue;
        }

        /* Fill until high watermark or EOF */
        while (ch_rb_used(ch) < AUDIO_CH_HIGH_WATERMARK) {
            status = CH378ByteRead(audio_ch378_buf, AUDIO_CH_READ_BLOCK, &real_len);
            if (status != ERR_SUCCESS || real_len == 0) {
                ch->eof = 1;
                break;
            }
            ch_rb_write(ch, audio_ch378_buf, real_len);
            ch->read_offset += real_len;
            if (real_len < AUDIO_CH_READ_BLOCK) {
                ch->eof = 1;
                break;
            }
        }
        channel_file_close(ch);
    }
}

void Audio_Process(void)
{
    audio_channel_t *ch;
    uint8_t status;
    uint16_t real_len;
    uint8_t attempts;
    uint32_t used, target, to_read;

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
            channel_file_close(ch);
            continue;
        }

        used = ch_rb_used(ch);

        /* Only start filling when buffer drops below low watermark.
         * Once we start, keep filling until high watermark (hysteresis).
         * This minimizes channel switches (= expensive seeks). */
        if (used > AUDIO_CH_LOW_WATERMARK) continue;

        /* How much to read: fill up to high watermark */
        target = AUDIO_CH_HIGH_WATERMARK - used;
        if (target < AUDIO_CH_READ_BLOCK) target = AUDIO_CH_READ_BLOCK;

        /* Cap to available free space (keep 1 byte for ring buffer sentinel) */
        if (target > ch_rb_free(ch) - 1) target = ch_rb_free(ch) - 1;
        if (target < AUDIO_CH_READ_BLOCK) continue; /* not enough room */

        /* CH378 has only one file handle: close other channels first */
        {
            uint8_t j;
            for (j = 0; j < AUDIO_MAX_CHANNELS; j++) {
                if (j != (uint8_t)(ch - CS43131_g.channels))
                    channel_file_close(&CS43131_g.channels[j]);
            }
        }

        /* Open file (reopen + seek if was closed by lock or other channel) */
        status = channel_file_open(ch);
        if (status != ERR_SUCCESS) {
            printf("Audio: ch%d open failed (0x%02X)\r\n",
                   (int)(ch - CS43131_g.channels), status);
            ch->eof = 1;
            continue;
        }

        /* Read in chunks until we reach target or hit EOF.
         * Sequential reads — no seek overhead within this loop. */
        to_read = target;
        while (to_read >= AUDIO_CH_READ_BLOCK) {
            status = CH378ByteRead(audio_ch378_buf, AUDIO_CH_READ_BLOCK, &real_len);
            if (status != ERR_SUCCESS || real_len == 0) {
                ch->eof = 1;
                channel_file_close(ch);
                break;
            }
            ch_rb_write(ch, audio_ch378_buf, real_len);
            ch->read_offset += real_len;
            if (real_len < AUDIO_CH_READ_BLOCK) {
                ch->eof = 1;
                channel_file_close(ch);
                break;
            }
            to_read -= real_len;
        }
        break; /* served this channel, return to main loop */
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

            vol = (int32_t)ch->volume;          /* 0..100 */
            /* pan: 0=full left, 128=center, 255=full right */
            pan_l = (255 - ch->pan);
            pan_r = ch->pan;

            /* Normalize by (100*255) so full volume + hard pan == unity gain,
             * restoring headroom vs the old >>15 which capped a single channel
             * at ~0.78 full-scale (and centre pan at ~0.39). The final clip
             * below still guards simultaneous full-scale channels. */
            l += ((int32_t)sl * vol * pan_l) / 25500;
            r += ((int32_t)sr * vol * pan_r) / 25500;

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
