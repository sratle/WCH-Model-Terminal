#ifndef __CH585F_BT_H__
#define __CH585F_BT_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"
#include "Protocol/protocol.h"

#define CH585F_BT_POLL_SIZE         128
#define CH585F_BT_CLI_CAPTURE_SIZE  32768
#define CH585F_BT_CLI_ASSEMBLE_SIZE 512

/* 标准帧单帧最大 payload
 * 约束：Protocol_PackFrame 的 len_field = 1 + data_len，必须 < 0xFF（流式帧标识）
 * data_len = chunk + 2（ext_cmd + FLAGS），故 chunk + 3 < 255 → chunk ≤ 251 */
#define CH585F_BT_STD_MAX_PAYLOAD   251

/* CLI_DATA FLAGS */
#define CLI_FLAG_SOF                0x01    /* bit0: Start of Frame */
#define CLI_FLAG_EOF                0x02    /* bit1: End of Frame   */

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
void CH585F_BT_SendCliData(uint8_t *data, uint16_t len);

/* 主动向 Display 推送无线在线/连接状态与最近 BT 流量 */
void CH585F_BT_PushStatus(void);
void CH585F_BT_PushTraffic(void);
/* 供 Display 进页拉取一次：重发在线/连接 + 流量 */
void CH585F_BT_ReportStatusToDisplay(void);

void CLI_Capture_Start(void);
void CLI_Capture_Stop(void);
uint8_t* CLI_Capture_Flush(uint16_t *out_len);

#ifdef __cplusplus
}
#endif

#endif
