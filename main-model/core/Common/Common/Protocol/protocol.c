/********************************** (C) COPYRIGHT *******************************
 * File Name          : protocol.c
 * Author             : Core Team
 * Version            : V1.0.0
 * Date               : 2026/04/22
 * Description        : Unified compact binary protocol stack for core firmware.
 *                      Frame format: [AA][SRC][DST][LEN][CMD][DATA...]
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
    /* err_* 字段已由 memset 清零 */
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
    /* Only clear frame header/meta; full memset is acceptable but
       we preserve the already-received frame data until overwritten. */
    memset(&ctx->frame, 0, sizeof(protocol_frame_t));
}

/*********************************************************************
 * @fn      Protocol_PackFrame
 *
 * @brief   Pack a protocol frame into a byte buffer.
 *
 * @param   src       - source module ID.
 * @param   dst       - destination module ID.
 * @param   cmd       - command opcode.
 * @param   data      - pointer to payload data (can be NULL if data_len==0).
 * @param   data_len  - payload data length (0 ~ 255).
 * @param   out_buf   - output byte buffer.
 * @param   out_size  - size of output buffer.
 *
 * @return  Total bytes written; 0 on error.
 *********************************************************************/
uint8_t Protocol_PackFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                           const uint8_t *data, uint8_t data_len,
                           uint8_t *out_buf, uint16_t out_size)
{
    uint8_t len_field;
    uint8_t total_len;

    if (out_buf == NULL || out_size == 0)
        return 0;

    if (data_len > PROTO_MAX_DATA_LEN)
        return 0;

    len_field = 1 + data_len;               /* LEN = CMD(1) + DATA(data_len) */
    total_len = 4 + len_field;              /* HEAD+SRC+DST+LEN + CMD+DATA */

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
            /* Otherwise: stay in WAIT_HEAD, discard byte */
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

            /* LEN must be at least 1 (CMD itself). Zero is illegal. */
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

            /* If LEN == 1, there is no data field; frame is complete. */
            if (ctx->frame.len == 1)
            {
                ctx->frame_ready = 1;
                ctx->state = PROTO_STATE_FRAME_READY;
                return 1;
            }
            else
            {
                ctx->state = PROTO_STATE_WAIT_DATA;
            }
            break;
        }

        case PROTO_STATE_WAIT_DATA:
        {
            uint8_t expected_data_len;

            /* Store byte if buffer has room */
            if (ctx->data_idx < PROTO_MAX_DATA_LEN)
            {
                ctx->frame.data[ctx->data_idx] = byte;
            }
            else
            {
                /* Buffer full, count overflow once per frame */
                if (ctx->data_idx == PROTO_MAX_DATA_LEN)
                    ctx->err_overflow++;
            }

            ctx->data_idx++;

            expected_data_len = ctx->frame.len - 1;

            /* Check if all data bytes have been received */
            if (ctx->data_idx >= expected_data_len)
            {
                ctx->frame_ready = 1;
                ctx->state = PROTO_STATE_FRAME_READY;
                return 1;
            }
            break;
        }

        case PROTO_STATE_FRAME_READY:
        {
            /* Frame is ready but caller has not reset yet.
               If a new HEAD arrives, start fresh immediately to avoid
               losing the next frame. */
            if (byte == PROTO_FRAME_HEAD)
            {
                ctx->err_frame_ready++;
                ctx->frame_ready = 0;
                ctx->data_idx = 0;
                memset(&ctx->frame, 0, sizeof(protocol_frame_t));
                ctx->frame.head = byte;
                ctx->state = PROTO_STATE_WAIT_SRC;
            }
            /* Otherwise: discard bytes until caller resets */
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

    if (frame->len < 2)  /* At least type + subtype required */
        return 0;

    out->type     = frame->data[0];
    out->subtype  = frame->data[1];
    out->hw_ver   = (frame->len > 2) ? frame->data[2] : 0x00;
    out->fw_major = (frame->len > 3) ? frame->data[3] : 0x00;
    out->fw_minor = (frame->len > 4) ? frame->data[4] : 0x00;

    return 1;
}
