/********************************** (C) COPYRIGHT *******************************
* File Name          : fmc_driver.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/20
* Description        : FMC NORSRAM Bank1 8080 mode driver for SSD1963.
*                      CMD/DATA address mapping via FMC_A1 (PB3).
********************************************************************************/
#ifndef __FMC_DRIVER_H
#define __FMC_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32h417.h"

/*=============================================================================
 *  FMC Bank1 NORSRAM1 Address Mapping
 *  Bank1 base: 0x60000000
 *  DC (RS) is connected to FMC_A1 (PB3).
 *  In 16-bit mode, internal address bit N+1 maps to FMC_AN.
 *  So FMC_A1 = 1 => address offset = 1 << (1+1) = 0x04.
 *=============================================================================*/
#define SSD1963_CMD_ADDR    ((uint32_t)0x60000000)
#define SSD1963_DATA_ADDR   ((uint32_t)0x60000004)

#define SSD1963_CMD         (*(volatile uint16_t *)SSD1963_CMD_ADDR)
#define SSD1963_DATA        (*(volatile uint16_t *)SSD1963_DATA_ADDR)

/*=============================================================================
 *  Function Prototypes
 *=============================================================================*/
void FMC_Driver_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __FMC_DRIVER_H */
