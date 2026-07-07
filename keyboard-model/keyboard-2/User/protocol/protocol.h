#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"
#include <string.h>

#define PROTO_FRAME_HEAD        0xAA

#define PROTO_FRAME_TAIL0       0xA5
#define PROTO_FRAME_TAIL1       0x5A
#define PROTO_FRAME_TAIL2       0xFC
#define PROTO_FRAME_TAIL3       0xFD

#define CMD_NOP                 0x00
#define CMD_GET_TYPE            0x01
#define CMD_GET_STATUS          0x02
#define CMD_SET_CONFIG          0x03
#define CMD_ACK                 0x04
#define CMD_NACK                0x05
#define CMD_EVT_NOTIFY          0x06
#define CMD_DATA_STREAM         0x07

#define CMD_KBD_SET_BACKLIGHT   0x21
#define CMD_KBD_GET_BACKLIGHT   0x22
#define CMD_KBD_SET_CONFIG      0x23
#define CMD_KBD_GET_STATUS      0x24
#define CMD_KBD_HID_REPORT      0x25
#define CMD_KBD_MUSIC_KEYS      0x26
#define CMD_KBD_MUSIC_BUTTONS   0x27
#define CMD_KBD_MUSIC_FADERS    0x28
#define CMD_KBD_GAME_INPUT      0x29
#define CMD_KBD_MUSIC_EVENT_CTRL 0x2A

#define MODULE_ID_CORE          0x00
#define MODULE_ID_KEYBOARD      0x20

#define MODULE_TYPE_KEYBOARD    0x04
#define MODULE_SUBTYPE_KBD_MAIN 0x01
#define MODULE_SUBTYPE_KBD_GAME 0x02
#define MODULE_SUBTYPE_KBD_MUSIC 0x03

#define PROTO_ERR_UNSUPPORTED_CMD   0x01
#define PROTO_ERR_INVALID_PARAM     0x02
#define PROTO_ERR_LEN_MISMATCH      0x03
#define PROTO_ERR_BUSY              0x04
#define PROTO_ERR_HW_FAULT          0x05

#define PROTO_MAX_DATA_LEN      64
#define PROTO_MAX_FRAME_LEN     (5 + PROTO_MAX_DATA_LEN + 4)

/* GAME_INPUT data length (9 bytes DATA + 1 byte CMD) */
#define GAME_INPUT_DATA_LEN     9
#define GAME_INPUT_FRAME_LEN    (1 + GAME_INPUT_DATA_LEN)

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
    protocol_frame_t frame;
    uint16_t data_idx;
    uint8_t  frame_ready;
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

#endif /* __PROTOCOL_H__ */
