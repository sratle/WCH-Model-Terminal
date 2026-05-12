#ifndef __CH585F_H
#define __CH585F_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "ch32h417_spi.h"
#include "ch32h417_rcc.h"
#include "debug.h"

/* CH585F SPI clock definition */
#define CH585F_SPI_CLOCK SPI_BaudRatePrescaler_Mode3
#define CH585F_SPI SPI4

/* CH585F SPI gpio definitions SPI4 */
#define CH585F_SPI_MOSI_PORT GPIOE
#define CH585F_SPI_MOSI_PIN GPIO_Pin_14
#define CH585F_SPI_MOSI_AF GPIO_AF5

#define CH585F_SPI_MISO_PORT GPIOE
#define CH585F_SPI_MISO_PIN GPIO_Pin_13
#define CH585F_SPI_MISO_AF GPIO_AF5

#define CH585F_SPI_SCK_PORT GPIOE
#define CH585F_SPI_SCK_PIN GPIO_Pin_12
#define CH585F_SPI_SCK_AF GPIO_AF5

#define CH585F_SPI_NSS_PORT GPIOE
#define CH585F_SPI_NSS_PIN GPIO_Pin_11
#define CH585F_SPI_NSS_AF GPIO_AF5

typedef struct
{
    uint8_t enable; // CH585F enable
} ch585f_t;

// 初始化CH585F结构体，初始化SPI和CH585F沟通编号和设置
void CH585F_Init(ch585f_t *ch585f);
void CH585F_Send_Data(ch585f_t *ch585f, uint8_t *data, uint16_t length);
void CH585F_Recv_Data(ch585f_t *ch585f, uint8_t *buf, uint16_t length);

#endif