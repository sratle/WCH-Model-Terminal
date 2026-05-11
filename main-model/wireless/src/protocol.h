/********************************** (C) COPYRIGHT *******************************
 * File Name          : protocol.h
 * Description        : Unified compact binary protocol frame pack / parse
 *******************************************************************************/

#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "CH58x_common.h"

#define PROTO_HEAD          0xAA
#define PROTO_TAIL0         0xA5
#define PROTO_TAIL1         0x5A
#define PROTO_TAIL2         0xFC
#define PROTO_TAIL3         0xFD

#define PROTO_FRAME_MAX_LEN 260
#define PROTO_DATA_MAX_LEN  255

/* Module IDs */
#define MODULE_ID_CORE          0x00
#define MODULE_ID_WIRELESS      0x01

/* Generic opcodes */
#define CMD_NOP                 0x00
#define CMD_GET_TYPE            0x01
#define CMD_GET_STATUS          0x02
#define CMD_SET_CONFIG          0x03
#define CMD_ACK                 0x04
#define CMD_NACK                0x05
#define CMD_EVT_NOTIFY          0x06
#define CMD_DATA_STREAM         0x07

/* Wireless specific opcodes */
#define CMD_BT_GET_STATUS       0x51
#define CMD_BT_SEND_DATA        0x52
#define CMD_BT_CONN_EVT         0x54
#define CMD_BT_RECV_DATA        0x55
#define CMD_BT_HID_REPORT       0x56
#define CMD_BT_SET_DISCOVERABLE 0x58
#define CMD_BT_RESET            0x59

/* Extended opcodes (base 0x50 + sub-cmd) */
#define CMD_BT_EXT_BASE         0x50
#define CMD_BT_EXT_SCAN         0x01
#define CMD_BT_EXT_DEVICE_LIST  0x02
#define CMD_BT_EXT_CONNECT      0x03
#define CMD_BT_EXT_PAIRING_MGMT 0x04
#define CMD_BT_EXT_SET_MODE     0x05
#define CMD_BT_EXT_CLI_DATA     0x07

/* NACK error codes */
#define PROTO_ERR_NONE                  0x00
#define PROTO_ERR_UNSUPPORTED_CMD       0x01
#define PROTO_ERR_INVALID_PARAM         0x02
#define PROTO_ERR_LEN_MISMATCH          0x03
#define PROTO_ERR_BUSY                  0x04
#define PROTO_ERR_HW_FAULT              0x05
#define PROTO_ERR_TIMEOUT               0x06

/* Parse state machine */
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
} proto_state_t;

typedef struct {
    uint8_t  head;
    uint8_t  src;
    uint8_t  dst;
    uint8_t  len;
    uint8_t  cmd;
    uint8_t  data[PROTO_DATA_MAX_LEN];
    uint8_t  tail[4];
    uint8_t  data_idx;
} proto_frame_t;

typedef void (*proto_frame_cb_t)(const proto_frame_t *frame);

void Protocol_Init(void);
bool Protocol_ParseByte(uint8_t byte, proto_frame_t *frame);
uint16_t Protocol_PackFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                            const uint8_t *data, uint8_t data_len,
                            uint8_t *out_buf, uint16_t out_buf_len);
uint16_t Protocol_PackAck(uint8_t src, uint8_t dst,
                          const uint8_t *rsp_data, uint8_t rsp_len,
                          uint8_t *out_buf, uint16_t out_buf_len);
uint16_t Protocol_PackNack(uint8_t src, uint8_t dst, uint8_t err_code,
                           uint8_t *out_buf, uint16_t out_buf_len);

#endif /* __PROTOCOL_H__ */
