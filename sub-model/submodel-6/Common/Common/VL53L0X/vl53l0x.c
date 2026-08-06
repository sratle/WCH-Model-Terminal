#include "vl53l0x.h"
#include <string.h>

#if VL53L0X_DEBUG_EN
#include "debug.h"
#endif

vl53l0x_ctx_t vl53l0x_ctx;

/* I2C timing delay — VL53L0X supports up to 400 kHz (datasheet Table 3).
 * Bit-bang clock kept conservative (~100-200 kHz). */
#define I2C_DELAY_LOOP    40

/* SCL/SDA macro helpers */
#define SCL_H()   GPIO_SetBits(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN)
#define SCL_L()   GPIO_ResetBits(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN)
#define SDA_H()   GPIO_SetBits(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN)
#define SDA_L()   GPIO_ResetBits(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN)
#define SDA_READ() GPIO_ReadInputDataBit(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN)
#define SCL_READ() GPIO_ReadInputDataBit(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN)

static void I2C_Delay(void)
{
    volatile uint8_t i;
    for (i = 0; i < I2C_DELAY_LOOP; i++)
        ;
}

static void I2C_Start(void)
{
    SDA_H();
    SCL_H();
    I2C_Delay();
    SDA_L();
    I2C_Delay();
    SCL_L();
    I2C_Delay();
}

static void I2C_Stop(void)
{
    SCL_L();
    SDA_L();
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    SDA_H();
    I2C_Delay();
}

static uint8_t I2C_WaitAck(void)
{
    uint8_t ack;

    SDA_H();
    I2C_Delay();
    SCL_H();
    I2C_Delay();
    ack = SDA_READ();
    SCL_L();
    I2C_Delay();

    return ack ? 1 : 0;   /* 1 = NACK */
}

static void I2C_SendByte(uint8_t byte)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        if (byte & 0x80)
            SDA_H();
        else
            SDA_L();

        byte <<= 1;
        I2C_Delay();
        SCL_H();
        I2C_Delay();
        SCL_L();
        I2C_Delay();
    }
}

static uint8_t I2C_ReadByte(uint8_t ack)
{
    uint8_t i;
    uint8_t byte = 0;

    SDA_H();

    for (i = 0; i < 8; i++)
    {
        byte <<= 1;
        SCL_H();
        I2C_Delay();
        if (SDA_READ())
            byte |= 0x01;
        SCL_L();
        I2C_Delay();
    }

    if (ack)
        SDA_L();
    else
        SDA_H();

    I2C_Delay();
    SCL_H();
    I2C_Delay();
    SCL_L();
    I2C_Delay();
    SDA_H();

    return byte;
}

/* ---------------------------------------------------------------------------
 * Register access layer — datasheet Section 3:
 *   - 8-bit register INDEX (single byte), NOT 16-bit
 *   - write:  Start, 0x52, INDEX, DATA..., Stop           (Figure 15/17)
 *   - read:   Start, 0x52, INDEX, Stop, Start, 0x53, DATA..., Stop (Figure 16/18)
 *   - auto-increment indexing for sequential read/write   (Figure 17/18)
 *   - multi-byte values are MSB first                     (Table 5)
 * All functions return 0 on success, 1 on I2C NACK/error.
 * ------------------------------------------------------------------------- */

static uint8_t VL53L0X_WriteReg(uint8_t reg, uint8_t value)
{
    I2C_Start();
    I2C_SendByte(VL53L0X_I2C_ADDR_8BIT);
    if (I2C_WaitAck()) { I2C_Stop(); vl53l0x_ctx.i2c_error_count++; return 1; }

    I2C_SendByte(reg);
    if (I2C_WaitAck()) { I2C_Stop(); vl53l0x_ctx.i2c_error_count++; return 1; }

    I2C_SendByte(value);
    if (I2C_WaitAck()) { I2C_Stop(); vl53l0x_ctx.i2c_error_count++; return 1; }

    I2C_Stop();
    return 0;
}

static uint8_t VL53L0X_WriteReg16Bit(uint8_t reg, uint16_t value)
{
    I2C_Start();
    I2C_SendByte(VL53L0X_I2C_ADDR_8BIT);
    if (I2C_WaitAck()) { I2C_Stop(); vl53l0x_ctx.i2c_error_count++; return 1; }

    I2C_SendByte(reg);
    if (I2C_WaitAck()) { I2C_Stop(); vl53l0x_ctx.i2c_error_count++; return 1; }

    I2C_SendByte((uint8_t)(value >> 8));      /* MSB first (datasheet Table 5) */
    if (I2C_WaitAck()) { I2C_Stop(); vl53l0x_ctx.i2c_error_count++; return 1; }

    I2C_SendByte((uint8_t)(value & 0xFF));
    if (I2C_WaitAck()) { I2C_Stop(); vl53l0x_ctx.i2c_error_count++; return 1; }

    I2C_Stop();
    return 0;
}

static uint8_t VL53L0X_ReadReg(uint8_t reg, uint8_t *value)
{
    I2C_Start();
    I2C_SendByte(VL53L0X_I2C_ADDR_8BIT);
    if (I2C_WaitAck()) { I2C_Stop(); vl53l0x_ctx.i2c_error_count++; return 1; }

    I2C_SendByte(reg);
    if (I2C_WaitAck()) { I2C_Stop(); vl53l0x_ctx.i2c_error_count++; return 1; }

    I2C_Start();   /* repeated start */
    I2C_SendByte(VL53L0X_I2C_ADDR_8BIT | 0x01);
    if (I2C_WaitAck()) { I2C_Stop(); vl53l0x_ctx.i2c_error_count++; return 1; }

    *value = I2C_ReadByte(0);   /* last byte: master NACK */
    I2C_Stop();
    return 0;
}

static uint8_t VL53L0X_ReadReg16Bit(uint8_t reg, uint16_t *value)
{
    I2C_Start();
    I2C_SendByte(VL53L0X_I2C_ADDR_8BIT);
    if (I2C_WaitAck()) { I2C_Stop(); vl53l0x_ctx.i2c_error_count++; return 1; }

    I2C_SendByte(reg);
    if (I2C_WaitAck()) { I2C_Stop(); vl53l0x_ctx.i2c_error_count++; return 1; }

    I2C_Start();   /* repeated start */
    I2C_SendByte(VL53L0X_I2C_ADDR_8BIT | 0x01);
    if (I2C_WaitAck()) { I2C_Stop(); vl53l0x_ctx.i2c_error_count++; return 1; }

    *value = ((uint16_t)I2C_ReadByte(1)) << 8;   /* MSB first, auto-increment */
    *value |= I2C_ReadByte(0);
    I2C_Stop();
    return 0;
}

/* GPIO init for I2C (PB14/PB15) and control pins (PB12/PB13) */
static void VL53L0X_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* CRITICAL: PB14(JTDI) and PB15(JTDO/TRACESWO) are JTAG pins by default.
     * Must disable SWJ to release them for GPIO.
     * CH32V103 has no JTAG-only-disable; this disables all SWJ (JTAG+SWD).
     * SWD download still works via WCH-Link after reset. */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);

    /* I2C SCL - open drain output */
    gpio.GPIO_Pin   = VL53L0X_SCL_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_OD;
    GPIO_Init(VL53L0X_SCL_PORT, &gpio);

    /* I2C SDA - open drain output */
    gpio.GPIO_Pin   = VL53L0X_SDA_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_OD;
    GPIO_Init(VL53L0X_SDA_PORT, &gpio);

    /* XSHUT - push-pull output, default high (sensor out of HW standby) */
    gpio.GPIO_Pin   = VL53L0X_XSHUT_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(VL53L0X_XSHUT_PORT, &gpio);
    GPIO_SetBits(VL53L0X_XSHUT_PORT, VL53L0X_XSHUT_PIN);

    /* GPIO1 - input with pull-up (data ready interrupt, open drain, active low) */
    gpio.GPIO_Pin   = VL53L0X_GPIO1_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_Init(VL53L0X_GPIO1_PORT, &gpio);

    /* Set I2C bus idle */
    SCL_H();
    SDA_H();
}

#if VL53L0X_DEBUG_EN
/* ===========================================================================
 * DEBUG helpers — hardware bring-up diagnostics (requires printf on UART1).
 * Enable by setting VL53L0X_DEBUG_EN to 1 in vl53l0x.h.
 * ========================================================================= */

/* I2C bus recovery — 9 clock pulses with SDA released, then STOP */
static void VL53L0X_I2C_BusRecovery(void)
{
    uint8_t i;

    printf("[LR] I2C bus recovery...\r\n");

    SDA_H(); SCL_H();
    Delay_Ms(1);
    printf("[LR] Before recovery: SCL=%d SDA=%d\r\n", SCL_READ(), SDA_READ());

    if (SDA_READ() == 0)
    {
        for (i = 0; i < 9; i++)
        {
            SCL_L(); Delay_Ms(1);
            SCL_H(); Delay_Ms(1);
            printf("[LR]   clock %d: SDA=%d\r\n", i, SDA_READ());
            if (SDA_READ()) break;
        }
        SDA_L(); Delay_Ms(1);
        SCL_H(); Delay_Ms(1);
        SDA_H(); Delay_Ms(1);
    }

    printf("[LR] After recovery: SCL=%d SDA=%d\r\n", SCL_READ(), SDA_READ());
}

/* Scan I2C bus for any responding device (address 0x03..0x77) */
static void VL53L0X_I2C_Scan(void)
{
    uint8_t addr, found = 0;

    printf("[LR] I2C bus scan...\r\n");

    for (addr = 0x03; addr <= 0x77; addr++)
    {
        I2C_Start();
        I2C_SendByte(addr << 1);
        if (I2C_WaitAck() == 0)
        {
            printf("[LR]   Device at 0x%02X (7-bit)\r\n", addr);
            found++;
        }
        I2C_Stop();
    }

    if (found == 0)
        printf("[LR]   No device found on I2C bus!\r\n");
    else
        printf("[LR]   Total: %d device(s)\r\n", found);
}

/* Scan with SCL/SDA genuinely SWAPPED (clock on SDA pin, data on SCL pin) */
static void VL53L0X_I2C_ScanSwapped(void)
{
    uint8_t addr, found = 0;

#define SWAP_CLK_H()  GPIO_SetBits(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN)
#define SWAP_CLK_L()  GPIO_ResetBits(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN)
#define SWAP_DAT_H()  GPIO_SetBits(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN)
#define SWAP_DAT_L()  GPIO_ResetBits(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN)
#define SWAP_DAT_RD() GPIO_ReadInputDataBit(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN)

    printf("[LR] I2C scan with SCL<->SDA SWAPPED...\r\n");

    for (addr = 0x03; addr <= 0x77; addr++)
    {
        uint8_t byte = addr << 1;
        uint8_t i;

        SWAP_DAT_H();
        SWAP_CLK_H();
        I2C_Delay();
        SWAP_DAT_L();
        I2C_Delay();
        SWAP_CLK_L();
        I2C_Delay();

        for (i = 0; i < 8; i++)
        {
            if (byte & 0x80) SWAP_DAT_H();
            else             SWAP_DAT_L();
            byte <<= 1;
            I2C_Delay();
            SWAP_CLK_H();
            I2C_Delay();
            SWAP_CLK_L();
            I2C_Delay();
        }

        SWAP_DAT_H();
        I2C_Delay();
        SWAP_CLK_H();
        I2C_Delay();
        if (SWAP_DAT_RD() == 0)
        {
            printf("[LR]   Device at 0x%02X (7-bit) *** SCL/SDA SWAPPED! ***\r\n", addr);
            found++;
        }
        SWAP_CLK_L();
        I2C_Delay();

        SWAP_DAT_L();
        I2C_Delay();
        SWAP_CLK_H();
        I2C_Delay();
        SWAP_DAT_H();
        I2C_Delay();
    }

    if (found == 0)
        printf("[LR]   No device found with swapped pins either\r\n");
    else
        printf("[LR]   Total: %d device(s) with SWAPPED pins\r\n", found);

#undef SWAP_CLK_H
#undef SWAP_CLK_L
#undef SWAP_DAT_H
#undef SWAP_DAT_L
#undef SWAP_DAT_RD
}

/* Slow pin toggle for multimeter verification of the level shifter path */
static void VL53L0X_PinToggleTest(void)
{
    uint8_t i;

    printf("[LR] Toggling SCL (PB14) 5x, 200ms half-period...\r\n");
    for (i = 0; i < 10; i++)
    {
        if (i & 1) SCL_H(); else SCL_L();
        Delay_Ms(200);
    }
    SCL_H();

    printf("[LR] Toggling SDA (PB15) 5x, 200ms half-period...\r\n");
    for (i = 0; i < 10; i++)
    {
        if (i & 1) SDA_H(); else SDA_L();
        Delay_Ms(200);
    }
    SDA_H();
}

void VL53L0X_DumpDiagnostics(void)
{
    printf("[LR] ===== DIAGNOSTICS START =====\r\n");
    printf("[LR] I2C error count so far: %d\r\n", vl53l0x_ctx.i2c_error_count);
    VL53L0X_I2C_BusRecovery();
    VL53L0X_PinToggleTest();
    VL53L0X_I2C_Scan();
    VL53L0X_I2C_ScanSwapped();
    printf("[LR] ===== DIAGNOSTICS END =====\r\n");
}
#endif /* VL53L0X_DEBUG_EN */

/* ===========================================================================
 * Driver implementation
 * ========================================================================= */

/* Single reference calibration step (VHV or phase).
 * Starts one ranging with the given init byte ORed into SYSRANGE_START,
 * waits for data ready (datasheet 2.10: GPIO1/interrupt or polling). */
static uint8_t VL53L0X_PerformSingleRefCalibration(uint8_t vhv_init_byte)
{
    uint8_t status;
    uint16_t timeout = 1000;

    if (VL53L0X_WriteReg(VL53L0X_REG_SYSRANGE_START, 0x01 | vhv_init_byte))
        return 1;

    do {
        if (VL53L0X_ReadReg(VL53L0X_REG_RESULT_INTERRUPT_STATUS, &status))
            return 1;
        if (status & 0x07)
            break;
        Delay_Ms(1);
    } while (--timeout);

    if (timeout == 0)
        return 1;

    VL53L0X_WriteReg(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    VL53L0X_WriteReg(VL53L0X_REG_SYSRANGE_START, 0x00);
    return 0;
}

/* VL53L0X initialization sequence — default ranging mode.
 * Based on datasheet 2.9 (power/boot), 3.2 (reference registers) and
 * ST API defaults: VL53L0X_DataInit() + getSpadInfo() + ref calibration.
 * Returns via vl53l0x_ctx.initialized = 1 on success. */
void VL53L0X_Init(void)
{
    uint8_t model_id, model_id2, rev_id, tmp;
    uint16_t timeout;

    memset(&vl53l0x_ctx, 0, sizeof(vl53l0x_ctx_t));

    VL53L0X_GPIO_Init();

    /* Hardware reset via XSHUT (datasheet 2.9.1 option 1):
     * XSHUT low = HW standby, raise XSHUT -> FW boot, tBOOT = 1.2ms max */
    GPIO_ResetBits(VL53L0X_XSHUT_PORT, VL53L0X_XSHUT_PIN);
    Delay_Ms(10);
    GPIO_SetBits(VL53L0X_XSHUT_PORT, VL53L0X_XSHUT_PIN);
    Delay_Ms(5);   /* tBOOT = 1.2ms max */

    /* Validate I2C interface with datasheet Table 4 reference registers:
     * 0xC0 = 0xEE, 0xC1 = 0xAA, 0xC2 = 0x10 */
    if (VL53L0X_ReadReg(VL53L0X_REG_IDENTIFICATION_MODEL_ID, &model_id) ||
        VL53L0X_ReadReg(0xC1, &model_id2) ||
        VL53L0X_ReadReg(VL53L0X_REG_IDENTIFICATION_REVISION_ID, &rev_id))
    {
#if VL53L0X_DEBUG_EN
        printf("[LR] FAIL: I2C communication error (NACK)\r\n");
        VL53L0X_DumpDiagnostics();
#endif
        return;
    }

    if (model_id != VL53L0X_MODEL_ID_EXPECTED)
    {
#if VL53L0X_DEBUG_EN
        printf("[LR] FAIL: model ID mismatch (0x%02X/0x%02X/0x%02X)\r\n",
               model_id, model_id2, rev_id);
        VL53L0X_DumpDiagnostics();
#endif
        return;
    }

    /* ---- DataInit: I2C standard mode + stop_variable + signal rate limit ---- */
    VL53L0X_WriteReg(0x88, 0x00);

    VL53L0X_WriteReg(0x80, 0x01);
    VL53L0X_WriteReg(0xFF, 0x01);
    VL53L0X_WriteReg(0x00, 0x00);
    VL53L0X_ReadReg(0x91, &vl53l0x_ctx.stop_variable);
    VL53L0X_WriteReg(0x00, 0x01);
    VL53L0X_WriteReg(0xFF, 0x00);
    VL53L0X_WriteReg(0x80, 0x00);

    /* Disable SIGNAL_RATE_MSRC (bit1) and SIGNAL_RATE_PRE_RANGE (bit4) limit checks */
    VL53L0X_ReadReg(VL53L0X_REG_MSRC_CONFIG_CONTROL, &tmp);
    VL53L0X_WriteReg(VL53L0X_REG_MSRC_CONFIG_CONTROL, tmp | 0x12);

    /* Signal rate limit 0.25 MCPS (9.7 fixed point) = 0.25 * 128 = 32 = 0x0020 */
    VL53L0X_WriteReg16Bit(VL53L0X_REG_SIGNAL_RATE_LIMIT, 0x0020);

    /* ---- StaticInit: read SPAD info from NVM ---- */
    VL53L0X_WriteReg(0x80, 0x01);
    VL53L0X_WriteReg(0xFF, 0x01);
    VL53L0X_WriteReg(0x00, 0x00);
    VL53L0X_WriteReg(0xFF, 0x06);
    VL53L0X_ReadReg(0x83, &tmp);
    VL53L0X_WriteReg(0x83, tmp | 0x04);
    VL53L0X_WriteReg(0xFF, 0x07);
    VL53L0X_WriteReg(0x81, 0x01);
    VL53L0X_WriteReg(0x80, 0x01);
    VL53L0X_WriteReg(0x94, 0x6B);
    VL53L0X_WriteReg(0x83, 0x00);

    timeout = 1000;
    do {
        VL53L0X_ReadReg(0x83, &tmp);
        if (tmp != 0x00) break;
        Delay_Ms(1);
    } while (--timeout);

    if (timeout == 0)
        return;   /* SPAD info read timeout */

    VL53L0X_WriteReg(0x83, 0x01);
    VL53L0X_ReadReg(0x92, &tmp);   /* [6:0]=SPAD count, [7]=aperture */

    VL53L0X_WriteReg(0x81, 0x00);
    VL53L0X_WriteReg(0xFF, 0x06);
    VL53L0X_ReadReg(0x83, &tmp);
    VL53L0X_WriteReg(0x83, tmp & ~0x04);
    VL53L0X_WriteReg(0xFF, 0x01);
    VL53L0X_WriteReg(0x00, 0x01);
    VL53L0X_WriteReg(0xFF, 0x00);
    VL53L0X_WriteReg(0x80, 0x00);
    /* NOTE: reference SPAD map programming (regs 0xB0/0x4E/0x4F) is omitted;
     * sensor keeps its NVM default SPAD configuration. */

    /* ---- Interrupt config: GPIO1 active on new sample ready ---- */
    VL53L0X_WriteReg(VL53L0X_REG_SYSTEM_INTERRUPT_CONFIG, 0x04);

    /* ---- PerformRefCalibration (VHV + phase), datasheet 2.3.1 ---- */
    VL53L0X_WriteReg(VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG, 0x00);
    if (VL53L0X_PerformSingleRefCalibration(0x40))
        return;   /* VHV calibration failed */

    VL53L0X_WriteReg(VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG, 0x02);
    if (VL53L0X_PerformSingleRefCalibration(0x00))
        return;   /* Phase calibration failed */

    /* Restore full ranging sequence */
    VL53L0X_WriteReg(VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG, 0xFF);

    /* Clear any pending interrupt */
    VL53L0X_WriteReg(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    vl53l0x_ctx.initialized = 1;
    vl53l0x_ctx.state = VL53L0X_STATE_IDLE;
}

uint8_t VL53L0X_IsInitialized(void)
{
    return vl53l0x_ctx.initialized;
}

void VL53L0X_StartContinuous(void)
{
    if (!vl53l0x_ctx.initialized)
        return;

    /* Start sequence: restore stop_variable, then SYSRANGE_START = 0x02
     * (continuous back-to-back ranging, datasheet 2.4) */
    VL53L0X_WriteReg(0x80, 0x01);
    VL53L0X_WriteReg(0xFF, 0x01);
    VL53L0X_WriteReg(0x00, 0x00);
    VL53L0X_WriteReg(0x91, vl53l0x_ctx.stop_variable);
    VL53L0X_WriteReg(0x00, 0x01);
    VL53L0X_WriteReg(0xFF, 0x00);
    VL53L0X_WriteReg(0x80, 0x00);

    VL53L0X_WriteReg(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    VL53L0X_WriteReg(VL53L0X_REG_SYSRANGE_START, 0x02);

    vl53l0x_ctx.state = VL53L0X_STATE_RANGING;
    vl53l0x_ctx.data_ready = 0;
}

void VL53L0X_StopContinuous(void)
{
    /* Stop: SYSRANGE_START = 0x01 (stop request; last measurement completes) */
    VL53L0X_WriteReg(VL53L0X_REG_SYSRANGE_START, 0x01);

    /* Restore NVM stop variable page */
    VL53L0X_WriteReg(0xFF, 0x01);
    VL53L0X_WriteReg(0x00, 0x00);
    VL53L0X_WriteReg(0x91, 0x00);
    VL53L0X_WriteReg(0x00, 0x01);
    VL53L0X_WriteReg(0xFF, 0x00);

    vl53l0x_ctx.state = VL53L0X_STATE_IDLE;
    vl53l0x_ctx.data_ready = 0;
}

uint8_t VL53L0X_IsDataReady(void)
{
    uint8_t status;

    if (vl53l0x_ctx.state != VL53L0X_STATE_RANGING)
        return 0;

    /* Polling mode (datasheet 2.7): new sample ready when (status & 0x07) == 0x04
     * with interrupt config = 0x04 (new sample ready) */
    if (VL53L0X_ReadReg(VL53L0X_REG_RESULT_INTERRUPT_STATUS, &status))
        return 0;

    return ((status & 0x07) == 0x04) ? 1 : 0;
}

uint16_t VL53L0X_ReadDistance(void)
{
    uint16_t distance;
    uint8_t  range_status;

    if (VL53L0X_ReadReg(VL53L0X_REG_RESULT_RANGE_STATUS, &range_status))
    {
        vl53l0x_ctx.last_distance_mm = VL53L0X_DISTANCE_INVALID;
        VL53L0X_ClearInterrupt();
        vl53l0x_ctx.data_ready = 0;
        return VL53L0X_DISTANCE_INVALID;
    }

    if (VL53L0X_ReadReg16Bit(VL53L0X_REG_RESULT_RANGE_VALUE, &distance))
    {
        vl53l0x_ctx.last_distance_mm = VL53L0X_DISTANCE_INVALID;
        VL53L0X_ClearInterrupt();
        vl53l0x_ctx.data_ready = 0;
        return VL53L0X_DISTANCE_INVALID;
    }

    vl53l0x_ctx.last_range_status = range_status;

    /* RESULT_RANGE_STATUS: DeviceError = bits [6:3].
     * 11 (RANGECOMPLETE) = valid measurement (ST API FixRangeStatus);
     * 0 = no new measurement; other values = error codes
     * (1=sigma, 2=signal weak, 4=MSRC no target, 6=phase, 7=sigma threshold,
     *  8=TCC, 9=phase consistency, 10=min clip, 12/13=algo under/overflow,
     *  14=range ignore threshold). */
    if (((range_status >> 3) & 0x0F) != 11 || distance == 0)
    {
        vl53l0x_ctx.last_distance_mm = VL53L0X_DISTANCE_INVALID;
    }
    else
    {
        vl53l0x_ctx.last_distance_mm = distance;
    }

    /* Clear interrupt after reading */
    VL53L0X_ClearInterrupt();

    vl53l0x_ctx.data_ready = 0;

    return vl53l0x_ctx.last_distance_mm;
}

void VL53L0X_ClearInterrupt(void)
{
    VL53L0X_WriteReg(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
}
