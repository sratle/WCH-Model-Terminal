/********************************** (C) COPYRIGHT  *******************************
 * File Name          : hardware.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : This file provides all the hardware firmware functions.
 *********************************************************************************
 * Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/
#include "hardware.h"
#include "i2c_soft.h"
#include "ch32h417_dma.h"
#include "ch32h417_spi.h"
#include "ch32h417_rcc.h"

#define AUDIO_BUFFER_SIZE 256
#define SPI2_TX_DMA_REQUEST 65
#define CS43131_I2C_ADDR 0x30
#define CS43131_PWR_XSP (1U << 7)
#define CS43131_PWR_ASP (1U << 6)
#define CS43131_PWR_DSDIF (1U << 5)
#define CS43131_PWR_HP (1U << 4)
#define CS43131_PWR_XTAL (1U << 3)
#define CS43131_PWR_PLL (1U << 2)
#define CS43131_PWR_CLKOUT (1U << 1)

static uint16_t audio_buffer_A[AUDIO_BUFFER_SIZE];
static uint16_t audio_buffer_B[AUDIO_BUFFER_SIZE];

static const uint16_t *song_data = NULL;
static volatile uint32_t song_data_length = 0;
static volatile uint32_t audio_data_offset = 0;

/*********************************************************************
 * @fn      Hardware
 *
 * @brief   Hardware entry.
 *
 * @return  none
 */
void Hardware_init(void)
{
    CS43131_init();
    Key_init();
}

/**
 * warn!!!验证板使用I2S2,I2C1，正式板使用I2S1,I2C2
 * 验证板PB3-CK,PA15-WS,PB5-SDO,PB9-OE,PB8-RST,PB6-SCL,PB7-SDA
 * 正式板PB12-WS,PB13-CK,PB15-SDO,PB9-OE,PB8-RST,PC0-SCL,PC1-SDA
 * CS43131 I2S
 */
void CS43131_init(void)
{
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2S_InitTypeDef I2S_InitStructure = {0};

    /* Enable clocks */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOA | RCC_HB2Periph_GPIOB | RCC_HB2Periph_AFIO, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI2, ENABLE);

    /* I2S2 GPIO: PB3-CK(AF6), PA15-WS(AF6), PB5-SDO(AF6) */
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF6);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource15, GPIO_AF6);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF7);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* OE(PB9) & RST(PB8) as output */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* OE pull up, RST pull down (initial low) */
    GPIO_SetBits(GPIOB, GPIO_Pin_9);
    GPIO_ResetBits(GPIOB, GPIO_Pin_8);

    /* Software I2C init */
    SoftI2C_Init();

    /* CS43131 I2C configuration */
    CS43131_I2C_Config();

    /* I2S2 init: slave tx, Philips, 16b, 44.1kHz, CPOL low, MCLK disable */
    I2S_StructInit (&I2S_InitStructure);
    I2S_InitStructure.I2S_Mode = I2S_Mode_SlaveTx;
    I2S_InitStructure.I2S_Standard = I2S_Standard_Phillips;
    I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_16b;
    I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Disable;
    I2S_InitStructure.I2S_AudioFreq = I2S_AudioFreq_44k;
    I2S_InitStructure.I2S_CPOL = I2S_CPOL_Low;
    I2S_Init (SPI2, &I2S_InitStructure);

    /* I2S2 DMA double buffer init */
    I2S2_DMA_DoubleBufferInit();

    I2S_Cmd (SPI2, ENABLE);
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
 * @brief   Configure CS43131 via I2C1 according to cs43131.c reference.
 *
 * @return  none
 */
void CS43131_I2C_Config(void)
{
    uint8_t tmp;
    uint32_t timeout;

    printf("CS43131_I2C_Config\r\n");

    /* RST low -> high, delay 10ms */
    GPIO_ResetBits(GPIOB, GPIO_Pin_8);
    Delay_Ms(1);
    GPIO_SetBits(GPIOB, GPIO_Pin_8);
    Delay_Ms(50);

    printf("CS43131_I2C_Config: RST low -> high, delay 10ms\r\n");

    /* 一、复位后基础初始化配置 */
    CS43131_I2C_WriteReg(0x010006, 0x06); /* MCLK_SRC_SEL=RCO, MCLK_INT=22.5792MHz */

    tmp = CS43131_I2C_ReadReg(0x010006);
    printf("CS43131_I2C_Config: MCLK_SRC_SEL=RCO, MCLK_INT=22.5792MHz, readback=0x%02X\r\n", tmp);

    CS43131_I2C_WriteReg(0x020052, 0x02); /* XTAL_IBIAS=12.5uA */
    // CS43131_I2C_WriteReg (0x0F0000, 0xFF); /* 清除中断状态1 */
    // CS43131_I2C_WriteReg (0x0F0001, 0xFF); /* 清除中断状态2 */
    // CS43131_I2C_WriteReg (0x0F0010, 0xE1); /* 使能PLL_READY/PLL_ERROR中断 */

    printf("CS43131_I2C_Config:PLL config\r\n");
    CS43131_I2C_WriteReg(0x020000, 0xF6); /* Power up XTAL */

    /* 二、PLL配置（26MHz -> 22.5792MHz） */
    CS43131_I2C_WriteReg(0x040002, 0x03); /* PLL_REF_PREDIV = 8 */
    CS43131_I2C_WriteReg(0x030008, 0x0A); /* PLL_OUT_DIV = 10 */
    CS43131_I2C_WriteReg(0x030005, 0x45); /* PLL_DIV_INT = 69 */
    CS43131_I2C_WriteReg(0x030004, 0x79); /* PLL_DIV_FRAC = 0x79 76 80 */
    CS43131_I2C_WriteReg(0x030003, 0x76);
    CS43131_I2C_WriteReg(0x030002, 0x80);
    CS43131_I2C_WriteReg(0x03001B, 0x01); /* PLL_MODE = 1 */
    CS43131_I2C_WriteReg(0x03000A, 0x6F); /* PLL_CAL_RATIO = 111 */

    printf("CS43131_I2C_Config:PLL cal\r\n");

    /* 三、晶振与PLL启动、锁定等待 */
    CS43131_I2C_WriteReg(0x020000, 0xF2); /* Power up XTAL | PLL */
    CS43131_I2C_WriteReg(0x030001, 0x01); /* PLL_START */

    printf("CS43131_I2C_Config:PLL start\r\n");

    /* 等待PLL锁定 */
    timeout = 10000;
    while (timeout--)
    {
        tmp = CS43131_I2C_ReadReg(0x0F0000);
        if (tmp & 0x04)
        {
            printf("CS43131_I2C_Config:PLL locked, status=0x%02X\r\n", tmp);
            break;
        }
        if (tmp & 0x02)
        {
            printf("CS43131_I2C_Config:PLL error, status=0x%02X\r\n", tmp);
            break;
        }
        Delay_Ms(1);
    }
    if (timeout == 0)
    {
        printf("CS43131_I2C_Config:PLL lock timeout\r\n");
    }

    printf("CS43131_I2C_Config:PLL lock done\r\n");

    /* 四、系统时钟源切换 */
    CS43131_I2C_WriteReg(0x040004, 0x01); /* CLKOUT source = PLL, divide 2 */
    CS43131_I2C_WriteReg(0x020000, 0xF0); /* Power up XTAL | PLL | CLKOUT */
    CS43131_I2C_WriteReg(0x010006, 0x05); /* MCLK_SRC_SEL=PLL */
    Delay_Ms(10);                          /* 等待时钟切换稳定 */

    printf("CS43131_I2C_Config:clock source switch\r\n");

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
    CS43131_I2C_WriteReg(0x040018, 0x0C); /* ASP Master Mode | ASP SCLK Inverted */
    CS43131_I2C_WriteReg(0x040019, 0x0A); /* 50/50 duty | FSD=1 | STP=0 */
    CS43131_I2C_WriteReg(0x050000, 0x00); /* ASP_RX_CH1 = 0 */
    CS43131_I2C_WriteReg(0x050001, 0x00); /* ASP_RX_CH2 = 0 */
    CS43131_I2C_WriteReg(0x05000A, 0x06); /* CH1: LRCK低电平采样, 16bit, 使能 */
    CS43131_I2C_WriteReg(0x05000B, 0x0E); /* CH2: LRCK高电平采样, 16bit, 使能 */
    CS43131_I2C_WriteReg(0x01000D, 0x00); /* ASP_3ST=0, 使能时钟输出 */

    printf("CS43131_I2C_Config:ASP I2S Master interface config\r\n");

    /* 六、PCM音频路径配置 */
    CS43131_I2C_WriteReg(0x090000, 0x02); /* 高通滤波开启, 快速滚降 */
    CS43131_I2C_WriteReg(0x090001, 0x10); /* 设置音量A */
    CS43131_I2C_WriteReg(0x090002, 0x10); /* 设置音量B */
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

    /* 九、最终模块使能 */
    CS43131_I2C_WriteReg(0x020000, 0xB0); /* Power up ASP | XTAL | PLL | CLKOUT */
    CS43131_I2C_WriteReg(0x020000, 0xA0); /* Power up HP | ASP | XTAL | PLL | CLKOUT */
    Delay_Ms(22);                         /* 功放软启动完成 */

    /* 恢复Pop-free设置 */
    CS43131_I2C_WriteReg(0x010010, 0x00); /* Pop-free setting restore */
    CS43131_I2C_WriteReg(0x080032, 0x00);

    /* 取消静音 */
    CS43131_I2C_WriteReg(0x090003, 0xEC); /* Disable mute */
}

void I2S2_DMA_DoubleBufferInit(void)
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
    DMA_InitStructure.DMA_BufferMode = DMA_DoubleBufferMode;
    DMA_InitStructure.DMA_Memory1BaseAddr = (uint32_t)audio_buffer_B;
    DMA_InitStructure.DMA_DoubleBuffer_StartMemory = DMA_DoubleBufferMode_Memory_0;

    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    DMA_MuxChannelConfig(DMA_MuxChannel1, SPI2_TX_DMA_REQUEST);

    DMA_ITConfig(DMA1_Channel1, DMA_IT_HT | DMA_IT_TC, ENABLE);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);
    DMA_Cmd(DMA1_Channel1, ENABLE);
}

/*********************************************************************
 * @fn      Audio_FillBuffer
 *
 * @brief   Fill audio buffer with next samples from song data.
 *
 * @param   buffer - pointer to buffer to fill.
 *
 * @return  none
 */
void Audio_FillBuffer(uint16_t *buffer)
{
    uint32_t i;

    for (i = 0; i < AUDIO_BUFFER_SIZE; i++)
    {
        if (audio_data_offset >= song_data_length)
        {
            buffer[i] = 0;
        }
        else
        {
            buffer[i] = song_data[audio_data_offset];
            audio_data_offset++;
        }
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
 */
void Audio_PlayStart(const uint16_t *data, uint32_t length)
{
    song_data = data;
    song_data_length = length;
    audio_data_offset = 0;

    Audio_FillBuffer(audio_buffer_A);
    Audio_FillBuffer(audio_buffer_B);
}

/*********************************************************************
 * @fn      Audio_GenerateTriangleWave
 *
 * @brief   Generate a 16-bit stereo triangle wave into buffer.
 *
 * @param   buffer - pointer to buffer to fill.
 * @param   length - number of samples to generate.
 * @param   amplitude - peak amplitude (e.g. 10000 for ~1/3 full scale).
 * @param   period  - number of samples per triangle wave period.
 *
 * @return  none
 */
void Audio_GenerateTriangleWave(uint16_t *buffer, uint32_t length, int16_t amplitude, uint32_t period)
{
    uint32_t i;
    int16_t sample;
    int32_t slope;

    slope = (2 * amplitude) / (period / 2);

    for (i = 0; i < length; i++)
    {
        uint32_t pos = i % period;

        if (pos < (period / 2))
        {
            sample = -amplitude + (int16_t)(slope * pos);
        }
        else
        {
            sample = amplitude - (int16_t)(slope * (pos - period / 2));
        }

        buffer[i] = (uint16_t)sample;
    }
}

/*********************************************************************
 * @fn      Audio_PlayTriangleWave
 *
 * @brief   Start playing triangle wave continuously.
 *
 * @param   amplitude - peak amplitude.
 * @param   period    - number of samples per period.
 *
 * @return  none
 */
void Audio_PlayTriangleWave(int16_t amplitude, uint32_t period)
{
    Audio_GenerateTriangleWave(audio_buffer_A, AUDIO_BUFFER_SIZE, amplitude, period);
    Audio_GenerateTriangleWave(audio_buffer_B, AUDIO_BUFFER_SIZE, amplitude, period);

    song_data = NULL;
    song_data_length = 0;
    audio_data_offset = 0;
}

/*********************************************************************
 * @fn      DMA1_Channel1_IRQHandler
 *
 * @brief   DMA1 Channel1 interrupt handler for I2S2 TX double buffer.
 *
 * @return  none
 */
void DMA1_Channel1_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1, DMA1_IT_HT1) == SET)
    {
        DMA_ClearITPendingBit(DMA1, DMA1_IT_HT1);
        printf("DMA_A");
        if (song_data != NULL)
        {
            Audio_FillBuffer(audio_buffer_A);
        }
        else
        {
            Audio_GenerateTriangleWave(audio_buffer_A, AUDIO_BUFFER_SIZE, 10000, 44);
        }
    }

    if (DMA_GetITStatus(DMA1, DMA1_IT_TC1) == SET)
    {
        DMA_ClearITPendingBit(DMA1, DMA1_IT_TC1);
        printf("DMA_B");
        if (song_data != NULL)
        {
            Audio_FillBuffer(audio_buffer_B);
        }
        else
        {
            Audio_GenerateTriangleWave(audio_buffer_B, AUDIO_BUFFER_SIZE, 10000, 44);
        }
    }
}

/**
 * Common Key init
 */
void Key_init(void)
{
}
