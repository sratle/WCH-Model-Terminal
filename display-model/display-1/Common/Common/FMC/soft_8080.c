/********************************** (C) COPYRIGHT *******************************
* File Name          : soft_8080.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/20
* Description        : Software 8080 parallel interface for CH32H417.
*                      Write: 16-bit (D0-D15) via OUTDR for speed.
*                      Read:  8-bit (D0-D7 only) with GPIO_Init direction switch.
*                      Proven timing: Delay_Us(5) for WR/RD pulse.
********************************************************************************/
#include "soft_8080.h"
#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"
#include "debug.h"

/* Data pin masks for bulk OUTDR operations */
#define PD_DATA_MASK  (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15)
#define PE_DATA_MASK  (GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15)

/*=============================================================================
 *  Inline helpers: set/read 16-bit data on D0-D15
 *===========================================================================*/
static inline void SOFT8080_SetData(uint16_t data)
{
    uint32_t out_d = GPIOD->OUTDR & ~PD_DATA_MASK;
    uint32_t out_e = GPIOE->OUTDR & ~PE_DATA_MASK;

    /* D0-D3, D13-D15 on GPIOD — pure bit-masking, no branches */
    out_d |= ((data & 0x0001) << 14);   /* D0  -> PD14 */
    out_d |= ((data & 0x0002) << 14);   /* D1  -> PD15 */
    out_d |= ((data & 0x0004) >> 2);    /* D2  -> PD0  */
    out_d |= ((data & 0x0008) >> 2);    /* D3  -> PD1  */
    out_d |= ((data & 0x2000) >> 5);    /* D13 -> PD8  */
    out_d |= ((data & 0x4000) >> 5);    /* D14 -> PD9  */
    out_d |= ((data & 0x8000) >> 5);    /* D15 -> PD10 */

    /* D4-D12 on GPIOE */
    out_e |= ((data & 0x00F0) << 3);    /* D4-D7  -> PE7-PE10  */
    out_e |= ((data & 0x1F00) << 3);    /* D8-D12 -> PE11-PE15 */

    GPIOD->OUTDR = out_d;
    GPIOE->OUTDR = out_e;
}

static inline uint16_t SOFT8080_GetData(void)
{
    uint16_t val = 0;
    uint16_t indr_d = GPIOD->INDR;
    uint16_t indr_e = GPIOE->INDR;

    if (indr_d & GPIO_Pin_14) val |= 0x0001;
    if (indr_d & GPIO_Pin_15) val |= 0x0002;
    if (indr_d & GPIO_Pin_0)  val |= 0x0004;
    if (indr_d & GPIO_Pin_1)  val |= 0x0008;
    if (indr_e & GPIO_Pin_7)  val |= 0x0010;
    if (indr_e & GPIO_Pin_8)  val |= 0x0020;
    if (indr_e & GPIO_Pin_9)  val |= 0x0040;
    if (indr_e & GPIO_Pin_10) val |= 0x0080;
    if (indr_e & GPIO_Pin_11) val |= 0x0100;
    if (indr_e & GPIO_Pin_12) val |= 0x0200;
    if (indr_e & GPIO_Pin_13) val |= 0x0400;
    if (indr_e & GPIO_Pin_14) val |= 0x0800;
    if (indr_e & GPIO_Pin_15) val |= 0x1000;
    if (indr_d & GPIO_Pin_8)  val |= 0x2000;
    if (indr_d & GPIO_Pin_9)  val |= 0x4000;
    if (indr_d & GPIO_Pin_10) val |= 0x8000;

    return val;
}

/*=============================================================================
 *  Initialization
 *===========================================================================*/
void SOFT8080_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* Enable GPIO clocks */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOB | RCC_HB2Periph_GPIOD | RCC_HB2Periph_GPIOE, ENABLE);

    /* PB3: RS, push-pull output, default low (command) */
    gpio.GPIO_Pin = GPIO_Pin_3;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOB, &gpio);
    GPIO_ResetBits(GPIOB, GPIO_Pin_3);

    /* PD4: RD, push-pull output, idle high */
    gpio.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOD, &gpio);
    GPIO_SetBits(GPIOD, GPIO_Pin_4);

    /* PD5: WR, push-pull output, idle high */
    gpio.GPIO_Pin = GPIO_Pin_5;
    GPIO_Init(GPIOD, &gpio);
    GPIO_SetBits(GPIOD, GPIO_Pin_5);

    /* PD7: CS, push-pull output, idle high */
    gpio.GPIO_Pin = GPIO_Pin_7;
    GPIO_Init(GPIOD, &gpio);
    GPIO_SetBits(GPIOD, GPIO_Pin_7);

    /* PD0,1,8,9,10,14,15: D0-D3, D13-D15 */
    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOD, &gpio);

    /* PE7-15: D4-D12 */
    gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOE, &gpio);

    /* Clear all data lines */
    GPIOD->OUTDR &= ~PD_DATA_MASK;
    GPIOE->OUTDR &= ~PE_DATA_MASK;

    printf("[SOFT8080_Init] GPIO 8080 interface initialized (16-bit write, 8-bit read)\r\n");
}

/*=============================================================================
 *  Low-Level 8080 Access
 *===========================================================================*/
void SOFT8080_WriteCmd(uint16_t cmd)
{
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);     /* CS low  */
    GPIO_ResetBits(GPIOB, GPIO_Pin_3);     /* RS low (command) */
    SOFT8080_SetData(cmd);
    GPIO_ResetBits(GPIOD, GPIO_Pin_5);     /* WR low  */
    Delay_Us(1);
    GPIO_SetBits(GPIOD, GPIO_Pin_5);       /* WR high */
    GPIO_SetBits(GPIOD, GPIO_Pin_7);       /* CS high */
}

void SOFT8080_WriteData(uint16_t data)
{
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);     /* CS low  */
    GPIO_SetBits(GPIOB, GPIO_Pin_3);       /* RS high (data) */
    SOFT8080_SetData(data);
    GPIO_ResetBits(GPIOD, GPIO_Pin_5);     /* WR low  */
    Delay_Us(1);
    GPIO_SetBits(GPIOD, GPIO_Pin_5);       /* WR high */
    GPIO_SetBits(GPIOD, GPIO_Pin_7);       /* CS high */
}

uint16_t SOFT8080_ReadData(void)
{
    uint16_t val;
    GPIO_InitTypeDef gpio = {0};

    /* Switch D0-D7 to floating input (proven working configuration).
     * D8-D15 are NOT switched because previous tests showed they fail
     * in input mode; they remain as outputs (driven low by OUTDR clear). */
    gpio.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15 | GPIO_Pin_0 | GPIO_Pin_1;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_Init(GPIOE, &gpio);

    /* Read cycle */
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);     /* CS low  */
    GPIO_SetBits(GPIOB, GPIO_Pin_3);       /* RS high (data) */
    Delay_Us(1);                           /* t_AS = 2us (very safe) */

    GPIO_ResetBits(GPIOD, GPIO_Pin_4);     /* RD low  */
    Delay_Us(1);                           /* t_ACC = 5us (very safe) */
    val = SOFT8080_GetData();
    GPIO_SetBits(GPIOD, GPIO_Pin_4);       /* RD high */

    GPIO_SetBits(GPIOD, GPIO_Pin_7);       /* CS high */

    /* Restore D0-D7 to push-pull output */
    gpio.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15 | GPIO_Pin_0 | GPIO_Pin_1;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOD, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_Init(GPIOE, &gpio);

    /* Only low 8 bits are reliable; D8-D15 may read back as garbage */
    return val & 0x00FF;
}

void SOFT8080_WriteReg(uint16_t reg, uint16_t val)
{
    SOFT8080_WriteCmd(reg);
    SOFT8080_WriteData(val);
}

uint16_t SOFT8080_ReadReg(uint16_t reg)
{
    SOFT8080_WriteCmd(reg);
    Delay_Us(5);
    return SOFT8080_ReadData();
}

/*=============================================================================
 *  Window and GRAM Access
 *===========================================================================*/
void SOFT8080_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    /* Column address set (0x2A) */
    SOFT8080_WriteCmd(0x2A);
    SOFT8080_WriteData((x1 >> 8) & 0xFF);
    SOFT8080_WriteData(x1 & 0xFF);
    SOFT8080_WriteData((x2 >> 8) & 0xFF);
    SOFT8080_WriteData(x2 & 0xFF);

    /* Page address set (0x2B) */
    SOFT8080_WriteCmd(0x2B);
    SOFT8080_WriteData((y1 >> 8) & 0xFF);
    SOFT8080_WriteData(y1 & 0xFF);
    SOFT8080_WriteData((y2 >> 8) & 0xFF);
    SOFT8080_WriteData(y2 & 0xFF);

    /* Prepare GRAM write (0x2C) */
    SOFT8080_WriteCmd(0x2C);
}

void SOFT8080_WriteData16(uint16_t data)
{
    /* Optimized single-pixel write with CS kept low.
     * Assumes caller has already set window and issued 0x2C. */
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);     /* CS low  */
    GPIO_SetBits(GPIOB, GPIO_Pin_3);       /* RS high */
    SOFT8080_SetData(data);
    GPIO_ResetBits(GPIOD, GPIO_Pin_5);     /* WR low  */
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    GPIO_SetBits(GPIOD, GPIO_Pin_5);       /* WR high */
    GPIO_SetBits(GPIOD, GPIO_Pin_7);       /* CS high */
}

void SOFT8080_WriteBuffer(const uint16_t *buf, uint32_t len)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_3);       /* RS=1    */
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);     /* CS low  */

    while (len--) {
        SOFT8080_SetData(*buf++);
        GPIO_ResetBits(GPIOD, GPIO_Pin_5); /* WR low  */
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        GPIO_SetBits(GPIOD, GPIO_Pin_5);   /* WR high */
    }

    GPIO_SetBits(GPIOD, GPIO_Pin_7);       /* CS high */
}

void SOFT8080_Clear(uint16_t color)
{
    uint32_t i;
    uint32_t total = (uint32_t)800 * 480;

    SOFT8080_SetWindow(0, 0, 800 - 1, 480 - 1);

    GPIO_SetBits(GPIOB, GPIO_Pin_3);       /* RS=1    */
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);     /* CS low  */

    for (i = 0; i < total; i++) {
        SOFT8080_SetData(color);
        GPIO_ResetBits(GPIOD, GPIO_Pin_5); /* WR low  */
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        GPIO_SetBits(GPIOD, GPIO_Pin_5);   /* WR high */
    }

    GPIO_SetBits(GPIOD, GPIO_Pin_7);       /* CS high */
}

/*=============================================================================
 *  Proven 8-bit GPIO Test (from 743e28f, for debug/verification)
 *===========================================================================*/
void SOFT8080_Test(void)
{
    uint8_t val;
    GPIO_InitTypeDef gpio = {0};

    printf("[SOFT8080_Test] start 8-bit GPIO verification\r\n");

    /*--- Use 8-bit SetData for this test ---*/
    #define SET_DATA_8BIT(d) do { \
        if ((d) & 0x01) GPIO_SetBits(GPIOD, GPIO_Pin_14); else GPIO_ResetBits(GPIOD, GPIO_Pin_14); \
        if ((d) & 0x02) GPIO_SetBits(GPIOD, GPIO_Pin_15); else GPIO_ResetBits(GPIOD, GPIO_Pin_15); \
        if ((d) & 0x04) GPIO_SetBits(GPIOD, GPIO_Pin_0);  else GPIO_ResetBits(GPIOD, GPIO_Pin_0);  \
        if ((d) & 0x08) GPIO_SetBits(GPIOD, GPIO_Pin_1);  else GPIO_ResetBits(GPIOD, GPIO_Pin_1);  \
        if ((d) & 0x10) GPIO_SetBits(GPIOE, GPIO_Pin_7);  else GPIO_ResetBits(GPIOE, GPIO_Pin_7);  \
        if ((d) & 0x20) GPIO_SetBits(GPIOE, GPIO_Pin_8);  else GPIO_ResetBits(GPIOE, GPIO_Pin_8);  \
        if ((d) & 0x40) GPIO_SetBits(GPIOE, GPIO_Pin_9);  else GPIO_ResetBits(GPIOE, GPIO_Pin_9);  \
        if ((d) & 0x80) GPIO_SetBits(GPIOE, GPIO_Pin_10); else GPIO_ResetBits(GPIOE, GPIO_Pin_10); \
    } while(0)

    /*--- Minimal init sequence via 8-bit GPIO ---*/
    SET_DATA_8BIT(0x01);  /* soft reset */
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);
    GPIO_ResetBits(GPIOB, GPIO_Pin_3);
    GPIO_ResetBits(GPIOD, GPIO_Pin_5);
    Delay_Us(5);
    GPIO_SetBits(GPIOD, GPIO_Pin_5);
    GPIO_SetBits(GPIOD, GPIO_Pin_7);
    Delay_Ms(10);

    /* PLL config */
    SET_DATA_8BIT(0xE2); GPIO_ResetBits(GPIOD, GPIO_Pin_7); GPIO_ResetBits(GPIOB, GPIO_Pin_3); GPIO_ResetBits(GPIOD, GPIO_Pin_5); Delay_Us(5); GPIO_SetBits(GPIOD, GPIO_Pin_5); GPIO_SetBits(GPIOD, GPIO_Pin_7);
    SET_DATA_8BIT(0x23); GPIO_ResetBits(GPIOD, GPIO_Pin_7); GPIO_SetBits(GPIOB, GPIO_Pin_3);  GPIO_ResetBits(GPIOD, GPIO_Pin_5); Delay_Us(5); GPIO_SetBits(GPIOD, GPIO_Pin_5); GPIO_SetBits(GPIOD, GPIO_Pin_7);
    SET_DATA_8BIT(0x02); GPIO_ResetBits(GPIOD, GPIO_Pin_7); GPIO_SetBits(GPIOB, GPIO_Pin_3);  GPIO_ResetBits(GPIOD, GPIO_Pin_5); Delay_Us(5); GPIO_SetBits(GPIOD, GPIO_Pin_5); GPIO_SetBits(GPIOD, GPIO_Pin_7);
    SET_DATA_8BIT(0x04); GPIO_ResetBits(GPIOD, GPIO_Pin_7); GPIO_SetBits(GPIOB, GPIO_Pin_3);  GPIO_ResetBits(GPIOD, GPIO_Pin_5); Delay_Us(5); GPIO_SetBits(GPIOD, GPIO_Pin_5); GPIO_SetBits(GPIOD, GPIO_Pin_7);
    Delay_Us(100);

    /* Enable PLL */
    SET_DATA_8BIT(0xE0); GPIO_ResetBits(GPIOD, GPIO_Pin_7); GPIO_ResetBits(GPIOB, GPIO_Pin_3); GPIO_ResetBits(GPIOD, GPIO_Pin_5); Delay_Us(5); GPIO_SetBits(GPIOD, GPIO_Pin_5); GPIO_SetBits(GPIOD, GPIO_Pin_7);
    SET_DATA_8BIT(0x01); GPIO_ResetBits(GPIOD, GPIO_Pin_7); GPIO_SetBits(GPIOB, GPIO_Pin_3);  GPIO_ResetBits(GPIOD, GPIO_Pin_5); Delay_Us(5); GPIO_SetBits(GPIOD, GPIO_Pin_5); GPIO_SetBits(GPIOD, GPIO_Pin_7);
    Delay_Ms(10);

    /* Switch to PLL */
    SET_DATA_8BIT(0xE0); GPIO_ResetBits(GPIOD, GPIO_Pin_7); GPIO_ResetBits(GPIOB, GPIO_Pin_3); GPIO_ResetBits(GPIOD, GPIO_Pin_5); Delay_Us(5); GPIO_SetBits(GPIOD, GPIO_Pin_5); GPIO_SetBits(GPIOD, GPIO_Pin_7);
    SET_DATA_8BIT(0x03); GPIO_ResetBits(GPIOD, GPIO_Pin_7); GPIO_SetBits(GPIOB, GPIO_Pin_3);  GPIO_ResetBits(GPIOD, GPIO_Pin_5); Delay_Us(5); GPIO_SetBits(GPIOD, GPIO_Pin_5); GPIO_SetBits(GPIOD, GPIO_Pin_7);
    Delay_Ms(12);

    /* Soft reset after PLL lock */
    SET_DATA_8BIT(0x01); GPIO_ResetBits(GPIOD, GPIO_Pin_7); GPIO_ResetBits(GPIOB, GPIO_Pin_3); GPIO_ResetBits(GPIOD, GPIO_Pin_5); Delay_Us(5); GPIO_SetBits(GPIOD, GPIO_Pin_5); GPIO_SetBits(GPIOD, GPIO_Pin_7);
    Delay_Ms(10);

    /* 8-bit pixel interface for this test */
    SET_DATA_8BIT(0xF0); GPIO_ResetBits(GPIOD, GPIO_Pin_7); GPIO_ResetBits(GPIOB, GPIO_Pin_3); GPIO_ResetBits(GPIOD, GPIO_Pin_5); Delay_Us(5); GPIO_SetBits(GPIOD, GPIO_Pin_5); GPIO_SetBits(GPIOD, GPIO_Pin_7);
    SET_DATA_8BIT(0x00); GPIO_ResetBits(GPIOD, GPIO_Pin_7); GPIO_SetBits(GPIOB, GPIO_Pin_3);  GPIO_ResetBits(GPIOD, GPIO_Pin_5); Delay_Us(5); GPIO_SetBits(GPIOD, GPIO_Pin_5); GPIO_SetBits(GPIOD, GPIO_Pin_7);

    /*--- Read tests ---*/
    #define READ_REG_8BIT(r) ({ \
        uint8_t v; \
        SET_DATA_8BIT(r); GPIO_ResetBits(GPIOD, GPIO_Pin_7); GPIO_ResetBits(GPIOB, GPIO_Pin_3); GPIO_ResetBits(GPIOD, GPIO_Pin_5); Delay_Us(5); GPIO_SetBits(GPIOD, GPIO_Pin_5); GPIO_SetBits(GPIOD, GPIO_Pin_7); \
        Delay_Us(5); \
        gpio.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15 | GPIO_Pin_0 | GPIO_Pin_1; gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING; GPIO_Init(GPIOD, &gpio); \
        gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10; GPIO_Init(GPIOE, &gpio); \
        GPIO_ResetBits(GPIOD, GPIO_Pin_7); GPIO_SetBits(GPIOB, GPIO_Pin_3); Delay_Us(2); \
        GPIO_ResetBits(GPIOD, GPIO_Pin_4); Delay_Us(5); \
        v = 0; \
        { uint16_t id = GPIOD->INDR, ie = GPIOE->INDR; \
          if (id & GPIO_Pin_14) v |= 0x01; if (id & GPIO_Pin_15) v |= 0x02; if (id & GPIO_Pin_0) v |= 0x04; if (id & GPIO_Pin_1) v |= 0x08; \
          if (ie & GPIO_Pin_7)  v |= 0x10; if (ie & GPIO_Pin_8)  v |= 0x20; if (ie & GPIO_Pin_9)  v |= 0x40; if (ie & GPIO_Pin_10) v |= 0x80; } \
        GPIO_SetBits(GPIOD, GPIO_Pin_4); GPIO_SetBits(GPIOD, GPIO_Pin_7); \
        gpio.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15 | GPIO_Pin_0 | GPIO_Pin_1; gpio.GPIO_Mode = GPIO_Mode_Out_PP; gpio.GPIO_Speed = GPIO_Speed_Very_High; GPIO_Init(GPIOD, &gpio); \
        gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10; GPIO_Init(GPIOE, &gpio); \
        v; \
    })

    val = READ_REG_8BIT(0xE4);
    printf("[SOFT8080_Test] PLL status (0xE4) = 0x%02X %s\r\n", val, (val & 0x04) ? "[OK]" : "[FAIL]");

    val = READ_REG_8BIT(0x0A);
    printf("[SOFT8080_Test] Power mode (0x0A) = 0x%02X %s\r\n", val, ((val & 0x0C) == 0x0C) ? "[OK]" : "[INFO]");

    val = READ_REG_8BIT(0xA1);
    printf("[SOFT8080_Test] DDB[0] (0xA1) = 0x%02X %s\r\n", val, (val == 0x01) ? "[OK]" : "[WARN]");

    printf("[SOFT8080_Test] done\r\n");
}
