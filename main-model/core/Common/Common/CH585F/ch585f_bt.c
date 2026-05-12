#include "ch585f_bt.h"
#include "CH585F.h"
#include "CLI/CLI.h"
#include "debug.h"
#include "hardware.h"
#include <string.h>

/* CLI 输出捕获缓冲区（被 debug.c _write 写入） */
uint8_t cli_capture_buf[CH585F_BT_CLI_CAPTURE_SIZE];
uint16_t cli_capture_len = 0;
uint8_t cli_capture_flag = 0;

ch585f_bt_ctx_t ch585f_bt_g;

/* 静态发送缓冲区 */
static uint8_t s_tx_buf[PROTO_MAX_FRAME_LEN];

/* CLI 多帧组装缓冲区 */
static uint8_t s_cli_assemble_buf[CH585F_BT_CLI_ASSEMBLE_SIZE];
static uint16_t s_cli_assemble_len = 0;

static void CH585F_BT_SendFrame(uint8_t cmd, const uint8_t *data, uint8_t data_len);
static void CH585F_BT_HandleConnEvt(const protocol_frame_t *frame);
static void CH585F_BT_HandleExtFrame(const protocol_frame_t *frame);
static void CH585F_BT_HandleCliData(const protocol_frame_t *frame);
void CH585F_BT_HandleFrame(void);

/*********************************************************************
 * @fn      CH585F_BT_Init
 *
 * @brief   初始化 CH585F 蓝牙协议封装层上下文
 *
 * @return  none
 *********************************************************************/
void CH585F_BT_Init(void)
{
    memset(&ch585f_bt_g, 0, sizeof(ch585f_bt_g));
    Protocol_InitRxCtx(&ch585f_bt_g.rx_ctx);
    cli_capture_len = 0;
    cli_capture_flag = 0;
}

/*********************************************************************
 * @fn      CH585F_BT_Poll
 *
 * @brief   主循环轮询入口：通过 SPI 读取上行数据并解析协议帧
 *
 * @return  none
 *********************************************************************/
void CH585F_BT_Poll(void)
{
    uint8_t rx_buf[CH585F_BT_POLL_SIZE];
    uint16_t i;
    uint8_t has_data = 0;

    CH585F_Recv_Data(&ch585f_g, rx_buf, CH585F_BT_POLL_SIZE);

    /* 调试：检查本轮是否读到非 0x00/0xFF 的有效数据 */
    for (i = 0; i < CH585F_BT_POLL_SIZE; i++)
    {
        if (rx_buf[i] != 0x00 && rx_buf[i] != 0xFF)
        {
            has_data = 1;
            break;
        }
    }

    if (has_data)
    {
        printf("[BT POLL] RAW [%d]: ", CH585F_BT_POLL_SIZE);
        for (i = 0; i < CH585F_BT_POLL_SIZE; i++)
        {
            printf("%02X ", rx_buf[i]);
        }
        printf("\r\n");
    }

    for (i = 0; i < CH585F_BT_POLL_SIZE; i++)
    {
        if (Protocol_ParseByte(&ch585f_bt_g.rx_ctx, rx_buf[i]))
        {
            printf("[BT] Frame ready: cmd=0x%02X, len=%d, data[0]=0x%02X\r\n",
                   ch585f_bt_g.rx_ctx.frame.cmd,
                   ch585f_bt_g.rx_ctx.frame.len,
                   ch585f_bt_g.rx_ctx.frame.data[0]);
            CH585F_BT_HandleFrame();
            Protocol_ResetRxCtx(&ch585f_bt_g.rx_ctx);
        }
    }
}

/*********************************************************************
 * @fn      CH585F_BT_HandleFrame
 *
 * @brief   解析就绪的协议帧，按 cmd 分发处理
 *
 * @return  none
 *********************************************************************/
void CH585F_BT_HandleFrame(void)
{
    protocol_frame_t *frame = &ch585f_bt_g.rx_ctx.frame;

    switch (frame->cmd)
    {
        case CMD_BT_CONN_EVT:
            CH585F_BT_HandleConnEvt(frame);
            break;
        case CMD_BT_EXTENSION:
            CH585F_BT_HandleExtFrame(frame);
            break;
        default:
            /* 其他命令暂不处理（GET_STATUS ACK 等） */
            break;
    }
}

/*********************************************************************
 * @fn      CH585F_BT_SendFrame
 *
 * @brief   通过 SPI 发送一帧标准协议数据给 CH585F
 *
 * @return  none
 *********************************************************************/
static void CH585F_BT_SendFrame(uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
    uint8_t len;
    len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_WIRELESS, cmd,
                             data, data_len,
                             s_tx_buf, sizeof(s_tx_buf));
    if (len > 0)
    {
        CH585F_Send_Data(&ch585f_g, s_tx_buf, len);
    }
}

/*********************************************************************
 * @fn      CH585F_BT_HandleConnEvt
 *
 * @brief   处理蓝牙连接/断开事件（CMD_BT_CONN_EVT，0x54）
 *
 * @return  none
 *********************************************************************/
static void CH585F_BT_HandleConnEvt(const protocol_frame_t *frame)
{
    uint8_t evt_type;
    uint8_t dev_type;

    if (frame->len < 25) return; /* 最小长度：1(evt) + 1(dev) + 6(mac) + 16(name) */

    evt_type = frame->data[0];
    dev_type = frame->data[1];

    if (dev_type == 0x04) /* 手机/PC（APP） */
    {
        if (evt_type == 0x00) /* 已连接 */
        {
            ch585f_bt_g.app_connected = 1;
            memcpy(ch585f_bt_g.app_mac, &frame->data[2], 6);
            memcpy(ch585f_bt_g.app_name, &frame->data[8], 16);
            printf("[BT] APP connected: %.16s\r\n", ch585f_bt_g.app_name);
        }
        else if (evt_type == 0x01) /* 已断开 */
        {
            ch585f_bt_g.app_connected = 0;
            printf("[BT] APP disconnected\r\n");
        }
    }
}

/*********************************************************************
 * @fn      CH585F_BT_HandleExtFrame
 *
 * @brief   处理扩展操作码帧（CMD_BT_EXTENSION，0x50），按 DATA[0] 子命令分发
 *
 * @return  none
 *********************************************************************/
static void CH585F_BT_HandleExtFrame(const protocol_frame_t *frame)
{
    uint8_t ext_cmd;

    if (frame->len < 2) return;

    ext_cmd = frame->data[0];

    switch (ext_cmd)
    {
        case CMD_BT_EXT_CLI_DATA:
            CH585F_BT_HandleCliData(frame);
            break;
        default:
            break;
    }
}

/*********************************************************************
 * @fn      CH585F_BT_HandleCliData
 *
 * @brief   处理 APP CLI 透传数据（扩展码 0x07）
 *
 *          协议格式（V2.2）:
 *          DATA[0] = 0x07 (扩展码)
 *          DATA[1] = FLAGS (bit0=SOF, bit1=EOF)
 *          DATA[2..N] = CLI 原始数据
 *
 *          支持多帧拆分：通过 FLAGS 的 SOF/EOF 位组装完整 CLI 命令。
 *
 * @return  none
 *********************************************************************/
static void CH585F_BT_HandleCliData(const protocol_frame_t *frame)
{
    uint8_t flags;
    uint8_t *cli_data;
    uint8_t cli_len;
    uint16_t out_len;
    uint8_t *out_buf;

    if (frame->len < 3) return; /* 至少: ext_cmd(1) + FLAGS(1) */

    if (!ch585f_bt_g.app_connected)
    {
        printf("[BT] CLI data received but APP not connected, dropped\r\n");
        return;
    }

    flags    = frame->data[1];
    cli_data = (uint8_t *)&frame->data[2];
    cli_len  = frame->len - 3; /* LEN - 1(CMD) - 1(ext_cmd) - 1(FLAGS) */

    /* SOF: 新消息开始，清空组装缓冲区 */
    if (flags & CLI_FLAG_SOF)
    {
        s_cli_assemble_len = 0;
    }

    /* 将本帧 CLI 数据追加到组装缓冲区 */
    if (cli_len > 0)
    {
        if (s_cli_assemble_len + cli_len > CH585F_BT_CLI_ASSEMBLE_SIZE)
        {
            cli_len = CH585F_BT_CLI_ASSEMBLE_SIZE - s_cli_assemble_len;
        }
        if (cli_len > 0)
        {
            memcpy(&s_cli_assemble_buf[s_cli_assemble_len], cli_data, cli_len);
            s_cli_assemble_len += cli_len;
        }
    }

    /* EOF: 消息结束，执行 CLI 并回传输出 */
    if (flags & CLI_FLAG_EOF)
    {
        if (s_cli_assemble_len > 0)
        {
            /* 确保字符串以 '\0' 结尾 */
            if (s_cli_assemble_len < CH585F_BT_CLI_ASSEMBLE_SIZE)
            {
                s_cli_assemble_buf[s_cli_assemble_len] = '\0';
            }
            else
            {
                s_cli_assemble_buf[CH585F_BT_CLI_ASSEMBLE_SIZE - 1] = '\0';
            }

            printf("[BT] CLI exec: len=%d, cmd='%s'\r\n",
                   s_cli_assemble_len, s_cli_assemble_buf);

            /* 执行 CLI 命令并捕获输出
             * CLI_Process 接收 uint8_t len，最大 255，超限截断
             */
            CLI_Capture_Start();
            if (s_cli_assemble_len > 255)
            {
                s_cli_assemble_len = 255;
            }
            CLI_Process(s_cli_assemble_buf, (uint8_t)s_cli_assemble_len);
            CLI_Capture_Stop();

            out_buf = CLI_Capture_Flush(&out_len);
            printf("[BT] CLI output: len=%d\r\n", out_len);
            if (out_len > 0)
            {
                CH585F_BT_SendCliData(out_buf, out_len);
            }
        }
    }
}

/*********************************************************************
 * @fn      CH585F_BT_SendCliData
 *
 * @brief   将 CLI 输出数据通过扩展码下发给 CH585F
 *
 *          协议格式（V2.2）:
 *          DATA[0] = 0x07 (扩展码)
 *          DATA[1] = FLAGS (bit0=SOF, bit1=EOF)
 *          DATA[2..N] = CLI 原始数据
 *
 *          若输出超过单帧容量（253 字节），自动拆分为多帧。
 *
 * @return  none
 *********************************************************************/
void CH585F_BT_SendCliData(uint8_t *data, uint16_t len)
{
    uint8_t buf[PROTO_MAX_DATA_LEN];
    uint16_t offset = 0;
    uint8_t chunk;
    uint8_t flags;

    while (offset < len)
    {
        chunk = (uint8_t)(len - offset);
        /* 每帧预留 2 字节给扩展码 + FLAGS */
        if (chunk > PROTO_MAX_DATA_LEN - 2)
        {
            chunk = PROTO_MAX_DATA_LEN - 2;
        }

        flags = 0;
        if (offset == 0)
        {
            flags |= CLI_FLAG_SOF;  /* 首帧 */
        }
        if (offset + chunk >= len)
        {
            flags |= CLI_FLAG_EOF;  /* 尾帧 */
        }

        buf[0] = CMD_BT_EXT_CLI_DATA;
        buf[1] = flags;
        memcpy(&buf[2], &data[offset], chunk);

        CH585F_BT_SendFrame(CMD_BT_EXTENSION, buf, chunk + 2);
        offset += chunk;
    }
}

/*********************************************************************
 * @fn      CLI_Capture_Start
 *
 * @brief   开始捕获 CLI 输出到缓冲区
 *
 * @return  none
 *********************************************************************/
void CLI_Capture_Start(void)
{
    cli_capture_flag = 1;
    cli_capture_len = 0;
}

/*********************************************************************
 * @fn      CLI_Capture_Stop
 *
 * @brief   停止捕获 CLI 输出
 *
 * @return  none
 *********************************************************************/
void CLI_Capture_Stop(void)
{
    cli_capture_flag = 0;
}

/*********************************************************************
 * @fn      CLI_Capture_Flush
 *
 * @brief   获取捕获的 CLI 输出缓冲区指针和长度，并清空计数器
 *
 * @param   out_len - 输出长度指针
 * @return  缓冲区指针
 *********************************************************************/
uint8_t* CLI_Capture_Flush(uint16_t *out_len)
{
    *out_len = cli_capture_len;
    cli_capture_len = 0;
    return cli_capture_buf;
}
