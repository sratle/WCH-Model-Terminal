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
            /* PS_AutoEnroll 应答格式:
             *   确认码(ack_code) + 参数1(step) + 参数2(sub_step)
             *
             * ack_code=0x00 时的步骤 (param1):
             *   0x00=合法性检测(命令接受) 0x01=获取图像 0x02=生成特征
             *   0x03=手指离开 0x04=合并模板 0x05=注册检验 0x06=存储模板
             *
             * ack_code!=0x00 时为错误码 (部分为中间步骤、部分为终止错误) */
            {
                uint8_t step = rx->buf[10];     /* param1: 步骤类型 */
                uint8_t sub_step = rx->buf[11]; /* param2: 子步骤 */

                fp_ctx.enroll_step = step;
                fp_ctx.enroll_sub_step = sub_step;
                fp_ctx.enroll_resp_count++;
                fp_ctx.enroll_idle_counter = 0;  /* 重置超时计数 */
                fp_ctx.enroll_progress_ready = 1;
            }

            /* 终止性错误码：注册流程结束
             * 中间错误码 (不结束): 0x02=无手指, 0x03=采图失败,
             * 0x04=太干, 0x05=太湿, 0x06=太乱, 0x07=特征点少, 0x17=残留指纹
             * 取消: 0x2C */
            if (ack_code == SYNO_ACK_ENROLL_CANCEL)
            {
                fp_ctx.state = FP_STATE_IDLE;
            }
            else if (ack_code != SYNO_ACK_OK &&
                     ack_code != 0x02 && ack_code != 0x03 && ack_code != 0x04 &&
                     ack_code != 0x05 && ack_code != 0x06 && ack_code != 0x07 &&
                     ack_code != 0x17)
            {
                fp_ctx.state = FP_STATE_IDLE;
            }
            /* 其他情况保持 ENROLLING，等待更多应答或超时 */
            break;
        }

        case SYNO_CMD_AUTO_MATCH:
        {
            /* PS_AutoIdentify 应答格式 (所有应答均为 pkg_dlen=8):
             *   buf[9]  = 确认码
             *   buf[10] = 参数 (0x00=合法性检测, 0x01=获取图像, 0x05=指纹比对)
             *   buf[11..12] = ID号 (大端)
             *   buf[13..14] = 得分 (大端)
             *
             * 参数=0x05 (指纹比对) 为最终步骤，此时 ID号和得分有效。 */
            if (ack_code == SYNO_ACK_OK)
            {
                fp_ctx.enroll_step = rx->buf[10];
                fp_ctx.last_page_id = ((uint16_t)rx->buf[11] << 8) + rx->buf[12];
                fp_ctx.last_match_score = ((uint16_t)rx->buf[13] << 8) + rx->buf[14];
                fp_ctx.enroll_idle_counter = 0;
                fp_ctx.enroll_progress_ready = 1;

                /* 参数=0x05 表示指纹比对完成，即最终结果 */
                if (fp_ctx.enroll_step == 0x05)
                {
                    fp_ctx.state = FP_STATE_IDLE;
                }
            }
            else
            {
                /* 非 0x00 确认码: 立即失败 */
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
                /* 索引表数据从 buf[10] 开始，最多 32 字节 (256 bits)
                 * pkg_dlen 包含 ack_code(1) + index_data(32) + checksum(2) = 35
                 * 因此 index_data 长度 = pkg_dlen - 3 */
                uint16_t data_len = (rx->pkg_dlen > 3) ? (rx->pkg_dlen - 3) : 0;
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
    uint8_t params[5];

    /* PS_AutoEnroll 参数 (共 5 字节):
     *   ID号(2B) + 录入次数(1B) + 参数(2B) */
    params[0] = (page_id >> 8) & 0xFF;  /* ID号高字节 */
    params[1] = page_id & 0xFF;         /* ID号低字节 */
    params[2] = count;                  /* 录入次数 */
    params[3] = 0x00;  /* 参数低字节: bit2=0(返回关键步骤) */
    params[4] = 0x00;  /* 参数高字节: bit8~15=预留 */

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_AUTO_ENROLL, params, 5);
    fp_ctx.last_cmd = SYNO_CMD_AUTO_ENROLL;
    fp_ctx.state = FP_STATE_ENROLLING;
    fp_ctx.enroll_step = 0;
    fp_ctx.enroll_sub_step = 0;
    fp_ctx.enroll_resp_count = 0;
    fp_ctx.enroll_idle_counter = 0;
    Fp_SendPacket(buf, len);
    return 0;
}

uint8_t Fp_AutoIdentify(uint8_t security_level)
{
    uint8_t buf[32];
    uint16_t len;
    uint8_t params[5];

    /* PS_AutoIdentify 参数 (共 5 字节):
     *   分数等级(1B) + ID号(2B, 0xFFFF=1:N搜索) + 参数(2B) */
    params[0] = security_level;
    params[1] = 0xFF;   /* ID号高字节 */
    params[2] = 0xFF;   /* ID号低字节 (0xFFFF = 1:N 全库搜索) */
    params[3] = 0x00;   /* 参数高字节 */
    params[4] = 0x00;   /* 参数低字节 (bit2=0: 返回关键步骤) */

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_AUTO_MATCH, params, 5);
    fp_ctx.last_cmd = SYNO_CMD_AUTO_MATCH;
    fp_ctx.state = FP_STATE_IDENTIFYING;
    fp_ctx.enroll_idle_counter = 0;
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

uint8_t Fp_WriteReg(uint8_t reg_num, uint8_t value)
{
    uint8_t buf[16];
    uint16_t len;
    uint8_t params[2];

    /* PS_WriteReg 参数 (共 2 字节):
     *   寄存器序号(1B) + 内容(1B) */
    params[0] = reg_num;
    params[1] = value;

    len = Fp_BuildCmdPacket(buf, SYNO_CMD_WRITE_REG, params, 2);
    fp_ctx.last_cmd = SYNO_CMD_WRITE_REG;
    Fp_SendPacket(buf, len);
    return 0;
}
