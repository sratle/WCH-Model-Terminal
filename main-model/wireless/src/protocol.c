/********************************** (C) COPYRIGHT *******************************
 * File Name          : protocol.c
 *******************************************************************************/

#include "protocol.h"

void Protocol_Init(void)
{
    /* Pure logic module, nothing to init */
}

bool Protocol_ParseByte(uint8_t byte, proto_frame_t *frame)
{
    static proto_state_t state = PROTO_STATE_WAIT_HEAD;
    static uint8_t data_cnt = 0;

    switch(state)
    {
        case PROTO_STATE_WAIT_HEAD:
            if(byte == PROTO_HEAD)
            {
                frame->head = byte;
                state = PROTO_STATE_WAIT_SRC;
            }
            break;

        case PROTO_STATE_WAIT_SRC:
            frame->src = byte;
            state = PROTO_STATE_WAIT_DST;
            break;

        case PROTO_STATE_WAIT_DST:
            frame->dst = byte;
            state = PROTO_STATE_WAIT_LEN;
            break;

        case PROTO_STATE_WAIT_LEN:
            frame->len = byte;
            if(frame->len == 0)
            {
                /* invalid len, reset */
                state = PROTO_STATE_WAIT_HEAD;
            }
            else if(frame->len == 1)
            {
                frame->data_idx = 0;
                state = PROTO_STATE_WAIT_TAIL0;
            }
            else
            {
                data_cnt = frame->len - 1; /* data bytes count */
                frame->data_idx = 0;
                state = PROTO_STATE_WAIT_DATA;
            }
            break;

        case PROTO_STATE_WAIT_DATA:
            frame->data[frame->data_idx++] = byte;
            data_cnt--;
            if(data_cnt == 0)
                state = PROTO_STATE_WAIT_TAIL0;
            break;

        case PROTO_STATE_WAIT_TAIL0:
            if(byte == PROTO_TAIL0)
            {
                frame->tail[0] = byte;
                state = PROTO_STATE_WAIT_TAIL1;
            }
            else
            {
                state = PROTO_STATE_WAIT_HEAD;
            }
            break;

        case PROTO_STATE_WAIT_TAIL1:
            if(byte == PROTO_TAIL1)
            {
                frame->tail[1] = byte;
                state = PROTO_STATE_WAIT_TAIL2;
            }
            else
            {
                state = PROTO_STATE_WAIT_HEAD;
            }
            break;

        case PROTO_STATE_WAIT_TAIL2:
            if(byte == PROTO_TAIL2)
            {
                frame->tail[2] = byte;
                state = PROTO_STATE_WAIT_TAIL3;
            }
            else
            {
                state = PROTO_STATE_WAIT_HEAD;
            }
            break;

        case PROTO_STATE_WAIT_TAIL3:
            if(byte == PROTO_TAIL3)
            {
                frame->tail[3] = byte;
                state = PROTO_STATE_WAIT_HEAD;
                return TRUE; /* frame ready */
            }
            else
            {
                state = PROTO_STATE_WAIT_HEAD;
            }
            break;

        default:
            state = PROTO_STATE_WAIT_HEAD;
            break;
    }
    return FALSE;
}

uint16_t Protocol_PackFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                            const uint8_t *data, uint8_t data_len,
                            uint8_t *out_buf, uint16_t out_buf_len)
{
    uint16_t total = 9 + data_len; /* head + src + dst + len + cmd + data + 4 tail */
    if(total > out_buf_len || data_len > PROTO_DATA_MAX_LEN)
        return 0;

    out_buf[0] = PROTO_HEAD;
    out_buf[1] = src;
    out_buf[2] = dst;
    out_buf[3] = 1 + data_len; /* len = cmd + data */
    out_buf[4] = cmd;
    if(data_len > 0)
        tmos_memcpy(&out_buf[5], data, data_len);
    out_buf[5 + data_len + 0] = PROTO_TAIL0;
    out_buf[5 + data_len + 1] = PROTO_TAIL1;
    out_buf[5 + data_len + 2] = PROTO_TAIL2;
    out_buf[5 + data_len + 3] = PROTO_TAIL3;

    return total;
}

uint16_t Protocol_PackAck(uint8_t src, uint8_t dst,
                          const uint8_t *rsp_data, uint8_t rsp_len,
                          uint8_t *out_buf, uint16_t out_buf_len)
{
    if(rsp_len > PROTO_DATA_MAX_LEN)
        return 0;
    return Protocol_PackFrame(src, dst, CMD_ACK, rsp_data, rsp_len, out_buf, out_buf_len);
}

uint16_t Protocol_PackNack(uint8_t src, uint8_t dst, uint8_t err_code,
                           uint8_t *out_buf, uint16_t out_buf_len)
{
    uint8_t data[1] = {err_code};
    return Protocol_PackFrame(src, dst, CMD_NACK, data, 1, out_buf, out_buf_len);
}
