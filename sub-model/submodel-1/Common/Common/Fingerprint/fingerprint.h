#ifndef __FINGERPRINT_H__
#define __FINGERPRINT_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"

/* Syno protocol constants */
#define SYNO_HEADER_HIGH       0xEF
#define SYNO_HEADER_LOW        0x01
#define SYNO_DEFAULT_ADDR      0xFFFFFFFF

#define SYNO_PKG_CMD           0x01
#define SYNO_PKG_DATA          0x02
#define SYNO_PKG_END           0x08

#define SYNO_CALC_SUM_START    6
#define SYNO_CMD_CODE_POS      9
#define SYNO_VARIABLE_START    10

#define SYNO_MAX_BUF           256

/* Syno confirmation codes */
#define SYNO_ACK_OK                    0x00
#define SYNO_ACK_COMM_ERR             0x01
#define SYNO_ACK_NO_FINGER            0x02
#define SYNO_ACK_GET_IMG_ERR          0x03
#define SYNO_ACK_NOT_MATCH            0x08
#define SYNO_ACK_NOT_SEARCHED         0x09
#define SYNO_ACK_ENROLL_ERR           0x1E
#define SYNO_ACK_LIB_FULL             0x1F
#define SYNO_ACK_FP_DUPLICATION       0x27
#define SYNO_ACK_ENROLL_CANCEL        0x2C

/* Syno command codes */
#define SYNO_CMD_AUTO_CANCEL          0x30
#define SYNO_CMD_AUTO_ENROLL          0x31
#define SYNO_CMD_AUTO_MATCH           0x32
#define SYNO_CMD_INTO_SLEEP           0x33
#define SYNO_CMD_HANDSHAKE            0x35
#define SYNO_CMD_RGB_CTRL             0x3C
#define SYNO_CMD_DEL_TEMPLATE         0x0C
#define SYNO_CMD_EMPTY_TEMPLATE       0x0D
#define SYNO_CMD_READ_SYS_PARA        0x0F
#define SYNO_CMD_WRITE_REG            0x0E
#define SYNO_CMD_VALID_TEMPLATE_NUM   0x1D
#define SYNO_CMD_READ_INDEX_TABLE     0x1F

/* LED function codes (matches Syno protocol) */
#define SYNO_LED_BREATH               0x01
#define SYNO_LED_FLICKER              0x02
#define SYNO_LED_ON                   0x03
#define SYNO_LED_OFF                  0x04
#define SYNO_LED_GRADUAL_ON           0x05
#define SYNO_LED_GRADUAL_OFF          0x06
#define SYNO_LED_HORSE                0x07

/* LED color codes (matches Syno protocol) */
#define SYNO_COLOR_OFF                0x00
#define SYNO_COLOR_BLUE              0x01
#define SYNO_COLOR_GREEN             0x02
#define SYNO_COLOR_CYAN              0x03
#define SYNO_COLOR_RED               0x04
#define SYNO_COLOR_MAGENTA           0x05
#define SYNO_COLOR_YELLOW            0x06
#define SYNO_COLOR_WHITE             0x07

/* Fingerprint module states */
typedef enum {
    FP_STATE_IDLE = 0,
    FP_STATE_ENROLLING,
    FP_STATE_IDENTIFYING,
    FP_STATE_SLEEPING
} fp_state_t;

/* Syno receive FSM states */
typedef enum {
    SYNO_RCV_FIRST_HEAD = 0,
    SYNO_RCV_SECOND_HEAD,
    SYNO_RCV_PKG_SIZE,
    SYNO_RCV_DATAS
} syno_rcv_state_t;

/* Syno receive context */
typedef struct {
    syno_rcv_state_t state;
    uint8_t  buf[SYNO_MAX_BUF];
    uint16_t buf_idx;
    uint16_t pkg_dlen;
    uint8_t  frame_ready;
} syno_rx_ctx_t;

/* Fingerprint module context */
typedef struct {
    fp_state_t     state;
    syno_rx_ctx_t  rx_ctx;
    uint8_t        last_cmd;
    uint8_t        last_ack;
    uint16_t       last_page_id;
    uint16_t       last_match_score;
    uint16_t       template_count;
    uint8_t        enroll_id;
    uint8_t        led_cmd_cached;
    uint8_t        led_cached_func;
    uint8_t        led_cached_color;
    uint8_t        led_cached_speed;
} fp_ctx_t;

void Fp_Init(void);
void Fp_ProcessRxData(const uint8_t *data, uint16_t len);

uint8_t Fp_HandShake(void);
uint8_t Fp_AutoEnroll(uint16_t page_id, uint8_t count);
uint8_t Fp_AutoIdentify(uint8_t security_level);
uint8_t Fp_Cancel(void);
uint8_t Fp_DeleteTemplate(uint16_t page_id, uint16_t count);
uint8_t Fp_EmptyTemplate(void);
uint8_t Fp_ReadValidTemplateNum(void);
uint8_t Fp_ReadIndexTable(uint8_t index);
uint8_t Fp_Sleep(void);
uint8_t Fp_ControlLED(uint8_t func, uint8_t color, uint8_t speed);
uint8_t Fp_WriteReg(uint8_t reg_num, uint16_t value);

uint8_t Fp_IsFrameReady(void);
void Fp_ResetRx(void);
void Fp_HandleResponse(void);

extern fp_ctx_t fp_ctx;

#ifdef __cplusplus
}
#endif

#endif
