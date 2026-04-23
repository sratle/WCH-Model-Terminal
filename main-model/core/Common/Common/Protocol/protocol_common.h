/********************************** (C) COPYRIGHT *******************************
 * File Name          : protocol_common.h
 * Author             : Core Team
 * Version            : V1.0.0
 * Date               : 2026/04/23
 * Description        : Common opcode helpers and dispatcher for unified protocol.
 *******************************************************************************/
#ifndef __PROTOCOL_COMMON_H__
#define __PROTOCOL_COMMON_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "protocol.h"

/* ============================================================================
 * 通用 NACK 错误码
 * ============================================================================ */
#define PROTO_ERR_NONE              0x00
#define PROTO_ERR_UNSUPPORTED_CMD   0x01    /* 不支持的命令 */
#define PROTO_ERR_INVALID_PARAM     0x02    /* 参数错误 */
#define PROTO_ERR_LEN_MISMATCH      0x03    /* 长度不匹配 */
#define PROTO_ERR_BUSY              0x04    /* 模块忙 */
#define PROTO_ERR_HW_FAULT          0x05    /* 硬件故障 */
#define PROTO_ERR_TIMEOUT           0x06    /* 超时 */
#define PROTO_ERR_USER_BASE         0x80    /* 用户自定义错误码起始 */

/* ============================================================================
 * 通用命令回调接口
 * ----------------------------------------------------------------------------
 * 各模块注册自己关心的回调，不支持的命令设为 NULL。
 * 回调返回 1 表示已处理，返回 0 表示拒绝处理（通用层将回复 NACK）。
 * ============================================================================ */
typedef struct {
    /**
     * @brief CMD_GET_TYPE 处理
     * @param req        请求帧
     * @param resp_buf   响应数据缓冲区（仅 DATA 域，不含帧头）
     * @param resp_size  resp_buf 大小
     * @param resp_len   实际写入 resp_buf 的字节数
     * @return 1=已处理并生成 resp_data，0=拒绝
     */
    uint8_t (*on_get_type)(const protocol_frame_t *req,
                           uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);

    uint8_t (*on_get_status)(const protocol_frame_t *req,
                             uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);

    uint8_t (*on_set_config)(const protocol_frame_t *req,
                             uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);

    void (*on_nop)(const protocol_frame_t *req);
    void (*on_evt_notify)(const protocol_frame_t *req);
    void (*on_data_stream)(const protocol_frame_t *req);
} protocol_common_cb_t;

/* ============================================================================
 * 辅助打包函数
 * ============================================================================ */

/**
 * @brief  快速构建空 ACK 帧
 * @return 实际写入的字节数；PROTO_PACK_ERR 表示错误
 */
uint8_t ProtocolCommon_Ack(uint8_t src, uint8_t dst,
                           uint8_t *out_buf, uint16_t out_size);

/**
 * @brief  快速构建 NACK 帧，携带错误码
 * @param  err_code  错误码，见 PROTO_ERR_* 定义
 * @return 实际写入的字节数；PROTO_PACK_ERR 表示错误
 */
uint8_t ProtocolCommon_Nack(uint8_t src, uint8_t dst, uint8_t err_code,
                            uint8_t *out_buf, uint16_t out_size);

/**
 * @brief  构建 CMD_GET_TYPE 标准响应（仅含 identity，无扩展信息）
 * @param  id  模块身份信息
 * @return 实际写入的字节数；PROTO_PACK_ERR 表示错误
 */
uint8_t ProtocolCommon_ReplyType(uint8_t src, uint8_t dst,
                                 const module_identity_t *id,
                                 uint8_t *out_buf, uint16_t out_size);

/**
 * @brief  构建 CMD_GET_TYPE 响应（含扩展信息）
 * @param  ext_data  扩展数据指针（可为 NULL，当 ext_len=0 时）
 * @param  ext_len   扩展数据长度（0 ~ 250，因 identity 固定占 5 字节）
 * @return 实际写入的字节数；PROTO_PACK_ERR 表示错误
 */
uint8_t ProtocolCommon_ReplyTypeExt(uint8_t src, uint8_t dst,
                                    const module_identity_t *id,
                                    const uint8_t *ext_data, uint8_t ext_len,
                                    uint8_t *out_buf, uint16_t out_size);

/* ============================================================================
 * 通用命令分发器
 * ============================================================================ */

/**
 * @brief  分发通用操作码（0x00~0x0F）
 * @param  req          请求帧
 * @param  cb           回调接口（可部分为 NULL）
 * @param  resp_buf     响应帧输出缓冲区
 * @param  resp_size    resp_buf 大小（建议 >= PROTO_MAX_FRAME_LEN）
 * @param  resp_len     实际写入 resp_buf 的字节数
 * @return 1=通用层已处理（resp_buf 中已生成完整响应帧）
 *         0=该帧不是通用命令，或通用层未处理，需交由模块专用逻辑
 *
 * @note   - 对于 CMD_ACK / CMD_NACK，直接返回 0（它们是响应，不是请求）。
 *         - 对于 CMD_GET_TYPE / CMD_GET_STATUS / CMD_SET_CONFIG：
 *           若回调存在且返回 1，自动打包 ACK 并附加 resp_buf 中的数据；
 *           若回调为 NULL 或返回 0，自动回复 NACK(ERR_UNSUPPORTED_CMD)。
 *         - 对于 CMD_NOP / CMD_EVT_NOTIFY / CMD_DATA_STREAM：调用回调（若存在），不生成响应。
 *         - resp_buf 会被复用：先作为回调的 DATA 输出缓冲区，再作为 PackFrame 的输出缓冲区。
 *           因内部使用 memmove，允许源数据与输出缓冲区重叠，但 resp_size 必须足够容纳
 *           完整响应帧（5 字节头 + resp_data_len）。
 */
uint8_t ProtocolCommon_Dispatch(const protocol_frame_t *req,
                                const protocol_common_cb_t *cb,
                                uint8_t *resp_buf, uint16_t resp_size,
                                uint8_t *resp_len);

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_COMMON_H__ */
