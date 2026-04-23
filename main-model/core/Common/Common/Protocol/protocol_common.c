/********************************** (C) COPYRIGHT *******************************
 * File Name          : protocol_common.c
 * Author             : Core Team
 * Version            : V1.0.0
 * Date               : 2026/04/23
 * Description        : Common opcode helpers and dispatcher implementation.
 *******************************************************************************/

#include "protocol_common.h"

/*********************************************************************
 * @fn      ProtocolCommon_Ack
 *
 * @brief   Pack an empty ACK frame.
 *
 * @return  Bytes written; PROTO_PACK_ERR on error.
 *********************************************************************/
uint8_t ProtocolCommon_Ack(uint8_t src, uint8_t dst,
                           uint8_t *out_buf, uint16_t out_size)
{
    return Protocol_PackFrame(src, dst, CMD_ACK, NULL, 0, out_buf, out_size);
}

/*********************************************************************
 * @fn      ProtocolCommon_Nack
 *
 * @brief   Pack a NACK frame with an error code.
 *
 * @return  Bytes written; PROTO_PACK_ERR on error.
 *********************************************************************/
uint8_t ProtocolCommon_Nack(uint8_t src, uint8_t dst, uint8_t err_code,
                            uint8_t *out_buf, uint16_t out_size)
{
    return Protocol_PackFrame(src, dst, CMD_NACK, &err_code, 1, out_buf, out_size);
}

/*********************************************************************
 * @fn      ProtocolCommon_ReplyType
 *
 * @brief   Pack a standard CMD_GET_TYPE response (identity only).
 *
 * @return  Bytes written; PROTO_PACK_ERR on error.
 *********************************************************************/
uint8_t ProtocolCommon_ReplyType(uint8_t src, uint8_t dst,
                                 const module_identity_t *id,
                                 uint8_t *out_buf, uint16_t out_size)
{
    return ProtocolCommon_ReplyTypeExt(src, dst, id, NULL, 0, out_buf, out_size);
}

/*********************************************************************
 * @fn      ProtocolCommon_ReplyTypeExt
 *
 * @brief   Pack a CMD_GET_TYPE response with optional extension data.
 *
 * @return  Bytes written; PROTO_PACK_ERR on error.
 *********************************************************************/
uint8_t ProtocolCommon_ReplyTypeExt(uint8_t src, uint8_t dst,
                                    const module_identity_t *id,
                                    const uint8_t *ext_data, uint8_t ext_len,
                                    uint8_t *out_buf, uint16_t out_size)
{
    uint8_t data[PROTO_MAX_DATA_LEN];
    uint8_t total_len;

    if (id == NULL || out_buf == NULL || out_size == 0)
        return PROTO_PACK_ERR;

    if (ext_len > (PROTO_MAX_DATA_LEN - 5))
        return PROTO_PACK_ERR;

    data[0] = id->type;
    data[1] = id->subtype;
    data[2] = id->hw_ver;
    data[3] = id->fw_major;
    data[4] = id->fw_minor;

    total_len = 5 + ext_len;
    if (ext_len > 0 && ext_data != NULL)
    {
        memcpy(&data[5], ext_data, ext_len);
    }

    return Protocol_PackFrame(src, dst, CMD_ACK, data, total_len, out_buf, out_size);
}

/*********************************************************************
 * @fn      ProtocolCommon_Dispatch
 *
 * @brief   Dispatch common opcodes (0x00~0x0F).
 *
 * @return  1 if handled and response packed into resp_buf.
 *          0 if not a common command or not handled.
 *********************************************************************/
uint8_t ProtocolCommon_Dispatch(const protocol_frame_t *req,
                                const protocol_common_cb_t *cb,
                                uint8_t *resp_buf, uint16_t resp_size,
                                uint8_t *resp_len)
{
    uint8_t cb_ret;
    uint8_t cmd;

    if (req == NULL || resp_buf == NULL || resp_len == NULL)
        return 0;

    cmd = req->cmd;

    /* CMD_ACK / CMD_NACK are responses, not requests */
    if (cmd == CMD_ACK || cmd == CMD_NACK)
        return 0;

    /* Only handle 0x00 ~ 0x0F common opcodes */
    if (cmd > CMD_DATA_STREAM)
        return 0;

    switch (cmd)
    {
        case CMD_NOP:
        {
            if (cb != NULL && cb->on_nop != NULL)
                cb->on_nop(req);
            /* NOP does not require a response */
            return 0;
        }

        case CMD_GET_TYPE:
        {
            if (cb != NULL && cb->on_get_type != NULL)
            {
                *resp_len = 0;
                cb_ret = cb->on_get_type(req, resp_buf, resp_size, resp_len);
                if (cb_ret && *resp_len > 0)
                {
                    return Protocol_PackFrame(req->dst, req->src, CMD_ACK,
                                              resp_buf, *resp_len,
                                              resp_buf, resp_size);
                }
                else if (cb_ret)
                {
                    return ProtocolCommon_Ack(req->dst, req->src, resp_buf, resp_size);
                }
            }
            return ProtocolCommon_Nack(req->dst, req->src,
                                       PROTO_ERR_UNSUPPORTED_CMD,
                                       resp_buf, resp_size);
        }

        case CMD_GET_STATUS:
        {
            if (cb != NULL && cb->on_get_status != NULL)
            {
                *resp_len = 0;
                cb_ret = cb->on_get_status(req, resp_buf, resp_size, resp_len);
                if (cb_ret && *resp_len > 0)
                {
                    return Protocol_PackFrame(req->dst, req->src, CMD_ACK,
                                              resp_buf, *resp_len,
                                              resp_buf, resp_size);
                }
                else if (cb_ret)
                {
                    return ProtocolCommon_Ack(req->dst, req->src, resp_buf, resp_size);
                }
            }
            return ProtocolCommon_Nack(req->dst, req->src,
                                       PROTO_ERR_UNSUPPORTED_CMD,
                                       resp_buf, resp_size);
        }

        case CMD_SET_CONFIG:
        {
            if (cb != NULL && cb->on_set_config != NULL)
            {
                *resp_len = 0;
                cb_ret = cb->on_set_config(req, resp_buf, resp_size, resp_len);
                if (cb_ret && *resp_len > 0)
                {
                    return Protocol_PackFrame(req->dst, req->src, CMD_ACK,
                                              resp_buf, *resp_len,
                                              resp_buf, resp_size);
                }
                else if (cb_ret)
                {
                    return ProtocolCommon_Ack(req->dst, req->src, resp_buf, resp_size);
                }
            }
            return ProtocolCommon_Nack(req->dst, req->src,
                                       PROTO_ERR_UNSUPPORTED_CMD,
                                       resp_buf, resp_size);
        }

        case CMD_EVT_NOTIFY:
        {
            if (cb != NULL && cb->on_evt_notify != NULL)
                cb->on_evt_notify(req);
            return 0;
        }

        case CMD_DATA_STREAM:
        {
            if (cb != NULL && cb->on_data_stream != NULL)
                cb->on_data_stream(req);
            return 0;
        }

        default:
            return 0;
    }
}
