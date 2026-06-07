#include "fingerprint.h"
#include "../Uart/uart_fp.h"
#include <string.h>

fp_ctx_t fp_ctx;

static uint16_t Fp_CalcSum(const uint8_t *data, uint16_t size)
{
    uint16_t sum = 0;
    uint16_t i;

    for (i = 0; i < size; i++)
    {
        sum += data[i];
    }

    return sum;
}

static void Fp_SendPacket(const uint8_t *data, uint16_t len)
{
    UartFp_SendData(data, len);
}

static uint16_t Fp_BuildCmdPacket(uint8_t *buf, uint8_t cmd_code,
                                   const uint8_t *params, uint8_t param_len)
{
    uint16_t data_size;
    uint16_t total_len;
    uint16_t sum;

    data_size = 3 + param_len;
    total_len = 11 + param_len + 2;

    buf[0] = SYNO_HEADER_HIGH;
    buf[1] = SYNO_HEADER_LOW;
    buf[2] = (SYNO_DEFAULT_ADDR >> 24) & 0xFF;
    buf[3] = (SYNO_DEFAULT_ADDR >> 16) & 0xFF;
    buf[4] = (SYNO_DEFAULT_ADDR >> 8) & 0xFF;
    buf[5] = SYNO_DEFAULT_ADDR & 0xFF;
    buf[6] = SYNO_PKG_CMD;
    buf[7] = (data_size >> 8) & 0xFF;
    buf[8] = data_size & 0xFF;
    buf[9] = cmd_code;

    if (param_len > 0 && params != NULL)
    {
        memcpy(&buf[10], params, param_len);
    }

    sum = Fp_CalcSum(&buf[SYNO_CALC_SUM_START], 4 + param_len);
    buf[10 + param_len] = (sum >> 8) & 0xFF;
    buf[10 + param_len + 1] = sum & 0xFF;

    return total_len;
}

void Fp_Init(void)
{
    memset(&fp_ctx, 0, sizeof(fp_ctx_t));
    fp_ctx.state = FP_STATE_IDLE;
    Fp_ResetRx();
}

void Fp_ResetRx(void)
{
    fp_ctx.rx_ctx.state = SYNO_RCV_FIRST_HEAD;
    fp_ctx.rx_ctx.buf_idx = 0;
    fp_ctx.rx_ctx.pkg_dlen = 0;
    fp_ctx.rx_ctx.frame_ready = 0;
}

void Fp_ProcessRxData(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint8_t byte;
    syno_rx_ctx_t *rx = &fp_ctx.rx_ctx;

    for (i = 0; i < len; i++)
    {
        byte = data[i];

        switch (rx->state)
        {
            case SYNO_RCV_FIRST_HEAD:
            {
                if (byte == SYNO_HEADER_HIGH)
                {
                    rx->buf_idx = 0;
                    rx->buf[rx->buf_idx++] = byte;
                    rx->state = SYNO_RCV_SECOND_HEAD;
                }
                break;
            }

            case SYNO_RCV_SECOND_HEAD:
            {
                if (byte == SYNO_HEADER_LOW)
                {
                    rx->buf[rx->buf_idx++] = byte;
                    rx->state = SYNO_RCV_PKG_SIZE;
                }
                else
                {
                    rx->buf_idx = 0;
                    rx->state = SYNO_RCV_FIRST_HEAD;
                }
                break;
            }

            case SYNO_RCV_PKG_SIZE:
            {
                rx->buf[rx->buf_idx++] = byte;
                if (rx->buf_idx >= 9)
                {
                    rx->pkg_dlen = ((uint16_t)rx->buf[7] << 8) + rx->buf[8];
                    rx->pkg_type = rx->buf[6];  /* 记录包类型: CMD(0x01) 或 DATA(0x02) */
                    rx->state = SYNO_RCV_DATAS;
                }
                break;
            }

            case SYNO_RCV_DATAS:
            {
                if (rx->buf_idx < SYNO_MAX_BUF)
                {
                    rx->buf[rx->buf_idx++] = byte;
                }

                if (rx->buf_idx >= (9 + rx->pkg_dlen))
                {
                    uint16_t calc_sum = Fp_CalcSum(&rx->buf[SYNO_CALC_SUM_START],
                                                    rx->buf_idx - 8);
                    uint16_t recv_sum = ((uint16_t)rx->buf[rx->buf_idx - 2] << 8)
                                      + rx->buf[rx->buf_idx - 1];

                    if (calc_sum == recv_sum)
                    {
                        rx->frame_ready = 1;
                        rx->state = SYNO_RCV_FIRST_HEAD;
                        return;
                    }

                    Fp_ResetRx();
                }
                break;
            }

            default:
            {
                Fp_ResetRx();
                break;
            }
        }

        if (rx->buf_idx >= SYNO_MAX_BUF)
        {
            Fp_ResetRx();
        }
    }
}

uint8_t Fp_IsFrameReady(void)
{
    return fp_ctx.rx_ctx.frame_ready;
}

void Fp_HandleResponse(void)
{
    syno_rx_ctx_t *rx = &fp_ctx.rx_ctx;
    uint8_t ack_code;
    uint8_t cmd_code;

    if (!rx->frame_ready)
        return;

    /* DATA 包 (0x02): 注册进度等中间数据 */
    if (rx->pkg_type == SYNO_PKG_DATA)
    {
        if (fp_ctx.last_cmd == SYNO_CMD_AUTO_ENROLL && fp_ctx.state == FP_STATE_ENROLLING)
        {
            /* Syno AutoEnroll DATA 包格式:
             * buf[9]  = 当前步骤 (已成功采集次数)
             * buf[10] = 总步骤 (需要采集次数) */
            if (rx->pkg_dlen >= 3)
            {
                fp_ctx.enroll_progress = rx->buf[9];
                fp_ctx.enroll_total = rx->buf[10];
                fp_ctx.enroll_progress_ready = 1;
            }
        }
        Fp_ResetRx();
        return;
    }

    /* CMD 包 (0x01): 命令响应 */
    cmd_code = fp_ctx.last_cmd;
    ack_code = rx->buf[9];

    switch (cmd_code)
    {
        case SYNO_CMD_HANDSHAKE:
        {
            break;
        }

        case SYNO_CMD_AUTO_ENROLL:
        {
            if (ack_code == SYNO_ACK_OK)
            {
                uint16_t page_id = ((uint16_t)rx->buf[10] << 8) + rx->buf[11];
                fp_ctx.enroll_id = (uint8_t)page_id;
                fp_ctx.state = FP_STATE_IDLE;
            }
            else if (ack_code == SYNO_ACK_ENROLL_CANCEL)
            {
                fp_ctx.state = FP_STATE_IDLE;
            }
            else
            {
                fp_ctx.state = FP_STATE_IDLE;
            }
            break;
        }

        case SYNO_CMD_AUTO_MATCH:
        {
            if (ack_code == SYNO_ACK_OK)
            {
                uint16_t page_id = ((uint16_t)rx->buf[10] << 8) + rx->buf[11];
                uint16_t score = ((uint16_t)rx->buf[12] << 8) + rx->buf[13];
                fp_ctx.last_page_id = page_id;
                fp_ctx.last_match_score = score;
                fp_ctx.state = FP_STATE_IDLE;
            }
            else
            {
                fp_ctx.last_page_id = 0xFFFF;
                fp_ctx.last_match_score = 0;
                fp_ctx.state = FP_STATE_IDLE;
            }
            break;
        }

        case SYNO_CMD_VALID_TEMPLATE_NUM:
        {
            if (ack_code == SYNO_ACK_OK)
            {
                fp_ctx.template_count = ((uint16_t)rx->buf[10] << 8) + rx->buf[11];
            }
            break;
        }

        case SYNO_CMD_READ_INDEX_TABLE:
        {
            if (ack_code == SYNO_ACK_OK)
            {
                /* 索引表数据从 buf[10] 开始，最多 32 字节 (256 bits) */
                uint16_t data_len = rx->pkg_dlen - 1;  /* 减去 ack_code */
                if (data_len > 32)
                    data_len = 32;
                memcpy(fp_ctx.index_table, &rx->buf[10], data_len);
                fp_ctx.index_ready = 1;
            }
            break;
        }

        case SYNO_CMD_INTO_SLEEP:
        {
            fp_ctx.state = FP_STATE_SLEEPING;
            break;
        }

        case SYNO_CMD_RGB_CTRL:
        {
            break;
        }

        case SYNO_CMD_DEL_TEMPLATE:
        case SYNO_CMD_EMPTY_TEMPLATE:
        {
            break;
        }

        default:
        {
            break;
        }
    }

    fp_ctx.last_ack = ack_code;
    Fp_ResetRx();
}

uint8_t Fp_HandShake(void)
{
    uint8_t buf[16];
    uint16_t len;

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_HANDSHAKE, NULL, 0);
    fp_ctx.last_cmd = SYNO_CMD_HANDSHAKE;
    Fp_SendPacket(buf, len);
    return 0;
}

uint8_t Fp_AutoEnroll(uint16_t page_id, uint8_t count)
{
    uint8_t buf[32];
    uint16_t len;
    uint8_t params[6];

    params[0] = (page_id >> 8) & 0xFF;
    params[1] = page_id & 0xFF;
    params[2] = count;
    params[3] = 0x00;
    params[4] = 0x00;
    params[5] = 0x00;

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_AUTO_ENROLL, params, 6);
    fp_ctx.last_cmd = SYNO_CMD_AUTO_ENROLL;
    fp_ctx.state = FP_STATE_ENROLLING;
    Fp_SendPacket(buf, len);
    return 0;
}

uint8_t Fp_AutoIdentify(uint8_t security_level)
{
    uint8_t buf[32];
    uint16_t len;
    uint8_t params[6];

    params[0] = security_level;
    params[1] = 0x00;
    params[2] = 0x00;
    params[3] = 0x00;
    params[4] = 0xFF;
    params[5] = 0xFF;

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_AUTO_MATCH, params, 6);
    fp_ctx.last_cmd = SYNO_CMD_AUTO_MATCH;
    fp_ctx.state = FP_STATE_IDENTIFYING;
    Fp_SendPacket(buf, len);
    return 0;
}

uint8_t Fp_Cancel(void)
{
    uint8_t buf[16];
    uint16_t len;

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_AUTO_CANCEL, NULL, 0);
    fp_ctx.last_cmd = SYNO_CMD_AUTO_CANCEL;
    Fp_SendPacket(buf, len);
    fp_ctx.state = FP_STATE_IDLE;
    return 0;
}

uint8_t Fp_DeleteTemplate(uint16_t page_id, uint16_t count)
{
    uint8_t buf[32];
    uint16_t len;
    uint8_t params[4];

    params[0] = (page_id >> 8) & 0xFF;
    params[1] = page_id & 0xFF;
    params[2] = (count >> 8) & 0xFF;
    params[3] = count & 0xFF;

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_DEL_TEMPLATE, params, 4);
    fp_ctx.last_cmd = SYNO_CMD_DEL_TEMPLATE;
    Fp_SendPacket(buf, len);
    return 0;
}

uint8_t Fp_EmptyTemplate(void)
{
    uint8_t buf[16];
    uint16_t len;

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_EMPTY_TEMPLATE, NULL, 0);
    fp_ctx.last_cmd = SYNO_CMD_EMPTY_TEMPLATE;
    Fp_SendPacket(buf, len);
    return 0;
}

uint8_t Fp_ReadValidTemplateNum(void)
{
    uint8_t buf[16];
    uint16_t len;

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_VALID_TEMPLATE_NUM, NULL, 0);
    fp_ctx.last_cmd = SYNO_CMD_VALID_TEMPLATE_NUM;
    Fp_SendPacket(buf, len);
    return 0;
}

uint8_t Fp_ReadIndexTable(uint8_t index)
{
    uint8_t buf[16];
    uint16_t len;
    uint8_t params[1];

    params[0] = index;

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_READ_INDEX_TABLE, params, 1);
    fp_ctx.last_cmd = SYNO_CMD_READ_INDEX_TABLE;
    Fp_SendPacket(buf, len);
    return 0;
}

uint8_t Fp_Sleep(void)
{
    uint8_t buf[16];
    uint16_t len;

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_INTO_SLEEP, NULL, 0);
    fp_ctx.last_cmd = SYNO_CMD_INTO_SLEEP;
    Fp_SendPacket(buf, len);
    return 0;
}

uint8_t Fp_ControlLED(uint8_t func, uint8_t color, uint8_t speed)
{
    uint8_t buf[32];
    uint16_t len;
    uint8_t params[5];

    if (fp_ctx.state == FP_STATE_SLEEPING)
    {
        fp_ctx.led_cmd_cached = 1;
        fp_ctx.led_cached_func = func;
        fp_ctx.led_cached_color = color;
        fp_ctx.led_cached_speed = speed;
        return 0;
    }

    params[0] = func;
    params[1] = color;
    params[2] = color;
    params[3] = speed;
    params[4] = 0x00;

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_RGB_CTRL, params, 5);
    fp_ctx.last_cmd = SYNO_CMD_RGB_CTRL;
    Fp_SendPacket(buf, len);
    return 0;
}

uint8_t Fp_WriteReg(uint8_t reg_num, uint16_t value)
{
    uint8_t buf[32];
    uint16_t len;
    uint8_t params[4];

    params[0] = reg_num;
    params[1] = 0x00;
    params[2] = (value >> 8) & 0xFF;
    params[3] = value & 0xFF;

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_WRITE_REG, params, 4);
    fp_ctx.last_cmd = SYNO_CMD_WRITE_REG;
    Fp_SendPacket(buf, len);
    return 0;
}
