/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : Keyboard-1 main program.
 *                      CH32V103C8T6, 72MHz HSE, UART1 debug @ 921600.
 *                      5x 74HC165 daisy-chain for 39-key matrix reading.
 *                      TIM2 drives periodic scan, main loop handles events.
 *                      Key events and HID reports are buffered and flushed
 *                      outside the scan cycle to avoid blocking.
 *********************************************************************************/

#include "debug.h"
#include "./74HC165/74HC165.h"
#include "./74HC165/key.h"
#include "./CH9329/ch9329.h"
#include <string.h>

/* Global typedef */

/* Global define */

/* Global Variable */

/* Scan flag: set by TIM2 ISR, cleared by main loop */
volatile uint8_t g_scan_flag = 0;

/* Raw 74HC165 read buffer */
static uint8_t hc165_buf[HC165_CHIP_COUNT];

/* Previous HID report for change detection */
static uint8_t last_hid_report[CH9329_KB_DATA_LEN] = {0};

/* ====================================================================
 * Event buffer is defined in key.c / key.h (key_evt_buf, key_evt_count)
 * HID report snapshot for deferred printf
 * ==================================================================== */
static uint8_t   hid_pending = 0;
static uint8_t   hid_report_buf[CH9329_KB_DATA_LEN];

/*********************************************************************
 * @fn      BuildHIDReport
 *
 * @brief   Build 8-byte HID keyboard report from current key states.
 *          [0] = modifier bitmask (Ctrl/Shift/Alt/GUI)
 *          [1] = reserved (0x00)
 *          [2..7] = up to 6 regular HID key codes
 *
 * @param   report  Output buffer (must be >= 8 bytes).
 *
 * @return  Number of regular keys in report (0-6).
 *********************************************************************/
static uint8_t BuildHIDReport(uint8_t *report)
{
    uint8_t i, slot;
    uint8_t mod = 0;

    report[0] = 0x00; /* Modifier byte */
    report[1] = 0x00; /* Reserved */
    for (i = 2; i < CH9329_KB_DATA_LEN; i++) report[i] = 0x00;

    slot = 2;
    for (i = 0; i < KEY_COUNT; i++)
    {
        if (Key_GetState(i) != KEY_STATE_PRESSED)
            continue;

        /* Modifier key: accumulate bits */
        if (key_mod_bits[i] != 0)
        {
            mod |= key_mod_bits[i];
        }
        /* Regular key: fill next available slot */
        else if (key_hid_codes[i] != 0x00 && slot < CH9329_KB_DATA_LEN)
        {
            report[slot++] = key_hid_codes[i];
        }
    }

    report[0] = mod;
    return (slot - 2);
}

/*********************************************************************
 * @fn      SendHIDIfNeeded
 *
 * @brief   Build HID report and send to CH9329 if state changed.
 *          Rate-limited: minimum 8ms between sends to match USB
 *          HID polling rate (125Hz) and prevent CH9329 buffer overrun.
 *          Stores report snapshot for deferred printf.
 *
 * @return  none
 *********************************************************************/
static void SendHIDIfNeeded(void)
{
    static uint8_t report[CH9329_KB_DATA_LEN];
    static uint8_t ticks_since_send = 0xFF; /* Start large so first send is immediate */

    /* Rate limit: 4 scan cycles x 2ms = 8ms minimum between sends */
    if (ticks_since_send < 4)
    {
        ticks_since_send++;
        return;
    }

    BuildHIDReport(report);

    /* Only send if report changed since last time */
    if (memcmp(report, last_hid_report, CH9329_KB_DATA_LEN) != 0)
    {
        CH9329_SendKeyboard(report);
        memcpy(last_hid_report, report, CH9329_KB_DATA_LEN);
        memcpy(hid_report_buf, report, CH9329_KB_DATA_LEN);
        ticks_since_send = 0;
        hid_pending = 1;
    }
}

/*********************************************************************
 * @fn      FlushEvents
 *
 * @brief   Print buffered key events and HID report outside the
 *          scan cycle.  Called when g_scan_flag is 0 (idle time).
 *
 * @return  none
 *********************************************************************/
static void FlushEvents(void)
{
    uint8_t i, k;

    for (i = 0; i < key_evt_count; i++)
    {
        printf("[KEY] %-7s: #%02d [%s]\r\n",
               key_evt_buf[i].pressed ? "Press" : "Release",
               key_evt_buf[i].key_idx + 1,
               key_names[key_evt_buf[i].key_idx]);
    }

    if (hid_pending)
    {
        printf("[HID] mod=0x%02X keys=", hid_report_buf[0]);
        for (k = 2; k < CH9329_KB_DATA_LEN; k++)
        {
            if (hid_report_buf[k]) printf("0x%02X ", hid_report_buf[k]);
        }
        printf("\r\n");
        hid_pending = 0;
    }

    key_evt_count = 0;
}

/*********************************************************************
 * @fn      Timer2_Init
 *
 * @brief   Initialize TIM2 for periodic key matrix scanning.
 *          Prescaler=71, Period=1999 -> 2ms interrupt at 72MHz.
 *
 * @return  none
 *********************************************************************/
static void Timer2_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    /* Enable TIM2 clock (APB1 peripheral) */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* Time base configuration: 72MHz / (71+1) = 1MHz, period = 2000 -> 2ms */
    TIM_TimeBaseStructure.TIM_Period = 2000 - 1;        /* Auto-reload value */
    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;       /* Prescaler */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    /* Enable update interrupt */
    TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    /* NVIC configuration */
    NVIC_SetPriority(TIM2_IRQn, 0);                     /* Highest priority */
    NVIC_EnableIRQ(TIM2_IRQn);

    /* Start timer */
    TIM_Cmd(TIM2, ENABLE);
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program entry.
 *          1. Init system clock, debug UART, 74HC165, key module, TIM2
 *          2. Main loop: on scan flag, read matrix and process events
 *
 * @return  none
 *********************************************************************/
int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);

    printf("\r\n===== Keyboard-1 Starting =====\r\n");
    printf("SystemClk: %d\r\n", SystemCoreClock);
    printf("ChipID: %08x\r\n", DBGMCU_GetCHIPID());
    printf("Keys: %d, Scan: %dms, Debounce: %dms\r\n",
           KEY_COUNT, KEY_SCAN_PERIOD_MS,
           KEY_SCAN_PERIOD_MS * KEY_DEBOUNCE_CNT);

    /* Initialize hardware */
    HC165_Init();
    Key_Init();
    CH9329_Init();
    Timer2_Init();

    printf("Init complete. CH9329 on USART2 @ 115200, Debug @ 921600\r\n");
    printf("Waiting for key events...\r\n");

    while (1)
    {
        if (g_scan_flag)
        {
            g_scan_flag = 0;

            /* Read 74HC165 shift register chain (40 bits, 5 bytes) */
            HC165_Read(hc165_buf);

            /* Update debounce counters with raw data */
            Key_Scan(hc165_buf);

            /* Detect state changes -> buffer events (no printf here) */
            Key_Process();

            /* Build HID report and send to CH9329 if changed */
            SendHIDIfNeeded();
        }
        else
        {
            /* Idle time: flush buffered events via printf */
            FlushEvents();
        }
    }
}
