#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"
#include <string.h>

/* ============================================================================
 * 协议常量定义
 * ============================================================================ */

/* 帧头 */
#define PROTO_FRAME_HEAD        0xAA

/* 帧尾 */
#define PROTO_FRAME_TAIL0       0xA5
#define PROTO_FRAME_TAIL1       0x5A
#define PROTO_FRAME_TAIL2       0xFC
#define PROTO_FRAME_TAIL3       0xFD

/* 通用操作码 (0x00 ~ 0x0F) */
#define CMD_NOP                 0x00
#define CMD_GET_TYPE            0x01
#define CMD_GET_STATUS          0x02
#define CMD_SET_CONFIG          0x03
#define CMD_ACK                 0x04
#define CMD_NACK                0x05
#define CMD_EVT_NOTIFY          0x06
#define CMD_DATA_STREAM         0x07

/* 模块 ID */
#define MODULE_ID_CORE          0x00
#define MODULE_ID_WIRELESS      0x01
#define MODULE_ID_DISPLAY       0x10
#define MODULE_ID_KEYBOARD      0x20
#define MODULE_ID_POWER         0x30
#define MODULE_ID_SUBMODEL_1    0x40
#define MODULE_ID_SUBMODEL_2    0x41
#define MODULE_ID_SUBMODEL_3    0x42

/* 模块类型编号 (CMD_GET_TYPE 响应 DATA[0]) */
#define MODULE_TYPE_RESERVED    0x00
#define MODULE_TYPE_DISPLAY     0x01
#define MODULE_TYPE_WIRELESS    0x02
#define MODULE_TYPE_POWER       0x03
#define MODULE_TYPE_KEYBOARD    0x04
#define MODULE_TYPE_SUBMODEL    0x05

/* ============================================================================
 * 模块子类型编号 (CMD_GET_TYPE 响应 DATA[1])
 * ----------------------------------------------------------------------------
 * 规则:
 *   - 0x00 : 保留 (RESERVED)
 *   - 0x01 : 该大类的标准/自身实现 (Standard / Self)
 *   - 0x02 ~ 0xFF : 扩展实现
 * ----------------------------------------------------------------------------
 * 模块的完整身份 = 模块类型 (DATA[0]) + 模块子类型 (DATA[1])
 * ============================================================================ */

/* Display 子类型 */
#define MODULE_SUBTYPE_DISPLAY_RESERVED     0x00
#define MODULE_SUBTYPE_DISPLAY_LCD          0x01
#define MODULE_SUBTYPE_DISPLAY_EINK         0x02

/* Wireless 子类型 */
#define MODULE_SUBTYPE_WIRELESS_RESERVED    0x00
#define MODULE_SUBTYPE_WIRELESS_BT          0x01    /* 标准蓝牙/无线模块 (CH585F) */

/* Power 子类型 */
#define MODULE_SUBTYPE_POWER_RESERVED       0x00
#define MODULE_SUBTYPE_POWER_STANDARD       0x01    /* 标准供电模块 */

/* Keyboard 子类型 */
#define MODULE_SUBTYPE_KEYBOARD_RESERVED    0x00
#define MODULE_SUBTYPE_KEYBOARD_MAIN        0x01
#define MODULE_SUBTYPE_KEYBOARD_GAME        0x02
#define MODULE_SUBTYPE_KEYBOARD_MUSIC       0x03

/* Submodel 子类型 */
#define MODULE_SUBTYPE_SUBMODEL_RESERVED    0x00
#define MODULE_SUBTYPE_SUBMODEL_FINGERPRINT 0x01
#define MODULE_SUBTYPE_SUBMODEL_HEALTH      0x02
#define MODULE_SUBTYPE_SUBMODEL_NFC         0x03
#define MODULE_SUBTYPE_SUBMODEL_TOUCH_RING  0x04
#define MODULE_SUBTYPE_SUBMODEL_RGB         0x05
#define MODULE_SUBTYPE_SUBMODEL_INFRARED    0x06
#define MODULE_SUBTYPE_SUBMODEL_SUB_DISPLAY 0x07

/* 长度限制 */
#define PROTO_MAX_DATA_LEN      255
#define PROTO_MAX_FRAME_LEN     264

/* 打包/解析辅助宏 */
#define PROTO_PACK_ERR              0       /* PackFrame 错误返回值 */
#define PROTO_DATA_LEN(frame)       ((frame).len > 0 ? (frame).len - 1 : 0)
#define PROTO_FRAME_TOTAL_LEN(dlen) (5 + (dlen) + 4)   /* HEAD+SRC+DST+LEN+CMD+DATA+TAIL(4) */

/* 各模块建议接收缓冲区大小 */
#define PROTO_BUF_SIZE_DISPLAY  512
#define PROTO_BUF_SIZE_KEYBOARD 64
#define PROTO_BUF_SIZE_POWER    64
#define PROTO_BUF_SIZE_SUBMODEL 256
#define PROTO_BUF_SIZE_WIRELESS 512

/* ============================================================================
 * 数据结构定义
 * ============================================================================ */

/* 接收状态机状态 */
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

/* 协议帧结构（ unpacked 形式，便于访问） */
typedef struct {
    uint8_t head;                       /* 0xAA */
    uint8_t src;                        /* 源模块ID */
    uint8_t dst;                        /* 目标模块ID */
    uint8_t len;                        /* CMD + DATA 总字节数 */
    uint8_t cmd;                        /* 操作码 */
    uint8_t data[PROTO_MAX_DATA_LEN];   /* 数据域 */
} protocol_frame_t;

/* 模块身份（CMD_GET_TYPE 响应 DATA[0..4] 的标准解析形式） */
typedef struct {
    uint8_t type;       /* DATA[0]: 模块类型编号 */
    uint8_t subtype;    /* DATA[1]: 模块子类型编号 */
    uint8_t hw_ver;     /* DATA[2]: 硬件版本号 */
    uint8_t fw_major;   /* DATA[3]: 固件主版本号 */
    uint8_t fw_minor;   /* DATA[4]: 固件次版本号 */
} module_identity_t;

/* 接收上下文 */
typedef struct {
    protocol_state_t state;             /* 当前解析状态 */
    protocol_frame_t frame;             /* 解析中的帧 */
    uint8_t data_idx;                   /* 当前数据域接收索引 */
    uint8_t frame_ready;                /* 帧就绪标志 (1=就绪) */
    /* 错误统计（调试用，Init 时清零，Reset 不清零） */
    uint16_t err_len_zero;              /* LEN=0 非法帧计数 */
    uint16_t err_overflow;              /* DATA 域溢出计数 */
    uint16_t err_frame_ready;           /* 帧未取走又收到新帧计数 */
} protocol_rx_ctx_t;

/* ============================================================================
 * 函数声明
 * ============================================================================ */

/**
 * @brief  初始化接收上下文
 * @param  ctx 接收上下文指针
 * @note   建议在模块初始化时调用
 */
void Protocol_InitRxCtx(protocol_rx_ctx_t *ctx);

/**
 * @brief  重置接收上下文
 * @param  ctx 接收上下文指针
 * @note   在取出就绪帧后调用，以准备接收下一帧
 */
void Protocol_ResetRxCtx(protocol_rx_ctx_t *ctx);

/**
 * @brief  打包协议帧
 * @param  src       源模块ID
 * @param  dst       目标模块ID
 * @param  cmd       操作码
 * @param  data      数据域指针（可为NULL，当data_len=0时）
 * @param  data_len  数据域长度（0 ~ 255）
 * @param  out_buf   输出缓冲区
 * @param  out_size  输出缓冲区大小
 * @return 实际写入的字节数；0表示参数错误或缓冲区不足
 *
 * @note   输出格式: [HEAD][SRC][DST][LEN][CMD][DATA...][TAIL0][TAIL1][TAIL2][TAIL3]
 *         LEN = 1 + data_len（包含CMD自身）
 *         TAIL = A5 5A FC FD
 */
uint8_t Protocol_PackFrame(uint8_t src, uint8_t dst, uint8_t cmd,
                           const uint8_t *data, uint8_t data_len,
                           uint8_t *out_buf, uint16_t out_size);

/**
 * @brief  接收状态机逐字节处理
 * @param  ctx   接收上下文指针
 * @param  byte  新接收到的字节
 * @return 1=帧已就绪（ctx->frame 有效），0=未就绪
 *
 * @note   - 返回1后，必须通过 Protocol_ResetRxCtx() 重置状态机才能继续接收。
 *         - 仅在 WAIT_HEAD 状态下识别 0xAA 为新帧起始，数据域中的 0xAA 不会触发重解析。
 *         - 若 LEN 字段为 0（非法），状态机自动丢弃并回到 WAIT_HEAD。
 *         - 若数据域长度超过 PROTO_MAX_DATA_LEN，超出的字节会被丢弃但不影响帧完成判断。
 */
uint8_t Protocol_ParseByte(protocol_rx_ctx_t *ctx, uint8_t byte);

/**
 * @brief  从协议帧中解析模块身份
 * @param  frame  协议帧指针（需确保 frame->len >= 2）
 * @param  out    输出模块身份结构体
 * @return 1=解析成功，0=帧数据不足
 *
 * @note   仅解析 DATA[0..4]，不检查 frame->cmd 是否为 CMD_GET_TYPE。
 *         调用者应自行确认当前帧是类型响应帧。
 */
uint8_t Protocol_ParseIdentity(const protocol_frame_t *frame, module_identity_t *out);

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H__ */
