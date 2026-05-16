#include "CH585F.h"
#include "ch32h417_exti.h"

extern ch585f_t ch585f_g;

/* Private helper: assert NSS (PE11 output push-pull, drive low) */
static void CH585F_NSS_Assert(void)
{
    EXTI->INTENR &= ~EXTI_Line11;
    GPIOE->CFGHR = (GPIOE->CFGHR & ~(0x0F << 12)) | (0x01 << 12);
    GPIO_ResetBits(CH585F_SPI_NSS_PORT, CH585F_SPI_NSS_PIN);
    ch585f_g.nss_notify = 0;
    EXTI->INTFR = EXTI_Line11;
}

/* Private helper: release NSS (PE11 input pull-up, allow CH585F to pull it low) */
static void CH585F_NSS_Release(void)
{
    GPIO_SetBits(CH585F_SPI_NSS_PORT, CH585F_SPI_NSS_PIN);
    GPIOE->CFGHR = (GPIOE->CFGHR & ~(0x0F << 12)) | (0x08 << 12);
    GPIOE->BSHR = GPIO_Pin_11;
    EXTI->INTENR |= EXTI_Line11;
}

void CH585F_Init(ch585f_t *ch585f)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef SPI_InitStructure = {0};
    EXTI_InitTypeDef EXTI_InitStructure = {0};

    /* Enable GPIO, SPI4 and AFIO clocks */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOE, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI4, ENABLE);

    /* Configure SPI4 GPIO: MOSI(PE14-AF5), MISO(PE13-AF5), SCK(PE12-AF5) */
    GPIO_PinAFConfig(CH585F_SPI_MOSI_PORT, GPIO_PinSource14, CH585F_SPI_MOSI_AF);
    GPIO_PinAFConfig(CH585F_SPI_MISO_PORT, GPIO_PinSource13, CH585F_SPI_MISO_AF);
    GPIO_PinAFConfig(CH585F_SPI_SCK_PORT, GPIO_PinSource12, CH585F_SPI_SCK_AF);

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

    /* PE11: NSS / Notify line. Default as input pull-up to receive CH585F notify pulse */
    GPIO_InitStructure.GPIO_Pin = CH585F_SPI_NSS_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(CH585F_SPI_NSS_PORT, &GPIO_InitStructure);

    /* Configure EXTI11 on PE11: falling edge trigger (CH585F pulls PB12 low to notify) */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOE, GPIO_PinSource11);
    EXTI_InitStructure.EXTI_Line = EXTI_Line11;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* Enable EXTI15_8 NVIC */
    NVIC_EnableIRQ(EXTI15_8_IRQn);

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
    ch585f->nss_notify = 0;
}

// 发送数据
void CH585F_Send_Data(ch585f_t *ch585f, uint8_t *data, uint16_t length)
{
    uint16_t i;
    CH585F_NSS_Assert();
    for (i = 0; i < length; i++)
    {
        while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(CH585F_SPI, data[i]);
        while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_RXNE) == RESET);
        (void)SPI_I2S_ReceiveData(CH585F_SPI);
    }
    /* 等待 SPI 总线空闲后再拉高 NSS */
    while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_BSY) == SET);
    CH585F_NSS_Release();
}

// 接收数据，通过发送dummy字节同步读取MISO数据
void CH585F_Recv_Data(ch585f_t *ch585f, uint8_t *buf, uint16_t length)
{
    uint16_t i;
    CH585F_NSS_Assert();
    for (i = 0; i < length; i++)
    {
        while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(CH585F_SPI, 0x00);
        while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_RXNE) == RESET);
        buf[i] = (uint8_t)SPI_I2S_ReceiveData(CH585F_SPI);
    }
    /* 等待 SPI 总线空闲后再拉高 NSS */
    while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_BSY) == SET);
    CH585F_NSS_Release();
}

// SPI 全双工传输：同时发送 tx_buf 和接收 rx_buf
void CH585F_SPI_Transfer(ch585f_t *ch585f, uint8_t *tx_buf, uint8_t *rx_buf, uint16_t length)
{
    uint16_t i;
    CH585F_NSS_Assert();
    for (i = 0; i < length; i++)
    {
        while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_TXE) == RESET);
        SPI_I2S_SendData(CH585F_SPI, tx_buf[i]);
        while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_RXNE) == RESET);
        rx_buf[i] = (uint8_t)SPI_I2S_ReceiveData(CH585F_SPI);
    }
    /* 等待 SPI 总线空闲后再拉高 NSS */
    while (SPI_I2S_GetFlagStatus(CH585F_SPI, SPI_I2S_FLAG_BSY) == SET);
    CH585F_NSS_Release();
}

/* EXTI15_8 IRQHandler: PE11 falling edge = CH585F has data to send */
void EXTI15_8_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI15_8_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line11) != RESET)
    {
        ch585f_g.nss_notify = 1;
        EXTI_ClearITPendingBit(EXTI_Line11);
    }
}
