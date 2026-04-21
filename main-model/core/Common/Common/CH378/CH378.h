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

// CH378中断和复位引脚定义
#define CH378_INT_PORT GPIOC
#define CH378_INT_PIN GPIO_Pin_4
#define CH378_RSTI_PORT GPIOC
#define CH378_RSTI_PIN GPIO_Pin_5

#define CH378_Device_TF 0x03
#define CH378_Device_USB 0x06

typedef struct {
    uint8_t enable; // CH378 enable
    uint8_t now_device; // 0x03为TF卡，0x06为U盘
} ch378_t;

// 初始化CH378结构体，初始化SPI和CH378沟通编号和设置
void CH378_RSTI_HIGH(void);
void CH378_RSTI_LOW(void);
void CH378_SCS_HIGH(void);
void CH378_SCS_LOW(void);

void CH378_Init(ch378_t *ch378);
void CH378_Send_Cmd(ch378_t *ch378, uint8_t cmd, uint8_t *wbuf, uint8_t wlen);

uint8_t CH378_Send_Byte(ch378_t *ch378, uint8_t data);
uint8_t CH378_Read_Byte(ch378_t *ch378);

void CH378_Device_Select(ch378_t *ch378, uint8_t device);
void CH378_Open_File(ch378_t *ch378, uint8_t *file_name);
void CH378_Close_File(ch378_t *ch378, uint8_t *file_name);
void CH378_Read_File(ch378_t *ch378, uint8_t *file_name, uint8_t *rbuf, uint16_t len);
void CH378_Edit_File(ch378_t *ch378, uint8_t *file_name, uint8_t *wbuf, uint16_t len);

#endif