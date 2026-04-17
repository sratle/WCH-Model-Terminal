#ifndef __CH378_H
#define __CH378_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"

// CH378 SPI clock definition
#define CH378_SPI_CLOCK SPI_BaudRatePrescaler_Mode3
#define CH378_SPI SPI1

/* CH378 gpio definitions SPI1*/
#define CH378_SPI_MOSI_PORT GPIOA
#define CH378_SPI_MOSI_PIN GPIO_Pin_7
#define CH378_SPI_MOSI_AF GPIO_AF5

#define CH378_SPI_MISO_PORT GPIOA
#define CH378_SPI_MISO_PIN GPIO_Pin_6
#define CH378_SPI_MISO_AF GPIO_AF5

#define CH378_SPI_SCK_PORT GPIOA
#define CH378_SPI_SCK_PIN GPIO_Pin_5
#define CH378_SPI_SCK_AF GPIO_AF5

#define CH378_SPI_NSS_PORT GPIOA
#define CH378_SPI_NSS_PIN GPIO_Pin_4
#define CH378_SPI_NSS_AF GPIO_AF5

typedef struct {
    uint8_t enable; // CH378 enable
} ch378_t;

// 初始化CH378结构体，初始化SPI和CH378沟通编号和设置
void CH378_Init(ch378_t *ch378);
void CH378_Send_Data(ch378_t *ch378, uint8_t *data, uint16_t length);

#endif