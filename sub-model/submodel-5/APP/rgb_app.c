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

__INTERRUPT __HIGH_CODE void UART0_IRQHandler(void)
{
    /* 不使用中断，轮询方式接收 */
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
    R8_UART0_IER = RB_IER_TXD_EN;    /* 仅使能 TXD 引脚，不使能 RX 中断 */

    /* 不使能 UART0 NVIC 中断，使用轮询方式接收 */
    /* PFIC_EnableIRQ(UART0_IRQn); */

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
    WS2812_Clear();
}

void App_ProcessUART(void)
{
    /* 轮询方式接收 */
    while (R8_UART0_LSR & RB_LSR_DATA_RDY) {
        uint8_t b = R8_UART0_RBR;
        Protocol_ParseByte(&s_proto_rx_ctx, b);
    }

    /* 完整帧就绪时处理 */
    if (s_proto_rx_ctx.frame_ready) {
        ProcessFrame(&s_proto_rx_ctx.frame);
        Protocol_ResetRxCtx(&s_proto_rx_ctx);
    }
}

void App_UpdateEffect(void)
{
    uint32_t delay_count;
    const effect_state_t *st = Effect_GetState();

    /* 更新效果渲染 */
    Effect_Update();

    /* 使用 __NOP() 循环控制 RGB 变化速度
     * speed 0 = 最慢 (~30000 cycles), speed 255 = 最快 (~500 cycles)
     * CH585F @ 60MHz: 1 cycle ≈ 16.7ns
     * 30000 cycles ≈ 500us, 500 cycles ≈ 8.3us */
    delay_count = (uint32_t)(30000 - ((uint32_t)st->speed * 29500 / 255));
    if (delay_count < 500) delay_count = 500;
    while (delay_count--) {
        __asm__ volatile("nop");
    }
}
