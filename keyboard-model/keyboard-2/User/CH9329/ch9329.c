/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch9329.c
 * Description        : CH9329 UART-to-USB HID chip driver implementation.
 *                      Uses USART2 to send HID keyboard and mouse reports
 *                      via CH9329 protocol transmission mode (mode 0).
 *
 * Frame format:  [0x57][0xAB][ADDR][CMD][LEN][DATA...][SUM]
 * Checksum:      SUM = HEAD + ADDR + CMD + LEN + DATA (all bytes, 8-bit wrap)
 *********************************************************************************/
#include "ch9329.h"

/* ====================================================================
 * USART2 low-level
 * ==================================================================== */

static void CH9329_UART2_Init(void)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;

    /* Enable clocks */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* PA2 = USART2_TX: AF push-pull output */
    gpio.GPIO_Pin   = CH9329_TX_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(CH9329_TX_PORT, &gpio);

    /* PA3 = USART2_RX: not used, configure as input pull-up to avoid floating */
    gpio.GPIO_Pin  = CH9329_RX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(CH9329_RX_PORT, &gpio);

    /* USART2 configuration: 115200 8N1 */
    usart.USART_BaudRate            = 115200;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Tx;
    USART_Init(CH9329_UART, &usart);

    USART_Cmd(CH9329_UART, ENABLE);
}

/*********************************************************************
 * @fn      CH9329_SendByte
 *
 * @brief   Send a single byte via USART2 (polling mode).
 *
 * @param   byte  Byte to transmit.
 *
 * @return  none
 *********************************************************************/
static void CH9329_SendByte(uint8_t byte)
{
    while (USART_GetFlagStatus(CH9329_UART, USART_FLAG_TXE) == RESET)
        ;
    USART_SendData(CH9329_UART, byte);
}

/* ====================================================================
 * CH9329 protocol layer
 * ==================================================================== */

static uint8_t CH9329_CalcSum(uint8_t cmd, uint8_t len, const uint8_t *data)
{
    uint8_t sum;
    uint8_t i;

    sum  = CH9329_HEAD_0;
    sum += CH9329_HEAD_1;
    sum += CH9329_ADDR_DEFAULT;
    sum += cmd;
    sum += len;

    for (i = 0; i < len; i++)
    {
        sum += data[i];
    }

    return sum;
}

static void CH9329_SendFrame(uint8_t cmd, uint8_t len, const uint8_t *data)
{
    uint8_t sum;
    uint8_t i;

    sum = CH9329_CalcSum(cmd, len, data);

    /* Header */
    CH9329_SendByte(CH9329_HEAD_0);
    CH9329_SendByte(CH9329_HEAD_1);
    /* Address */
    CH9329_SendByte(CH9329_ADDR_DEFAULT);
    /* Command */
    CH9329_SendByte(cmd);
    /* Length */
    CH9329_SendByte(len);
    /* Data */
    for (i = 0; i < len; i++)
    {
        CH9329_SendByte(data[i]);
    }
    /* Checksum */
    CH9329_SendByte(sum);
}

/* ====================================================================
 * Public API
 * ==================================================================== */

void CH9329_Init(void)
{
    CH9329_UART2_Init();
}

uint8_t CH9329_SendKeyboard(const uint8_t *hid_data)
{
    if (hid_data == NULL)
        return 0;

    CH9329_SendFrame(CH9329_CMD_KB_GENERAL, CH9329_KB_DATA_LEN, hid_data);

    return 1;
}

uint8_t CH9329_SendMouse(const uint8_t *mouse_data)
{
    if (mouse_data == NULL)
        return 0;

    CH9329_SendFrame(CH9329_CMD_MS_REL, CH9329_MS_DATA_LEN, mouse_data);

    return 1;
}
