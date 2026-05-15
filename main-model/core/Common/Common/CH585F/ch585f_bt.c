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

/* 静态发送缓冲区：Protocol_PackFrame 打包完整帧后，写入 TX FIFO */
static uint8_t s_tx_buf[PROTO_MAX_FRAME_LEN];

/* CLI 多帧组装缓冲区 */
static uint8_t s_cli_assemble_buf[CH585F_BT_CLI_ASSEMBLE_SIZE];
static uint16_t s_cli_assemble_len = 0;

/* SPI 发送 FIFO（poll 时顺带发送，避免在 HandleCliData 中独立发导致 Slave 错过） */
#define SPI_TX_FIFO_SIZE    2048
static uint8_t  s_spi_tx_fifo[SPI_TX_FIFO_SIZE];
static uint16_t s_spi_tx_head = 0;
static uint16_t s_spi_tx_tail = 0;

/* 接收状态机卡住计数器：连续 N 次 poll 未完成帧则强制复位 */
static uint8_t s_rx_stuck_cnt = 0;
#define RX_STUCK_THRESHOLD  5

static uint16_t spi_tx_fifo_write(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++)
    {
        uint16_t next = (s_spi_tx_head + 1) % SPI_TX_FIFO_SIZE;
        if (next == s_spi_tx_tail)
            break;
        s_spi_tx_fifo[s_spi_tx_head] = data[i];
        s_spi_tx_head = next;
    }
    return i;
}

static uint16_t spi_tx_fifo_read(uint8_t *out, uint16_t max_len)
{
    uint16_t i;
    for (i = 0; i < max_len; i++)
    {
        if (s_spi_tx_tail == s_spi_tx_head)
            break;
        out[i] = s_spi_tx_fifo[s_spi_tx_tail];
        s_spi_tx_tail = (s_spi_tx_tail + 1) % SPI_TX_FIFO_SIZE;
    }
    return i;
}

static uint16_t spi_tx_fifo_count(void)
{
    if (s_spi_tx_head >= s_spi_tx_tail)
    {
        return s_spi_tx_head - s_spi_tx_tail;
    }
    else
    {
        return SPI_TX_FIFO_SIZE - s_spi_tx_tail + s_spi_tx_head;
    }
}

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
    uint8_t tx_buf[CH585F_BT_POLL_SIZE];
    uint16_t i;
    uint16_t tx_len;
    uint8_t has_rx_data = 0;
    static uint16_t s_idle_cnt = 0;
    uint8_t do_poll = 0;

    /* 触发条件1：CH585F 通过 NSS 通知有数据 */
    if (ch585f_g.nss_notify)
    {
        do_poll = 1;
        ch585f_g.nss_notify = 0;
    }

    /* 触发条件2：Core 有下行数据待发 */
    if (spi_tx_fifo_count() > 0)
    {
        do_poll = 1;
    }

    /* 触发条件3：定期保活（每 10ms 至少一次），防止通知丢失或死锁 */
    if (++s_idle_cnt >= 10)
    {
        do_poll = 1;
        s_idle_cnt = 0;
    }

    if (!do_poll)
    {
        return;
    }

    s_idle_cnt = 0;

    /* 从 TX FIFO 取待发送数据，不足补 0x00（dummy） */
    tx_len = spi_tx_fifo_read(tx_buf, CH585F_BT_POLL_SIZE);
    if (tx_len < CH585F_BT_POLL_SIZE)
    {
        memset(&tx_buf[tx_len], 0x00, CH585F_BT_POLL_SIZE - tx_len);
    }

    /* 全双工传输：发送 tx_buf 的同时接收 rx_buf */
    CH585F_SPI_Transfer(&ch585f_g, tx_buf, rx_buf, CH585F_BT_POLL_SIZE);

    /* 调试：检查本轮是否读到非 0x00/0xFF 的有效数据，仅在有效数据时打印 */
    for (i = 0; i < CH585F_BT_POLL_SIZE; i++)
    {
        if (rx_buf[i] != 0x00 && rx_buf[i] != 0xFF)
        {
            has_rx_data = 1;
            break;
        }
    }

    if (has_rx_data)
    {
        if (tx_len > 0)
        {
            printf("[BT POLL] TX [%d]: ", tx_len);
            for (i = 0; i < tx_len && i < 16; i++)
            {
                printf("%02X ", tx_buf[i]);
            }
            if (tx_len > 16) printf("...");
            printf("\r\n");
        }
        printf("[BT POLL] RX [%d]: ", CH585F_BT_POLL_SIZE);
        for (i = 0; i < CH585F_BT_POLL_SIZE; i++)
        {
            printf("%02X ", rx_buf[i]);
        }
        printf("\r\n");
    }

    /* 解析接收到的数据。
     * 注意：此处不可在帧就绪时 break，因为剩余字节已随本轮 SPI 传输接收，
     * break 会导致它们被永久丢弃。Protocol_ResetRxCtx() 会将状态重置为
     * WAIT_HEAD，因此循环可以继续解析下一帧。 */
    for (i = 0; i < CH585F_BT_POLL_SIZE; i++)
    {
        if (Protocol_ParseByte(&ch585f_bt_g.rx_ctx, rx_buf[i]))
        {
            s_rx_stuck_cnt = 0;
            printf("[BT] Frame ready: cmd=0x%02X, len=%d, data[0]=0x%02X\r\n",
                   ch585f_bt_g.rx_ctx.frame.cmd,
                   ch585f_bt_g.rx_ctx.frame.len,
                   ch585f_bt_g.rx_ctx.frame.data[0]);
            CH585F_BT_HandleFrame();
            Protocol_ResetRxCtx(&ch585f_bt_g.rx_ctx);
        }
    }

    /* 状态机卡住检测：不在 WAIT_HEAD 且连续多次没收到完整帧 */
    if (ch585f_bt_g.rx_ctx.state != PROTO_STATE_WAIT_HEAD)
    {
        s_rx_stuck_cnt++;
        if (s_rx_stuck_cnt >= RX_STUCK_THRESHOLD)
        {
            printf("[BT] RX state machine stuck (state=%d, cnt=%d), force reset\r\n",
                   ch585f_bt_g.rx_ctx.state, s_rx_stuck_cnt);
            Protocol_ResetRxCtx(&ch585f_bt_g.rx_ctx);
            s_rx_stuck_cnt = 0;
        }
    }
    else
    {
        s_rx_stuck_cnt = 0;
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
/*********************************************************************
 * @fn      CH585F_BT_SendFrame
 *
 * @brief   打包完整协议帧并写入 SPI TX FIFO，等 poll 时顺带发出
 *
 * @return  none
 *********************************************************************/
static void CH585F_BT_SendFrame(uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
    uint8_t len;
    uint16_t written;

    len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_WIRELESS, cmd,
                             data, data_len,
                             s_tx_buf, sizeof(s_tx_buf));
    if (len > 0)
    {
        written = spi_tx_fifo_write(s_tx_buf, len);
        if (written < len)
        {
            printf("[BT] SendFrame: TX FIFO full! Dropped %d bytes\r\n", len - written);
        }
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
    uint16_t cli_len;
    uint16_t out_len;
    uint8_t *out_buf;

    if (frame->len < 3) return; /* 至少: ext_cmd(1) + FLAGS(1) */

    /* 若收到 CLI 数据但 app_connected 未置位，自动标记为已连接
     * （CH585F 可能未发送或 Core 漏收了 CMD_BT_CONN_EVT）
     */
    if (!ch585f_bt_g.app_connected)
    {
        ch585f_bt_g.app_connected = 1;
        printf("[BT] APP auto-connected via CLI data\r\n");
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
        uint16_t copy_len = cli_len;
        if (s_cli_assemble_len + copy_len > CH585F_BT_CLI_ASSEMBLE_SIZE)
        {
            copy_len = CH585F_BT_CLI_ASSEMBLE_SIZE - s_cli_assemble_len;
        }
        if (copy_len > 0)
        {
            memcpy(&s_cli_assemble_buf[s_cli_assemble_len], cli_data, copy_len);
            s_cli_assemble_len += copy_len;
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

            /* 去掉末尾的 \r 和 \n，避免 CLI_Process 不认识 */
            while (s_cli_assemble_len > 0)
            {
                uint8_t last = s_cli_assemble_buf[s_cli_assemble_len - 1];
                if (last == '\r' || last == '\n')
                    s_cli_assemble_len--;
                else
                    break;
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
    uint16_t j;

    /* 调试：打印 CLI 输出内容 */
    printf("[BT] SendCliData input: len=%d\r\n", len);
    printf("[BT] CLI output content: ");
    for (j = 0; j < len && j < 128; j++)
    {
        uint8_t c = data[j];
        if (c >= 0x20 && c < 0x7F)
            printf("%c", c);
        else if (c == '\r')
            printf("\\r");
        else if (c == '\n')
            printf("\\n");
        else
            printf("\\x%02X", c);
    }
    if (len > 128) printf("...");
    printf("\r\n");

    while (offset < len)
    {
        chunk = (uint8_t)(len - offset);
        /* 每帧预留 2 字节给扩展码 + FLAGS，标准帧最大 payload 253 字节 */
        if (chunk > CH585F_BT_STD_MAX_PAYLOAD)
        {
            chunk = CH585F_BT_STD_MAX_PAYLOAD;
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

    printf("[BT] SendCliData: queued %d bytes (%d frames) to TX FIFO\r\n",
           offset, (len + CH585F_BT_STD_MAX_PAYLOAD - 1) / CH585F_BT_STD_MAX_PAYLOAD);
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
