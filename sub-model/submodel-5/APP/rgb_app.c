/*********************************************************************
 * File Name          : rgb_app.c
 * Description        : RGB application layer — 100fps frame-based arch.
 *                      UART0 (PA14-TX, PA15-RX) @ 230400 with Core.
 *                      Each frame: render → WS2812 send → 8ms delay.
 *********************************************************************/

#include "rgb_app.h"
#include "protocol.h"
#include "ws2812.h"
#include "color.h"
#include "effect.h"

/* ==================================================================== */
/*  100fps Frame Timing                                                 */
/*  帧周期 = 10ms，其中 WS2812 传输 ≈ 1.5ms，剩余 ≈ 8ms 分块延时    */
/*  实测 1 nop ≈ 300ns (含循环开销)                                   */
/*  8ms / 100 chunks = 0.08ms/chunk ÷ 300ns ≈ 266 iters                */
/* ==================================================================== */
#define FRAME_DELAY_CHUNKS      100
#define FRAME_DELAY_ITERS       266

/* ==================================================================== */
/*  UART0 RX ring buffer (ISR → main loop)                              */
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

/* Flag: set to 1 after receiving CMD_SUB_SET_MODE from Core.
 * Before this, App_UpdateEffect() skips WS2812 refresh to avoid
 * displaying uninitialized/default color on power-up. */
static uint8_t s_mode_received = 0;

/* UART TX output buffer */
static uint8_t s_uart_tx_buf[PROTO_MAX_FRAME_LEN];

/* ==================================================================== */
/*  UART0 RX Interrupt Handler                                          */
/*  ISR 只做一件事：读 RBR 并写入 ring buffer，不做协议解析。           */
/*  这样 ISR 极短（~20 cycles），WS2812 发送期间也允许其打断，         */
/*  避免 FIFO 溢出丢数据。                                              */
/* ==================================================================== */
__INTERRUPT __HIGH_CODE void UART0_IRQHandler(void)
{
    while (R8_UART0_LSR & RB_LSR_DATA_RDY) {
        uint8_t b = R8_UART0_RBR;
        uint16_t next = (s_uart_rx_head + 1) % UART_RX_BUF_SIZE;
        if (next != s_uart_rx_tail) {
            s_uart_rx_buf[s_uart_rx_head] = b;
            s_uart_rx_head = next;
        }
        /* 如果 ring buffer 满则丢弃字节（不应发生，buffer 足够大） */
    }
}

/* ==================================================================== */
/*  UART Init                                                           */
/* ==================================================================== */

void App_UART_Init(void)
{
    /* 关闭 RF 天线开关：CH585 的 RB_RF_ANT_SW_EN 会占用 PA14/PA15，
     * 与 UART0 重映射后的 PA14(TX)/PA15(RX) 冲突，必须先禁用 */
    GPIOPinRemap(DISABLE, RB_RF_ANT_SW_EN);

    /* Enable UART0 pin remap: PA15=RX, PA14=TX */
    GPIOPinRemap(ENABLE, RB_PIN_UART0);

    /* PA14 = TX (output, 先拉高), PA15 = RX (input, 上拉) */
    GPIOA_SetBits(GPIO_Pin_14);
    GPIOA_ModeCfg(GPIO_Pin_14, GPIO_ModeOut_PP_5mA);
    GPIOA_ModeCfg(GPIO_Pin_15, GPIO_ModeIN_PU);

    /* 直接配置 UART0 寄存器 */
    UART0_BaudRateCfg(UART_BAUDRATE);
    R8_UART0_FCR = (0 << 6) | RB_FCR_TX_FIFO_CLR | RB_FCR_RX_FIFO_CLR | RB_FCR_FIFO_EN;
    R8_UART0_LCR = RB_LCR_WORD_SZ;   /* 8N1 */
    R8_UART0_DIV = 1;

    /* 使能 UART0 RX 中断：ISR 将接收字节写入 ring buffer */
    R8_UART0_IER = RB_IER_TXD_EN;
    UART0_INTCfg(ENABLE, RB_IER_RECV_RDY);
    PFIC_EnableIRQ(UART0_IRQn);

    /* 初始化协议解析上下文 */
    Protocol_InitRxCtx(&s_proto_rx_ctx);
}

/* ==================================================================== */
/*  UART TX helper                                                      */
/* ==================================================================== */

static void UART0_SendBuf(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        while (R8_UART0_TFC == UART_FIFO_SIZE)
            ;
        R8_UART0_THR = buf[i];
    }
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
    const effect_state_t *st = Effect_GetState();
    /* DATA: [mode:1][R:1][G:1][B:1][brightness:1][speed:1] */
    uint8_t status_data[6] = {
        (uint8_t)st->mode, st->r, st->g, st->b,
        st->brightness, st->speed
    };
    App_SendAck(status_data, sizeof(status_data));
}

static void HandleSetMode(const protocol_frame_t *frame)
{
    uint8_t subcmd = (frame->len >= 1) ? frame->data[0] : 0;

    switch (subcmd) {
        case 0x01: /* 设置 RGB 模式: [SUB=0x01][mode][R][G][B][brightness][speed] */
            if (frame->len >= 7) {
                uint8_t mode = frame->data[1];
                if (mode > RGB_MODE_MARQUEE) {
                    App_SendNack(PROTO_ERR_INVALID_PARAM);
                    return;
                }
                Effect_SetMode((rgb_mode_t)mode,
                               frame->data[2], frame->data[3], frame->data[4],
                               frame->data[5], frame->data[6]);
                s_mode_received = 1;
                App_SendAck(NULL, 0);
            } else {
                App_SendNack(PROTO_ERR_LEN_MISMATCH);
            }
            break;

        case 0x02: /* 传输自定义帧: [SUB=0x02][frame_idx][RGB888:147] */
            if (frame->len >= 2 + WS2812_LED_COUNT * 3) {
                Effect_SetCustomFrame(frame->data[1], &frame->data[2]);
                App_SendAck(NULL, 0);
            } else {
                App_SendNack(PROTO_ERR_LEN_MISMATCH);
            }
            break;

        case 0x03: /* 播放自定义动画: [SUB=0x03][frame_count][interval_hi][interval_lo] */
            if (frame->len >= 4) {
                uint16_t interval = ((uint16_t)frame->data[2] << 8) | frame->data[3];
                Effect_PlayCustom(frame->data[1], interval);
                App_SendAck(NULL, 0);
            } else {
                App_SendNack(PROTO_ERR_LEN_MISMATCH);
            }
            break;

        default:
            App_SendNack(PROTO_ERR_INVALID_PARAM);
            break;
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

    /* 正式模式：等待 Core 发送 CMD_SUB_SET_MODE 后再显示 */
}

void App_ProcessUART(void)
{
    /* 从 ring buffer 中取出 ISR 已接收的字节，送入协议解析器。
     * 每次解析出完整帧后立即处理并重置，避免后续帧的字节被
     * PROTO_STATE_FRAME_READY 状态丢弃。 */
    while (s_uart_rx_tail != s_uart_rx_head) {
        uint8_t b = s_uart_rx_buf[s_uart_rx_tail];
        s_uart_rx_tail = (s_uart_rx_tail + 1) % UART_RX_BUF_SIZE;
        Protocol_ParseByte(&s_proto_rx_ctx, b);

        /* 完整帧就绪时立即处理并重置，再继续解析后续字节 */
        if (s_proto_rx_ctx.frame_ready) {
            ProcessFrame(&s_proto_rx_ctx.frame);
            Protocol_ResetRxCtx(&s_proto_rx_ctx);
        }
    }
}

void App_UpdateEffect(void)
{
    /* 1. 渲染当前帧数据到 LED buffer + 发送到 WS2812
     *    Effect_Update() 内部调用 WS2812_Refresh()
     *    传输期间全局中断禁用，保证时序精确 (~1.5ms) */
    Effect_Update();

    /* 2. 帧间延时 ≈ 8ms，分 10 块，块间处理 UART
     *    保持 PB14 低电平（WS2812 reset 状态） */
    {
        uint8_t chunk;
        for (chunk = 0; chunk < FRAME_DELAY_CHUNKS; chunk++) {
            uint32_t d = FRAME_DELAY_ITERS;
            do {
                __asm__ volatile("nop");
            } while (--d);
            App_ProcessUART();
        }
    }
}
