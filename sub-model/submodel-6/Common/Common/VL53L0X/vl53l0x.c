#include "vl53l0x.h"
#include <string.h>
#include "debug.h"

vl53l0x_ctx_t vl53l0x_ctx;

/* I2C timing delay — must be slow enough for VL53L0X (max 400 kHz I2C).
 * At 72 MHz each loop iteration is ~6 cycles, so 10 * 6 / 72 = ~0.83 us
 * per half-cycle → ~600 kHz SCL, safe margin. */
#define I2C_DELAY_US    10

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
    for (i = 0; i < I2C_DELAY_US * 8; i++)
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

    return ack ? 1 : 0;
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

/* Write a 16-bit register */
static uint8_t VL53L0X_WriteReg16(uint16_t reg, uint8_t value)
{
    I2C_Start();
    I2C_SendByte(VL53L0X_I2C_ADDR_8BIT);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_SendByte((uint8_t)(reg >> 8));
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_SendByte((uint8_t)(reg & 0xFF));
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_SendByte(value);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_Stop();
    return 0;
}

/* Write a 32-bit register (used for some VL53L0X regs) */
static uint8_t VL53L0X_WriteReg32(uint16_t reg, uint32_t value)
{
    I2C_Start();
    I2C_SendByte(VL53L0X_I2C_ADDR_8BIT);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_SendByte((uint8_t)(reg >> 8));
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_SendByte((uint8_t)(reg & 0xFF));
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_SendByte((uint8_t)(value >> 24));
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_SendByte((uint8_t)(value >> 16));
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_SendByte((uint8_t)(value >> 8));
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_SendByte((uint8_t)(value & 0xFF));
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_Stop();
    return 0;
}

/* Read a 16-bit register */
static uint8_t VL53L0X_ReadReg16(uint16_t reg, uint8_t *value)
{
    I2C_Start();
    I2C_SendByte(VL53L0X_I2C_ADDR_8BIT);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_SendByte((uint8_t)(reg >> 8));
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_SendByte((uint8_t)(reg & 0xFF));
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_Start();
    I2C_SendByte(VL53L0X_I2C_ADDR_8BIT | 0x01);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    *value = I2C_ReadByte(0);
    I2C_Stop();
    return 0;
}

/* Read a 16-bit register, return 16-bit value */
static uint8_t VL53L0X_ReadReg16_Word(uint16_t reg, uint16_t *value)
{
    I2C_Start();
    I2C_SendByte(VL53L0X_I2C_ADDR_8BIT);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_SendByte((uint8_t)(reg >> 8));
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_SendByte((uint8_t)(reg & 0xFF));
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    I2C_Start();
    I2C_SendByte(VL53L0X_I2C_ADDR_8BIT | 0x01);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }

    *value = ((uint16_t)I2C_ReadByte(1)) << 8;
    *value |= I2C_ReadByte(0);
    I2C_Stop();
    return 0;
}

/* GPIO init for I2C (PB14/PB15) and control pins (PB12/PB13) */
static void VL53L0X_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* CRITICAL: PB14(JTDI) and PB15(JTDI/TRACESWO) are JTAG pins by default.
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

    /* XSHUT - push-pull output, default high */
    gpio.GPIO_Pin   = VL53L0X_XSHUT_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(VL53L0X_XSHUT_PORT, &gpio);
    GPIO_SetBits(VL53L0X_XSHUT_PORT, VL53L0X_XSHUT_PIN);

    /* GPIO1 - input with pull-up (data ready interrupt) */
    gpio.GPIO_Pin   = VL53L0X_GPIO1_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_Init(VL53L0X_GPIO1_PORT, &gpio);

    /* Set I2C bus idle */
    SCL_H();
    SDA_H();
}

/* DEBUG: I2C bus recovery — 9 clock pulses with SDA high, then STOP.
 * This can unstick a sensor that is stuck mid-transaction (SDA held low). */
static void VL53L0X_I2C_BusRecovery(void)
{
    uint8_t i;

    printf("[IR] I2C bus recovery...\r\n");

    /* Check if SDA is stuck low */
    SDA_H(); SCL_H();
    Delay_Ms(1);
    printf("[IR] Before recovery: SCL=%d SDA=%d\r\n", SCL_READ(), SDA_READ());

    if (SDA_READ() == 0)
    {
        /* SDA stuck low — clock SCL 9 times to release */
        for (i = 0; i < 9; i++)
        {
            SCL_L(); Delay_Ms(1);
            SCL_H(); Delay_Ms(1);
            printf("[IR]   clock %d: SDA=%d\r\n", i, SDA_READ());
            if (SDA_READ()) break;  /* SDA released */
        }
        /* Generate STOP */
        SDA_L(); Delay_Ms(1);
        SCL_H(); Delay_Ms(1);
        SDA_H(); Delay_Ms(1);
    }

    printf("[IR] After recovery: SCL=%d SDA=%d\r\n", SCL_READ(), SDA_READ());
}

/* DEBUG: Scan I2C bus with SCL/SDA SWAPPED to detect wiring swap */
static void VL53L0X_I2C_ScanSwapped(void)
{
    uint8_t addr, found = 0;

    printf("[IR] I2C scan with SCL<->SDA SWAPPED...\r\n");

    for (addr = 0x03; addr <= 0x77; addr++)
    {
        /* START: SCL(PB14) high, drive SDA(PB15) low while SCL high */
        GPIO_SetBits(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN);   /* SDA -> PB15 high */
        GPIO_SetBits(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN);   /* SCL -> PB14 high */
        I2C_Delay();
        GPIO_ResetBits(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN); /* SDA -> PB15 low (START) */
        I2C_Delay();
        GPIO_ResetBits(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN); /* SCL -> PB14 low */
        I2C_Delay();

        /* Send address byte, MSB first — but on swapped pins */
        uint8_t byte = addr << 1;
        uint8_t i;
        for (i = 0; i < 8; i++)
        {
            if (byte & 0x80)
                GPIO_SetBits(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN);   /* SDA -> PB15 */
            else
                GPIO_ResetBits(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN);
            byte <<= 1;
            I2C_Delay();
            GPIO_SetBits(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN);       /* SCL -> PB14 high */
            I2C_Delay();
            GPIO_ResetBits(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN);     /* SCL -> PB14 low */
            I2C_Delay();
        }

        /* ACK: release SDA(PB15), clock SCL(PB14), read SDA(PB15) */
        GPIO_SetBits(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN);
        I2C_Delay();
        GPIO_SetBits(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN);
        I2C_Delay();
        if (GPIO_ReadInputDataBit(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN) == 0)
        {
            printf("[IR]   Device at 0x%02X (7-bit) *** SCL/SDA SWAPPED! ***\r\n", addr);
            found++;
        }
        GPIO_ResetBits(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN);
        I2C_Delay();

        /* STOP */
        GPIO_ResetBits(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN);
        I2C_Delay();
        GPIO_SetBits(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN);
        I2C_Delay();
        GPIO_SetBits(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN);
        I2C_Delay();
    }

    if (found == 0)
        printf("[IR]   No device found with swapped pins either\r\n");
    else
        printf("[IR]   Total: %d device(s) with SWAPPED pins\r\n", found);
}

/* DEBUG: Scan I2C bus for any responding device (address 0x03..0x77) */
static void VL53L0X_I2C_Scan(void)
{
    uint8_t addr, found = 0;

    printf("[IR] I2C bus scan...\r\n");
    printf("[IR] SCL=%d SDA=%d (idle state)\r\n",
           GPIO_ReadInputDataBit(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN),
           GPIO_ReadInputDataBit(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN));

    for (addr = 0x03; addr <= 0x77; addr++)
    {
        I2C_Start();
        I2C_SendByte(addr << 1);   /* write mode */
        if (I2C_WaitAck() == 0)    /* ACK received */
        {
            printf("[IR]   Device at 0x%02X (7-bit)\r\n", addr);
            found++;
        }
        I2C_Stop();
    }

    if (found == 0)
        printf("[IR]   No device found on I2C bus!\r\n");
    else
        printf("[IR]   Total: %d device(s)\r\n", found);
}

/* DEBUG: Raw I2C transaction to 0x29 with per-bit SDA trace */
static void VL53L0X_I2C_RawDebug(void)
{
    uint8_t byte = VL53L0X_I2C_ADDR_8BIT;  /* 0x52 */
    uint8_t i, sda_val;

    printf("[IR] Raw I2C debug: addr 0x29 (write byte=0x%02X)\r\n", byte);

    /* START condition */
    printf("[IR] START: ");
    SDA_H(); SCL_H();
    Delay_Ms(1);
    printf("SCL=%d SDA=%d -> ", SCL_READ(), SDA_READ());
    SDA_L();
    Delay_Ms(1);
    printf("SDA_L -> SCL=%d SDA=%d -> ", SCL_READ(), SDA_READ());
    SCL_L();
    Delay_Ms(1);
    printf("SCL_L -> SCL=%d SDA=%d\r\n", SCL_READ(), SDA_READ());

    /* Send address byte, bit by bit with trace */
    for (i = 0; i < 8; i++)
    {
        if (byte & 0x80) { SDA_H(); printf("[IR] bit%d=1 ", i); }
        else             { SDA_L(); printf("[IR] bit%d=0 ", i); }
        byte <<= 1;
        Delay_Ms(1);
        SCL_H();
        Delay_Ms(1);
        sda_val = SDA_READ();
        printf("(SCL=1 SDA=%d) ", sda_val);
        SCL_L();
        Delay_Ms(1);
        printf("(SCL=0 SDA=%d)\r\n", SDA_READ());
    }

    /* ACK phase: release SDA, clock SCL, read SDA */
    printf("[IR] ACK phase: ");
    SDA_H();
    Delay_Ms(1);
    printf("SDA released -> SDA=%d, ", SDA_READ());
    SCL_H();
    Delay_Ms(1);
    sda_val = SDA_READ();
    printf("SCL_H -> SDA=%d -> %s\r\n", sda_val, sda_val ? "NACK" : "ACK!");
    SCL_L();
    Delay_Ms(1);

    /* STOP condition */
    SDA_L();
    Delay_Ms(1);
    SCL_H();
    Delay_Ms(1);
    SDA_H();
    Delay_Ms(1);
    printf("[IR] STOP done, SCL=%d SDA=%d\r\n", SCL_READ(), SDA_READ());
}

/* DEBUG: Verify GPIO output by toggling and reading back */
static void VL53L0X_GPIO_Diag(void)
{
    uint8_t scl_r, sda_r, xshut_r;

    printf("[IR] GPIO diagnostic...\r\n");

    /* Test SCL (PB15) */
    SCL_L(); scl_r = GPIO_ReadInputDataBit(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN);
    printf("[IR] SCL L->read=%d ", scl_r);
    SCL_H(); scl_r = GPIO_ReadInputDataBit(VL53L0X_SCL_PORT, VL53L0X_SCL_PIN);
    printf("H->read=%d ", scl_r);

    /* Test SDA (PB14) */
    SDA_L(); sda_r = GPIO_ReadInputDataBit(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN);
    printf("SDA L->read=%d ", sda_r);
    SDA_H(); sda_r = GPIO_ReadInputDataBit(VL53L0X_SDA_PORT, VL53L0X_SDA_PIN);
    printf("H->read=%d ", sda_r);

    /* Test XSHUT (PB12) */
    GPIO_ResetBits(VL53L0X_XSHUT_PORT, VL53L0X_XSHUT_PIN);
    xshut_r = GPIO_ReadInputDataBit(VL53L0X_XSHUT_PORT, VL53L0X_XSHUT_PIN);
    printf("XSHUT L->read=%d ", xshut_r);
    GPIO_SetBits(VL53L0X_XSHUT_PORT, VL53L0X_XSHUT_PIN);
    xshut_r = GPIO_ReadInputDataBit(VL53L0X_XSHUT_PORT, VL53L0X_XSHUT_PIN);
    printf("H->read=%d\r\n", xshut_r);

    /* Check GPIO1 (PB13) input */
    printf("[IR] GPIO1(PB13)=%d\r\n",
           GPIO_ReadInputDataBit(VL53L0X_GPIO1_PORT, VL53L0X_GPIO1_PIN));

    /* Summary: open-drain pins should read back what we set.
     * If L->read=1, the pin is not actually driving low (wiring issue or pin conflict). */
    printf("[IR] Expected: SCL L=0 H=1, SDA L=0 H=1, XSHUT L=0 H=1\r\n");
}

/* VL53L0X initialization sequence — default ranging mode
 * Based on ST VL53L0X API VL53L0X_DataInit() + VL53L0X_StaticInit() +
 * VL53L0X_PerformRefCalibration() + VL53L0X_SetDeviceMode() defaults.
 * Only the mandatory register writes are kept; optional tuning params
 * use sensor's internal NVM defaults. */
void VL53L0X_Init(void)
{
    uint16_t model_id;
    uint8_t i2c_err;

    memset(&vl53l0x_ctx, 0, sizeof(vl53l0x_ctx_t));

    printf("[IR] GPIO init...\r\n");
    VL53L0X_GPIO_Init();

    /* DEBUG: Verify GPIO pins are actually driving */
    VL53L0X_GPIO_Diag();

    /* Hardware reset via XSHUT — monitor GPIO1 to see if sensor is alive */
    printf("[IR] XSHUT reset with GPIO1 monitor...\r\n");
    printf("[IR] GPIO1 before reset: %d\r\n",
           GPIO_ReadInputDataBit(VL53L0X_GPIO1_PORT, VL53L0X_GPIO1_PIN));
    GPIO_ResetBits(VL53L0X_XSHUT_PORT, VL53L0X_XSHUT_PIN);
    Delay_Ms(10);
    printf("[IR] GPIO1 during reset (XSHUT low): %d\r\n",
           GPIO_ReadInputDataBit(VL53L0X_GPIO1_PORT, VL53L0X_GPIO1_PIN));
    GPIO_SetBits(VL53L0X_XSHUT_PORT, VL53L0X_XSHUT_PIN);
    Delay_Ms(5);
    printf("[IR] GPIO1 5ms after release: %d\r\n",
           GPIO_ReadInputDataBit(VL53L0X_GPIO1_PORT, VL53L0X_GPIO1_PIN));
    Delay_Ms(45);
    printf("[IR] GPIO1 50ms after release: %d\r\n",
           GPIO_ReadInputDataBit(VL53L0X_GPIO1_PORT, VL53L0X_GPIO1_PIN));

    /* DEBUG: Bus recovery in case sensor is stuck */
    VL53L0X_I2C_BusRecovery();

    /* DEBUG: Scan I2C bus to see if sensor is present */
    VL53L0X_I2C_Scan();

    /* DEBUG: Scan with SCL/SDA swapped to detect wiring swap */
    VL53L0X_I2C_ScanSwapped();

    /* DEBUG: Ultra-slow I2C scan (10ms per half-cycle) to rule out timing */
    {
        uint8_t addr, found = 0;
        printf("[IR] Ultra-slow I2C scan (10ms/bit)...\r\n");
        for (addr = 0x20; addr <= 0x30; addr++)
        {
            uint8_t byte = addr << 1;
            uint8_t i, ack_val;

            /* START */
            SDA_H(); SCL_H(); Delay_Ms(10);
            SDA_L(); Delay_Ms(10);
            SCL_L(); Delay_Ms(10);

            /* Send address byte */
            for (i = 0; i < 8; i++)
            {
                if (byte & 0x80) SDA_H(); else SDA_L();
                byte <<= 1;
                Delay_Ms(10);
                SCL_H(); Delay_Ms(10);
                SCL_L(); Delay_Ms(10);
            }

            /* ACK */
            SDA_H(); Delay_Ms(10);
            SCL_H(); Delay_Ms(10);
            ack_val = SDA_READ();
            SCL_L(); Delay_Ms(10);

            if (ack_val == 0)
            {
                printf("[IR]   Device at 0x%02X (ultra-slow)\r\n", addr);
                found++;
            }

            /* STOP */
            SDA_L(); Delay_Ms(10);
            SCL_H(); Delay_Ms(10);
            SDA_H(); Delay_Ms(10);
        }
        if (found == 0)
            printf("[IR]   No device found (ultra-slow)\r\n");
    }

    /* DEBUG: Brute-force scan — try every PB pin pair as SCL/SDA
     * This finds the sensor even if PCB traces go to unexpected pins */
    {
        /* GPIOB pin mapping: bit position to GPIO_Pin_x */
        static const uint16_t pb_pins[] = {
            GPIO_Pin_0,  GPIO_Pin_1,  GPIO_Pin_2,  GPIO_Pin_3,
            GPIO_Pin_4,  GPIO_Pin_5,  GPIO_Pin_6,  GPIO_Pin_7,
            GPIO_Pin_8,  GPIO_Pin_9,  GPIO_Pin_10, GPIO_Pin_11,
            GPIO_Pin_12, GPIO_Pin_13, GPIO_Pin_14, GPIO_Pin_15
        };
        uint8_t scl_idx, sda_idx;

        printf("[IR] Brute-force I2C scan on ALL GPIOB pin pairs...\r\n");

        for (scl_idx = 0; scl_idx < 16; scl_idx++)
        {
            for (sda_idx = 0; sda_idx < 16; sda_idx++)
            {
                if (scl_idx == sda_idx) continue;

                GPIO_InitTypeDef gpio;
                uint8_t byte = 0x52; /* VL53L0X write addr */
                uint8_t i, ack_val;

                /* Configure both pins as open-drain output */
                gpio.GPIO_Pin   = pb_pins[scl_idx];
                gpio.GPIO_Speed = GPIO_Speed_50MHz;
                gpio.GPIO_Mode  = GPIO_Mode_Out_OD;
                GPIO_Init(GPIOB, &gpio);

                gpio.GPIO_Pin   = pb_pins[sda_idx];
                GPIO_Init(GPIOB, &gpio);

                /* Both high (idle) */
                GPIO_SetBits(GPIOB, pb_pins[scl_idx] | pb_pins[sda_idx]);
                Delay_Ms(1);

                /* START: SDA low while SCL high */
                GPIO_ResetBits(GPIOB, pb_pins[sda_idx]);
                Delay_Ms(1);
                GPIO_ResetBits(GPIOB, pb_pins[scl_idx]);
                Delay_Ms(1);

                /* Send 0x52 (VL53L0X write address) */
                for (i = 0; i < 8; i++)
                {
                    if (byte & 0x80)
                        GPIO_SetBits(GPIOB, pb_pins[sda_idx]);
                    else
                        GPIO_ResetBits(GPIOB, pb_pins[sda_idx]);
                    byte <<= 1;
                    Delay_Ms(1);
                    GPIO_SetBits(GPIOB, pb_pins[scl_idx]);
                    Delay_Ms(1);
                    GPIO_ResetBits(GPIOB, pb_pins[scl_idx]);
                    Delay_Ms(1);
                }

                /* ACK: release SDA, clock SCL, read SDA */
                GPIO_SetBits(GPIOB, pb_pins[sda_idx]);
                Delay_Ms(1);
                GPIO_SetBits(GPIOB, pb_pins[scl_idx]);
                Delay_Ms(1);
                ack_val = GPIO_ReadInputDataBit(GPIOB, pb_pins[sda_idx]);
                GPIO_ResetBits(GPIOB, pb_pins[scl_idx]);
                Delay_Ms(1);

                if (ack_val == 0)
                {
                    printf("[IR]   *** FOUND at 0x29: SCL=PB%d SDA=PB%d ***\r\n",
                           scl_idx, sda_idx);
                }

                /* STOP */
                GPIO_ResetBits(GPIOB, pb_pins[sda_idx]);
                Delay_Ms(1);
                GPIO_SetBits(GPIOB, pb_pins[scl_idx]);
                Delay_Ms(1);
                GPIO_SetBits(GPIOB, pb_pins[sda_idx]);
                Delay_Ms(1);
            }
        }
        printf("[IR] Brute-force scan done\r\n");
    }

    /* DEBUG: Raw I2C transaction trace for address 0x29 */
    VL53L0X_I2C_RawDebug();

    /* Verify model ID — if I2C or sensor is not responding, bail out */
    printf("[IR] Read model ID...\r\n");
    i2c_err = VL53L0X_ReadReg16_Word(VL53L0X_REG_IDENTIFICATION_MODEL_ID, &model_id);
    printf("[IR] I2C err=%d, model_id=0x%04X (expect 0xEEAA)\r\n", i2c_err, model_id);

    if (i2c_err)
    {
        printf("[IR] FAIL: I2C communication error\r\n");
        return;
    }

    if (model_id != VL53L0X_MODEL_ID_EXPECTED)
    {
        printf("[IR] FAIL: model ID mismatch\r\n");
        return;
    }

    /* ---- DataInit sequence (VHV config select) ---- */
    printf("[IR] DataInit...\r\n");
    VL53L0X_WriteReg16(0x0080, 0x01);
    VL53L0X_WriteReg16(0x00FF, 0x01);
    VL53L0X_WriteReg16(0x0080, 0x00);

    {
        uint8_t vhv;
        VL53L0X_ReadReg16(0x0081, &vhv);
        printf("[IR] VHV read=0x%02X\r\n", vhv);
        VL53L0X_WriteReg16(0x0081, vhv | 0x01);
    }

    VL53L0X_WriteReg16(0x0080, 0x01);
    VL53L0X_WriteReg16(0x00FF, 0x00);
    VL53L0X_WriteReg16(0x0080, 0x00);

    VL53L0X_WriteReg16(0x0060, 0x00);

    /* ---- StaticInit: VHV ref calibration pattern ---- */
    printf("[IR] StaticInit...\r\n");
    VL53L0X_WriteReg16(0x0080, 0x01);
    VL53L0X_WriteReg16(0x00FF, 0x01);
    VL53L0X_WriteReg16(0x0080, 0x00);

    {
        uint8_t vhv;
        VL53L0X_ReadReg16(0x0081, &vhv);
        VL53L0X_WriteReg16(0x0081, vhv & 0xFE);
        VL53L0X_ReadReg16(0x0081, &vhv);
        VL53L0X_WriteReg16(0x0081, vhv | 0x01);
    }

    VL53L0X_WriteReg16(0x0080, 0x01);
    VL53L0X_WriteReg16(0x00FF, 0x00);
    VL53L0X_WriteReg16(0x0080, 0x00);

    /* ---- PerformRefCalibration (VHV + phase) ---- */
    printf("[IR] RefCalibration VHV...\r\n");
    /* VHV calibration: set SYSRANGE_START=0x01 (single shot), wait for ready */
    VL53L0X_WriteReg16(VL53L0X_REG_SYSRANGE_START, 0x01);
    {
        uint8_t val;
        uint16_t timeout = 1000;
        do {
            VL53L0X_ReadReg16(VL53L0X_REG_RESULT_RANGE_STATUS, &val);
            if (val & 0x01) break;
            Delay_Ms(1);
        } while (--timeout);
        printf("[IR] VHV cal done, timeout=%d, status=0x%02X\r\n", timeout, val);
    }
    VL53L0X_WriteReg16(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    /* Phase calibration */
    printf("[IR] RefCalibration Phase...\r\n");
    VL53L0X_WriteReg16(0x0060, 0x00);
    VL53L0X_WriteReg16(0x0060, 0x01);
    {
        uint8_t val;
        uint16_t timeout = 1000;
        do {
            VL53L0X_ReadReg16(0x0060, &val);
            if (val & 0x01) break;
            Delay_Ms(1);
        } while (--timeout);
        printf("[IR] Phase cal done, timeout=%d, val=0x%02X\r\n", timeout, val);
    }
    VL53L0X_WriteReg16(0x0060, 0x00);

    /* ---- Set interrupt on new sample ready ---- */
    VL53L0X_WriteReg16(VL53L0X_REG_SYSTEM_INTERRUPT_CONFIG, 0x04);

    /* Clear any pending interrupt */
    VL53L0X_WriteReg16(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    vl53l0x_ctx.initialized = 1;
    vl53l0x_ctx.state = VL53L0X_STATE_IDLE;

    printf("[IR] Init SUCCESS\r\n");
}

uint8_t VL53L0X_IsInitialized(void)
{
    return vl53l0x_ctx.initialized;
}

void VL53L0X_StartContinuous(void)
{
    if (!vl53l0x_ctx.initialized)
    {
        printf("[IR] StartContinuous FAIL: not initialized\r\n");
        return;
    }

    /* Clear any pending interrupt */
    VL53L0X_WriteReg16(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    /* Start continuous ranging mode: bit0=0, bit1=1 → 0x02 */
    VL53L0X_WriteReg16(VL53L0X_REG_SYSRANGE_START, 0x02);

    vl53l0x_ctx.state = VL53L0X_STATE_RANGING;
    vl53l0x_ctx.data_ready = 0;

    printf("[IR] Continuous ranging STARTED\r\n");
}

void VL53L0X_StopContinuous(void)
{
    /* Stop ranging: write 0x00 to SYSRANGE_START */
    VL53L0X_WriteReg16(VL53L0X_REG_SYSRANGE_START, 0x00);

    /* Clear any pending interrupt */
    VL53L0X_WriteReg16(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    vl53l0x_ctx.state = VL53L0X_STATE_IDLE;
    vl53l0x_ctx.data_ready = 0;

    printf("[IR] Continuous ranging STOPPED\r\n");
}

uint8_t VL53L0X_IsDataReady(void)
{
    uint8_t status;

    if (vl53l0x_ctx.state != VL53L0X_STATE_RANGING)
        return 0;

    /* Read interrupt status */
    if (VL53L0X_ReadReg16(VL53L0X_REG_RESULT_INTERRUPT_STATUS, &status))
        return 0;

    /* New sample ready when (status & 0x07) == 0x04 */
    return ((status & 0x07) == 0x04) ? 1 : 0;
}

uint16_t VL53L0X_ReadDistance(void)
{
    uint16_t distance;
    uint8_t  range_status;

    /* Read range status byte (register 0x0014) */
    VL53L0X_ReadReg16(0x0014, &range_status);

    /* Read distance: register 0x0014 + offset 10 = 0x001E */
    if (VL53L0X_ReadReg16_Word(0x001E, &distance))
    {
        vl53l0x_ctx.last_distance_mm = 0xFFFF;
        VL53L0X_ClearInterrupt();
        vl53l0x_ctx.data_ready = 0;
        return 0xFFFF;
    }

    /* range_status bits [4:0]: 0..4 = valid measurement, >=5 = error
     * Per VL53L0X datasheet: RangeStatus = (range_status >> 3) & 0x0F
     * Values 0-3 are usable, 4-7 are various errors.
     * Simplified: if bit4 is set (value >= 16), or lower 5 bits >= 5, mark invalid. */
    if ((range_status & 0x1F) >= 0x05)
    {
        vl53l0x_ctx.last_distance_mm = 0xFFFF;
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
    VL53L0X_WriteReg16(VL53L0X_REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
}
