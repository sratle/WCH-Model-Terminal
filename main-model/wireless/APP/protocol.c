/*********************************************************************
 * File Name          : protocol.c
 * Description        : SPI protocol frame parser & packer.
 *                      Supports standard fixed-length frames and
 *                      streaming frames (LEN=0xFF, tail-terminated).
 *********************************************************************/

#include "protocol.h"

/* ==================================================================== */
/*  Local State                                                         */
/* ==================================================================== */
static proto_state_t    s_state = PROTO_STATE_WAIT_HEAD;
static proto_frame_t    s_frame;
static uint16_t         s_data_idx = 0;
static uint16_t         s_data_len = 0;

/* Streaming frame buffer */
static uint8_t          s_stream_buf[PROTO_STREAM_MAX_DATA];
static uint16_t         s_stream_idx = 0;

/* Tentative tail bytes buffer for backtracking */
static uint8_t          s_tentative_buf[3];
static uint8_t          s_tentative_cnt = 0;

/* Callbacks */
static proto_std_frame_cb_t     s_std_cb = NULL;
static proto_stream_chunk_cb_t  s_stream_cb = NULL;

/* ==================================================================== */
/*  Internal Helpers                                                    */
/* ==================================================================== */

static void ResetParser(void)
{
    s_state = PROTO_STATE_WAIT_HEAD;
    s_data_idx = 0;
    s_data_len = 0;
    s_stream_idx = 0;
    s_tentative_cnt = 0;
    memset(&s_frame, 0, sizeof(s_frame));
}

static void FlushTentativeToStream(void)
{
    uint8_t i;
    for (i = 0; i < s_tentative_cnt; i++) {
        if (s_stream_idx < PROTO_STREAM_MAX_DATA) {
            s_stream_buf[s_stream_idx++] = s_tentative_buf[i];
        }
    }
    s_tentative_cnt = 0;
}

static void SubmitStandardFrame(void)
{
    if (s_std_cb != NULL) {
        s_std_cb(&s_frame);
    }
    ResetParser();
}

static void SubmitStreamFrame(uint8_t is_final, uint8_t flags)
{
    if (s_stream_cb != NULL && s_stream_idx > 0) {
        s_stream_cb(s_stream_buf, s_stream_idx, is_final, flags);
    }
    s_stream_idx = 0;
    if (is_final) {
        ResetParser();
    }
}

/* ==================================================================== */
/*  Public API                                                          */
/* ==================================================================== */

void Protocol_Init(void)
{
    ResetParser();
    s_std_cb = NULL;
    s_stream_cb = NULL;
}

void Protocol_RegisterCallbacks(proto_std_frame_cb_t std_cb,
                                proto_stream_chunk_cb_t stream_cb)
{
    s_std_cb = std_cb;
    s_stream_cb = stream_cb;
}

bool Protocol_ParseByte(uint8_t byte)
{
    switch (s_state) {
        /* ---------------------------------------------------------- */
        case PROTO_STATE_WAIT_HEAD:
            if (byte == PROTO_HEAD) {
                s_frame.head = byte;
                s_state = PROTO_STATE_WAIT_SRC;
            }
            break;

        /* ---------------------------------------------------------- */
        case PROTO_STATE_WAIT_SRC:
            s_frame.src = byte;
            s_state = PROTO_STATE_WAIT_DST;
            break;

        /* ---------------------------------------------------------- */
        case PROTO_STATE_WAIT_DST:
            s_frame.dst = byte;
            s_state = PROTO_STATE_WAIT_LEN;
            break;

        /* ---------------------------------------------------------- */
        case PROTO_STATE_WAIT_LEN:
            s_frame.len = byte;
            s_state = PROTO_STATE_WAIT_CMD;
            break;

        /* ---------------------------------------------------------- */
        case PROTO_STATE_WAIT_CMD:
            s_frame.cmd = byte;
            if (s_frame.len == PROTO_LEN_STREAMING) {
                /* Streaming frame: tail-terminated */
                s_stream_idx = 0;
                s_tentative_cnt = 0;
                s_state = PROTO_STATE_STREAM_DATA;
            } else {
                s_data_len = (s_frame.len > 0) ? (s_frame.len - 1) : 0;
                s_data_idx = 0;
                if (s_data_len == 0) {
                    s_state = PROTO_STATE_WAIT_TAIL0;
                } else if (s_data_len <= PROTO_DATA_MAX_LEN) {
                    s_state = PROTO_STATE_WAIT_DATA;
                } else {
                    /* Invalid LEN, reset */
                    ResetParser();
                }
            }
            break;

        /* ---------------------------------------------------------- */
        case PROTO_STATE_WAIT_DATA:
            if (s_data_idx < PROTO_DATA_MAX_LEN) {
                s_frame.data[s_data_idx++] = byte;
            }
            if (s_data_idx >= s_data_len) {
                s_state = PROTO_STATE_WAIT_TAIL0;
            }
            break;

        /* ---------------------------------------------------------- */
        case PROTO_STATE_WAIT_TAIL0:
            if (byte == PROTO_TAIL0) {
                s_state = PROTO_STATE_WAIT_TAIL1;
            } else {
                ResetParser();
            }
            break;

        case PROTO_STATE_WAIT_TAIL1:
            if (byte == PROTO_TAIL1) {
                s_state = PROTO_STATE_WAIT_TAIL2;
            } else {
                ResetParser();
            }
            break;

        case PROTO_STATE_WAIT_TAIL2:
            if (byte == PROTO_TAIL2) {
                s_state = PROTO_STATE_WAIT_TAIL3;
            } else {
                ResetParser();
            }
            break;

        case PROTO_STATE_WAIT_TAIL3:
            if (byte == PROTO_TAIL3) {
                s_frame.tail[0] = PROTO_TAIL0;
                s_frame.tail[1] = PROTO_TAIL1;
                s_frame.tail[2] = PROTO_TAIL2;
                s_frame.tail[3] = PROTO_TAIL3;
                SubmitStandardFrame();
                return TRUE;
            } else {
                ResetParser();
            }
            break;

        /* ---------------------------------------------------------- */
        /*  Streaming frame: tail-terminated with backtracking          */
        /* ---------------------------------------------------------- */
        case PROTO_STATE_STREAM_DATA:
            if (byte == PROTO_TAIL0) {
                s_tentative_buf[0] = byte;
                s_tentative_cnt = 1;
                s_state = PROTO_STATE_TENTATIVE_TAIL1;
            } else {
                if (s_stream_idx < PROTO_STREAM_MAX_DATA) {
                    s_stream_buf[s_stream_idx++] = byte;
                }
            }
            break;

        case PROTO_STATE_TENTATIVE_TAIL1:
            if (byte == PROTO_TAIL1) {
                s_tentative_buf[1] = byte;
                s_tentative_cnt = 2;
                s_state = PROTO_STATE_TENTATIVE_TAIL2;
            } else {
                /* TAIL0 was data, flush it and current byte */
                if (s_stream_idx < PROTO_STREAM_MAX_DATA) {
                    s_stream_buf[s_stream_idx++] = s_tentative_buf[0];
                }
                if (s_stream_idx < PROTO_STREAM_MAX_DATA) {
                    s_stream_buf[s_stream_idx++] = byte;
                }
                s_tentative_cnt = 0;
                s_state = PROTO_STATE_STREAM_DATA;
            }
            break;

        case PROTO_STATE_TENTATIVE_TAIL2:
            if (byte == PROTO_TAIL2) {
                s_tentative_buf[2] = byte;
                s_tentative_cnt = 3;
                s_state = PROTO_STATE_TENTATIVE_TAIL3;
            } else {
                /* TAIL0 and TAIL1 were data */
                if (s_stream_idx < PROTO_STREAM_MAX_DATA) {
                    s_stream_buf[s_stream_idx++] = s_tentative_buf[0];
                }
                if (s_stream_idx < PROTO_STREAM_MAX_DATA) {
                    s_stream_buf[s_stream_idx++] = s_tentative_buf[1];
                }
                if (s_stream_idx < PROTO_STREAM_MAX_DATA) {
                    s_stream_buf[s_stream_idx++] = byte;
                }
                s_tentative_cnt = 0;
                s_state = PROTO_STATE_STREAM_DATA;
            }
            break;

        case PROTO_STATE_TENTATIVE_TAIL3:
            if (byte == PROTO_TAIL3) {
                /* Frame complete! Extract FLAGS from first data byte if present */
                uint8_t flags = 0;
                if (s_stream_idx > 0) {
                    flags = s_stream_buf[0];
                }
                uint8_t is_eof = (flags & CLI_FLAG_EOF) ? 1 : 0;
                uint8_t is_sof = (flags & CLI_FLAG_SOF) ? 1 : 0;

                /* For EOF frames, submit and reset. For non-EOF, submit chunk but keep parsing */
                if (is_eof) {
                    SubmitStreamFrame(1, flags);
                    return TRUE;
                } else {
                    /* Non-EOF streaming frame: submit chunk, then expect next frame */
                    SubmitStreamFrame(0, flags);
                    s_state = PROTO_STATE_WAIT_HEAD;
                    return TRUE;
                }
            } else {
                /* TAIL0, TAIL1, TAIL2 were data */
                if (s_stream_idx < PROTO_STREAM_MAX_DATA) {
                    s_stream_buf[s_stream_idx++] = s_tentative_buf[0];
                }
                if (s_stream_idx < PROTO_STREAM_MAX_DATA) {
                    s_stream_buf[s_stream_idx++] = s_tentative_buf[1];
                }
                if (s_stream_idx < PROTO_STREAM_MAX_DATA) {
                    s_stream_buf[s_stream_idx++] = s_tentative_buf[2];
                }
                if (s_stream_idx < PROTO_STREAM_MAX_DATA) {
                    s_stream_buf[s_stream_idx++] = byte;
                }
                s_tentative_cnt = 0;
                s_state = PROTO_STATE_STREAM_DATA;
            }
            break;

        /* ---------------------------------------------------------- */
        default:
            ResetParser();
            break;
    }

    return FALSE;
}

proto_state_t Protocol_GetState(void)
{
    return s_state;
}

uint16_t Protocol_PackFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                            const uint8_t *data, uint8_t data_len,
                            uint8_t *out_buf, uint16_t out_buf_len)
{
    uint16_t total_len = 9 + data_len;
    uint8_t i;

    if (out_buf_len < total_len) {
        return 0;
    }
    if (data_len > PROTO_DATA_MAX_LEN) {
        return 0;
    }

    out_buf[0] = PROTO_HEAD;
    out_buf[1] = src;
    out_buf[2] = dst;
    out_buf[3] = data_len + 1;  /* LEN = CMD + DATA */
    out_buf[4] = cmd;

    for (i = 0; i < data_len; i++) {
        out_buf[5 + i] = data[i];
    }

    out_buf[5 + data_len + 0] = PROTO_TAIL0;
    out_buf[5 + data_len + 1] = PROTO_TAIL1;
    out_buf[5 + data_len + 2] = PROTO_TAIL2;
    out_buf[5 + data_len + 3] = PROTO_TAIL3;

    return total_len;
}

uint16_t Protocol_PackStreamFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                                  const uint8_t *data, uint16_t data_len,
                                  uint8_t *out_buf, uint16_t out_buf_len)
{
    uint16_t total_len = 9 + data_len;
    uint16_t i;

    if (out_buf_len < total_len) {
        return 0;
    }
    if (data_len > PROTO_STREAM_MAX_DATA) {
        return 0;
    }

    out_buf[0] = PROTO_HEAD;
    out_buf[1] = src;
    out_buf[2] = dst;
    out_buf[3] = PROTO_LEN_STREAMING;
    out_buf[4] = cmd;

    for (i = 0; i < data_len; i++) {
        out_buf[5 + i] = data[i];
    }

    out_buf[5 + data_len + 0] = PROTO_TAIL0;
    out_buf[5 + data_len + 1] = PROTO_TAIL1;
    out_buf[5 + data_len + 2] = PROTO_TAIL2;
    out_buf[5 + data_len + 3] = PROTO_TAIL3;

    return total_len;
}

uint16_t Protocol_PackAck(uint8_t src, uint8_t dst,
                          const uint8_t *rsp_data, uint8_t rsp_len,
                          uint8_t *out_buf, uint16_t out_buf_len)
{
    uint16_t total_len = 9 + rsp_len;
    uint8_t i;

    if (out_buf_len < total_len || rsp_len > PROTO_DATA_MAX_LEN - 1) {
        return 0;
    }

    out_buf[0] = PROTO_HEAD;
    out_buf[1] = src;
    out_buf[2] = dst;
    out_buf[3] = rsp_len + 2;  /* LEN = CMD(1) + rsp_data */
    out_buf[4] = CMD_ACK;

    for (i = 0; i < rsp_len; i++) {
        out_buf[5 + i] = rsp_data[i];
    }

    out_buf[5 + rsp_len + 0] = PROTO_TAIL0;
    out_buf[5 + rsp_len + 1] = PROTO_TAIL1;
    out_buf[5 + rsp_len + 2] = PROTO_TAIL2;
    out_buf[5 + rsp_len + 3] = PROTO_TAIL3;

    return total_len;
}

uint16_t Protocol_PackNack(uint8_t src, uint8_t dst, uint8_t err_code,
                           uint8_t *out_buf, uint16_t out_buf_len)
{
    if (out_buf_len < 10) {
        return 0;
    }

    out_buf[0] = PROTO_HEAD;
    out_buf[1] = src;
    out_buf[2] = dst;
    out_buf[3] = 0x02;      /* LEN = CMD(1) + err_code(1) */
    out_buf[4] = CMD_NACK;
    out_buf[5] = err_code;
    out_buf[6] = PROTO_TAIL0;
    out_buf[7] = PROTO_TAIL1;
    out_buf[8] = PROTO_TAIL2;
    out_buf[9] = PROTO_TAIL3;

    return 10;
}

void Protocol_Reset(void)
{
    ResetParser();
}
