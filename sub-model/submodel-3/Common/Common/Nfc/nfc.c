#include "nfc.h"
#include <string.h>

nfc_ctx_t nfc_ctx;

/* ---- UART2 (NFC module) GPIO & peripheral init ---- */

static void Nfc_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* TX (PA2) — reserved, configured as AF push-pull */
    gpio.GPIO_Pin   = NFC_TX_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(NFC_TX_PORT, &gpio);

    /* RX (PA3) */
    gpio.GPIO_Pin  = NFC_RX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(NFC_RX_PORT, &gpio);
}

void Nfc_Init(void)
{
    USART_InitTypeDef usart;

    Nfc_GPIO_Init();

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    usart.USART_BaudRate            = NFC_BAUDRATE;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(NFC_USART, &usart);

    USART_ITConfig(NFC_USART, USART_IT_RXNE, ENABLE);

    NVIC_SetPriority(NFC_IRQn, 2);
    NVIC_EnableIRQ(NFC_IRQn);

    USART_Cmd(NFC_USART, ENABLE);

    /* Init context */
    memset(&nfc_ctx, 0, sizeof(nfc_ctx_t));
    nfc_ctx.parse_state = NFC_PARSE_WAIT_HEAD;
}

/* ---- Byte-by-byte NFC frame parser (called from USART2 ISR) ----
 *
 * NFC module 8-byte frame:
 *   [0] 0xAA header
 *   [1] route (0x30~0x33)
 *   [2..6] card_number (5 bytes)
 *   [7] parity (XOR of bytes [0..6])
 */

void Nfc_ParseByte(uint8_t byte)
{
    switch (nfc_ctx.parse_state)
    {
        case NFC_PARSE_WAIT_HEAD:
        {
            if (byte == NFC_FRAME_HEAD)
            {
                nfc_ctx.frame_buf[0] = byte;
                nfc_ctx.frame_idx = 1;
                nfc_ctx.parse_state = NFC_PARSE_WAIT_ROUTE;
            }
            break;
        }

        case NFC_PARSE_WAIT_ROUTE:
        {
            if (byte >= NFC_ROUTE_MIN && byte <= NFC_ROUTE_MAX)
            {
                nfc_ctx.frame_buf[1] = byte;
                nfc_ctx.frame_idx = 2;
                nfc_ctx.parse_state = NFC_PARSE_WAIT_CARD_0;
            }
            else
            {
                nfc_ctx.parse_state = NFC_PARSE_WAIT_HEAD;
            }
            break;
        }

        case NFC_PARSE_WAIT_CARD_0:
            nfc_ctx.frame_buf[2] = byte;
            nfc_ctx.frame_idx = 3;
            nfc_ctx.parse_state = NFC_PARSE_WAIT_CARD_1;
            break;

        case NFC_PARSE_WAIT_CARD_1:
            nfc_ctx.frame_buf[3] = byte;
            nfc_ctx.frame_idx = 4;
            nfc_ctx.parse_state = NFC_PARSE_WAIT_CARD_2;
            break;

        case NFC_PARSE_WAIT_CARD_2:
            nfc_ctx.frame_buf[4] = byte;
            nfc_ctx.frame_idx = 5;
            nfc_ctx.parse_state = NFC_PARSE_WAIT_CARD_3;
            break;

        case NFC_PARSE_WAIT_CARD_3:
            nfc_ctx.frame_buf[5] = byte;
            nfc_ctx.frame_idx = 6;
            nfc_ctx.parse_state = NFC_PARSE_WAIT_CARD_4;
            break;

        case NFC_PARSE_WAIT_CARD_4:
            nfc_ctx.frame_buf[6] = byte;
            nfc_ctx.frame_idx = 7;
            nfc_ctx.parse_state = NFC_PARSE_WAIT_PARITY;
            break;

        case NFC_PARSE_WAIT_PARITY:
        {
            uint8_t parity = 0;
            uint8_t i;

            nfc_ctx.frame_buf[7] = byte;

            /* XOR parity check: parity = frame_buf[0] ^ [1] ^ ... ^ [6] */
            for (i = 0; i < 7; i++)
                parity ^= nfc_ctx.frame_buf[i];

            if (parity == byte)
            {
                /* Valid frame: extract card_number first, card_id last.
                 * Main loop uses card_id != 0 as "new data ready" flag,
                 * so card_number must be written before card_id. */
                memcpy(nfc_ctx.card_number, &nfc_ctx.frame_buf[2], 5);
                nfc_ctx.card_id = nfc_ctx.frame_buf[1] - NFC_ROUTE_MIN + 1;
            }

            nfc_ctx.parse_state = NFC_PARSE_WAIT_HEAD;
            break;
        }

        default:
            nfc_ctx.parse_state = NFC_PARSE_WAIT_HEAD;
            break;
    }
}

/* ---- Debounce & card-absent logic (called from main loop) ---- */

static uint8_t CardNumbersEqual(const uint8_t *a, const uint8_t *b)
{
    uint8_t i;
    for (i = 0; i < 5; i++)
    {
        if (a[i] != b[i])
            return 0;
    }
    return 1;
}

void Nfc_ResetCard(void)
{
    nfc_ctx.reported_card_id = 0;
    memset(nfc_ctx.reported_card_number, 0, 5);
    nfc_ctx.debounce_card_id = 0;
    memset(nfc_ctx.debounce_card_number, 0, 5);
    nfc_ctx.debounce_count = 0;
    nfc_ctx.card_ready = 0;
}

void Nfc_Process(void)
{
    /* If a valid card was parsed by ISR (card_id != 0) */
    if (nfc_ctx.card_id != 0)
    {
        nfc_ctx.frame_alive_counter = 0;

        /* Check if this is the same card as the debounce candidate */
        if (nfc_ctx.card_id == nfc_ctx.debounce_card_id &&
            CardNumbersEqual(nfc_ctx.card_number, nfc_ctx.debounce_card_number))
        {
            nfc_ctx.debounce_count++;
        }
        else
        {
            /* Different card: reset debounce */
            nfc_ctx.debounce_card_id = nfc_ctx.card_id;
            memcpy(nfc_ctx.debounce_card_number, nfc_ctx.card_number, 5);
            nfc_ctx.debounce_count = 1;
        }

        /* Debounce threshold reached and not yet reported */
        if (nfc_ctx.debounce_count >= NFC_DEBOUNCE_THRESHOLD)
        {
            if (nfc_ctx.reported_card_id != nfc_ctx.debounce_card_id ||
                !CardNumbersEqual(nfc_ctx.reported_card_number, nfc_ctx.debounce_card_number))
            {
                /* New card detected after debounce: mark ready for reporting */
                nfc_ctx.reported_card_id = nfc_ctx.debounce_card_id;
                memcpy(nfc_ctx.reported_card_number, nfc_ctx.debounce_card_number, 5);
                nfc_ctx.card_ready = 1;
            }
        }

        /* Clear the parsed card_id so we only process each ISR parse once */
        nfc_ctx.card_id = 0;
    }
    else
    {
        /* No valid card frame this iteration */
        nfc_ctx.frame_alive_counter++;

        if (nfc_ctx.frame_alive_counter > NFC_CARD_ABSENT_TIMEOUT)
        {
            if (nfc_ctx.reported_card_id != 0)
            {
                /* Card has left: reset all state */
                Nfc_ResetCard();
            }
            nfc_ctx.frame_alive_counter = NFC_CARD_ABSENT_TIMEOUT;
        }
    }
}
