#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include "CH58x_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================== */
/*  Frame Delimiters                                                    */
/* ==================================================================== */
#define PROTO_HEAD          0xAA
#define PROTO_TAIL0         0xA5
#define PROTO_TAIL1         0x5A
#define PROTO_TAIL2         0xFC
#define PROTO_TAIL3         0xFD

/* LEN = 0xFF indicates streaming frame: DATA length determined by tail scan */
#define PROTO_LEN_STREAMING 0xFF

/* Standard frame limits */
#define PROTO_FRAME_MAX_LEN     264     /* 1+1+1+1+1+255+4 */
#define PROTO_DATA_MAX_LEN      255

/* Streaming frame max DATA (implementation limit, ~20KB) */
#define PROTO_STREAM_MAX_DATA   20480

/* ==================================================================== */
/*  Module IDs                                                          */
/* ==================================================================== */
#define MODULE_ID_CORE          0x00
#define MODULE_ID_WIRELESS      0x01

/* ==================================================================== */
/*  Generic Opcodes (0x00 ~ 0x0F)                                       */
/* ==================================================================== */
#define CMD_NOP                 0x00
#define CMD_GET_TYPE            0x01
#define CMD_GET_STATUS          0x02
#define CMD_SET_CONFIG          0x03
#define CMD_ACK                 0x04
#define CMD_NACK                0x05
#define CMD_EVT_NOTIFY          0x06
#define CMD_DATA_STREAM         0x07

/* ==================================================================== */
/*  Wireless Opcodes (0x51 ~ 0x5F)                                      */
/* ==================================================================== */
#define CMD_BT_GET_STATUS       0x51
#define CMD_BT_SEND_DATA        0x52
#define CMD_BT_CONN_EVT         0x54
#define CMD_BT_RECV_DATA        0x55
#define CMD_BT_HID_REPORT       0x56
#define CMD_BT_SET_DISCOVERABLE 0x58
#define CMD_BT_RESET            0x59

/* ==================================================================== */
/*  Extended Opcodes (CMD = 0x50, DATA[0] = sub-cmd)                    */
/* ==================================================================== */
#define CMD_BT_EXT_BASE         0x50
#define CMD_BT_EXT_SCAN         0x01
#define CMD_BT_EXT_DEVICE_LIST  0x02
#define CMD_BT_EXT_CONNECT      0x03
#define CMD_BT_EXT_PAIRING_MGMT 0x04
#define CMD_BT_EXT_SET_MODE     0x05
#define CMD_BT_EXT_CLI_DATA     0x07

/* ==================================================================== */
/*  NACK Error Codes                                                    */
/* ==================================================================== */
#define PROTO_ERR_NONE                  0x00
#define PROTO_ERR_UNSUPPORTED_CMD       0x01
#define PROTO_ERR_INVALID_PARAM         0x02
#define PROTO_ERR_LEN_MISMATCH          0x03
#define PROTO_ERR_BUSY                  0x04
#define PROTO_ERR_HW_FAULT              0x05
#define PROTO_ERR_TIMEOUT               0x06

/* ==================================================================== */
/*  CLI Data Flags                                                      */
/* ==================================================================== */
#define CLI_FLAG_SOF                    0x01
#define CLI_FLAG_EOF                    0x02

/* ==================================================================== */
/*  Frame Structure                                                     */
/* ==================================================================== */
typedef struct {
    uint8_t head;
    uint8_t src;
    uint8_t dst;
    uint8_t len;
    uint8_t cmd;
    uint8_t data[PROTO_DATA_MAX_LEN];   /* For standard frames */
    uint8_t tail[4];
} proto_frame_t;

/* Streaming frame: data pointer + length (data stored externally) */
typedef struct {
    uint8_t head;
    uint8_t src;
    uint8_t dst;
    uint8_t len;        /* Always 0xFF */
    uint8_t cmd;
    uint8_t *data;      /* Pointer to external buffer */
    uint16_t data_len;  /* Actual data length */
    uint8_t tail[4];
} proto_stream_frame_t;

/* ==================================================================== */
/*  Parse State Machine                                                 */
/* ==================================================================== */
typedef enum {
    PROTO_STATE_WAIT_HEAD = 0,
    PROTO_STATE_WAIT_SRC,
    PROTO_STATE_WAIT_DST,
    PROTO_STATE_WAIT_LEN,
    PROTO_STATE_WAIT_CMD,
    PROTO_STATE_WAIT_DATA,
    PROTO_STATE_WAIT_TAIL0,
    PROTO_STATE_WAIT_TAIL1,
    PROTO_STATE_WAIT_TAIL2,
    PROTO_STATE_WAIT_TAIL3,
    /* Streaming states (tail-terminated) */
    PROTO_STATE_STREAM_DATA,
    PROTO_STATE_TENTATIVE_TAIL1,
    PROTO_STATE_TENTATIVE_TAIL2,
    PROTO_STATE_TENTATIVE_TAIL3,
    PROTO_STATE_FRAME_READY
} proto_state_t;

/* ==================================================================== */
/*  Callbacks                                                           */
/* ==================================================================== */

/* Standard frame completed callback */
typedef void (*proto_std_frame_cb_t)(const proto_frame_t *frame);

/* Streaming frame data chunk callback (called incrementally) */
typedef void (*proto_stream_chunk_cb_t)(const uint8_t *chunk, uint16_t chunk_len,
                                         uint8_t is_final, uint8_t flags);

/* ==================================================================== */
/*  API Functions                                                       */
/* ==================================================================== */

void Protocol_Init(void);

/* Register callbacks for frame processing */
void Protocol_RegisterCallbacks(proto_std_frame_cb_t std_cb,
                                proto_stream_chunk_cb_t stream_cb);

/* Feed one byte to the parser. Returns TRUE when a frame is ready. */
bool Protocol_ParseByte(uint8_t byte);

/* Get the current parser state */
proto_state_t Protocol_GetState(void);

/* Pack a standard frame into output buffer. Returns packed length. */
uint16_t Protocol_PackFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                            const uint8_t *data, uint8_t data_len,
                            uint8_t *out_buf, uint16_t out_buf_len);

/* Pack a streaming frame (LEN=0xFF) into output buffer.
 * The caller must ensure out_buf is large enough (9 + data_len).
 * Returns packed length. */
uint16_t Protocol_PackStreamFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                                  const uint8_t *data, uint16_t data_len,
                                  uint8_t *out_buf, uint16_t out_buf_len);

/* Convenience: pack ACK frame */
uint16_t Protocol_PackAck(uint8_t src, uint8_t dst,
                          const uint8_t *rsp_data, uint8_t rsp_len,
                          uint8_t *out_buf, uint16_t out_buf_len);

/* Convenience: pack NACK frame */
uint16_t Protocol_PackNack(uint8_t src, uint8_t dst, uint8_t err_code,
                           uint8_t *out_buf, uint16_t out_buf_len);

/* Reset parser state machine */
void Protocol_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H */
