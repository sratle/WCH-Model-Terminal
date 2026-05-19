/********************************** (C) COPYRIGHT *******************************
* File Name          : fmc_driver.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/20
* Description        : FMC NORSRAM Bank1 initialization for SSD1963 8080 mode.
*                      16-bit data bus, FMC_A1 as DC (RS), NE1 as CS.
********************************************************************************/
#include "fmc_driver.h"
#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"
#include "debug.h"

/*=============================================================================
 *  Pin Mapping (from Display_1_Plan.md Section 2.1 / 2.2)
 *  Data bus:  PD14/15/0/1, PE7~15, PD8/9/10  (FMC_D0~D15)
 *  Control:   PD5=NWE, PD4=NOE, PD7=NE1, PB3=A1(RS), PA0=RESET
 *=============================================================================*/

void FMC_Driver_Init(void)
{
    FMC_NORSRAMInitTypeDef FMC_NORSRAMInitStructure = {0};
    FMC_NORSRAMTimingInitTypeDef readWriteTiming = {0};
    FMC_NORSRAMTimingInitTypeDef writeTiming = {0};
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    printf("[FMC_Driver_Init] start\r\n");

    /* Enable peripheral clocks */
    RCC_HBPeriphClockCmd(RCC_HBPeriph_FMC, ENABLE);
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOA | RCC_HB2Periph_GPIOB |
                           RCC_HB2Periph_GPIOD | RCC_HB2Periph_GPIOE |
                           RCC_HB2Periph_AFIO, ENABLE);

    /* CRITICAL: PB3 is JTDO (JTAG) by default. Must disable SWJ before PB3
     * can function as FMC_A1 (RS/DC). This frees PB3 for alternate function.
     * Note: SWD debugging is also disabled; use bootloader for reflashing. */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);

    /* PA0: SSD1963 RESET, push-pull output, default high */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_0);

    /* PB3: FMC_A1 (RS/DC), alternate function push-pull */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF12);

    /* PD0,1,4,5,8,9,10,14,15: FMC D2,D3,NOE,NWE,D13,D14,D15,D0,D1 */
    /* PD7 is NOT used as FMC_NE1; CS is controlled manually via GPIO */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 |
                                  GPIO_Pin_5 | GPIO_Pin_8 | GPIO_Pin_9 |
                                  GPIO_Pin_10 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource0, GPIO_AF12);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource1, GPIO_AF12);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource4, GPIO_AF12);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF12);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF12);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF12);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource10, GPIO_AF12);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource14, GPIO_AF12);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource15, GPIO_AF12);

    /* PD7: CS manual control, push-pull output, default low (SSD1963 always selected) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);

    /* PE7~15: FMC_D4~D12 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 |
                                  GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 |
                                  GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOE, GPIO_PinSource7, GPIO_AF12);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource8, GPIO_AF12);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource9, GPIO_AF12);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource10, GPIO_AF12);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource11, GPIO_AF12);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource12, GPIO_AF12);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource13, GPIO_AF12);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource14, GPIO_AF12);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource15, GPIO_AF12);

    /*-------------------------------------------------------------------------
     *  FMC NORSRAM Timing
     *  Values taken from the verified CH32H417 ATK-MD0280 example.
     *  User confirmed HCLK = 100MHz (1 cycle = 10ns).
     *  Read cycle: ~150ns address + 150ns data = 300ns+ (very safe).
     *  Write cycle: ~30ns data setup (may be tight; increase to 0x05 if unstable).
     *------------------------------------------------------------------------*/
    readWriteTiming.FMC_AddressSetupTime = 0x0F;
    readWriteTiming.FMC_AddressHoldTime = 0x03;
    readWriteTiming.FMC_DataSetupTime = 0x0F;
    readWriteTiming.FMC_BusTurnAroundDuration = 0x00;
    readWriteTiming.FMC_CLKDivision = 0x00;
    readWriteTiming.FMC_DataLatency = 0x00;
    readWriteTiming.FMC_AccessMode = FMC_AccessMode_A;

    /* Write timing aligned with WCH official ATK-MD0280 example (lcd.c):
     *  ADDSET=0x00, DATAST=0x03, BUSTURN=0x00 => 5 HCLK cycles per write
     *  At 100MHz HCLK: 50ns per write, ~19ms for full 800x480 fill.
     *  SSD1963 8080 spec: t_PWLW min 12ns, t_AS min 1ns, t_AH min 2ns.
     *  All comfortably met at 50ns cycle time. */
    writeTiming.FMC_AddressSetupTime = 0x00;
    writeTiming.FMC_AddressHoldTime = 0x00;
    writeTiming.FMC_DataSetupTime = 0x03;
    writeTiming.FMC_BusTurnAroundDuration = 0x00;
    writeTiming.FMC_AccessMode = FMC_AccessMode_A;

    FMC_NORSRAMInitStructure.FMC_Bank = FMC_Bank1_NORSRAM1;
    FMC_NORSRAMInitStructure.FMC_DataAddressMux = FMC_DataAddressMux_Disable;
    FMC_NORSRAMInitStructure.FMC_MemoryType = FMC_MemoryType_SRAM;
    FMC_NORSRAMInitStructure.FMC_MemoryDataWidth = FMC_MemoryDataWidth_16b;
    FMC_NORSRAMInitStructure.FMC_BurstAccessMode = FMC_BurstAccessMode_Disable;
    FMC_NORSRAMInitStructure.FMC_WaitSignalPolarity = FMC_WaitSignalPolarity_Low;
    FMC_NORSRAMInitStructure.FMC_AsynchronousWait = FMC_AsynchronousWait_Disable;
    FMC_NORSRAMInitStructure.FMC_WaitSignalActive = FMC_WaitSignalActive_BeforeWaitState;
    FMC_NORSRAMInitStructure.FMC_WriteOperation = FMC_WriteOperation_Enable;
    FMC_NORSRAMInitStructure.FMC_WaitSignal = FMC_WaitSignal_Disable;
    FMC_NORSRAMInitStructure.FMC_ExtendedMode = FMC_ExtendedMode_Enable;
    FMC_NORSRAMInitStructure.FMC_WriteBurst = FMC_WriteBurst_Disable;
    FMC_NORSRAMInitStructure.FMC_ReadWriteTimingStruct = &readWriteTiming;
    FMC_NORSRAMInitStructure.FMC_WriteTimingStruct = &writeTiming;
    FMC_NORSRAMInitStructure.FMC_CPSIZE = FMC_CPSIZE_None;
    FMC_NORSRAMInitStructure.FMC_BMP = FMC_BMP_Mode0;

    FMC_NORSRAMInit(&FMC_NORSRAMInitStructure);
    FMC_NORSRAMCmd(FMC_Bank1_NORSRAM1, ENABLE);

    printf("[FMC_Driver_Init] done\r\n");
}
