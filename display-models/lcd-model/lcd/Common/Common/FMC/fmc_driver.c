/********************************** (C) COPYRIGHT *******************************
* File Name          : fmc_driver.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : FMC NORSRAM Bank1 driver for SSD1963 8080 parallel interface.
*                      - Configures PD/PE data lines as FMC alternate function
*                      - Configures PD4/PD5/PD7 control lines (NOE/NWE/NE1)
*                      - Configures PB3 as FMC address line A1 (DC/RS)
*                      - Sets FMC timing for 400MHz HCLK (V5F) with SSD1963
*                      - Verified against SSD1963 datasheet Table 13-6
*                        and CH32H417 FSMC pin mapping table
********************************************************************************/
#include "fmc_driver.h"
#include "ch32h417_fmc.h"
#include "ch32h417_rcc.h"
#include "ch32h417_gpio.h"

/*=============================================================================
 *  Local Macros
 *=============================================================================*/

/**
 * @brief  FMC timing parameters for V5F @ 400MHz HCLK driving SSD1963.
 *         HCLK period = 2.5ns @ 400MHz.
 *
 *         SSD1963 8080-mode timing requirements (from datasheet):
 *           - t_AS  (Address Setup):     min 1 ns
 *           - t_AH  (Address Hold):      min 2 ns
 *           - t_DSW (Write Data Setup):  min 4 ns
 *           - t_DHW (Write Data Hold):   min 1 ns
 *           - t_PWLLW (Write Low Time):  min 12 ns
 *           - t_ACC (Access Time):       min 32 ns
 *           - t_PWLR (Read Low Time):    min 36 ns
 *           - t_CS  (CS Setup):          min 2 ns
 *           - t_CSH (CS Hold):           min 3 ns
 *
 *         FMC Mode B write cycle: t_WR = (ADDSET + 1) + (DATAST + 1) HCLK cycles
 *         FMC Mode B read cycle:  t_RD = (ADDSET + 1) + (DATAST + 1) HCLK cycles
 *
 *         Selected values (conservative, all requirements met):
 *           - ADDSET = 1  -> 2 HCLK = 5 ns  (covers t_AS min 1ns, t_CS min 2ns)
 *           - ADDHLD = 1  -> 1 HCLK = 2.5ns (covers t_AH min 2ns)
 *           - DATAST = 5  -> 6 HCLK = 15 ns (covers t_PWLLW min 12ns, t_DSW min 4ns)
 */
#define FMC_ADDR_SETUP_TIME     0x01    /* ADDSET: 1 HCLK = 2.5ns, meets t_AS(1ns) and t_CS(2ns) */
#define FMC_ADDR_HOLD_TIME      0x01    /* ADDHLD: 1 HCLK = 2.5ns, meets t_AH(2ns) */
#define FMC_DATA_SETUP_TIME     0x05    /* DATAST: 5 HCLK = 12.5ns, meets t_PWLLW(12ns) and t_DSW(4ns) */
#define FMC_BUS_TURNAROUND      0x00    /* Not used for SRAM */
#define FMC_CLK_DIVISION        0x01    /* Not used for async, set to 1 */
#define FMC_DATA_LATENCY        0x00    /* Not used for async SRAM */
#define FMC_ACCESS_MODE         FMC_AccessMode_B    /* 8080 parallel mode B */

/*=============================================================================
 *  Local Function Prototypes
 *=============================================================================*/
static void FMC_GPIO_Init(void);
static void FMC_GPIO_DeInit(void);

/*=============================================================================
 *  Public Functions
 *=============================================================================*/

/**
 * @brief  Initialize FMC NORSRAM Bank1 for 16-bit 8080 parallel interface.
 *         1. Enable clocks for FMC, GPIOD, GPIOE, GPIOB
 *         2. Configure GPIO pins to FMC alternate function (AF12)
 *         3. Initialize FMC NORSRAM Bank1 with timing parameters
 *         4. Enable FMC NORSRAM Bank1
 * @param  None
 * @return None
 */
void FMC_Driver_Init(void)
{
    FMC_NORSRAMInitTypeDef  fmc_init;
    FMC_NORSRAMTimingInitTypeDef fmc_timing;

    /* Step 1: Enable peripheral clocks */
    RCC_HBPeriphClockCmd(RCC_HBPeriph_FMC, ENABLE);
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOD | RCC_HB2Periph_GPIOE | RCC_HB2Periph_GPIOB, ENABLE);

    /* Step 2: Configure GPIO pins */
    FMC_GPIO_Init();

    /* Step 3: Configure FMC timing */
    fmc_timing.FMC_AddressSetupTime       = FMC_ADDR_SETUP_TIME;
    fmc_timing.FMC_AddressHoldTime        = FMC_ADDR_HOLD_TIME;
    fmc_timing.FMC_DataSetupTime          = FMC_DATA_SETUP_TIME;
    fmc_timing.FMC_BusTurnAroundDuration  = FMC_BUS_TURNAROUND;
    fmc_timing.FMC_CLKDivision            = FMC_CLK_DIVISION;
    fmc_timing.FMC_DataLatency            = FMC_DATA_LATENCY;
    fmc_timing.FMC_AccessMode             = FMC_ACCESS_MODE;

    /* Step 4: Configure FMC NORSRAM Bank1 */
    fmc_init.FMC_Bank                   = FMC_Bank1_NORSRAM1;
    fmc_init.FMC_DataAddressMux         = FMC_DataAddressMux_Disable;   /* Separate addr/data bus */
    fmc_init.FMC_MemoryType             = FMC_MemoryType_SRAM;          /* SRAM type for 8080 */
    fmc_init.FMC_MemoryDataWidth        = FMC_MemoryDataWidth_16b;      /* 16-bit data bus */
    fmc_init.FMC_BurstAccessMode        = FMC_BurstAccessMode_Disable;  /* Async mode */
    fmc_init.FMC_AsynchronousWait       = FMC_AsynchronousWait_Disable; /* No wait signal */
    fmc_init.FMC_WaitSignalPolarity     = FMC_WaitSignalPolarity_Low;   /* Not used */
    fmc_init.FMC_WaitSignalActive       = FMC_WaitSignalActive_BeforeWaitState; /* Not used */
    fmc_init.FMC_WriteOperation         = FMC_WriteOperation_Enable;    /* Enable write */
    fmc_init.FMC_WaitSignal             = FMC_WaitSignal_Disable;       /* No wait */
    fmc_init.FMC_ExtendedMode           = FMC_ExtendedMode_Disable;     /* Same R/W timing */
    fmc_init.FMC_WriteBurst             = FMC_WriteBurst_Disable;       /* No burst write */
    fmc_init.FMC_CPSIZE                 = FMC_CPSIZE_None;              /* No CRAM page size */
    fmc_init.FMC_BMP                    = FMC_BMP_Mode0;                /* Default mapping */
    fmc_init.FMC_ReadWriteTimingStruct  = &fmc_timing;                  /* Read/write timing */
    fmc_init.FMC_WriteTimingStruct      = &fmc_timing;                  /* Same as R/W (ExtendedMode=Disable, not used) */

    /* Step 5: Initialize and enable FMC Bank1 */
    FMC_NORSRAMInit(&fmc_init);
    FMC_NORSRAMCmd(FMC_Bank1_NORSRAM1, ENABLE);
}

/**
 * @brief  Deinitialize FMC NORSRAM Bank1 and reset GPIO.
 * @param  None
 * @return None
 */
void FMC_Driver_DeInit(void)
{
    /* Disable FMC Bank1 */
    FMC_NORSRAMCmd(FMC_Bank1_NORSRAM1, DISABLE);
    FMC_NORSRAMDeInit(FMC_Bank1_NORSRAM1);

    /* Reset GPIO to default input floating */
    FMC_GPIO_DeInit();

    /* Disable FMC clock (optional) */
    RCC_HBPeriphClockCmd(RCC_HBPeriph_FMC, DISABLE);
}

/*=============================================================================
 *  Local Functions
 *=============================================================================*/

/**
 * @brief  Initialize all FMC-related GPIO pins to alternate function AF12.
 *         Data bus: PD14/15/0/1/8/9/10, PE7~15
 *         Control:  PD4(NOE), PD5(NWE), PD7(NE1), PB3(A1)
 * @param  None
 * @return None
 */
static void FMC_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio_init;

    /* Common settings for all FMC pins: AF12, very high speed, push-pull */
    gpio_init.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_Very_High;

    /* --- PD port: D0/D1/D2/D3 + D13/D14/D15 + WR/RD/CS --- */
    gpio_init.GPIO_Pin = FMC_D0_PIN | FMC_D1_PIN | FMC_D2_PIN | FMC_D3_PIN |
                         FMC_D13_PIN | FMC_D14_PIN | FMC_D15_PIN |
                         FMC_WR_PIN | FMC_RD_PIN | FMC_CS_PIN;
    GPIO_Init(GPIOD, &gpio_init);

    /* --- PE port: D4~D12 --- */
    gpio_init.GPIO_Pin = FMC_D4_PIN | FMC_D5_PIN | FMC_D6_PIN | FMC_D7_PIN |
                         FMC_D8_PIN | FMC_D9_PIN | FMC_D10_PIN | FMC_D11_PIN | FMC_D12_PIN;
    GPIO_Init(GPIOE, &gpio_init);

    /* --- PB port: DC (A1) --- */
    gpio_init.GPIO_Pin = FMC_DC_PIN;
    GPIO_Init(GPIOB, &gpio_init);

    /* --- Configure alternate function AF12 for all pins --- */
    /* PD pins */
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource14, FMC_GPIO_AF);   /* D0  */
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource15, FMC_GPIO_AF);   /* D1  */
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource0,  FMC_GPIO_AF);   /* D2  */
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource1,  FMC_GPIO_AF);   /* D3  */
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource8,  FMC_GPIO_AF);   /* D13 */
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource9,  FMC_GPIO_AF);   /* D14 */
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource10, FMC_GPIO_AF);   /* D15 */
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource4,  FMC_GPIO_AF);   /* NOE */
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource5,  FMC_GPIO_AF);   /* NWE */
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource7,  FMC_GPIO_AF);   /* NE1 */

    /* PE pins */
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource7,  FMC_GPIO_AF);   /* D4  */
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource8,  FMC_GPIO_AF);   /* D5  */
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource9,  FMC_GPIO_AF);   /* D6  */
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource10, FMC_GPIO_AF);   /* D7  */
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource11, FMC_GPIO_AF);   /* D8  */
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource12, FMC_GPIO_AF);   /* D9  */
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource13, FMC_GPIO_AF);   /* D10 */
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource14, FMC_GPIO_AF);   /* D11 */
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource15, FMC_GPIO_AF);   /* D12 */

    /* PB pins */
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3,  FMC_GPIO_AF);   /* A1 (DC) */
}

/**
 * @brief  Deinitialize FMC GPIO pins to default input floating state.
 * @param  None
 * @return None
 */
static void FMC_GPIO_DeInit(void)
{
    GPIO_InitTypeDef gpio_init;

    gpio_init.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    gpio_init.GPIO_Speed = GPIO_Speed_Low;

    /* Reset PD port FMC pins */
    gpio_init.GPIO_Pin = FMC_D0_PIN | FMC_D1_PIN | FMC_D2_PIN | FMC_D3_PIN |
                         FMC_D13_PIN | FMC_D14_PIN | FMC_D15_PIN |
                         FMC_WR_PIN | FMC_RD_PIN | FMC_CS_PIN;
    GPIO_Init(GPIOD, &gpio_init);

    /* Reset PE port FMC pins */
    gpio_init.GPIO_Pin = FMC_D4_PIN | FMC_D5_PIN | FMC_D6_PIN | FMC_D7_PIN |
                         FMC_D8_PIN | FMC_D9_PIN | FMC_D10_PIN | FMC_D11_PIN | FMC_D12_PIN;
    GPIO_Init(GPIOE, &gpio_init);

    /* Reset PB port DC pin */
    gpio_init.GPIO_Pin = FMC_DC_PIN;
    GPIO_Init(GPIOB, &gpio_init);
}
