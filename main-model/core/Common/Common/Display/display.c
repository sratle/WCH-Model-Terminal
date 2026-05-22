#include "display.h"
#include "../Protocol/protocol_common.h"
#include "../CS43131/cs43131.h"
#include "../CH378/CH378.h"
#include "../CH585F/ch585f.h"
#include <string.h>

display_t *display_ptr = NULL;

/* External globals from hardware.c */
extern cs43131_t CS43131_g;
extern ch378_t ch378_g;
extern ch585f_t ch585f_g;

/* ============================================================================
 * 前向声明：Display 专用命令 handler
 * ============================================================================ */

/* 请求/动作类 handler：返回 1=成功，0=失败 */
static uint8_t Display_HandleMusicControl(const protocol_frame_t *req,
                                          uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleVolumeControl(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleGetBrightness(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleGetRotation(const protocol_frame_t *req,
                                         uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleGetScreenState(const protocol_frame_t *req,
                                            uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleAppLaunch(const protocol_frame_t *req,
                                       uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleAppClose(const protocol_frame_t *req,
                                      uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleAppData(const protocol_frame_t *req,
                                     uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleGetModuleStatus(const protocol_frame_t *req,
                                             uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleRequestFileList(const protocol_frame_t *req,
                                             uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleFileRead(const protocol_frame_t *req,
                                      uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleFileSave(const protocol_frame_t *req,
                                      uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleFileOperation(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandlePlayMusic(const protocol_frame_t *req,
                                       uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleBtControl(const protocol_frame_t *req,
                                       uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleSaveConfig(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleLoadConfig(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleSetRgbMode(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleBulkTransfer(const protocol_frame_t *req,
                                          uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);

/* 事件类 handler（Core -> Display，无需响应） */
static void Display_HandleInputEvent(const protocol_frame_t *req);
static void Display_HandleModuleStatus(const protocol_frame_t *req);
static void Display_HandleBtEvent(const protocol_frame_t *req);
static void Display_HandleSubmodelEvent(const protocol_frame_t *req);
static void Display_HandlePowerEvent(const protocol_frame_t *req);
static void Display_HandleConfigResult(const protocol_frame_t *req);
static void Display_HandleErrorReport(const protocol_frame_t *req);

/* ============================================================================
 * UART 初始化与数据收发
 * ============================================================================ */

void Display_Init(display_t *display)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOF, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART4, ENABLE);

    GPIO_PinAFConfig(DISPLAY_UART_TX_PORT, GPIO_PinSource4, DISPLAY_UART_TX_AF);
    GPIO_PinAFConfig(DISPLAY_UART_RX_PORT, GPIO_PinSource3, DISPLAY_UART_RX_AF);

    GPIO_InitStructure.GPIO_Pin = DISPLAY_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(DISPLAY_UART_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = DISPLAY_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(DISPLAY_UART_RX_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = DISPLAY_UART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(DISPLAY_UART, &USART_InitStructure);
    USART_Cmd(DISPLAY_UART, ENABLE);

    USART_ITConfig(DISPLAY_UART, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(DISPLAY_UART_IRQn, 1 << 4);
    NVIC_EnableIRQ(DISPLAY_UART_IRQn);

    Protocol_InitRxCtx(&display->rx_ctx);

    display->bulk_state = BULK_IDLE;
    display->bulk_total_size = 0;
    display->bulk_received = 0;
    display->bulk_block_size = DISPLAY_BULK_BLOCK_SIZE;

    Display_Get_Type(display);
    display_ptr = display;
}

void Display_UART_IRQ_Handler(display_t *display)
{
    if (display == NULL)
        return;

    if (USART_GetITStatus(DISPLAY_UART, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(DISPLAY_UART);
        Protocol_ParseByte(&display->rx_ctx, byte);
    }
}

void Display_Get_Type(display_t *display)
{
    (void)display;
}

void Display_Send_Data(display_t *display, const uint8_t *data, uint16_t length)
{
    uint16_t i;
    (void)display;

    for (i = 0; i < length; i++)
    {
        while (USART_GetFlagStatus(DISPLAY_UART, USART_FLAG_TC) == RESET)
            ;
        USART_SendData(DISPLAY_UART, *data++);
    }
}

/* ============================================================================
 * 通用命令回调
 * ============================================================================ */

static uint8_t display_on_get_type(const protocol_frame_t *req,
                                   uint8_t *resp_buf, uint16_t resp_size,
                                   uint8_t *resp_len)
{
    (void)req;
    (void)resp_size;
    resp_buf[0] = MODULE_TYPE_DISPLAY;
    resp_buf[1] = MODULE_SUBTYPE_DISPLAY_LCD;
    resp_buf[2] = 0x01; /* hw_ver */
    resp_buf[3] = 0x01; /* fw_major */
    resp_buf[4] = 0x00; /* fw_minor */
    *resp_len = 5;
    return 1;
}

static const protocol_common_cb_t display_common_cb = {
    .on_get_type = display_on_get_type,
};

/* ============================================================================
 * 专用命令分发辅助函数
 * ============================================================================ */

/**
 * @brief  请求/动作类 handler 的统一分发与 ACK/NACK 打包
 */
static uint8_t display_dispatch_req(const protocol_frame_t *req,
                                    uint8_t (*handler)(const protocol_frame_t *,
                                                       uint8_t *, uint16_t, uint8_t *),
                                    uint8_t *resp_buf, uint16_t resp_size)
{
    uint8_t data_len = 0;

    if (handler == NULL)
        return ProtocolCommon_Nack(req->dst, req->src, PROTO_ERR_UNSUPPORTED_CMD,
                                   resp_buf, resp_size);

    if (!handler(req, resp_buf, resp_size, &data_len))
        return ProtocolCommon_Nack(req->dst, req->src, PROTO_ERR_UNSUPPORTED_CMD,
                                   resp_buf, resp_size);

    if (data_len > 0)
        return Protocol_PackFrame(req->dst, req->src, CMD_ACK,
                                  resp_buf, data_len, resp_buf, resp_size);

    return ProtocolCommon_Ack(req->dst, req->src, resp_buf, resp_size);
}

/* ============================================================================
 * 主循环帧处理
 * ============================================================================ */

void Display_Process(display_t *display)
{
    uint8_t resp[PROTO_MAX_FRAME_LEN];
    uint8_t resp_len = 0;
    uint8_t handled = 0;
    protocol_frame_t *req;

    if (display == NULL || !display->rx_ctx.frame_ready)
        return;

    req = &display->rx_ctx.frame;

    /* 1. 通用命令（0x00~0x0F） */
    if (ProtocolCommon_Dispatch(req, &display_common_cb,
                                resp, sizeof(resp), &resp_len))
    {
        handled = 1;
    }
    /* 2. Display 扩展操作码：CMD = 0x10, DATA[0] 为子命令 */
    else if (req->cmd == CMD_DISP_EXTENSION)
    {
        if (req->len >= 2)
        {
            uint8_t ext = req->data[0];
            switch (ext)
            {
                /* ---- 请求/动作类（Display -> Core，需要 ACK/NACK） ---- */
                case CMD_DISP_EXT_APP_LAUNCH:
                    resp_len = display_dispatch_req(req, Display_HandleAppLaunch,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_APP_CLOSE:
                    resp_len = display_dispatch_req(req, Display_HandleAppClose,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_APP_DATA:
                    resp_len = display_dispatch_req(req, Display_HandleAppData,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_GET_MODULE_STATUS:
                    resp_len = display_dispatch_req(req, Display_HandleGetModuleStatus,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_REQUEST_FILE_LIST:
                    resp_len = display_dispatch_req(req, Display_HandleRequestFileList,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_FILE_READ:
                    resp_len = display_dispatch_req(req, Display_HandleFileRead,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_FILE_SAVE:
                    resp_len = display_dispatch_req(req, Display_HandleFileSave,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_FILE_OPERATION:
                    resp_len = display_dispatch_req(req, Display_HandleFileOperation,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_PLAY_MUSIC:
                    resp_len = display_dispatch_req(req, Display_HandlePlayMusic,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_BT_CONTROL:
                    resp_len = display_dispatch_req(req, Display_HandleBtControl,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_SAVE_CONFIG:
                    resp_len = display_dispatch_req(req, Display_HandleSaveConfig,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_LOAD_CONFIG:
                    resp_len = display_dispatch_req(req, Display_HandleLoadConfig,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_SET_RGB_MODE:
                    resp_len = display_dispatch_req(req, Display_HandleSetRgbMode,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_BULK_TRANSFER:
                    resp_len = display_dispatch_req(req, Display_HandleBulkTransfer,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;

                /* ---- 事件类（Core -> Display，无需响应） ---- */
                case CMD_DISP_EXT_MODULE_STATUS:
                    Display_HandleModuleStatus(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_BT_EVENT:
                    Display_HandleBtEvent(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_SUBMODEL_EVENT:
                    Display_HandleSubmodelEvent(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_POWER_EVENT:
                    Display_HandlePowerEvent(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_CONFIG_RESULT:
                    Display_HandleConfigResult(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_ERROR_REPORT:
                    Display_HandleErrorReport(req);
                    handled = 1;
                    break;

                /* ---- 设置类（Core -> Display，发送即忘） ---- */
                case CMD_DISP_EXT_SUBDISP_CONTENT:
                case CMD_DISP_EXT_SUBDISP_CONFIG:
                    /* Display 收到后执行，不回复 */
                    handled = 1;
                    break;

                /* ---- 文件列表响应（Core -> Display，作为 REQUEST_FILE_LIST 的回复） ---- */
                case CMD_DISP_EXT_FILE_LIST:
                    handled = 1;
                    break;

                default:
                    resp_len = ProtocolCommon_Nack(req->dst, req->src,
                                                   PROTO_ERR_UNSUPPORTED_CMD,
                                                   resp, sizeof(resp));
                    handled = 1;
                    break;
            }
        }
        else
        {
            resp_len = ProtocolCommon_Nack(req->dst, req->src,
                                           PROTO_ERR_INVALID_PARAM,
                                           resp, sizeof(resp));
            handled = 1;
        }
    }
    /* 3. Display 基础操作码：0x11 ~ 0x1E */
    else if (req->cmd >= CMD_DISP_SET_BRIGHTNESS && req->cmd <= CMD_DISP_FACTORY_RESET)
    {
        switch (req->cmd)
        {
            /* ---- 事件类（Core -> Display，无需响应） ---- */
            case CMD_DISP_INPUT_EVENT:
                Display_HandleInputEvent(req);
                handled = 1;
                break;

            /* ---- 查询类（Display -> Core，需要 ACK + 数据） ---- */
            case CMD_DISP_GET_BRIGHTNESS:
                resp_len = display_dispatch_req(req, Display_HandleGetBrightness,
                                                resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_DISP_GET_ROTATION:
                resp_len = display_dispatch_req(req, Display_HandleGetRotation,
                                                resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_DISP_GET_SCREEN_STATE:
                resp_len = display_dispatch_req(req, Display_HandleGetScreenState,
                                                resp, sizeof(resp));
                handled = 1;
                break;

            /* ---- 动作类（Display -> Core，需要 ACK/NACK） ---- */
            case CMD_DISP_MUSIC_CONTROL:
                resp_len = display_dispatch_req(req, Display_HandleMusicControl,
                                                resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_DISP_VOLUME_CONTROL:
                resp_len = display_dispatch_req(req, Display_HandleVolumeControl,
                                                resp, sizeof(resp));
                handled = 1;
                break;

            /* ---- 设置类（Core -> Display，发送即忘） ---- */
            case CMD_DISP_SET_BRIGHTNESS:
            case CMD_DISP_SHOW_PAGE:
            case CMD_DISP_UPDATE_STATUS:
            case CMD_DISP_SET_ROTATION:
            case CMD_DISP_SCREEN_CONTROL:
            case CMD_DISP_SHOW_NOTICE:
            case CMD_DISP_MUSIC_STATUS:
            case CMD_DISP_FACTORY_RESET:
                /* Display 收到后执行，不回复 */
                handled = 1;
                break;

            default:
                resp_len = ProtocolCommon_Nack(req->dst, req->src,
                                               PROTO_ERR_UNSUPPORTED_CMD,
                                               resp, sizeof(resp));
                handled = 1;
                break;
        }
    }

    /* 发送响应（如果有） */
    if (handled && resp_len > 0)
        Display_Send_Data(display, resp, resp_len);

    Protocol_ResetRxCtx(&display->rx_ctx);
}

/* ============================================================================
 * 基础操作码 handler 实现
 * ============================================================================ */

/* ---- 输入事件（事件类，Core -> Display） ---- */
static void Display_HandleInputEvent(const protocol_frame_t *req)
{
    /* Core 将输入事件转发给 Display，Display 侧更新 UI。
     * 此 handler 在 Core 侧不需要做任何事，因为 Core 是发送方。
     * 如果此函数被调用，说明 Display 发来了 INPUT_EVENT（异常情况），
     * 静默忽略即可。 */
    (void)req;
}

/* ---- 音乐播放控制（动作类，Display -> Core） ---- */
static uint8_t Display_HandleMusicControl(const protocol_frame_t *req,
                                          uint8_t *resp_buf, uint16_t resp_size,
                                          uint8_t *resp_len)
{
    uint8_t ctrl_type;
    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;

    if (PROTO_DATA_LEN(*req) < 1)
        return 0;

    ctrl_type = req->data[0];

    switch (ctrl_type)
    {
        case MUSIC_CTRL_TOGGLE:
            if (Audio_GetState() == AUDIO_STATE_PLAYING)
                Audio_Pause();
            else if (Audio_GetState() == AUDIO_STATE_PAUSED)
                Audio_Resume();
            else
                return 0; /* IDLE/STOPPED 状态无法 toggle */
            break;
        case MUSIC_CTRL_PLAY:
            /* 播放需要指定文件，通过 CMD_DISP_EXT_PLAY_MUSIC 实现 */
            if (Audio_GetState() == AUDIO_STATE_PAUSED)
                Audio_Resume();
            else
                return 0;
            break;
        case MUSIC_CTRL_PAUSE:
            if (Audio_GetState() == AUDIO_STATE_PLAYING)
                Audio_Pause();
            else
                return 0;
            break;
        case MUSIC_CTRL_STOP:
            if (Audio_GetState() == AUDIO_STATE_PLAYING ||
                Audio_GetState() == AUDIO_STATE_PAUSED)
                Audio_PlayStop();
            else
                return 0;
            break;
        case MUSIC_CTRL_PREV:
        case MUSIC_CTRL_NEXT:
            /* 上一首/下一首需要播放列表管理，待实现 */
            return 0;
        case MUSIC_CTRL_SEEK_FWD:
        case MUSIC_CTRL_SEEK_BACK:
            /* 快进/快退需要 WAV 文件 seek，待实现 */
            return 0;
        case MUSIC_CTRL_SET_MODE:
            /* 播放模式设置，待实现 */
            break;
        default:
            return 0;
    }

    return 1; /* ACK 空 */
}

/* ---- 音量控制（动作/查询类，Display -> Core） ---- */
static uint8_t Display_HandleVolumeControl(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len)
{
    uint8_t op;

    if (PROTO_DATA_LEN(*req) < 1)
        return 0;

    op = req->data[0];

    switch (op)
    {
        case VOLUME_OP_SET:
            if (PROTO_DATA_LEN(*req) < 2)
                return 0;
            Audio_SetVolume(req->data[1]);
            return 1; /* ACK 空 */

        case VOLUME_OP_GET:
            if (resp_size < 1)
                return 0;
            resp_buf[0] = Audio_GetVolume();
            *resp_len = 1;
            return 1;

        default:
            return 0;
    }
}

/* ---- 查询亮度（查询类，Display -> Core） ---- */
static uint8_t Display_HandleGetBrightness(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len)
{
    (void)req;
    /* 亮度由 Display 自身管理，Core 不存储亮度值。
     * 此 handler 在 Core 侧被调用说明 Display 发来了 GET_BRIGHTNESS，
     * 返回默认值或上次设置的值。 */
    if (resp_size < 1)
        return 0;
    resp_buf[0] = 50; /* 默认亮度 */
    *resp_len = 1;
    return 1;
}

/* ---- 查询旋转角度（查询类，Display -> Core） ---- */
static uint8_t Display_HandleGetRotation(const protocol_frame_t *req,
                                         uint8_t *resp_buf, uint16_t resp_size,
                                         uint8_t *resp_len)
{
    (void)req;
    if (resp_size < 1)
        return 0;
    resp_buf[0] = 0; /* 默认角度 0 */
    *resp_len = 1;
    return 1;
}

/* ---- 查询屏幕状态（查询类，Display -> Core） ---- */
static uint8_t Display_HandleGetScreenState(const protocol_frame_t *req,
                                            uint8_t *resp_buf, uint16_t resp_size,
                                            uint8_t *resp_len)
{
    (void)req;
    if (resp_size < 4)
        return 0;
    /* 返回屏幕状态：[亮屏/息屏:1][亮度:1][旋转角度:1][自动息屏超时:1] */
    resp_buf[0] = 0x01; /* 亮屏 */
    resp_buf[1] = 50;   /* 亮度 */
    resp_buf[2] = 0;    /* 旋转角度 */
    resp_buf[3] = 30;   /* 自动息屏超时(秒) */
    *resp_len = 4;
    return 1;
}

/* ============================================================================
 * 扩展操作码 handler 实现
 * ============================================================================ */

/* ---- 启动应用（动作类，Display -> Core） ---- */
static uint8_t Display_HandleAppLaunch(const protocol_frame_t *req,
                                       uint8_t *resp_buf, uint16_t resp_size,
                                       uint8_t *resp_len)
{
    (void)req;
    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;
    /* 应用管理待实现 */
    return 1;
}

/* ---- 关闭应用（动作类，Display -> Core） ---- */
static uint8_t Display_HandleAppClose(const protocol_frame_t *req,
                                      uint8_t *resp_buf, uint16_t resp_size,
                                      uint8_t *resp_len)
{
    (void)req;
    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;
    return 1;
}

/* ---- 应用数据传输（数据通道，双向） ---- */
static uint8_t Display_HandleAppData(const protocol_frame_t *req,
                                     uint8_t *resp_buf, uint16_t resp_size,
                                     uint8_t *resp_len)
{
    (void)req;
    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;
    /* 应用层数据通道，协议层不强制 ACK */
    return 1;
}

/* ---- 获取模块状态（查询类，Display -> Core） ---- */
static uint8_t Display_HandleGetModuleStatus(const protocol_frame_t *req,
                                             uint8_t *resp_buf, uint16_t resp_size,
                                             uint8_t *resp_len)
{
    (void)req;
    /* 返回扩展码 0x04 + 事件类型 0x02 + 模块列表
     * 格式: [0x04][0x02][N][模块1_ID][模块1_类型][模块1_子类型]...
     * 当前返回空列表 */
    if (resp_size < 3)
        return 0;
    resp_buf[0] = CMD_DISP_EXT_MODULE_STATUS;
    resp_buf[1] = 0x02; /* 完整模块列表 */
    resp_buf[2] = 0;    /* 模块数量 */
    *resp_len = 3;
    return 1;
}

/* ---- 请求文件列表（查询类，Display -> Core） ---- */
static uint8_t Display_HandleRequestFileList(const protocol_frame_t *req,
                                             uint8_t *resp_buf, uint16_t resp_size,
                                             uint8_t *resp_len)
{
    /* 请求文件列表需要异步回调，此处返回 ACK 表示已接受请求，
     * 实际文件列表通过 Display_SendFileList 异步发送 */
    (void)req;
    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;
    return 1;
}

/* ---- 文件读取（动作类，触发 Bulk Mode） ---- */
static uint8_t Display_HandleFileRead(const protocol_frame_t *req,
                                      uint8_t *resp_buf, uint16_t resp_size,
                                      uint8_t *resp_len)
{
    /* ACK + Bulk 参数: [总大小:4][块大小:2] */
    /* 实际文件读取需要 CH378 操作，此处返回框架 */
    (void)req;
    if (resp_size < 6)
        return 0;
    resp_buf[0] = 0; resp_buf[1] = 0; resp_buf[2] = 0; resp_buf[3] = 0; /* 总大小 */
    resp_buf[4] = (DISPLAY_BULK_BLOCK_SIZE >> 8) & 0xFF;
    resp_buf[5] = DISPLAY_BULK_BLOCK_SIZE & 0xFF;
    *resp_len = 6;
    return 1;
}

/* ---- 文件保存（动作类，触发 Bulk Mode） ---- */
static uint8_t Display_HandleFileSave(const protocol_frame_t *req,
                                      uint8_t *resp_buf, uint16_t resp_size,
                                      uint8_t *resp_len)
{
    (void)req;
    if (resp_size < 6)
        return 0;
    resp_buf[0] = 0; resp_buf[1] = 0; resp_buf[2] = 0; resp_buf[3] = 0;
    resp_buf[4] = (DISPLAY_BULK_BLOCK_SIZE >> 8) & 0xFF;
    resp_buf[5] = DISPLAY_BULK_BLOCK_SIZE & 0xFF;
    *resp_len = 6;
    return 1;
}

/* ---- 文件操作（动作类，Display -> Core） ---- */
static uint8_t Display_HandleFileOperation(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len)
{
    uint8_t op_type;
    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;

    if (PROTO_DATA_LEN(*req) < 2) /* ext_code + op_type 至少 2 字节 */
        return 0;

    op_type = req->data[1];

    switch (op_type)
    {
        case FILE_OP_MKDIR:
        case FILE_OP_DELETE:
        case FILE_OP_RMDIR:
        case FILE_OP_RENAME:
            /* 需要路径参数，待 CH378 集成实现 */
            return 1;
        default:
            return 0;
    }
}

/* ---- 播放指定音乐文件（动作类，Display -> Core） ---- */
static uint8_t Display_HandlePlayMusic(const protocol_frame_t *req,
                                       uint8_t *resp_buf, uint16_t resp_size,
                                       uint8_t *resp_len)
{
    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;

    /* DATA[0] = ext_code, DATA[1..N] = 文件路径 */
    if (PROTO_DATA_LEN(*req) < 2)
        return 0;

    /* TODO: 使用 CH378 打开文件并启动 WAV 播放 */
    return 1;
}

/* ---- 蓝牙控制请求（动作类，Display -> Core） ---- */
static uint8_t Display_HandleBtControl(const protocol_frame_t *req,
                                       uint8_t *resp_buf, uint16_t resp_size,
                                       uint8_t *resp_len)
{
    uint8_t ctrl_type;
    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;

    if (PROTO_DATA_LEN(*req) < 2)
        return 0;

    ctrl_type = req->data[1];

    switch (ctrl_type)
    {
        case BT_CTRL_SCAN_START:
        case BT_CTRL_SCAN_STOP:
        case BT_CTRL_CONNECT:
        case BT_CTRL_DISCONNECT:
        case BT_CTRL_DISCONNECT_ALL:
        case BT_CTRL_SET_DISCOVERABLE:
            /* 需要通过 SPI 向 CH585F 发送命令，待实现 */
            return 1;
        default:
            return 0;
    }
}

/* ---- 保存配置（动作类，Display -> Core） ---- */
static uint8_t Display_HandleSaveConfig(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len)
{
    (void)req;
    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;
    /* TODO: 使用 CH378 保存配置文件 */
    return 1;
}

/* ---- 加载配置（查询类，Display -> Core） ---- */
static uint8_t Display_HandleLoadConfig(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len)
{
    (void)req;
    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;
    /* TODO: 使用 CH378 加载配置文件 */
    return 1;
}

/* ---- 设置 RGB 灯效模式（动作类，Display -> Core） ---- */
static uint8_t Display_HandleSetRgbMode(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len)
{
    (void)req;
    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;
    /* TODO: 通过 Submodel 转发给 RGB 模块 */
    return 1;
}

/* ---- 批量传输控制（控制类，双向） ---- */
static uint8_t Display_HandleBulkTransfer(const protocol_frame_t *req,
                                          uint8_t *resp_buf, uint16_t resp_size,
                                          uint8_t *resp_len)
{
    (void)req;
    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;
    /* TODO: Bulk transfer 完成确认处理 */
    return 1;
}

/* ---- 模块状态事件（事件类，Core -> Display） ---- */
static void Display_HandleModuleStatus(const protocol_frame_t *req)
{
    (void)req;
    /* Core 侧不需要处理，此事件是 Core 发给 Display 的 */
}

/* ---- 蓝牙事件（事件类，Core -> Display） ---- */
static void Display_HandleBtEvent(const protocol_frame_t *req)
{
    (void)req;
}

/* ---- Submodel 事件（事件类，Core -> Display） ---- */
static void Display_HandleSubmodelEvent(const protocol_frame_t *req)
{
    (void)req;
}

/* ---- 电源事件（事件类，Core -> Display） ---- */
static void Display_HandlePowerEvent(const protocol_frame_t *req)
{
    (void)req;
}

/* ---- 配置结果（事件类，Core -> Display） ---- */
static void Display_HandleConfigResult(const protocol_frame_t *req)
{
    (void)req;
}

/* ---- 错误上报（事件类，Display -> Core） ---- */
static void Display_HandleErrorReport(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 记录 Display 上报的错误 */
}

/* ============================================================================
 * Core 主动发送函数实现（Core -> Display 方向）
 * ============================================================================ */

void Display_SendInputEvent(display_t *display, uint8_t dev_type,
                            const uint8_t *report, uint8_t report_len)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[1 + 8]; /* dev_type + max report (keyboard=8) */
    uint16_t frame_len;

    if (display == NULL || report == NULL || report_len == 0)
        return;
    if (report_len > 8)
        report_len = 8; /* 限制最大报告长度 */

    data[0] = dev_type;
    memcpy(&data[1], report, report_len);

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_INPUT_EVENT,
                                   data, 1 + report_len,
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display, buf, frame_len);
}

void Display_SendMusicStatus(display_t *display)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[11 + 64]; /* 状态(1) + 时间(4) + 时长(4) + 音量(1) + 曲目名(变长) */
    uint16_t frame_len;
    audio_state_t state;
    uint32_t play_time;
    const char *track_name;
    uint8_t track_len;
    uint8_t vol;

    if (display == NULL)
        return;

    state = Audio_GetState();
    play_time = Audio_GetPlayTime_ms();
    track_name = Audio_GetCurrentTrackName();
    vol = Audio_GetVolume();

    /* 播放状态 */
    switch (state)
    {
        case AUDIO_STATE_IDLE:    data[0] = MUSIC_STATE_IDLE; break;
        case AUDIO_STATE_PLAYING: data[0] = MUSIC_STATE_PLAYING; break;
        case AUDIO_STATE_PAUSED:  data[0] = MUSIC_STATE_PAUSED; break;
        case AUDIO_STATE_STOPPED: data[0] = MUSIC_STATE_STOPPED; break;
        default:                  data[0] = MUSIC_STATE_IDLE; break;
    }

    /* 当前播放时间（大端） */
    data[1] = (play_time >> 24) & 0xFF;
    data[2] = (play_time >> 16) & 0xFF;
    data[3] = (play_time >> 8) & 0xFF;
    data[4] = play_time & 0xFF;

    /* 总时长（大端，暂填 0 = 未知） */
    data[5] = 0; data[6] = 0; data[7] = 0; data[8] = 0;

    /* 音量 */
    data[9] = vol;

    /* 曲目名称 */
    if (track_name != NULL)
    {
        track_len = (uint8_t)strlen(track_name);
        if (track_len > 63)
            track_len = 63;
        memcpy(&data[10], track_name, track_len);
    }
    else
    {
        track_len = 0;
    }

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_MUSIC_STATUS,
                                   data, 10 + track_len,
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display, buf, frame_len);
}

void Display_SendModuleStatus(display_t *display, uint8_t evt_type,
                              uint8_t module_id, uint8_t module_type,
                              uint8_t module_subtype)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[5]; /* ext_code + evt_type + module_id + type + subtype */
    uint16_t frame_len;

    if (display == NULL)
        return;

    data[0] = CMD_DISP_EXT_MODULE_STATUS;
    data[1] = evt_type;
    data[2] = module_id;
    data[3] = module_type;
    data[4] = module_subtype;

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_EXTENSION,
                                   data, 5,
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display, buf, frame_len);
}

void Display_SendModuleList(display_t *display)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[3 + 3 * 8]; /* ext_code + evt_type + count + max 8 modules */
    uint16_t frame_len;
    uint8_t count = 0;

    if (display == NULL)
        return;

    data[0] = CMD_DISP_EXT_MODULE_STATUS;
    data[1] = 0x02; /* 完整模块列表 */
    /* TODO: 填充实际模块列表 */
    data[2] = count;

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_EXTENSION,
                                   data, 3 + count * 3,
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display, buf, frame_len);
}

void Display_SendFileList(display_t *display, uint8_t status,
                          const uint8_t *list_data, uint16_t list_len)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[2 + 255]; /* ext_code + status + list_data */
    uint16_t frame_len;
    uint16_t payload_len;

    if (display == NULL)
        return;

    data[0] = CMD_DISP_EXT_FILE_LIST;
    data[1] = status;

    payload_len = list_len;
    if (payload_len > 255 - 2)
        payload_len = 255 - 2;

    if (list_data != NULL && payload_len > 0)
        memcpy(&data[2], list_data, payload_len);

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_EXTENSION,
                                   data, 2 + (uint8_t)payload_len,
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display, buf, frame_len);
}

void Display_SendBtEvent(display_t *display, uint8_t evt_type,
                         const uint8_t *evt_data, uint8_t evt_data_len)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[2 + 64]; /* ext_code + evt_type + evt_data */
    uint16_t frame_len;

    if (display == NULL)
        return;

    data[0] = CMD_DISP_EXT_BT_EVENT;
    data[1] = evt_type;

    if (evt_data != NULL && evt_data_len > 0)
    {
        if (evt_data_len > 64)
            evt_data_len = 64;
        memcpy(&data[2], evt_data, evt_data_len);
    }
    else
    {
        evt_data_len = 0;
    }

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_EXTENSION,
                                   data, 2 + evt_data_len,
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display, buf, frame_len);
}

void Display_SendSubmodelEvent(display_t *display, uint8_t sub_type,
                               uint8_t sub_cmd,
                               const uint8_t *evt_data, uint8_t evt_data_len)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[3 + 64]; /* ext_code + sub_type + sub_cmd + evt_data */
    uint16_t frame_len;

    if (display == NULL)
        return;

    data[0] = CMD_DISP_EXT_SUBMODEL_EVENT;
    data[1] = sub_type;
    data[2] = sub_cmd;

    if (evt_data != NULL && evt_data_len > 0)
    {
        if (evt_data_len > 64)
            evt_data_len = 64;
        memcpy(&data[3], evt_data, evt_data_len);
    }
    else
    {
        evt_data_len = 0;
    }

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_EXTENSION,
                                   data, 3 + evt_data_len,
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display, buf, frame_len);
}

void Display_SendPowerEvent(display_t *display, uint8_t evt_type,
                            const uint8_t *evt_data, uint8_t evt_data_len)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[2 + 16]; /* ext_code + evt_type + evt_data */
    uint16_t frame_len;

    if (display == NULL)
        return;

    data[0] = CMD_DISP_EXT_POWER_EVENT;
    data[1] = evt_type;

    if (evt_data != NULL && evt_data_len > 0)
    {
        if (evt_data_len > 16)
            evt_data_len = 16;
        memcpy(&data[2], evt_data, evt_data_len);
    }
    else
    {
        evt_data_len = 0;
    }

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_EXTENSION,
                                   data, 2 + evt_data_len,
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display, buf, frame_len);
}

void Display_SendConfigResult(display_t *display, uint8_t result,
                              uint8_t err_code,
                              const uint8_t *config_data, uint16_t config_len)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[3 + 64]; /* ext_code + result + err_code + config_data */
    uint16_t frame_len;
    uint16_t payload_len;

    if (display == NULL)
        return;

    data[0] = CMD_DISP_EXT_CONFIG_RESULT;
    data[1] = result;
    data[2] = err_code;

    payload_len = config_len;
    if (payload_len > 64)
        payload_len = 64;

    if (config_data != NULL && payload_len > 0)
        memcpy(&data[3], config_data, payload_len);

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_EXTENSION,
                                   data, 3 + (uint8_t)payload_len,
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display, buf, frame_len);
}

void Display_SendUpdateStatus(display_t *display, const uint8_t *status_data,
                              uint8_t status_len)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint16_t frame_len;

    if (display == NULL || status_data == NULL || status_len == 0)
        return;

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_UPDATE_STATUS,
                                   status_data, status_len,
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display, buf, frame_len);
}

void Display_SendScreenControl(display_t *display, uint8_t ctrl_type,
                               uint8_t param)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[2]; /* ctrl_type + param */
    uint16_t frame_len;

    if (display == NULL)
        return;

    data[0] = ctrl_type;
    data[1] = param;

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_SCREEN_CONTROL,
                                   data, (ctrl_type == SCREEN_CTRL_SET_TIMEOUT) ? 2 : 1,
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display, buf, frame_len);
}

void Display_SendNotice(display_t *display, uint8_t priority,
                        const char *title, const char *content)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[1 + 16 + 128]; /* priority + title(16) + content */
    uint16_t frame_len;
    uint8_t content_len;

    if (display == NULL)
        return;

    data[0] = priority;

    /* 标题固定 16 字节 */
    memset(&data[1], 0, 16);
    if (title != NULL)
    {
        uint8_t tlen = (uint8_t)strlen(title);
        if (tlen > 16) tlen = 16;
        memcpy(&data[1], title, tlen);
    }

    /* 内容变长 */
    content_len = 0;
    if (content != NULL)
    {
        content_len = (uint8_t)strlen(content);
        if (content_len > 128)
            content_len = 128;
        memcpy(&data[17], content, content_len);
    }

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_SHOW_NOTICE,
                                   data, 17 + content_len,
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display, buf, frame_len);
}

void Display_SendSubdispContent(display_t *display, uint8_t content_type,
                                const uint8_t *content, uint16_t content_len)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[2 + 64]; /* ext_code + content_type + content */
    uint16_t frame_len;
    uint16_t payload_len;

    if (display == NULL)
        return;

    data[0] = CMD_DISP_EXT_SUBDISP_CONTENT;
    data[1] = content_type;

    payload_len = content_len;
    if (payload_len > 64)
        payload_len = 64;

    if (content != NULL && payload_len > 0)
        memcpy(&data[2], content, payload_len);

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_EXTENSION,
                                   data, 2 + (uint8_t)payload_len,
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display, buf, frame_len);
}
