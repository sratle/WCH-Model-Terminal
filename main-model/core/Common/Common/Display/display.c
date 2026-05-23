#include "display.h"
#include "../Protocol/protocol_common.h"
#include "../CS43131/cs43131.h"
#include "../CH378/CH378.h"
#include "../CH585F/ch585f.h"
#include "../hardware.h"
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
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint16_t len;

    len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                             CMD_GET_TYPE, NULL, 0,
                             buf, sizeof(buf));
    if (len > 0)
    {
        Display_Send_Data(display, buf, len);
        display->type_requested = 1;
        printf("[Display] CMD_GET_TYPE sent\r\n");
    }
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

    /* 0. 处理 Display 返回的 ACK 响应（Core 发送请求后 Display 的回复） */
    if (req->cmd == CMD_ACK)
    {
        module_identity_t id;
        if (Protocol_ParseIdentity(req, &id))
        {
            /* 首次 GET_TYPE 响应 */
            if (display->type_requested && !display->type_received)
            {
                display->type_received = 1;
                display->type_id = id.subtype;
                display->identity = id;
                printf("[Display] GET_TYPE ACK: type=0x%02X subtype=0x%02X hw=0x%02X fw=%d.%d\r\n",
                       id.type, id.subtype, id.hw_ver, id.fw_major, id.fw_minor);
            }
            /* 更新心跳在线状态 */
            Hardware_Hb_MarkOnline(MODULE_ID_DISPLAY, id.type, id.subtype);
        }
        else if (display->type_requested && !display->type_received)
        {
            printf("[Display] GET_TYPE ACK: data too short (len=%d)\r\n", req->len);
        }
        handled = 1;
    }
    /* 处理 Display 返回的 NACK 响应 */
    else if (req->cmd == CMD_NACK)
    {
        if (display->type_requested && !display->type_received)
        {
            uint8_t err_code = (req->len >= 2) ? req->data[0] : 0xFF;
            printf("[Display] GET_TYPE NACK: err=0x%02X\r\n", err_code);
        }
        handled = 1;
    }
    /* 1. 通用命令（0x00~0x0F） */
    else if (ProtocolCommon_Dispatch(req, &display_common_cb,
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

    /* 音乐状态变化同步：仅在状态/音量/曲目变化时推送给 Display */
    if (display->type_received && Audio_IsStatusDirty()) {
        Display_SendMusicStatus(display);
        Audio_ClearStatusDirty();
    }
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
            printf("[Display] VOLUME_OP_SET: vol=%d\r\n", req->data[1]);
            return 1; /* ACK 空 */

        case VOLUME_OP_GET:
            if (resp_size < 1)
                return 0;
            resp_buf[0] = Audio_GetVolume();
            *resp_len = 1;
            printf("[Display] VOLUME_OP_GET: vol=%d\r\n", resp_buf[0]);
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
/* ---- 请求文件列表（异步发送） ---- */

/* 目录列表回调：收集文件条目 */
typedef struct {
    uint8_t buf[253]; /* max data payload: 255 - ext(1) - status(1) - count(1) */
    uint8_t count;
    uint8_t full;
} file_list_collect_t;

static file_list_collect_t s_fl_collect;

static void display_file_list_cb(const char *name, uint8_t is_dir, uint32_t size)
{
    if (s_fl_collect.full) return;
    if (s_fl_collect.count >= 12) { s_fl_collect.full = 1; return; } /* max 12 entries per frame */

    uint16_t off = (uint16_t)s_fl_collect.count * 21;
    if (off + 21 > sizeof(s_fl_collect.buf)) { s_fl_collect.full = 1; return; }

    uint8_t *entry = &s_fl_collect.buf[off];
    entry[0] = is_dir ? 0x01 : 0x00;
    entry[1] = (size >> 24) & 0xFF;
    entry[2] = (size >> 16) & 0xFF;
    entry[3] = (size >> 8) & 0xFF;
    entry[4] = size & 0xFF;

    /* Copy name, max 16 bytes, zero-padded */
    uint8_t nlen = (uint8_t)strlen(name);
    if (nlen > 16) nlen = 16;
    memcpy(&entry[5], name, nlen);
    if (nlen < 16) memset(&entry[5 + nlen], 0, 16 - nlen);

    s_fl_collect.count++;
}

static uint8_t Display_HandleRequestFileList(const protocol_frame_t *req,
                                             uint8_t *resp_buf, uint16_t resp_size,
                                             uint8_t *resp_len)
{
    char req_path[65];
    uint8_t path_len;

    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;

    if (display_ptr == NULL)
        return 0;

    /* CH378 被音乐流式播放占用时，拒绝文件列表请求 */
    if (Audio_IsStreaming()) {
        printf("[Display] FileList rejected: CH378 busy (music streaming)\r\n");
        Display_SendFileList(display_ptr, 0x04 /* PROTO_ERR_BUSY */, NULL, 0);
        return 1;
    }

    /* Extract path: req->data[0] = ext code, req->data[1..] = path string */
    path_len = (uint8_t)(PROTO_DATA_LEN(*req) > 1 ? PROTO_DATA_LEN(*req) - 1 : 0);
    if (path_len > 64) path_len = 64;
    if (path_len > 0)
        memcpy(req_path, &req->data[1], path_len);
    req_path[path_len] = '\0';

    /* Save current CH378 directory */
    char saved_path[65];
    const char *cur = CH378_Dir_Get_Path();
    strncpy(saved_path, cur, sizeof(saved_path) - 1);
    saved_path[sizeof(saved_path) - 1] = '\0';

    /* Navigate to requested path */
    CH378_Dir_Go_Root(&ch378_g);
    if (req_path[0] != '\0') {
        /* Parse path components separated by '\\' */
        char path_copy[65];
        strncpy(path_copy, req_path, sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';

        char *p = path_copy;
        while (*p) {
            /* Skip leading separator */
            if (*p == '\\') p++;
            if (*p == '\0') break;

            /* Find end of component */
            char *end = p;
            while (*end && *end != '\\') end++;
            if (*end == '\\') { *end = '\0'; end++; }

            /* Enter directory */
            if (CH378_Dir_Enter(&ch378_g, p) != 0) {
                /* Directory not found, send error status */
                Display_SendFileList(display_ptr, 0x01, NULL, 0);
                /* Restore original path */
                CH378_Dir_Go_Root(&ch378_g);
                if (saved_path[0] != '\0') {
                    char *sp = saved_path;
                    while (*sp) {
                        if (*sp == '\\') sp++;
                        if (*sp == '\0') break;
                        char *se = sp;
                        while (*se && *se != '\\') se++;
                        if (*se == '\\') { *se = '\0'; se++; }
                        CH378_Dir_Enter(&ch378_g, sp);
                        sp = se;
                    }
                }
                return 1; /* ACK already sent via Display_SendFileList */
            }
            p = end;
        }
    }

    /* Collect directory entries */
    memset(&s_fl_collect, 0, sizeof(s_fl_collect));
    CH378_Dir_List(&ch378_g, display_file_list_cb);

    /* Send file list to Display */
    uint16_t list_len = (uint16_t)s_fl_collect.count * 21;
    /* Prepend count byte: Display_SendFileList expects [count][entries...] */
    /* But Display_SendFileList already adds ext_code + status, so we pass:
     *   list_data = [count_byte][entry0][entry1]...
     *   list_len  = 1 + entries * 21                                      */
    uint8_t list_buf[254];
    list_buf[0] = s_fl_collect.count;
    if (list_len > 0)
        memcpy(&list_buf[1], s_fl_collect.buf, list_len);

    Display_SendFileList(display_ptr, 0x00, list_buf, 1 + list_len);

    /* Restore original CH378 directory */
    CH378_Dir_Go_Root(&ch378_g);
    if (saved_path[0] != '\0') {
        char *sp = saved_path;
        while (*sp) {
            if (*sp == '\\') sp++;
            if (*sp == '\0') break;
            char *se = sp;
            while (*se && *se != '\\') se++;
            if (*se == '\\') { *se = '\0'; se++; }
            CH378_Dir_Enter(&ch378_g, sp);
            sp = se;
        }
    }

    return 1; /* ACK (empty, file list already sent) */
}

/* ---- 文件读取（动作类，触发 Bulk Mode） ---- */
static uint8_t Display_HandleFileRead(const protocol_frame_t *req,
                                      uint8_t *resp_buf, uint16_t resp_size,
                                      uint8_t *resp_len)
{
    char path[65];
    uint8_t path_len;
    uint32_t file_size;
    char dir_part[65];
    char *file_part;
    int i;

    /* ACK + Bulk 参数: [总大小:4][块大小:2] */
    if (resp_size < 6)
        return 0;

    /* Extract path: req->data[0]=ext, req->data[1..]=path */
    path_len = (uint8_t)(PROTO_DATA_LEN(*req) > 1 ? PROTO_DATA_LEN(*req) - 1 : 0);
    if (path_len > 64) path_len = 64;
    if (path_len > 0)
        memcpy(path, &req->data[1], path_len);
    path[path_len] = '\0';

    printf("[Display] FileRead: \"%s\"\r\n", path);

    /* Split path into directory + filename */
    strncpy(dir_part, path, sizeof(dir_part) - 1);
    dir_part[sizeof(dir_part) - 1] = '\0';
    file_part = NULL;
    for (i = (int)strlen(dir_part) - 1; i >= 0; i--) {
        if (dir_part[i] == '\\') {
            dir_part[i] = '\0';
            file_part = &path[i + 1];
            break;
        }
    }
    if (!file_part) { file_part = path; dir_part[0] = '\0'; }

    /* Save and navigate to directory */
    char saved_path[65];
    strncpy(saved_path, CH378_Dir_Get_Path(), sizeof(saved_path) - 1);
    saved_path[sizeof(saved_path) - 1] = '\0';

    CH378_Dir_Go_Root(&ch378_g);
    if (dir_part[0] != '\0') {
        char *p = dir_part;
        while (*p) {
            if (*p == '\\') p++;
            if (*p == '\0') break;
            char *end = p;
            while (*end && *end != '\\') end++;
            if (*end == '\\') { *end = '\0'; end++; }
            if (CH378_Dir_Enter(&ch378_g, p) != 0) {
                printf("[Display] FileRead: cannot enter dir\r\n");
                goto read_fail;
            }
            p = end;
        }
    }

    /* Get file size */
    file_size = CH378_File_GetSize(&ch378_g, file_part);
    if (file_size == 0) {
        printf("[Display] FileRead: file not found or empty\r\n");
        goto read_fail;
    }

    /* Restore directory */
    CH378_Dir_Go_Root(&ch378_g);
    {
        char *sp = saved_path;
        while (*sp) {
            if (*sp == '\\') sp++;
            if (*sp == '\0') break;
            char *se = sp;
            while (*se && *se != '\\') se++;
            if (*se == '\\') { *se = '\0'; se++; }
            CH378_Dir_Enter(&ch378_g, sp);
            sp = se;
        }
    }

    /* Return bulk transfer parameters */
    resp_buf[0] = (file_size >> 24) & 0xFF;
    resp_buf[1] = (file_size >> 16) & 0xFF;
    resp_buf[2] = (file_size >> 8) & 0xFF;
    resp_buf[3] = file_size & 0xFF;
    resp_buf[4] = (DISPLAY_BULK_BLOCK_SIZE >> 8) & 0xFF;
    resp_buf[5] = DISPLAY_BULK_BLOCK_SIZE & 0xFF;
    *resp_len = 6;

    if (display_ptr) {
        display_ptr->bulk_state = BULK_READING;
        display_ptr->bulk_total_size = file_size;
        display_ptr->bulk_received = 0;
        display_ptr->bulk_block_size = DISPLAY_BULK_BLOCK_SIZE;
    }

    printf("[Display] FileRead: size=%lu\r\n", file_size);
    return 1;

read_fail:
    CH378_Dir_Go_Root(&ch378_g);
    {
        char *sp = saved_path;
        while (*sp) {
            if (*sp == '\\') sp++;
            if (*sp == '\0') break;
            char *se = sp;
            while (*se && *se != '\\') se++;
            if (*se == '\\') { *se = '\0'; se++; }
            CH378_Dir_Enter(&ch378_g, sp);
            sp = se;
        }
    }
    return 0;
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
    char path[65];
    uint8_t path_len;
    uint8_t result;

    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;

    if (PROTO_DATA_LEN(*req) < 2) /* ext_code + op_type 至少 2 字节 */
        return 0;

    /* CH378 被音乐流式播放占用时，拒绝文件操作 */
    if (Audio_IsStreaming()) {
        printf("[Display] FileOp rejected: CH378 busy\r\n");
        return 0; /* NACK */
    }

    op_type = req->data[1];

    /* Extract path: req->data[2..] = path string */
    path_len = (uint8_t)(PROTO_DATA_LEN(*req) > 2 ? PROTO_DATA_LEN(*req) - 2 : 0);
    if (path_len > 64) path_len = 64;
    if (path_len > 0)
        memcpy(path, &req->data[2], path_len);
    path[path_len] = '\0';

    printf("[Display] FileOp: type=%d path=\"%s\"\r\n", op_type, path);

    switch (op_type)
    {
        case FILE_OP_MKDIR:
            result = CH378_Create_Long_Dir((const uint8_t *)path, NULL);
            if (result != 0) {
                printf("[Display] mkdir failed: %d\r\n", result);
                return 0;
            }
            return 1;

        case FILE_OP_DELETE:
            result = CH378_Erase_Long_Name(NULL, (const uint8_t *)path);
            if (result != 0) {
                /* Fallback to short name erase */
                result = CH378FileErase((uint8_t *)path);
            }
            if (result != 0) {
                printf("[Display] delete failed: %d\r\n", result);
                return 0;
            }
            return 1;

        case FILE_OP_RMDIR:
            /* CH378 can only erase empty directories */
            result = CH378FileErase((uint8_t *)path);
            if (result != 0) {
                printf("[Display] rmdir failed: %d\r\n", result);
                return 0;
            }
            return 1;

        case FILE_OP_RENAME:
            /* Rename requires old + new name, not yet fully implemented */
            printf("[Display] rename not implemented\r\n");
            return 0;

        default:
            return 0;
    }
}

/* ---- 播放指定音乐文件（动作类，Display -> Core） ---- */
static uint8_t Display_HandlePlayMusic(const protocol_frame_t *req,
                                       uint8_t *resp_buf, uint16_t resp_size,
                                       uint8_t *resp_len)
{
    char path[65];
    uint8_t path_len;
    uint8_t header_buf[512];
    wav_info_t wav_info;
    uint8_t status;
    uint16_t real_count;

    (void)resp_buf;
    (void)resp_size;
    (void)resp_len;

    /* DATA[0] = ext_code, DATA[1..N] = 文件路径 */
    if (PROTO_DATA_LEN(*req) < 2)
        return 0;

    path_len = (uint8_t)(PROTO_DATA_LEN(*req) - 1);
    if (path_len > 64) path_len = 64;
    memcpy(path, &req->data[1], path_len);
    path[path_len] = '\0';

    printf("[Display] PlayMusic: \"%s\"\r\n", path);

    /* Stop current playback */
    Audio_PlayStop();

    /* Save current directory */
    char saved_path[65];
    const char *cur = CH378_Dir_Get_Path();
    strncpy(saved_path, cur, sizeof(saved_path) - 1);
    saved_path[sizeof(saved_path) - 1] = '\0';

    /* Parse path to get directory + filename */
    char dir_path[65];
    char *filename;
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';

    /* Find last backslash to split dir + filename */
    filename = NULL;
    for (int i = (int)strlen(dir_path) - 1; i >= 0; i--) {
        if (dir_path[i] == '\\') {
            dir_path[i] = '\0';
            filename = &path[i + 1];
            break;
        }
    }
    if (filename == NULL) {
        /* No directory separator, file is in current dir */
        filename = path;
        dir_path[0] = '\0';
    }

    /* Navigate to file directory */
    CH378_Dir_Go_Root(&ch378_g);
    if (dir_path[0] != '\0') {
        char *p = dir_path;
        while (*p) {
            if (*p == '\\') p++;
            if (*p == '\0') break;
            char *end = p;
            while (*end && *end != '\\') end++;
            if (*end == '\\') { *end = '\0'; end++; }
            if (CH378_Dir_Enter(&ch378_g, p) != 0) {
                printf("[Display] PlayMusic: cannot enter dir \"%s\"\r\n", p);
                goto restore_and_fail;
            }
            p = end;
        }
    }

    /* Open file and read WAV header */
    status = CH378FileOpen((uint8_t *)filename);
    if (status != 0x14 /* USB_INT_DISK_READ */) {
        printf("[Display] PlayMusic: cannot open \"%s\" (status=0x%02X)\r\n", filename, status);
        goto restore_and_fail;
    }

    /* Read first 512 bytes (WAV header) */
    uint8_t hdr_status = CH378ByteRead(header_buf, sizeof(header_buf), &real_count);
    if (hdr_status != 0 || real_count < 44) {
        printf("[Display] PlayMusic: cannot read header\r\n");
        CH378FileClose(0);
        goto restore_and_fail;
    }

    /* Parse WAV header */
    if (Audio_ParseWAVHeader(header_buf, &wav_info) != 0) {
        printf("[Display] PlayMusic: invalid WAV header\r\n");
        CH378FileClose(0);
        goto restore_and_fail;
    }

    /* Seek to data chunk */
    if (wav_info.data_offset > 0) {
        CH378ByteLocate(wav_info.data_offset);
    }

    /* Set track name for display */
    Audio_SetCurrentTrack(filename);

    /* Start streaming playback */
    Audio_PlayWAV_Start(&wav_info);

    /* Restore directory */
    CH378_Dir_Go_Root(&ch378_g);
    if (saved_path[0] != '\0') {
        char *sp = saved_path;
        while (*sp) {
            if (*sp == '\\') sp++;
            if (*sp == '\0') break;
            char *se = sp;
            while (*se && *se != '\\') se++;
            if (*se == '\\') { *se = '\0'; se++; }
            CH378_Dir_Enter(&ch378_g, sp);
            sp = se;
        }
    }

    printf("[Display] PlayMusic: started \"%s\" (SR=%lu, %u-bit, %u-ch)\r\n",
           filename, wav_info.sample_rate, wav_info.bits_per_sample, wav_info.num_channels);
    return 1;

restore_and_fail:
    /* Restore directory on failure */
    CH378_Dir_Go_Root(&ch378_g);
    if (saved_path[0] != '\0') {
        char *sp = saved_path;
        while (*sp) {
            if (*sp == '\\') sp++;
            if (*sp == '\0') break;
            char *se = sp;
            while (*se && *se != '\\') se++;
            if (*se == '\\') { *se = '\0'; se++; }
            CH378_Dir_Enter(&ch378_g, sp);
            sp = se;
        }
    }
    return 0;
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

void Display_SendHidStatus(display_t *display, uint8_t dev_type, uint8_t connected)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[3]; /* ext_code + evt + dev_type */
    uint16_t frame_len;

    if (display == NULL)
        return;

    data[0] = CMD_DISP_EXT_HID_STATUS;
    data[1] = connected ? HID_EVT_CONNECTED : HID_EVT_DISCONNECTED;
    data[2] = dev_type;

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_EXTENSION,
                                   data, 3,
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

    /* 总时长（大端） */
    uint32_t duration = Audio_GetDuration_ms();
    data[5] = (duration >> 24) & 0xFF;
    data[6] = (duration >> 16) & 0xFF;
    data[7] = (duration >> 8) & 0xFF;
    data[8] = duration & 0xFF;

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

    /*
     * list_data format expected by Display parser:
     *   list_data[0] = entry count (N)
     *   list_data[1..] = N entries, each 21 bytes (attr(1) + size(4) + name(16))
     *
     * So the full DATA field is:
     *   DATA[0] = ext_code, DATA[1] = status, DATA[2] = count, DATA[3..] = entries
     */
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
