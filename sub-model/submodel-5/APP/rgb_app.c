/*********************************************************************
 * File Name          : rgb_app.c
 * Description        : RGB application layer implementation.
 *                      UART0 (PA14-TX, PA15-RX) communication with Core.
 *                      Protocol command dispatch for RGB submodel.
 *                      Uses frame_ready flag pattern (like keyboard-1).
 *********************************************************************/

#include "rgb_app.h"
#include "protocol.h"
#include "ws2812.h"
#include "color.h"
#include "effect.h"

/* ==================================================================== */
/*  SysTick millisecond counter                                         */
/* ==================================================================== */
volatile uint32_t g_systick_ms = 0;

__INTERRUPT void SysTick_IRQHandler(void)
{
    /* Clear SysTick interrupt flag (CH585 RISC-V: SR register) */
    SysTick->SR &= ~SysTick_SR_CNTIF;
    g_systick_ms++;
}

void App_SysTick_Init(void)
{
    /* CH585 RISC-V SysTick: CTLR/CNT/CMP/SR registers */
    SysTick->CTLR = 0;
    SysTick->CMP  = (GetSysClock() / 1000) - 1;  /* 1ms tick */
    SysTick->CNTL = 0;
    PFIC_EnableIRQ(SysTick_IRQn);
    SysTick->CTLR = SysTick_CTLR_STRE  |
                    SysTick_CTLR_STCLK |
                    SysTick_CTLR_STIE  |
                    SysTick_CTLR_STE;
}

/* ==================================================================== */
/*  UART0 RX ring buffer                                                */
/* ==================================================================== */
static uint8_t  s_uart_rx_buf[UART_RX_BUF_SIZE];
static volatile uint16_t s_uart_rx_head = 0;
static volatile uint16_t s_uart_rx_tail = 0;

/* Protocol receive context (same pattern as keyboard-1) */
protocol_rx_ctx_t s_proto_rx_ctx;

/* Our own module ID, learned from the first received frame's DST field.
 * Submodel-5 can be plugged into slot 1 (0x40), slot 2 (0x41), or slot 3 (0x42).
 * Initially MODULE_ID_SUBMODEL (0x40), updated on first frame reception. */
static uint8_t s_my_module_id = MODULE_ID_SUBMODEL;

/* UART TX output buffer */
static uint8_t s_uart_tx_buf[PROTO_MAX_FRAME_LEN];

__INTERRUPT void UART0_IRQHandler(void)
{
    volatile uint8_t iir = R8_UART0_IIR;
    (void)iir;  /* Read IIR to acknowledge interrupt */

    if (R8_UART0_LSR & RB_LSR_DATA_RDY) {
        while (R8_UART0_LSR & RB_LSR_DATA_RDY) {
            uint8_t b = R8_UART0_RBR;
            uint16_t next = (s_uart_rx_head + 1) % UART_RX_BUF_SIZE;
            if (next != s_uart_rx_tail) {
                s_uart_rx_buf[s_uart_rx_head] = b;
                s_uart_rx_head = next;
            }
        }
    }
}

/* ==================================================================== */
/*  UART Init                                                           */
/* ==================================================================== */

void App_UART_Init(void)
{
    /* Enable UART0 pin remap: PA15=RX, PA14=TX */
    GPIOPinRemap(ENABLE, RB_PIN_UART0);

    /* PA14 = TX (output), PA15 = RX (input) */
    GPIOA_ModeCfg(GPIO_Pin_14, GPIO_ModeOut_PP_5mA);
    GPIOA_ModeCfg(GPIO_Pin_15, GPIO_ModeIN_PU);

    /* Initialize UART0 */
    UART0_DefInit();
    UART0_BaudRateCfg(UART_BAUDRATE);

    /* 1-byte trigger for RX interrupt */
    UART0_ByteTrigCfg(UART_1BYTE_TRIG);

    /* Enable RX data interrupt */
    UART0_INTCfg(ENABLE, RB_IER_RECV_RDY);

    /* Enable UART0 IRQ */
    PFIC_EnableIRQ(UART0_IRQn);

    /* Initialize protocol parser context */
    Protocol_InitRxCtx(&s_proto_rx_ctx);
}

/* ==================================================================== */
/*  UART TX helper                                                      */
/* ==================================================================== */

static void UART0_SendBuf(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        /* Wait while TX FIFO is full (not empty bit cleared) */
        while (!(R8_UART0_LSR & RB_LSR_TX_FIFO_EMP))
            ;
        R8_UART0_THR = buf[i];
    }
    /* Wait for all bytes to finish transmitting */
    while (!(R8_UART0_LSR & RB_LSR_TX_ALL_EMP))
        ;
}

/* ==================================================================== */
/*  Protocol Send Helpers                                               */
/* ==================================================================== */

void App_SendFrame(uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
    uint16_t len = Protocol_PackFrame(s_my_module_id, MODULE_ID_CORE,
                                       cmd, data, data_len,
                                       s_uart_tx_buf, sizeof(s_uart_tx_buf));
    if (len > 0) {
        UART0_SendBuf(s_uart_tx_buf, len);
    }
}

void App_SendAck(const uint8_t *rsp_data, uint8_t rsp_len)
{
    /* ACK frame: SRC = our module ID, DST = Core (0x00) */
    uint16_t len = Protocol_PackFrame(s_my_module_id, MODULE_ID_CORE,
                                       CMD_ACK, rsp_data, rsp_len,
                                       s_uart_tx_buf, sizeof(s_uart_tx_buf));
    if (len > 0) {
        UART0_SendBuf(s_uart_tx_buf, len);
    }
}

void App_SendNack(uint8_t err_code)
{
    App_SendFrame(CMD_NACK, &err_code, 1);
}

/* ==================================================================== */
/*  Command Handlers                                                    */
/* ==================================================================== */

static void HandleGetType(const protocol_frame_t *frame)
{
    (void)frame;
    uint8_t type_data[5] = {
        MODULE_TYPE_SUBMODEL,       /* 0x05 */
        MODULE_SUBTYPE_SUBMODEL_RGB,/* 0x05 */
        MODULE_HW_VERSION, MODULE_FW_MAJOR, MODULE_FW_MINOR
    };
    App_SendAck(type_data, sizeof(type_data));
}

static void HandleGetStatus(const protocol_frame_t *frame)
{
    (void)frame;
    uint8_t status_data[4] = { 0x00, 0x00, 0x00, 0x00 };
    App_SendAck(status_data, sizeof(status_data));
}

static void HandleSetMode(const protocol_frame_t *frame)
{
    if (frame->len >= 2 && frame->data[0] < 4) {
        /* mode(1) + r(1) + g(1) + b(1) + brightness(1) + speed(1) */
        uint8_t r = (frame->len >= 3) ? frame->data[1] : 0xFF;
        uint8_t g = (frame->len >= 4) ? frame->data[2] : 0xFF;
        uint8_t b = (frame->len >= 5) ? frame->data[3] : 0xFF;
        uint8_t br = (frame->len >= 6) ? frame->data[4] : 0xFF;
        uint8_t sp = (frame->len >= 7) ? frame->data[5] : 0x80;
        Effect_SetMode((rgb_mode_t)frame->data[0], r, g, b, br, sp);
        App_SendAck(NULL, 0);
    } else {
        App_SendNack(PROTO_ERR_INVALID_PARAM);
    }
}

static void HandleSetConfig(const protocol_frame_t *frame)
{
    if (frame->len >= 4) {
        uint8_t r = frame->data[0];
        uint8_t g = frame->data[1];
        uint8_t b = frame->data[2];
        Effect_SetMode(RGB_MODE_SOLID, r, g, b, 0xFF, 0x80);
        App_SendAck(NULL, 0);
    } else {
        App_SendNack(PROTO_ERR_LEN_MISMATCH);
    }
}

/* ==================================================================== */
/*  Frame Processing (called when frame_ready is set)                   */
/* ==================================================================== */

static void ProcessFrame(const protocol_frame_t *frame)
{
    /* Only handle frames destined to Submodel slots (0x40~0x42) */
    if (frame->dst < MODULE_ID_SUBMODEL || frame->dst > MODULE_ID_SUBMODEL + 2) {
        return;
    }

    /* Learn our module ID from the DST field of the first valid frame */
    if (s_my_module_id != frame->dst) {
        s_my_module_id = frame->dst;
    }

    switch (frame->cmd) {
        case CMD_GET_TYPE:
            HandleGetType(frame);
            break;

        case CMD_GET_STATUS:
            HandleGetStatus(frame);
            break;

        case CMD_SUB_SET_MODE:
            HandleSetMode(frame);
            break;

        case CMD_SUB_SET_CONFIG:
            HandleSetConfig(frame);
            break;

        case CMD_NOP:
            /* Heartbeat, no response needed */
            break;

        default:
            App_SendNack(PROTO_ERR_UNSUPPORTED_CMD);
            break;
    }
}

/* ==================================================================== */
/*  Public API                                                          */
/* ==================================================================== */

void App_Init(void)
{
    WS2812_Init();
    Effect_Init();

    /* Start with all LEDs off */
    WS2812_Clear();
}

void App_ProcessUART(void)
{
    /* Feed received bytes into protocol parser */
    while (s_uart_rx_head != s_uart_rx_tail) {
        uint8_t b = s_uart_rx_buf[s_uart_rx_tail];
        s_uart_rx_tail = (s_uart_rx_tail + 1) % UART_RX_BUF_SIZE;
        Protocol_ParseByte(&s_proto_rx_ctx, b);
    }

    /* Check if a complete frame is ready (same pattern as keyboard-1) */
    if (s_proto_rx_ctx.frame_ready) {
        ProcessFrame(&s_proto_rx_ctx.frame);
        Protocol_ResetRxCtx(&s_proto_rx_ctx);
    }
}

void App_UpdateEffect(void)
{
    Effect_Update(App_GetMs());
}
