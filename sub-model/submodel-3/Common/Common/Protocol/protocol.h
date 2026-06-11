#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"
#include <string.h>

/* Frame delimiters */
#define PROTO_FRAME_HEAD        0xAA

#define PROTO_FRAME_TAIL0       0xA5
#define PROTO_FRAME_TAIL1       0x5A
#define PROTO_FRAME_TAIL2       0xFC
#define PROTO_FRAME_TAIL3       0xFD

/* General operation codes */
#define CMD_NOP                 0x00
#define CMD_GET_TYPE            0x01
#define CMD_GET_STATUS          0x02
#define CMD_SET_CONFIG          0x03
#define CMD_ACK                 0x04
#define CMD_NACK                0x05
#define CMD_EVT_NOTIFY          0x06
#define CMD_DATA_STREAM         0x07

/* Submodel operation codes */
#define CMD_SUB_EVT_NOTIFY      0x40
#define CMD_SUB_SET_MODE        0x41
#define CMD_SUB_GET_STATUS      0x42
#define CMD_SUB_DATA_REPORT     0x43
#define CMD_SUB_SET_CONFIG      0x44
#define CMD_SUB_ACTION_RESULT   0x45
#define CMD_SUB_BULK_TRANSFER   0x46

/* Module IDs */
#define MODULE_ID_CORE          0x00
#define MODULE_ID_SUBMODEL_1    0x40
#define MODULE_ID_SUBMODEL_2    0x41
#define MODULE_ID_SUBMODEL_3    0x42

/* Module type / subtype */
#define MODULE_TYPE_SUBMODEL    0x05
#define MODULE_SUBTYPE_NFC      0x03

/* Error codes */
#define PROTO_ERR_UNSUPPORTED_CMD   0x01
#define PROTO_ERR_INVALID_PARAM     0x02
#define PROTO_ERR_LEN_MISMATCH      0x03
#define PROTO_ERR_BUSY              0x04
#define PROTO_ERR_HW_FAULT          0x05

/* Protocol limits — len 字段 8bit, max=255, 减去 CMD 1 字节 = 254, 留余量用 252 */
#define PROTO_MAX_DATA_LEN      252
#define PROTO_MAX_FRAME_LEN     (5 + PROTO_MAX_DATA_LEN + 4)

/* NFC subcommand IDs */
#define NFC_SUB_CARD_DETECT     0x01
#define NFC_SUB_QUERY_STATUS    0x00

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
    PROTO_STATE_FRAME_READY
} protocol_state_t;

typedef struct {
    uint8_t  head;
    uint8_t  src;
    uint8_t  dst;
    uint8_t  len;
    uint8_t  cmd;
    uint8_t  data[PROTO_MAX_DATA_LEN];
} protocol_frame_t;

typedef struct {
    protocol_state_t state;
    protocol_frame_t frame;       /* ISR 写入缓冲区 */
    protocol_frame_t read_frame;  /* 主循环读取缓冲区（双缓冲） */
    uint16_t data_idx;
    uint8_t  frame_ready;
    uint8_t  frame_consumed;      /* 主循环已处理完当前帧，ISR 可覆盖 read_frame */
} protocol_rx_ctx_t;

void Protocol_InitRxCtx(protocol_rx_ctx_t *ctx);
void Protocol_ResetRxCtx(protocol_rx_ctx_t *ctx);
uint16_t Protocol_PackFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                            const uint8_t *data, uint8_t data_len,
                            uint8_t *out_buf, uint16_t out_size);
uint8_t Protocol_ParseByte(protocol_rx_ctx_t *ctx, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif
