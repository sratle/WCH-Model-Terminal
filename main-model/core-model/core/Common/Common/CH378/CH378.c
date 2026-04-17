#include "CH378.h"

void CH378_Init(ch378_t *ch378)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef SPI_InitStructure = {0};

    /* Enable GPIO and SPI4 clocks */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI4, ENABLE);

    /* Configure SPI1 GPIO: MOSI(PA7-AF5), MISO(PA6-AF5), SCK(PA5-AF5), NSS(PA4-AF5) */
    GPIO_PinAFConfig(CH378_SPI_MOSI_PORT, GPIO_PinSource7, CH378_SPI_MOSI_AF);
    GPIO_PinAFConfig(CH378_SPI_MISO_PORT, GPIO_PinSource6, CH378_SPI_MISO_AF);
    GPIO_PinAFConfig(CH378_SPI_SCK_PORT, GPIO_PinSource5, CH378_SPI_SCK_AF);
    GPIO_PinAFConfig(CH378_SPI_NSS_PORT, GPIO_PinSource4, CH378_SPI_NSS_AF);

    GPIO_InitStructure.GPIO_Pin = CH378_SPI_MOSI_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH378_SPI_MOSI_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = CH378_SPI_MISO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(CH378_SPI_MISO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = CH378_SPI_SCK_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH378_SPI_SCK_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = CH378_SPI_NSS_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH378_SPI_NSS_PORT, &GPIO_InitStructure);

    /* SPI4 init: master, full duplex, 8bit, CPOL low, CPHA 1 edge, software NSS, MSB first */
    SPI_StructInit(&SPI_InitStructure);
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_Mode7;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(CH378_SPI, &SPI_InitStructure);

    SPI_Cmd(CH378_SPI, ENABLE);

    ch378->enable = 1;
}

// 发送数据，入口参数是CH378结构体指针，发送数据，发送数据长度
void CH378_Send_Data(ch378_t *ch378, uint8_t *data, uint16_t length)
{
    uint16_t i;
    for (i = 0; i < length; i++)
    {
        while (SPI_I2S_GetFlagStatus(CH378_SPI, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(CH378_SPI, data[i]);
        while (SPI_I2S_GetFlagStatus(CH378_SPI, SPI_I2S_FLAG_RXNE) == RESET);
        (void)SPI_I2S_ReceiveData(CH378_SPI);
    }
}