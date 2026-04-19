/********************************** (C) COPYRIGHT *******************************
* File Name          : fmc_driver.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : FMC NORSRAM Bank1 driver for SSD1963 8080 parallel interface.
*                      Defines GPIO pin mappings, FMC base address, and command/data
*                      address mapping for 16-bit 8080 mode.
********************************************************************************/
#ifndef __FMC_DRIVER_H
#define __FMC_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32h417.h"

/*=============================================================================
 *  FMC Bank1 Base Address (NORSRAM1)
 *  Bank1 region: 0x6000 0000 ~ 0x6FFF FFFF
 *=============================================================================*/
#define FMC_BANK1_BASE_ADDR     ((uint32_t)0x60000000)

/*=============================================================================
 *  SSD1963 Command / Data Address Mapping
 *  DC (RS) pin is connected to PB3 (FSMC_A1).
 *  When A1 = 0 -> DC = 0 (Command)
 *  When A1 = 1 -> DC = 1 (Data), address offset = 2^(1+1) = 4
 *=============================================================================*/
#define SSD1963_CMD_ADDR        (*(volatile uint16_t *)(FMC_BANK1_BASE_ADDR + 0x00000000))
#define SSD1963_DATA_ADDR       (*(volatile uint16_t *)(FMC_BANK1_BASE_ADDR + 0x00000004))

/*=============================================================================
 *  FMC GPIO Pin Definitions
 *  Data bus: FMC_D0~D15 (16-bit)
 *  Control bus: WR(PD5-NWE), RD(PD4-NOE), CS(PD7-NE1), DC(PB3-A1)
 *=============================================================================*/

/* --- PD port data lines --- */
#define FMC_D0_PIN              GPIO_Pin_14
#define FMC_D1_PIN              GPIO_Pin_15
#define FMC_D2_PIN              GPIO_Pin_0
#define FMC_D3_PIN              GPIO_Pin_1
#define FMC_D13_PIN             GPIO_Pin_8
#define FMC_D14_PIN             GPIO_Pin_9
#define FMC_D15_PIN             GPIO_Pin_10

/* --- PE port data lines --- */
#define FMC_D4_PIN              GPIO_Pin_7
#define FMC_D5_PIN              GPIO_Pin_8
#define FMC_D6_PIN              GPIO_Pin_9
#define FMC_D7_PIN              GPIO_Pin_10
#define FMC_D8_PIN              GPIO_Pin_11
#define FMC_D9_PIN              GPIO_Pin_12
#define FMC_D10_PIN             GPIO_Pin_13
#define FMC_D11_PIN             GPIO_Pin_14
#define FMC_D12_PIN             GPIO_Pin_15

/* --- Control lines --- */
#define FMC_WR_PIN              GPIO_Pin_5      /* PD5 - FSMC_NWE */
#define FMC_RD_PIN              GPIO_Pin_4      /* PD4 - FSMC_NOE */
#define FMC_CS_PIN              GPIO_Pin_7      /* PD7 - FSMC_NE1 */
#define FMC_DC_PIN              GPIO_Pin_3      /* PB3 - FSMC_A1  */

/* --- GPIO Ports --- */
#define FMC_D0_D3_PORT          GPIOD
#define FMC_D13_D15_PORT        GPIOD
#define FMC_D4_D12_PORT         GPIOE
#define FMC_CTRL_PORT           GPIOD
#define FMC_DC_PORT             GPIOB

/* --- Alternate Function ---
 * Verified from CH32H417 FSMC pin mapping table:
 *   PD7(AF12)=FSMC_NE1, PD4(AF12)=FSMC_NOE, PD5(AF12)=FSMC_NWE,
 *   PB3(AF12)=FSMC_A1, PD14/15/0/1/8/9/10(AF12)=FSMC_Dx,
 *   PE7~15(AF12)=FSMC_Dx
 */
#define FMC_GPIO_AF             GPIO_AF12       /* FSMC/FMC alternate function */

/*=============================================================================
 *  Function Prototypes
 *=============================================================================*/

/**
 * @brief  Initialize FMC NORSRAM Bank1 for 16-bit 8080 parallel interface.
 *         Configures GPIO pins to FMC alternate function and sets up
 *         FMC timing parameters for SSD1963.
 * @param  None
 * @return None
 */
void FMC_Driver_Init(void);

/**
 * @brief  Deinitialize FMC NORSRAM Bank1.
 * @param  None
 * @return None
 */
void FMC_Driver_DeInit(void);

#ifdef __cplusplus
}
#endif

#endif /* __FMC_DRIVER_H */
