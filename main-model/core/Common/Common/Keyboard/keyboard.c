#include "keyboard.h"
#include "../Protocol/protocol_common.h"
#include "../Display/display.h"
#include "../hardware.h"
#include "../CH378/CH378.h"
#include "../CS43131/cs43131.h"

keyboard_t *keyboard_ptr = NULL;

/* ============================================================================
 * Music Keyboard 状态
 * ============================================================================ */
static uint8_t music_active = 0;           /* 1=音乐键盘已启动事件上报 */
static uint8_t prev_key_bitmap[3] = {0};   /* 上一帧琴键位图 */
static uint8_t current_playing_key = 0;    /* 当前正在播放的琴键 ID (1~24, 0=无) */

/* ============================================================================
 * 前向声明：Keyboard 专用命令 handler
 * ============================================================================ */

/* 请求类 handler：返回 1=成功（resp_buf 写入 DATA 域，*resp_len=DATA 长度），0=失败 */
static uint8_t Keyboard_HandleSetBacklight(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len);
static uint8_t Keyboard_HandleGetBacklight(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len);
static uint8_t Keyboard_HandleSetConfig(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len);
static uint8_t Keyboard_HandleGetStatus(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len);

/* 事件类 handler：无返回值 */
static void Keyboard_HandleHidReport(const protocol_frame_t *req);
static void Keyboard_HandleMusicKeys(const protocol_frame_t *req);
static void Keyboard_HandleMusicButtons(const protocol_frame_t *req);
static void Keyboard_HandleMusicFaders(const protocol_frame_t *req);
static void Keyboard_HandleGameInput(const protocol_frame_t *req);

/* ============================================================================
 * UART 初始化与数据收发
 * ============================================================================ */

void Keyboard_Init(keyboard_t *keyboard)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOE, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART3, ENABLE);

    GPIO_PinAFConfig(KEYBOARD_UART_TX_PORT, GPIO_PinSource10, KEYBOARD_UART_TX_AF);
    GPIO_PinAFConfig(KEYBOARD_UART_RX_PORT, GPIO_PinSource11, KEYBOARD_UART_RX_AF);

    GPIO_InitStructure.GPIO_Pin = KEYBOARD_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(KEYBOARD_UART_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = KEYBOARD_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(KEYBOARD_UART_RX_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = KEYBOARD_UART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(KEYBOARD_UART, &USART_InitStructure);
    USART_Cmd(KEYBOARD_UART, ENABLE);

    // 使能接收中断并配置 NVIC
    USART_ITConfig(KEYBOARD_UART, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(KEYBOARD_UART_IRQn, 1 << 4);
    NVIC_EnableIRQ(KEYBOARD_UART_IRQn);

    // 初始化协议接收状态机
    Protocol_InitRxCtx(&keyboard->rx_ctx);

    Keyboard_Get_Type(keyboard);
    keyboard_ptr = keyboard;
}

void Keyboard_UART_IRQ_Handler(keyboard_t *keyboard)
{
    if (keyboard == NULL)
        return;

    if (USART_GetITStatus(KEYBOARD_UART, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(KEYBOARD_UART);
        Protocol_ParseByte(&keyboard->rx_ctx, byte);
    }
}

void Keyboard_Get_Type(keyboard_t *keyboard)
{
    (void)keyboard;
}

void Keyboard_Send_Data(keyboard_t *keyboard, uint8_t *data, uint16_t length)
{
    int i = 0;

    (void)keyboard;

    for (i = 0; i < length; i++)
    {
        while (USART_GetFlagStatus(KEYBOARD_UART, USART_FLAG_TC) == RESET)
            ;
        USART_SendData(KEYBOARD_UART, *data++);
    }
}

/* ============================================================================
 * 通用命令回调
 * ============================================================================ */

static uint8_t keyboard_on_get_type(const protocol_frame_t *req,
                                    uint8_t *resp_buf, uint16_t resp_size,
                                    uint8_t *resp_len)
{
    (void)req;
    (void)resp_size;
    resp_buf[0] = MODULE_TYPE_KEYBOARD;
    resp_buf[1] = MODULE_SUBTYPE_KEYBOARD_MAIN;
    resp_buf[2] = 0x01; /* hw_ver */
    resp_buf[3] = 0x01; /* fw_major */
    resp_buf[4] = 0x00; /* fw_minor */
    *resp_len = 5;
    return 1;
}

static const protocol_common_cb_t keyboard_common_cb = {
    .on_get_type = keyboard_on_get_type,
};

/* ============================================================================
 * 专用命令分发辅助函数
 * ============================================================================ */

static uint8_t keyboard_dispatch_req(const protocol_frame_t *req,
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

void Keyboard_Process(keyboard_t *keyboard)
{
    uint8_t resp[PROTO_MAX_FRAME_LEN];
    uint8_t resp_len = 0;
    uint8_t handled = 0;
    protocol_frame_t *req;

    if (keyboard == NULL || !keyboard->rx_ctx.frame_ready)
        return;

    req = &keyboard->rx_ctx.frame;

    /* 0. 处理 Keyboard 返回的 ACK 响应（心跳 GET_TYPE 的回复） */
    if (req->cmd == CMD_ACK)
    {
        module_identity_t id;
        if (Protocol_ParseIdentity(req, &id))
        {
            Hardware_Hb_MarkOnline(MODULE_ID_KEYBOARD, id.type, id.subtype);
            keyboard->type_id = id.subtype;
        }
        handled = 1;
    }
    /* 1. 通用命令（0x00~0x0F） */
    else if (ProtocolCommon_Dispatch(req, &keyboard_common_cb,
                                resp, sizeof(resp), &resp_len))
    {
        handled = 1;
    }
    /* 2. Keyboard 专用操作码：0x21 ~ 0x2A */
    else if (req->cmd >= CMD_KBD_SET_BACKLIGHT && req->cmd <= CMD_KBD_MUSIC_EVENT_CTRL)
    {
        switch (req->cmd)
        {
            /* ---- Core→Kbd 请求类 ---- */
            case CMD_KBD_SET_BACKLIGHT:
                resp_len = keyboard_dispatch_req(req, Keyboard_HandleSetBacklight,
                                                 resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_KBD_GET_BACKLIGHT:
                resp_len = keyboard_dispatch_req(req, Keyboard_HandleGetBacklight,
                                                 resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_KBD_SET_CONFIG:
                resp_len = keyboard_dispatch_req(req, Keyboard_HandleSetConfig,
                                                 resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_KBD_GET_STATUS:
                resp_len = keyboard_dispatch_req(req, Keyboard_HandleGetStatus,
                                                 resp, sizeof(resp));
                handled = 1;
                break;

            /* ---- Kbd→Core 事件类（不需要 ACK） ---- */
            case CMD_KBD_HID_REPORT:
                Keyboard_HandleHidReport(req);
                handled = 1;
                break;
            case CMD_KBD_MUSIC_KEYS:
                Keyboard_HandleMusicKeys(req);
                handled = 1;
                break;
            case CMD_KBD_MUSIC_BUTTONS:
                Keyboard_HandleMusicButtons(req);
                handled = 1;
                break;
            case CMD_KBD_MUSIC_FADERS:
                Keyboard_HandleMusicFaders(req);
                handled = 1;
                break;
            case CMD_KBD_GAME_INPUT:
                Keyboard_HandleGameInput(req);
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
        Keyboard_Send_Data(keyboard, resp, resp_len);

    Protocol_ResetRxCtx(&keyboard->rx_ctx);
}

/* ============================================================================
 * Keyboard 专用命令 handler 实现
 * ============================================================================ */

/* ---- 请求类 handler ---- */
static uint8_t Keyboard_HandleSetBacklight(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    return 0; /* 暂不支持 */
}

static uint8_t Keyboard_HandleGetBacklight(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    return 0;
}

static uint8_t Keyboard_HandleSetConfig(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    return 0;
}

static uint8_t Keyboard_HandleGetStatus(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    return 0;
}

/* ---- 事件类 handler ---- */
static void Keyboard_HandleHidReport(const protocol_frame_t *req)
{
    uint8_t report_type;
    uint8_t report_len;
    const uint8_t *report_data;

    if (req->len < 2)
        return;

    report_type = req->data[0];
    report_data = &req->data[1];
    report_len = req->len - 1;

    if (report_type == 0x00)
    {
        if (report_len < 8)
            return;

        printf("[KBD] HID KB | Mod: 0x%02X | Keys:", report_data[0]);
        for (uint8_t i = 2; i < 8; i++)
        {
            if (report_data[i])
                printf(" 0x%02X", report_data[i]);
        }
        printf("\r\n");

        Display_SendInputEvent(&display_g, INPUT_DEV_KEYBOARD,
                               report_data, 8);
    }
    else if (report_type == 0x01)
    {
        if (report_len < 4)
            return;

        printf("[KBD] HID MS | Btn: 0x%02X | X: %+4d | Y: %+4d | W: %+4d\r\n",
               report_data[0],
               (int8_t)report_data[1],
               (int8_t)report_data[2],
               (int8_t)report_data[3]);

        Display_SendInputEvent(&display_g, INPUT_DEV_MOUSE,
                               report_data, 4);
    }
}

/* ============================================================================
 * Music Keyboard (Keyboard-3) 事件处理
 * ============================================================================ */

/*
 * 播放指定琴键对应的钢琴音色 WAV 文件
 * key_id: 1~24 → \SOUND\PIANO-01.WAV ~ PIANO-24.WAV
 */
static void Music_PlayPianoKey(uint8_t key_id)
{
    char path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint8_t header[512];
    uint16_t real_len;
    wav_info_t info;

    if (key_id < 1 || key_id > 24)
        return;

    /* 停止当前播放 */
    Audio_PlayStop();

    /* 构建路径: \SOUND\PIANO-xx.WAV (8.3 短文件名) */
    snprintf(path, sizeof(path), "\\SOUND\\PIANO-%02d.WAV", key_id);

    /* 打开文件 */
    status = CH378FileOpen((uint8_t*)path);
    if (status != ERR_SUCCESS) {
        printf("[PIANO] Cannot open %s (0x%02X)\r\n", path, status);
        return;
    }

    /* 读取 WAV 头 */
    status = CH378ByteRead(header, 512, &real_len);
    if (status != ERR_SUCCESS || real_len < 44) {
        printf("[PIANO] Failed to read header\r\n");
        CH378FileClose(0);
        return;
    }

    /* 解析头 */
    if (Audio_ParseWAVHeader(header, &info) != 0) {
        CH378FileClose(0);
        return;
    }

    /* 关闭文件：新引擎自行管理 CH378 读取 */
    CH378FileClose(0);

    /* 启动流式播放 */
    if (Audio_ChannelStart(0, path, &info) != 0) {
        printf("[PIANO] Failed to start channel\r\n");
        return;
    }
    Audio_SetCurrentTrack(path);

    printf("[PIANO] Key %d → %s (%luHz %dbit %dch)\r\n",
           key_id, path,
           (unsigned long)info.sample_rate,
           info.bits_per_sample, info.num_channels);
}

static void Keyboard_HandleMusicKeys(const protocol_frame_t *req)
{
    const uint8_t *bitmap;
    uint8_t i;
    uint8_t pressed_count = 0;
    uint8_t last_pressed_key = 0;
    uint8_t new_key_pressed = 0;

    if (req->len < 4)  /* CMD + 3 bytes bitmap */
        return;

    bitmap = &req->data[0];

    /* 打印当前琴键位图 */
    printf("[MUSIC] Keys: %02X %02X %02X\r\n", bitmap[0], bitmap[1], bitmap[2]);

    /* 检测新按下的键（从 bitmap 中查找） */
    for (i = 0; i < 24; i++) {
        uint8_t byte_idx = i >> 3;
        uint8_t bit_mask = 1u << (i & 7);
        uint8_t is_pressed = (bitmap[byte_idx] & bit_mask) ? 1 : 0;
        uint8_t was_pressed = (prev_key_bitmap[byte_idx] & bit_mask) ? 1 : 0;

        if (is_pressed) {
            pressed_count++;
            last_pressed_key = i + 1;  /* 1-based key ID */
            if (!was_pressed) {
                new_key_pressed = 1;
            }
        }
    }

    /* 处理琴键事件
     * 延音逻辑：松键不主动停止播放，让 WAV 自然衰减结束。
     * 只有新音符触发或外部音频打断才会停止当前播放。 */
    if (new_key_pressed) {
        /* 有新键按下 → 播放最后按下的键（打断当前音符） */
        current_playing_key = last_pressed_key;
        Music_PlayPianoKey(current_playing_key);
    }
    else if (pressed_count > 0 && current_playing_key != 0) {
        /* 有键仍按住：检查当前播放的键是否还在按下列表中 */
        uint8_t byte_idx = (current_playing_key - 1) >> 3;
        uint8_t bit_mask = 1u << ((current_playing_key - 1) & 7);
        if (!(bitmap[byte_idx] & bit_mask)) {
            /* 当前播放的键已释放，切换到最后一个仍按下的键 */
            current_playing_key = last_pressed_key;
            Music_PlayPianoKey(current_playing_key);
        }
    }
    /* pressed_count == 0: 所有键释放 → 不主动停止，让 WAV 自然延音结束 */

    /* 保存当前位图 */
    prev_key_bitmap[0] = bitmap[0];
    prev_key_bitmap[1] = bitmap[1];
    prev_key_bitmap[2] = bitmap[2];
}

static void Keyboard_HandleMusicButtons(const protocol_frame_t *req)
{
    const uint8_t *bitmap;

    if (req->len < 3)  /* CMD + 2 bytes bitmap */
        return;

    bitmap = &req->data[0];
    printf("[MUSIC] Buttons: %02X %02X\r\n", bitmap[0], bitmap[1]);
}

static void Keyboard_HandleMusicFaders(const protocol_frame_t *req)
{
    uint16_t fader_l, fader_m, fader_r;
    uint8_t pct_l, pct_m, pct_r;

    if (req->len < 7)  /* CMD + 6 bytes (3 x uint16 big-endian) */
        return;

    fader_l = ((uint16_t)req->data[0] << 8) | req->data[1];
    fader_m = ((uint16_t)req->data[2] << 8) | req->data[3];
    fader_r = ((uint16_t)req->data[4] << 8) | req->data[5];

    /* Convert 16-bit (0~65535) to percentage (0~100%) */
    pct_l = (uint8_t)((uint32_t)fader_l * 100 / 65535);
    pct_m = (uint8_t)((uint32_t)fader_m * 100 / 65535);
    pct_r = (uint8_t)((uint32_t)fader_r * 100 / 65535);

    printf("[MUSIC] Faders: L=%3u%% M=%3u%% R=%3u%%\r\n", pct_l, pct_m, pct_r);
}

static void Keyboard_HandleGameInput(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 解析摇杆 X/Y（int8）和按键状态 bitmask */
}

/* ============================================================================
 * Music Keyboard 控制 API
 * ============================================================================ */

uint8_t Keyboard_Music_Start(void)
{
    uint8_t data[1];
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint16_t len;

    if (keyboard_ptr == NULL)
        return 0;

    if (keyboard_ptr->type_id != MODULE_SUBTYPE_KEYBOARD_MUSIC)
        return 0;

    data[0] = 0x01;  /* start */
    len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_KEYBOARD,
                             CMD_KBD_MUSIC_EVENT_CTRL,
                             data, 1, buf, sizeof(buf));
    if (len == 0)
        return 0;

    Keyboard_Send_Data(keyboard_ptr, buf, len);
    music_active = 1;

    /* 清空状态 */
    prev_key_bitmap[0] = prev_key_bitmap[1] = prev_key_bitmap[2] = 0;
    current_playing_key = 0;

    printf("[MUSIC] Keyboard-3 event reporting STARTED\r\n");
    return 1;
}

uint8_t Keyboard_Music_Stop(void)
{
    uint8_t data[1];
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint16_t len;

    if (keyboard_ptr == NULL)
        return 0;

    if (keyboard_ptr->type_id != MODULE_SUBTYPE_KEYBOARD_MUSIC)
        return 0;

    data[0] = 0x00;  /* stop */
    len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_KEYBOARD,
                             CMD_KBD_MUSIC_EVENT_CTRL,
                             data, 1, buf, sizeof(buf));
    if (len == 0)
        return 0;

    Keyboard_Send_Data(keyboard_ptr, buf, len);
    music_active = 0;

    /* 停止播放 */
    Audio_PlayStop();
    current_playing_key = 0;
    prev_key_bitmap[0] = prev_key_bitmap[1] = prev_key_bitmap[2] = 0;

    printf("[MUSIC] Keyboard-3 event reporting STOPPED\r\n");
    return 1;
}

uint8_t Keyboard_Music_IsActive(void)
{
    return music_active;
}
