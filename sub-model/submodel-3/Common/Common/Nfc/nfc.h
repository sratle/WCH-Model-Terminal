#ifndef __NFC_H__
#define __NFC_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"

/* NFC module UART2 configuration */
#define NFC_USART              USART2
#define NFC_IRQn               USART2_IRQn
#define NFC_BAUDRATE           9600

#define NFC_TX_PORT            GPIOA
#define NFC_TX_PIN             GPIO_Pin_2
#define NFC_RX_PORT            GPIOA
#define NFC_RX_PIN             GPIO_Pin_3

/* NFC module raw frame constants (8 bytes) */
#define NFC_FRAME_HEAD         0xAA
#define NFC_FRAME_LEN          8
#define NFC_ROUTE_MIN          0x30
#define NFC_ROUTE_MAX          0x33

/* Debounce threshold: require this many consecutive identical
 * card reads before reporting to Core (one frame every 91ms) */
#define NFC_DEBOUNCE_THRESHOLD 10

/* Card-absent timeout in milliseconds.
 * NFC module sends a frame every 91ms while a card is present;
 * no valid frame for 500ms -> card has left. */
#define NFC_CARD_ABSENT_MS     500

/* NFC frame parser states */
typedef enum {
    NFC_PARSE_WAIT_HEAD = 0,
    NFC_PARSE_WAIT_ROUTE,
    NFC_PARSE_WAIT_CARD_0,
    NFC_PARSE_WAIT_CARD_1,
    NFC_PARSE_WAIT_CARD_2,
    NFC_PARSE_WAIT_CARD_3,
    NFC_PARSE_WAIT_CARD_4,
    NFC_PARSE_WAIT_PARITY
} nfc_parse_state_t;

/* NFC module context */
typedef struct {
    /* Parser state machine */
    nfc_parse_state_t parse_state;
    uint8_t  frame_buf[NFC_FRAME_LEN];
    uint8_t  frame_idx;

    /* Last valid parsed card data (written by ISR, consumed by main loop) */
    volatile uint8_t  card_id;  /* 1~4, 0 = no pending frame */
    uint8_t  card_number[5];

    /* Debounce: consecutive identical reads */
    uint8_t  debounce_card_id;
    uint8_t  debounce_card_number[5];
    uint8_t  debounce_count;

    /* Reported card (after debounce, sent to Core) */
    uint8_t  reported_card_id;
    uint8_t  reported_card_number[5];

    /* Timestamp (ms) of the last valid frame, for card-absent detection */
    uint32_t last_frame_ms;

    /* New card ready flag (set by debounce logic) */
    volatile uint8_t card_ready;
} nfc_ctx_t;

void Nfc_Init(void);
void Nfc_ParseByte(uint8_t byte);
void Nfc_Process(uint32_t now_ms);
void Nfc_ResetCard(void);

extern nfc_ctx_t nfc_ctx;

#ifdef __cplusplus
}
#endif

#endif
