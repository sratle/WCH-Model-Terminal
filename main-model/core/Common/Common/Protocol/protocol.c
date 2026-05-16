/********************************** (C) COPYRIGHT *******************************
 * File Name          : protocol.c
 * Author             : Core Team
 * Version            : V1.1.0
 * Date               : 2026/05/12
 * Description        : Unified compact binary protocol stack for core firmware.
 *                      Frame format: [AA][SRC][DST][LEN][CMD][DATA...][A5][5A][FC][FD]
 *                      Streaming frame (LEN=0xFF): DATA length by tail scanning.
 *******************************************************************************/

#include "protocol.h"

/*********************************************************************
 * @fn      Protocol_InitRxCtx
 *
 * @brief   Initialize a protocol receive context.
 *
 * @param   ctx - pointer to receive context.
 *
 * @return  none
 *********************************************************************/
void Protocol_InitRxCtx(protocol_rx_ctx_t *ctx)
{
    if (ctx == NULL)
        return;

    memset(ctx, 0, sizeof(protocol_rx_ctx_t));
    ctx->state = PROTO_STATE_WAIT_HEAD;
}

/*********************************************************************
 * @fn      Protocol_ResetRxCtx
 *
 * @brief   Reset receive context after a frame has been processed.
 *
 * @param   ctx - pointer to receive context.
 *
 * @return  none
 *********************************************************************/
void Protocol_ResetRxCtx(protocol_rx_ctx_t *ctx)
{
    if (ctx == NULL)
        return;

    ctx->state = PROTO_STATE_WAIT_HEAD;
    ctx->data_idx = 0;
    ctx->frame_ready = 0;
    memset(&ctx->frame, 0, sizeof(protocol_frame_t));
}

/*********************************************************************
 * @fn      store_data_byte
 *
 * @brief   Store one byte into the frame data buffer with overflow check.
 *
 * @param   ctx  - pointer to receive context.
 * @param   byte - byte to store.
 *
 * @return  none
 *********************************************************************/
static void store_data_byte(protocol_rx_ctx_t *ctx, uint8_t byte)
{
    if (ctx->data_idx < PROTO_MAX_DATA_LEN)
    {
        ctx->frame.data[ctx->data_idx] = byte;
    }
    else
    {
        if (ctx->data_idx == PROTO_MAX_DATA_LEN)
            ctx->err_overflow++;
    }
    ctx->data_idx++;
}

/*********************************************************************
 * @fn      Protocol_PackFrame
 *
 * @brief   Pack a standard protocol frame into a byte buffer.
 *
 * @param   src       - source module ID.
 * @param   dst       - destination module ID.
 * @param   cmd       - command opcode.
 * @param   data      - pointer to payload data (can be NULL if data_len==0).
 * @param   data_len  - payload data length (0 ~ 254).
 * @param   out_buf   - output byte buffer.
 * @param   out_size  - size of output buffer.
 *
 * @return  Total bytes written; 0 on error.
 *********************************************************************/
uint16_t Protocol_PackFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                           const uint8_t *data, uint8_t data_len,
                           uint8_t *out_buf, uint16_t out_size)
{
    uint8_t len_field;
    uint16_t total_len;

    if (out_buf == NULL || out_size == 0)
        return 0;

    /* Standard frame: LEN is 1 byte, max 255, so data_len max = 254 */
    if (data_len > 254)
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
        memmove(&out_buf[5], data, data_len);
    }

    out_buf[5 + data_len]     = PROTO_FRAME_TAIL0;
    out_buf[5 + data_len + 1] = PROTO_FRAME_TAIL1;
    out_buf[5 + data_len + 2] = PROTO_FRAME_TAIL2;
    out_buf[5 + data_len + 3] = PROTO_FRAME_TAIL3;

    return total_len;
}

/*********************************************************************
 * @fn      Protocol_PackStreamFrame
 *
 * @brief   Pack a streaming protocol frame (LEN = 0xFF).
 *
 * @param   src       - source module ID.
 * @param   dst       - destination module ID.
 * @param   cmd       - command opcode.
 * @param   data      - pointer to payload data.
 * @param   data_len  - payload data length (1 ~ PROTO_STREAM_MAX_DATA).
 * @param   out_buf   - output byte buffer.
 * @param   out_size  - size of output buffer.
 *
 * @return  Total bytes written; 0 on error.
 *********************************************************************/
uint16_t Protocol_PackStreamFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                                  const uint8_t *data, uint16_t data_len,
                                  uint8_t *out_buf, uint16_t out_size)
{
    uint16_t total_len;

    if (out_buf == NULL || out_size == 0)
        return 0;

    if (data_len > PROTO_STREAM_MAX_DATA)
        return 0;

    total_len = 5 + data_len + 4;

    if (out_size < total_len)
        return 0;

    out_buf[0] = PROTO_FRAME_HEAD;
    out_buf[1] = src;
    out_buf[2] = dst;
    out_buf[3] = PROTO_STREAM_LEN;
    out_buf[4] = cmd;

    if (data_len > 0 && data != NULL)
    {
        memmove(&out_buf[5], data, data_len);
    }

    out_buf[5 + data_len]     = PROTO_FRAME_TAIL0;
    out_buf[5 + data_len + 1] = PROTO_FRAME_TAIL1;
    out_buf[5 + data_len + 2] = PROTO_FRAME_TAIL2;
    out_buf[5 + data_len + 3] = PROTO_FRAME_TAIL3;

    return total_len;
}

/*********************************************************************
 * @fn      Protocol_ParseByte
 *
 * @brief   Feed one byte into the receive state machine.
 *
 * @param   ctx  - pointer to receive context.
 * @param   byte - received byte.
 *
 * @return  1 if a complete frame is ready, 0 otherwise.
 *********************************************************************/
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
                ctx->err_len_zero++;
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

            if (ctx->frame.len == PROTO_STREAM_LEN)
            {
                ctx->state = PROTO_STATE_WAIT_STREAM_DATA;
            }
            else if (ctx->frame.len == 1)
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
            uint16_t expected_data_len;

            store_data_byte(ctx, byte);

            expected_data_len = ctx->frame.len - 1;

            if (ctx->data_idx >= expected_data_len)
            {
                ctx->state = PROTO_STATE_WAIT_TAIL0;
            }
            break;
        }

        case PROTO_STATE_WAIT_STREAM_DATA:
        {
            if (byte == PROTO_FRAME_TAIL0)
            {
                ctx->state = PROTO_STATE_TENTATIVE_TAIL1;
            }
            else
            {
                store_data_byte(ctx, byte);
            }
            break;
        }

        case PROTO_STATE_TENTATIVE_TAIL1:
        {
            if (byte == PROTO_FRAME_TAIL1)
            {
                ctx->state = PROTO_STATE_TENTATIVE_TAIL2;
            }
            else
            {
                store_data_byte(ctx, PROTO_FRAME_TAIL0);
                if (byte == PROTO_FRAME_TAIL0)
                {
                    ctx->state = PROTO_STATE_TENTATIVE_TAIL1;
                }
                else
                {
                    store_data_byte(ctx, byte);
                    ctx->state = PROTO_STATE_WAIT_STREAM_DATA;
                }
            }
            break;
        }

        case PROTO_STATE_TENTATIVE_TAIL2:
        {
            if (byte == PROTO_FRAME_TAIL2)
            {
                ctx->state = PROTO_STATE_TENTATIVE_TAIL3;
            }
            else
            {
                store_data_byte(ctx, PROTO_FRAME_TAIL0);
                store_data_byte(ctx, PROTO_FRAME_TAIL1);
                if (byte == PROTO_FRAME_TAIL0)
                {
                    ctx->state = PROTO_STATE_TENTATIVE_TAIL1;
                }
                else
                {
                    store_data_byte(ctx, byte);
                    ctx->state = PROTO_STATE_WAIT_STREAM_DATA;
                }
            }
            break;
        }

        case PROTO_STATE_TENTATIVE_TAIL3:
        {
            if (byte == PROTO_FRAME_TAIL3)
            {
                ctx->frame_ready = 1;
                ctx->frame.len = ctx->data_idx + 1;
                ctx->state = PROTO_STATE_FRAME_READY;
                return 1;
            }
            else
            {
                store_data_byte(ctx, PROTO_FRAME_TAIL0);
                store_data_byte(ctx, PROTO_FRAME_TAIL1);
                store_data_byte(ctx, PROTO_FRAME_TAIL2);
                if (byte == PROTO_FRAME_TAIL0)
                {
                    ctx->state = PROTO_STATE_TENTATIVE_TAIL1;
                }
                else
                {
                    store_data_byte(ctx, byte);
                    ctx->state = PROTO_STATE_WAIT_STREAM_DATA;
                }
            }
            break;
        }

        case PROTO_STATE_WAIT_TAIL0:
        {
            if (byte == PROTO_FRAME_TAIL0)
            {
                ctx->state = PROTO_STATE_WAIT_TAIL1;
            }
            else
            {
                ctx->state = PROTO_STATE_WAIT_HEAD;
            }
            break;
        }

        case PROTO_STATE_WAIT_TAIL1:
        {
            if (byte == PROTO_FRAME_TAIL1)
            {
                ctx->state = PROTO_STATE_WAIT_TAIL2;
            }
            else
            {
                ctx->state = PROTO_STATE_WAIT_HEAD;
            }
            break;
        }

        case PROTO_STATE_WAIT_TAIL2:
        {
            if (byte == PROTO_FRAME_TAIL2)
            {
                ctx->state = PROTO_STATE_WAIT_TAIL3;
            }
            else
            {
                ctx->state = PROTO_STATE_WAIT_HEAD;
            }
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
                ctx->err_frame_ready++;
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

/*********************************************************************
 * @fn      Protocol_ParseIdentity
 *
 * @brief   Parse module identity from a protocol frame.
 *
 * @param   frame - pointer to protocol frame.
 * @param   out   - pointer to module_identity_t to fill.
 *
 * @return  1 on success, 0 if frame data is too short.
 *********************************************************************/
uint8_t Protocol_ParseIdentity(const protocol_frame_t *frame, module_identity_t *out)
{
    if (frame == NULL || out == NULL)
        return 0;

    if (frame->len < 2)
        return 0;

    out->type     = frame->data[0];
    out->subtype  = frame->data[1];
    out->hw_ver   = (frame->len > 2) ? frame->data[2] : 0x00;
    out->fw_major = (frame->len > 3) ? frame->data[3] : 0x00;
    out->fw_minor = (frame->len > 4) ? frame->data[4] : 0x00;

    return 1;
}
