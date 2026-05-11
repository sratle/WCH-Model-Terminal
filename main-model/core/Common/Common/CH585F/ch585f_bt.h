#ifndef __CH585F_BT_H__
#define __CH585F_BT_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"
#include "Protocol/protocol.h"

#define CH585F_BT_POLL_SIZE         32
#define CH585F_BT_CLI_CAPTURE_SIZE  512

/* CLI 输出捕获（供 debug.c _write 使用） */
extern uint8_t cli_capture_buf[CH585F_BT_CLI_CAPTURE_SIZE];
extern uint16_t cli_capture_len;
extern uint8_t cli_capture_flag;

typedef struct
{
    protocol_rx_ctx_t rx_ctx;
    uint8_t app_connected;
    uint8_t app_mac[6];
    uint8_t app_name[16];
} ch585f_bt_ctx_t;

extern ch585f_bt_ctx_t ch585f_bt_g;

void CH585F_BT_Init(void);
void CH585F_BT_Poll(void);
void CH585F_BT_SendCliData(uint8_t *data, uint8_t len);

void CLI_Capture_Start(void);
void CLI_Capture_Stop(void);
uint8_t* CLI_Capture_Flush(uint16_t *out_len);

#ifdef __cplusplus
}
#endif

#endif
