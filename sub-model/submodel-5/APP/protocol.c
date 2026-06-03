/*********************************************************************
 * File Name          : protocol.c
 * Description        : Protocol stack - identical to keyboard-1's proven
 *                      implementation. Context-based parser with
 *                      frame_ready flag (no callbacks).
 *********************************************************************/

#include "protocol.h"

void Protocol_InitRxCtx(protocol_rx_ctx_t *ctx)
{
    if (ctx == NULL)
        return;

    memset(ctx, 0, sizeof(protocol_rx_ctx_t));
    ctx->state = PROTO_STATE_WAIT_HEAD;
}

void Protocol_ResetRxCtx(protocol_rx_ctx_t *ctx)
{
    if (ctx == NULL)
        return;

    ctx->state = PROTO_STATE_WAIT_HEAD;
    ctx->data_idx = 0;
    ctx->frame_ready = 0;
    memset(&ctx->frame, 0, sizeof(protocol_frame_t));
}

uint16_t Protocol_PackFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                            const uint8_t *data, uint8_t data_len,
                            uint8_t *out_buf, uint16_t out_size)
{
    uint8_t len_field;
    uint16_t total_len;

    if (out_buf == NULL || out_size == 0)
        return 0;

    if (data_len > PROTO_MAX_DATA_LEN)
        return 0;

    len_field = 1 + data_len;
    total_len = 4 + len_field + 4;

    if (out_size < total_len)
        return 0;

    out_buf[0] = PROTO_FRAME_HEAD;
    out_buf[1] = src;
    out_buf[2] = dst;
    out_buf[3] = len_field;
    out_buf[4] = cmd;

    if (data_len > 0 && data != NULL)
    {
        memcpy(&out_buf[5], data, data_len);
    }

    out_buf[5 + data_len]     = PROTO_FRAME_TAIL0;
    out_buf[5 + data_len + 1] = PROTO_FRAME_TAIL1;
    out_buf[5 + data_len + 2] = PROTO_FRAME_TAIL2;
    out_buf[5 + data_len + 3] = PROTO_FRAME_TAIL3;

    return total_len;
}

uint8_t Protocol_ParseByte(protocol_rx_ctx_t *ctx, uint8_t byte)
{
    if (ctx == NULL)
        return 0;

    switch (ctx->state)
    {
        case PROTO_STATE_WAIT_HEAD:
        {
            if (byte == PROTO_FRAME_HEAD)
            {
                ctx->frame.head = byte;
                ctx->state = PROTO_STATE_WAIT_SRC;
            }
            break;
        }

        case PROTO_STATE_WAIT_SRC:
        {
            ctx->frame.src = byte;
            ctx->state = PROTO_STATE_WAIT_DST;
            break;
        }

        case PROTO_STATE_WAIT_DST:
        {
            ctx->frame.dst = byte;
            ctx->state = PROTO_STATE_WAIT_LEN;
            break;
        }

        case PROTO_STATE_WAIT_LEN:
        {
            ctx->frame.len = byte;

            if (ctx->frame.len == 0)
            {
                ctx->state = PROTO_STATE_WAIT_HEAD;
            }
            else
            {
                ctx->state = PROTO_STATE_WAIT_CMD;
            }
            break;
        }

        case PROTO_STATE_WAIT_CMD:
        {
            ctx->frame.cmd = byte;
            ctx->data_idx = 0;

            if (ctx->frame.len == 1)
            {
                ctx->state = PROTO_STATE_WAIT_TAIL0;
            }
            else
            {
                ctx->state = PROTO_STATE_WAIT_DATA;
            }
            break;
        }

        case PROTO_STATE_WAIT_DATA:
        {
            if (ctx->data_idx < PROTO_MAX_DATA_LEN)
            {
                ctx->frame.data[ctx->data_idx] = byte;
            }
            ctx->data_idx++;

            if (ctx->data_idx >= ctx->frame.len - 1)
            {
                ctx->state = PROTO_STATE_WAIT_TAIL0;
            }
            break;
        }

        case PROTO_STATE_WAIT_TAIL0:
        {
            ctx->state = (byte == PROTO_FRAME_TAIL0)
                         ? PROTO_STATE_WAIT_TAIL1
                         : PROTO_STATE_WAIT_HEAD;
            break;
        }

        case PROTO_STATE_WAIT_TAIL1:
        {
            ctx->state = (byte == PROTO_FRAME_TAIL1)
                         ? PROTO_STATE_WAIT_TAIL2
                         : PROTO_STATE_WAIT_HEAD;
            break;
        }

        case PROTO_STATE_WAIT_TAIL2:
        {
            ctx->state = (byte == PROTO_FRAME_TAIL2)
                         ? PROTO_STATE_WAIT_TAIL3
                         : PROTO_STATE_WAIT_HEAD;
            break;
        }

        case PROTO_STATE_WAIT_TAIL3:
        {
            if (byte == PROTO_FRAME_TAIL3)
            {
                ctx->frame_ready = 1;
                ctx->state = PROTO_STATE_FRAME_READY;
                return 1;
            }
            else
            {
                ctx->state = PROTO_STATE_WAIT_HEAD;
            }
            break;
        }

        case PROTO_STATE_FRAME_READY:
        {
            if (byte == PROTO_FRAME_HEAD)
            {
                ctx->frame_ready = 0;
                ctx->data_idx = 0;
                memset(&ctx->frame, 0, sizeof(protocol_frame_t));
                ctx->frame.head = byte;
                ctx->state = PROTO_STATE_WAIT_SRC;
            }
            break;
        }

        default:
        {
            ctx->state = PROTO_STATE_WAIT_HEAD;
            break;
        }
    }

    return 0;
}
