#ifndef __SOFT_8080_H__
#define __SOFT_8080_H__

#include <stdint.h>
#include "ch32h417.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CS_HIGH() do { GPIOD->BSHR = GPIO_Pin_7; } while(0)
#define CS_LOW()  do { GPIOD->BCR  = GPIO_Pin_7; } while(0)
#define RS_HIGH() do { GPIOB->BSHR = GPIO_Pin_3; } while(0)
#define RS_LOW()  do { GPIOB->BCR  = GPIO_Pin_3; } while(0)
#define WR_HIGH() do { GPIOD->BSHR = GPIO_Pin_5; } while(0)
#define WR_LOW()  do { GPIOD->BCR  = GPIO_Pin_5; } while(0)
#define RD_HIGH() do { GPIOD->BSHR = GPIO_Pin_4; } while(0)
#define RD_LOW()  do { GPIOD->BCR  = GPIO_Pin_4; } while(0)

/* Initialize GPIO pins for software 8080 interface */
void SOFT8080_Init(void);

/* Optional hardware test (proven working 8-bit GPIO test sequence) */
void SOFT8080_Test(void);

/* Low-level 8080 access (16-bit write, 8-bit reliable read) */
void SOFT8080_WriteCmd(uint16_t cmd);
void SOFT8080_WriteData(uint16_t data);
uint16_t SOFT8080_ReadData(void);
void SOFT8080_WriteReg(uint16_t reg, uint16_t val);
uint16_t SOFT8080_ReadReg(uint16_t reg);

/* Window / GRAM access */
void SOFT8080_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void SOFT8080_WriteData16(uint16_t data);
void SOFT8080_WriteBuffer(const uint16_t *buf, uint32_t len);
void SOFT8080_Clear(uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* __SOFT_8080_H__ */
