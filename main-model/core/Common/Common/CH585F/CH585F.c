#include "CH585F.h"

void CH585F_Init(ch585f_t *ch585f)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef SPI_InitStructure = {0};

    /* Enable GPIO and SPI4 clocks */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOE, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI4, ENABLE);

    /* Configure SPI4 GPIO: MOSI(PE14-AF5), MISO(PE13-AF5), SCK(PE12-AF5), NSS(PE11-AF5) */
    GPIO_PinAFConfig(CH585F_SPI_MOSI_PORT, GPIO_PinSource14, CH585F_SPI_MOSI_AF);
    GPIO_PinAFConfig(CH585F_SPI_MISO_PORT, GPIO_PinSource13, CH585F_SPI_MISO_AF);
    GPIO_PinAFConfig(CH585F_SPI_SCK_PORT, GPIO_PinSource12, CH585F_SPI_SCK_AF);
    GPIO_PinAFConfig(CH585F_SPI_NSS_PORT, GPIO_PinSource11, CH585F_SPI_NSS_AF);

    GPIO_InitStructure.GPIO_Pin = CH585F_SPI_MOSI_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH585F_SPI_MOSI_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = CH585F_SPI_MISO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(CH585F_SPI_MISO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = CH585F_SPI_SCK_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH585F_SPI_SCK_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = CH585F_SPI_NSS_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(CH585F_SPI_NSS_PORT, &GPIO_InitStructure);

    /* NSS default high (deselect slave) */
    GPIO_SetBits(CH585F_SPI_NSS_PORT, CH585F_SPI_NSS_PIN);

    /* SPI4 init: master, full duplex, 8bit, CPOL low, CPHA 1 edge, software NSS, MSB first */
    SPI_StructInit(&SPI_InitStructure);
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = CH585F_SPI_CLOCK;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(CH585F_SPI, &SPI_InitStructure);

    SPI_Cmd(CH585F_SPI, ENABLE);

    ch585f->enable = 1;
}

// 发送数据，入口参数是CH585F结构体指针，发送数据，发送数据长度
void CH585F_Send_Data(ch585f_t *ch585f, uint8_t *data, uint16_t length)
{
    uint16_t i;
    GPIO_ResetBits(CH585F_SPI_NSS_PORT, CH585F_SPI_NSS_PIN);
    for (i = 0; i < length; i++)
    {
        while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(CH585F_SPI, data[i]);
        while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_RXNE) == RESET);
        (void)SPI_I2S_ReceiveData(CH585F_SPI);
    }
    /* 等待 SPI 总线空闲后再拉高 NSS，防止最后一字节传输被截断 */
    while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_BSY) == SET);
    GPIO_SetBits(CH585F_SPI_NSS_PORT, CH585F_SPI_NSS_PIN);
}

// 接收数据，通过发送dummy字节同步读取MISO数据
void CH585F_Recv_Data(ch585f_t *ch585f, uint8_t *buf, uint16_t length)
{
    uint16_t i;
    GPIO_ResetBits(CH585F_SPI_NSS_PORT, CH585F_SPI_NSS_PIN);
    for (i = 0; i < length; i++)
    {
        while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(CH585F_SPI, 0x00);
        while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_RXNE) == RESET);
        buf[i] = (uint8_t)SPI_I2S_ReceiveData(CH585F_SPI);
    }
    /* 等待 SPI 总线空闲后再拉高 NSS，确保 Slave 完整发送最后一字节 */
    while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_BSY) == SET);
    GPIO_SetBits(CH585F_SPI_NSS_PORT, CH585F_SPI_NSS_PIN);
}

// SPI 全双工传输：同时发送 tx_buf 和接收 rx_buf
void CH585F_SPI_Transfer(ch585f_t *ch585f, uint8_t *tx_buf, uint8_t *rx_buf, uint16_t length)
{
    uint16_t i;
    GPIO_ResetBits(CH585F_SPI_NSS_PORT, CH585F_SPI_NSS_PIN);
    for (i = 0; i < length; i++)
    {
        while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(CH585F_SPI, tx_buf[i]);
        while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_RXNE) == RESET);
        rx_buf[i] = (uint8_t)SPI_I2S_ReceiveData(CH585F_SPI);
    }
    /* 等待 SPI 总线空闲后再拉高 NSS */
    while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_BSY) == SET);
    GPIO_SetBits(CH585F_SPI_NSS_PORT, CH585F_SPI_NSS_PIN);
}