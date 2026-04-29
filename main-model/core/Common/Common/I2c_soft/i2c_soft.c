/********************************** (C) COPYRIGHT *******************************
 * File Name          : i2c_soft.c
 * Author             : 
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : Software I2C library implementation for CH32H417.
 *******************************************************************************/
#include "i2c_soft.h"
#include "debug.h"

#define SOFT_I2C_DELAY_US   5

static void SoftI2C_Delay(void)
{
    Delay_Us(SOFT_I2C_DELAY_US);
}

static void SoftI2C_SCL_H(void)
{
    GPIO_SetBits(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN);
}

static void SoftI2C_SCL_L(void)
{
    GPIO_ResetBits(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN);
}

static void SoftI2C_SDA_H(void)
{
    GPIO_SetBits(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN);
}

static void SoftI2C_SDA_L(void)
{
    GPIO_ResetBits(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN);
}

static uint8_t SoftI2C_SDA_Read(void)
{
    return GPIO_ReadInputDataBit(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN);
}

static void SoftI2C_SDA_Out(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = SOFT_I2C_SDA_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(SOFT_I2C_SDA_PORT, &GPIO_InitStructure);
}

static void SoftI2C_SDA_In(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = SOFT_I2C_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SOFT_I2C_SDA_PORT, &GPIO_InitStructure);
}

void SoftI2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = SOFT_I2C_SCL_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(SOFT_I2C_SCL_PORT, &GPIO_InitStructure);

    SoftI2C_SDA_Out();

    SoftI2C_SCL_H();
    SoftI2C_SDA_H();
}

void SoftI2C_Start(void)
{
    SoftI2C_SDA_H();
    SoftI2C_SCL_H();
    SoftI2C_Delay();
    SoftI2C_SDA_L();
    SoftI2C_Delay();
    SoftI2C_SCL_L();
    SoftI2C_Delay();
}

void SoftI2C_Stop(void)
{
    SoftI2C_SCL_L();
    SoftI2C_SDA_L();
    SoftI2C_Delay();
    SoftI2C_SCL_H();
    SoftI2C_Delay();
    SoftI2C_SDA_H();
    SoftI2C_Delay();
}

void SoftI2C_SendByte(uint8_t txd)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        SoftI2C_SCL_L();
        if (txd & 0x80)
            SoftI2C_SDA_H();
        else
            SoftI2C_SDA_L();
        txd <<= 1;
        SoftI2C_Delay();
        SoftI2C_SCL_H();
        SoftI2C_Delay();
    }
    SoftI2C_SCL_L();
    SoftI2C_Delay();
}

uint8_t SoftI2C_WaitAck(void)
{
    uint16_t timeout = 5000;

    SoftI2C_SDA_In();
    SoftI2C_Delay();
    SoftI2C_SCL_H();
    SoftI2C_Delay();

    while (SoftI2C_SDA_Read())
    {
        if (--timeout == 0)
        {
            SoftI2C_SCL_L();
            SoftI2C_SDA_Out();
            return 1; /* Timeout: no ACK */
        }
    }

    /* SDA is low: ACK received. Latch SCL low before releasing bus. */
    SoftI2C_SCL_L();
    SoftI2C_Delay();
    SoftI2C_SDA_Out();
    return 0;
}

void SoftI2C_Ack(void)
{
    SoftI2C_SCL_L();
    SoftI2C_SDA_L();
    SoftI2C_Delay();
    SoftI2C_SCL_H();
    SoftI2C_Delay();
    SoftI2C_SCL_L();
    SoftI2C_Delay();
}

void SoftI2C_NAck(void)
{
    SoftI2C_SCL_L();
    SoftI2C_SDA_H();
    SoftI2C_Delay();
    SoftI2C_SCL_H();
    SoftI2C_Delay();
    SoftI2C_SCL_L();
    SoftI2C_Delay();
}

uint8_t SoftI2C_ReadByte(uint8_t ack)
{
    uint8_t i, dat = 0;

    SoftI2C_SDA_In();
    for (i = 0; i < 8; i++)
    {
        dat <<= 1;
        SoftI2C_SCL_L();
        SoftI2C_Delay();
        SoftI2C_SCL_H();
        SoftI2C_Delay();
        if (SoftI2C_SDA_Read())
            dat |= 0x01;
    }
    SoftI2C_SCL_L();
    SoftI2C_Delay();

    SoftI2C_SDA_Out();
    if (ack)
        SoftI2C_Ack();
    else
        SoftI2C_NAck();

    return dat;
}

uint8_t SoftI2C_WriteReg(uint8_t dev_addr, uint32_t reg_addr, uint8_t dat)
{
    uint8_t addr_buf[4];
    uint8_t i;

    addr_buf[0] = (reg_addr >> 16) & 0xFF;
    addr_buf[1] = (reg_addr >> 8) & 0xFF;
    addr_buf[2] = reg_addr & 0xFF;
    addr_buf[3] = 0x01;

    SoftI2C_Start();
    SoftI2C_SendByte(dev_addr << 1);
    if (SoftI2C_WaitAck()) goto i2c_wr_err;

    for (i = 0; i < 4; i++)
    {
        SoftI2C_SendByte(addr_buf[i]);
        if (SoftI2C_WaitAck()) goto i2c_wr_err;
    }

    SoftI2C_SendByte(dat);
    if (SoftI2C_WaitAck()) goto i2c_wr_err;

    SoftI2C_Stop();
    return 0;

i2c_wr_err:
    SoftI2C_Stop();
    return 1;
}

uint8_t SoftI2C_ReadReg(uint8_t dev_addr, uint32_t reg_addr, uint8_t *dat)
{
    uint8_t addr_buf[4];
    uint8_t i;

    addr_buf[0] = (reg_addr >> 16) & 0xFF;
    addr_buf[1] = (reg_addr >> 8) & 0xFF;
    addr_buf[2] = reg_addr & 0xFF;
    addr_buf[3] = 0x01;

    /* Write phase: send MAP + control byte */
    SoftI2C_Start();
    SoftI2C_SendByte(dev_addr << 1);
    if (SoftI2C_WaitAck()) goto i2c_rd_err;

    for (i = 0; i < 4; i++)
    {
        SoftI2C_SendByte(addr_buf[i]);
        if (SoftI2C_WaitAck()) goto i2c_rd_err;
    }

    /* Read phase: repeated start + read */
    SoftI2C_Start();
    SoftI2C_SendByte((dev_addr << 1) | 0x01);
    if (SoftI2C_WaitAck()) goto i2c_rd_err;

    *dat = SoftI2C_ReadByte(0);
    SoftI2C_Stop();
    return 0;

i2c_rd_err:
    SoftI2C_Stop();
    return 1;
}
