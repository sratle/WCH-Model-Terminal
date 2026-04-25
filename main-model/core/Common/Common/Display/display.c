#include "display.h"
#include "../Protocol/protocol_common.h"

display_t *display_ptr = NULL;

/* ============================================================================
 * 前向声明：Display 专用命令 handler
 * ============================================================================ */

/* 请求类 handler：返回 1=成功（resp_buf 写入 DATA 域，*resp_len=DATA 长度），0=失败 */
static uint8_t Display_HandleInputEvent(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleMusicControl(const protocol_frame_t *req,
                                          uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleVolumeSet(const protocol_frame_t *req,
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
static uint8_t Display_HandleBtScan(const protocol_frame_t *req,
                                    uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleBtConnect(const protocol_frame_t *req,
                                       uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleIrRangeReq(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleSaveConfig(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleLoadConfig(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleSetRgbMode(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleBulkTransfer(const protocol_frame_t *req,
                                          uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleSubdispContent(const protocol_frame_t *req,
                                            uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleSubdispConfig(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);
static uint8_t Display_HandleRequestData(const protocol_frame_t *req,
                                         uint8_t *resp_buf, uint16_t resp_size, uint8_t *resp_len);

/* 事件类 handler：无返回值 */
static void Display_HandleModuleStatus(const protocol_frame_t *req);
static void Display_HandleBtStatus(const protocol_frame_t *req);
static void Display_HandleFpStatus(const protocol_frame_t *req);
static void Display_HandleNfcTag(const protocol_frame_t *req);
static void Display_HandleHealthData(const protocol_frame_t *req);
static void Display_HandlePowerStatus(const protocol_frame_t *req);
static void Display_HandleConfigResult(const protocol_frame_t *req);
static void Display_HandleScreenWakeup(const protocol_frame_t *req);
static void Display_HandleScreenSleep(const protocol_frame_t *req);
static void Display_HandleErrorReport(const protocol_frame_t *req);
static void Display_HandleReportEvent(const protocol_frame_t *req);

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

    // 使能接收中断并配置 NVIC
    USART_ITConfig(DISPLAY_UART, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(DISPLAY_UART_IRQn, 1 << 4);
    NVIC_EnableIRQ(DISPLAY_UART_IRQn);

    // 初始化协议接收状态机
    Protocol_InitRxCtx(&display->rx_ctx);

    Display_Get_Type(display);
    display_ptr = display;
}

/*********************************************************************
 * @fn      Display_UART_IRQ_Handler
 *
 * @brief   Display UART interrupt handler.
 *
 * @return  none
 *********************************************************************/
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

// 获取Display类型，入口参数是Display结构体指针
void Display_Get_Type(display_t *display)
{
    (void)display;
}

// 发送数据，入口参数是Display结构体指针，发送数据，发送数据长度
void Display_Send_Data(display_t *display, uint8_t *data, uint16_t length)
{
    int i = 0;

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
 * @brief  请求类 handler 的统一分发与 ACK/NACK 打包
 * @param  req       请求帧
 * @param  handler   handler 函数指针（NULL 则直接 NACK）
 * @param  resp_buf  响应缓冲区
 * @param  resp_size 缓冲区大小
 * @return 实际写入 resp_buf 的字节数（完整响应帧）
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
                /* ---- 请求类 ---- */
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
                case CMD_DISP_EXT_BT_SCAN:
                    resp_len = display_dispatch_req(req, Display_HandleBtScan,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_BT_CONNECT:
                    resp_len = display_dispatch_req(req, Display_HandleBtConnect,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_IR_RANGE_REQ:
                    resp_len = display_dispatch_req(req, Display_HandleIrRangeReq,
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
                case CMD_DISP_EXT_SUBDISP_CONTENT:
                    resp_len = display_dispatch_req(req, Display_HandleSubdispContent,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_SUBDISP_CONFIG:
                    resp_len = display_dispatch_req(req, Display_HandleSubdispConfig,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;
                case CMD_DISP_EXT_REQUEST_DATA:
                    resp_len = display_dispatch_req(req, Display_HandleRequestData,
                                                    resp, sizeof(resp));
                    handled = 1;
                    break;

                /* ---- 事件类 ---- */
                case CMD_DISP_EXT_MODULE_STATUS:
                    Display_HandleModuleStatus(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_BT_STATUS:
                    Display_HandleBtStatus(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_FP_STATUS:
                    Display_HandleFpStatus(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_NFC_TAG:
                    Display_HandleNfcTag(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_HEALTH_DATA:
                    Display_HandleHealthData(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_POWER_STATUS:
                    Display_HandlePowerStatus(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_CONFIG_RESULT:
                    Display_HandleConfigResult(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_SCREEN_WAKEUP:
                    Display_HandleScreenWakeup(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_SCREEN_SLEEP:
                    Display_HandleScreenSleep(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_ERROR_REPORT:
                    Display_HandleErrorReport(req);
                    handled = 1;
                    break;
                case CMD_DISP_EXT_REPORT_EVENT:
                    Display_HandleReportEvent(req);
                    handled = 1;
                    break;

                /* 其余扩展码（多为 Core->Display 方向） */
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
            /* 扩展命令缺少子命令字节 */
            resp_len = ProtocolCommon_Nack(req->dst, req->src,
                                           PROTO_ERR_INVALID_PARAM,
                                           resp, sizeof(resp));
            handled = 1;
        }
    }
    /* 3. Display 基础操作码：0x11 ~ 0x1F */
    else if (req->cmd >= CMD_DISP_SET_BRIGHTNESS && req->cmd <= CMD_DISP_FACTORY_RESET)
    {
        switch (req->cmd)
        {
            case CMD_DISP_INPUT_EVENT:
                resp_len = display_dispatch_req(req, Display_HandleInputEvent,
                                                resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_DISP_MUSIC_CONTROL:
                resp_len = display_dispatch_req(req, Display_HandleMusicControl,
                                                resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_DISP_VOLUME_CONTROL:
                resp_len = display_dispatch_req(req, Display_HandleVolumeSet,
                                                resp, sizeof(resp));
                handled = 1;
                break;

            /* 其余基础码为 Core->Display 方向，Display 不应发来 */
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
 * Display 专用命令 handler 实现（框架，待填充业务逻辑）
 * ============================================================================ */

/* ---- 基础操作码：请求类 ---- */
static uint8_t Display_HandleInputEvent(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 解析输入事件类型（键盘/鼠标/触摸/虚拟触摸），转发到对应模块 */
    return 0; /* 暂不支持 */
}

static uint8_t Display_HandleMusicControl(const protocol_frame_t *req,
                                          uint8_t *resp_buf, uint16_t resp_size,
                                          uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 控制音乐播放（播放/暂停/停止/上一首/下一首） */
    return 0;
}

static uint8_t Display_HandleVolumeSet(const protocol_frame_t *req,
                                       uint8_t *resp_buf, uint16_t resp_size,
                                       uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 设置音量，操作 CS43131 */
    return 0;
}

/* ---- 扩展操作码：请求类 ---- */
static uint8_t Display_HandleAppLaunch(const protocol_frame_t *req,
                                       uint8_t *resp_buf, uint16_t resp_size,
                                       uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 启动指定应用 */
    return 0;
}

static uint8_t Display_HandleAppClose(const protocol_frame_t *req,
                                      uint8_t *resp_buf, uint16_t resp_size,
                                      uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 关闭指定应用 */
    return 0;
}

static uint8_t Display_HandleAppData(const protocol_frame_t *req,
                                     uint8_t *resp_buf, uint16_t resp_size,
                                     uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 处理应用间数据传递 */
    return 0;
}

static uint8_t Display_HandleGetModuleStatus(const protocol_frame_t *req,
                                             uint8_t *resp_buf, uint16_t resp_size,
                                             uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 查询模块状态（CH378/CS43131/CH585F 等） */
    return 0;
}

static uint8_t Display_HandleRequestFileList(const protocol_frame_t *req,
                                             uint8_t *resp_buf, uint16_t resp_size,
                                             uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 返回 CH378 文件列表 */
    return 0;
}

static uint8_t Display_HandleFileRead(const protocol_frame_t *req,
                                      uint8_t *resp_buf, uint16_t resp_size,
                                      uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 触发 Bulk Mode 读取文件 */
    return 0;
}

static uint8_t Display_HandleFileSave(const protocol_frame_t *req,
                                      uint8_t *resp_buf, uint16_t resp_size,
                                      uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 触发 Bulk Mode 保存文件 */
    return 0;
}

static uint8_t Display_HandleFileOperation(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 文件操作（删除/重命名/创建目录等） */
    return 0;
}

static uint8_t Display_HandlePlayMusic(const protocol_frame_t *req,
                                       uint8_t *resp_buf, uint16_t resp_size,
                                       uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 播放指定音乐文件 */
    return 0;
}

static uint8_t Display_HandleBtScan(const protocol_frame_t *req,
                                    uint8_t *resp_buf, uint16_t resp_size,
                                    uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 启动/停止蓝牙设备扫描（操作 CH585F） */
    return 0;
}

static uint8_t Display_HandleBtConnect(const protocol_frame_t *req,
                                       uint8_t *resp_buf, uint16_t resp_size,
                                       uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 连接/断开指定蓝牙设备 */
    return 0;
}

static uint8_t Display_HandleIrRangeReq(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 请求红外测距数据 */
    return 0;
}

static uint8_t Display_HandleSaveConfig(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 保存配置到 Flash/CH378 */
    return 0;
}

static uint8_t Display_HandleLoadConfig(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 从 Flash/CH378 加载配置 */
    return 0;
}

static uint8_t Display_HandleSetRgbMode(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 设置 RGB 模式（Submodel） */
    return 0;
}

static uint8_t Display_HandleBulkTransfer(const protocol_frame_t *req,
                                          uint8_t *resp_buf, uint16_t resp_size,
                                          uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 启动批量传输握手（配置 DMA + IDLE 中断） */
    return 0;
}

static uint8_t Display_HandleSubdispContent(const protocol_frame_t *req,
                                            uint8_t *resp_buf, uint16_t resp_size,
                                            uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 更新子显示屏内容 */
    return 0;
}

static uint8_t Display_HandleSubdispConfig(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 配置子显示屏参数 */
    return 0;
}

static uint8_t Display_HandleRequestData(const protocol_frame_t *req,
                                         uint8_t *resp_buf, uint16_t resp_size,
                                         uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 响应 Display 的数据请求 */
    return 0;
}

/* ---- 扩展操作码：事件类 ---- */
static void Display_HandleModuleStatus(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 处理模块状态主动上报 */
}

static void Display_HandleBtStatus(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 处理蓝牙状态变更 */
}

static void Display_HandleFpStatus(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 处理指纹模块状态 */
}

static void Display_HandleNfcTag(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 处理 NFC 标签事件 */
}

static void Display_HandleHealthData(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 处理健康数据上报 */
}

static void Display_HandlePowerStatus(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 处理供电状态变更 */
}

static void Display_HandleConfigResult(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 处理配置操作结果 */
}

static void Display_HandleScreenWakeup(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 处理屏幕唤醒事件 */
}

static void Display_HandleScreenSleep(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 处理屏幕休眠事件 */
}

static void Display_HandleErrorReport(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 处理错误上报 */
}

static void Display_HandleReportEvent(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 处理通用事件上报 */
}
