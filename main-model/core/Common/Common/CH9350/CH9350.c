#include "CH9350.h"
#include "../Display/display.h"
#include "../Protocol/protocol.h"
#include "../hardware.h"

// 全局CH9350实例指针（供中断服务函数调用）
static ch9350_t *g_ch9350_dev = NULL;

/*********************************************************************
 * @fn      CH9350_Init
 * @brief   CH9350初始化函数，配置串口、GPIO、接收中断
 * @param   ch9350 - CH9350管理结构体指针
 * @return  none
 */
void CH9350_Init(ch9350_t *ch9350)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    if (ch9350 == NULL)
        return;

    memset(ch9350, 0, sizeof(ch9350_t));
    g_ch9350_dev = ch9350;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA, ENABLE);
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_USART1, ENABLE);

    GPIO_PinAFConfig(CH9350_UART_TX_PORT, GPIO_PinSource9, CH9350_UART_TX_AF);
    GPIO_InitStructure.GPIO_Pin = CH9350_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH9350_UART_TX_PORT, &GPIO_InitStructure);

    GPIO_PinAFConfig(CH9350_UART_RX_PORT, GPIO_PinSource10, CH9350_UART_RX_AF);
    GPIO_InitStructure.GPIO_Pin = CH9350_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(CH9350_UART_RX_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = CH9350_UART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(CH9350_UART, &USART_InitStructure);

    ch9350->enable = 1;

    USART_ITConfig(CH9350_UART, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(CH9350_UART_IRQn, 2 << 4);
    NVIC_EnableIRQ(CH9350_UART_IRQn);

#if defined(Core_V3F)
    NVIC->IALLOCR[CH9350_UART_IRQn] = Core_ID_V3F;
#elif defined(Core_V5F)
    NVIC->IALLOCR[CH9350_UART_IRQn] = Core_ID_V5F;
#endif

    USART_Cmd(CH9350_UART, ENABLE);

    ch9350->ack_report = 0x00;
    ch9350->pid1 = 0x0000;
    ch9350->pid2 = 0x0000;
    ch9350->work_state = CH9350_STATE_2;
    ch9350->rx_timeout_cnt = 0;

    printf("[CH9350] Init done (State2)\r\n");
}

/*********************************************************************
 * @fn      CH9350_Send_Data
 * @brief   CH9350串口数据发送函数（带超时保护）
 * @param   ch9350 - CH9350管理结构体指针
 * @param   data - 待发送数据缓冲区
 * @param   length - 待发送数据长度
 * @return  none
 */
void CH9350_Send_Data(ch9350_t *ch9350, uint8_t *data, uint16_t length)
{
    uint16_t i = 0;
    uint32_t timeout;

    if ((ch9350 == NULL) || (ch9350->enable == 0) || (data == NULL) || (length == 0))
    {
        return;
    }

    for (i = 0; i < length; i++)
    {
        // 等待发送数据寄存器为空，带超时保护
        timeout = CH9350_SEND_TIMEOUT;
        while ((USART_GetFlagStatus(CH9350_UART, USART_FLAG_TXE) == RESET) && (timeout--))
            ;
        if (timeout == 0)
        {
            ch9350->frame_error = 1;
            return;
        }
        USART_SendData(CH9350_UART, data[i]);
    }
    // 等待发送完成，带超时保护
    timeout = CH9350_SEND_TIMEOUT;
    while ((USART_GetFlagStatus(CH9350_UART, USART_FLAG_TC) == RESET) && (timeout--))
        ;
}

/*********************************************************************
 * @fn      CH9350_UART_IRQ_Handler
 * @brief   CH9350串口中断接收处理函数（在串口中断服务函数中调用）
 * @param   ch9350 - CH9350管理结构体指针
 * @return  none
 */
void CH9350_UART_IRQ_Handler(ch9350_t *ch9350)
{
    uint8_t rx_data = 0;
    if ((ch9350 == NULL) || (ch9350->enable == 0))
    {
        // enable 为 0 时仍需清空中断标志，防止中断风暴
        if (USART_GetITStatus(CH9350_UART, USART_IT_RXNE) != RESET)
        {
            (void)USART_ReceiveData(CH9350_UART);
        }
        return;
    }

    // 检测接收中断标志
    if (USART_GetITStatus(CH9350_UART, USART_IT_RXNE) != RESET)
    {
        // 检查溢出错误（ORE）：必须先处理，否则会导致中断循环触发
        if (USART_GetFlagStatus(CH9350_UART, USART_FLAG_ORE) != RESET)
        {
            // ORE清除序列：读STATR -> 读DATAR
            // USART_GetFlagStatus已读STATR，再读一次DATAR即可清除
            (void)USART_ReceiveData(CH9350_UART);
            ch9350->frame_error = 1;
            ch9350->rx_len = 0;
            return;
        }

        // 读取接收到的字节（此操作自动清除RXNE标志）
        rx_data = (uint8_t)USART_ReceiveData(CH9350_UART);

        // 检查帧错误和噪声错误
        if ((USART_GetFlagStatus(CH9350_UART, USART_FLAG_FE) != RESET) ||
            (USART_GetFlagStatus(CH9350_UART, USART_FLAG_NE) != RESET))
        {
            // FE/NE通过读DATAR已清除
            ch9350->frame_error = 1;
            ch9350->rx_len = 0;
            return;
        }

        // 更新接收时间戳
        ch9350->rx_timeout_cnt = 0;

        // 缓冲区溢出保护
        if (ch9350->rx_len >= CH9350_RX_BUF_MAX_LEN)
        {
            ch9350->rx_len = 0;
            ch9350->frame_error = 1;
            memset(ch9350->rx_buffer, 0, CH9350_RX_BUF_MAX_LEN);
            return;
        }

        // 帧头检测：第1个字节必须是0x57，否则丢弃
        if ((ch9350->rx_len == 0) && (rx_data != CH9350_FRAME_HEAD1))
        {
            return;
        }
        // 帧头第2字节必须是0xAB，否则重置接收
        if ((ch9350->rx_len == 1) && (rx_data != CH9350_FRAME_HEAD2))
        {
            ch9350->rx_len = 0;
            return;
        }

        // 存入接收缓冲区
        ch9350->rx_buffer[ch9350->rx_len++] = rx_data;

        // 基础帧长度判断（最短帧：2字节帧头+1字节操作码=3字节）
        if (ch9350->rx_len >= 3)
        {
            // 状态2/3/4固定长度帧快速判断（可根据实际使用的工作状态裁剪）
            uint8_t op_code = ch9350->rx_buffer[2];
            uint16_t expect_len = 0;

            switch (op_code)
            {
            // 状态2 数据帧（固定长度，无应答）
            case CH9350_OP_KEYBOARD:
                expect_len = 11;
                break; // 57 AB 01 + 8字节键盘数据
            case CH9350_OP_MOUSE_REL:
                expect_len = 7;
                break; // 57 AB 02 + 4字节鼠标数据

            // 状态2 命令帧（固定长度，需应答）
            case CH9350_OP_STATUS_CHG:      // 0x80: 57 AB 80 + 1状态 = 4字节
                expect_len = 4;
                break;

            // 设备连接/断开帧（通用）
            case CH9350_OP_DEV_CONNECT:     // 0x81: 57 AB 81 + 1ID + 2长度 + N负载 + 2ID + 1校验
                if (ch9350->rx_len >= 6)
                {
                    uint16_t payload_len = ch9350->rx_buffer[4] | (ch9350->rx_buffer[5] << 8);
                    expect_len = 9 + payload_len;
                    if (expect_len > CH9350_RX_BUF_MAX_LEN)
                    {
                        ch9350->frame_error = 1;
                        ch9350->rx_len = 0;
                        return;
                    }
                }
                break;
            case CH9350_OP_DEV_DISCONNECT:  // 0x86: 57 AB 86 = 3字节
                expect_len = 3;
                break;

            default:
                // 未知操作码：不自动判断长度，依赖主循环超时或后续帧头同步
                break;
            }

            // 接收长度匹配，标记帧就绪；若超长则报错丢弃
            if (expect_len > 0)
            {
                if (ch9350->rx_len == expect_len)
                {
                    ch9350->frame_ready = 1;
                }
                else if (ch9350->rx_len > expect_len)
                {
                    ch9350->frame_error = 1;
                    ch9350->rx_len = 0;
                    return;
                }
            }
        }

        // 注意：RXNE标志已由USART_ReceiveData()自动清除，无需手动调用USART_ClearITPendingBit
    }
}

/*********************************************************************
 * @fn      CH9350_Parse_Frame
 * @brief   CH9350完整帧解析函数，提取有效数据到current_data
 * @param   ch9350 - CH9350管理结构体指针
 * @return  0-解析失败 1-解析成功
 */
uint8_t CH9350_Parse_Frame(ch9350_t *ch9350)
{
    if ((ch9350 == NULL) || (ch9350->frame_ready == 0))
    {
        return 0;
    }

    uint8_t op_code = ch9350->rx_buffer[2];
    uint8_t parse_success = 0;
    memset(ch9350->current_data, 0, CH9350_DATA_MAX_LEN);

    // 解析不同操作码的帧
    switch (op_code)
    {
    // 状态2/3/4 键盘数据帧
    case CH9350_OP_KEYBOARD:
        ch9350->device_type = CH9350_DEV_KEYBOARD;
        ch9350->data_len = 8;
        memcpy(ch9350->current_data, &ch9350->rx_buffer[3], 8);
        ch9350->keyboard_data.modifier = ch9350->current_data[0];
        ch9350->keyboard_data.reserved = ch9350->current_data[1];
        memcpy(ch9350->keyboard_data.key_code, &ch9350->current_data[2], 6);
        parse_success = 1;
        break;

    // 状态2 相对鼠标数据帧
    case CH9350_OP_MOUSE_REL:
        ch9350->device_type = CH9350_DEV_MOUSE_REL;
        ch9350->data_len = 4;
        memcpy(ch9350->current_data, &ch9350->rx_buffer[3], 4);
        ch9350->mouse_rel_data.button = ch9350->current_data[0];
        ch9350->mouse_rel_data.x_offset = (int8_t)ch9350->current_data[1];
        ch9350->mouse_rel_data.y_offset = (int8_t)ch9350->current_data[2];
        ch9350->mouse_rel_data.wheel = (int8_t)ch9350->current_data[3];
        parse_success = 1;
        break;

    // 设备连接命令（状态0/1/2/3/4通用）
    case CH9350_OP_DEV_CONNECT:
        ch9350->dev_connected = 1;
        ch9350->dev_disconnected = 0;
        ch9350->device_type = CH9350_DEV_NONE;
        if (ch9350->rx_len >= 4)
        {
            uint8_t id_byte = ch9350->rx_buffer[3];
            uint8_t dev_mask = id_byte & CH9350_ID_DEV_MASK;
            if (dev_mask == CH9350_ID_DEV_KEYBOARD)
                ch9350->connected_dev_type = CH9350_DEV_KEYBOARD;
            else if (dev_mask == CH9350_ID_DEV_MOUSE)
                ch9350->connected_dev_type = CH9350_DEV_MOUSE_REL;
            else
                ch9350->connected_dev_type = CH9350_DEV_NONE;
            ch9350->version = id_byte;  // 保留原始ID字节
        }
        parse_success = 1;
        break;

    // 设备断开命令（状态0/1/2/3/4通用）
    case CH9350_OP_DEV_DISCONNECT:
        ch9350->dev_connected = 0;
        ch9350->dev_disconnected = 1;
        ch9350->device_type = CH9350_DEV_NONE;
        ch9350->connected_dev_type = CH9350_DEV_NONE;
        // 清空已缓存的设备数据
        memset(&ch9350->keyboard_data, 0, sizeof(ch9350_keyboard_t));
        memset(&ch9350->mouse_rel_data, 0, sizeof(ch9350_mouse_rel_t));
        memset(&ch9350->prev_keyboard_data, 0, sizeof(ch9350_keyboard_t));
        memset(&ch9350->prev_mouse_rel_data, 0, sizeof(ch9350_mouse_rel_t));
        parse_success = 1;
        break;

    // 状态改变命令（状态2/3/4，需应答）
    case CH9350_OP_STATUS_CHG:
        if (ch9350->rx_len >= 4)
        {
            // 状态值低4位为report ID，高4位为100/101状态值
            ch9350->work_state = ch9350->rx_buffer[3] & 0x0F;
        }
        // 改为置位应答标志，由主循环发送，避免中断阻塞
        ch9350->ack_pending = 1;
        parse_success = 1;
        break;

    default:
        ch9350->frame_error = 1;
        break;
    }

    // 解析完成后重置接收状态
    ch9350->frame_ready = 0;
    ch9350->rx_len = 0;
    memset(ch9350->rx_buffer, 0, CH9350_RX_BUF_MAX_LEN);

    return parse_success;
}

/*********************************************************************
 * @fn      CH9350_Send_Ack
 * @brief   发送标准应答状态帧（0x12）
 * @param   ch9350 - CH9350管理结构体指针
 * @return  none
 * @note    应答帧格式（11字节）：
 *          [57][AB][12][PID1_L][PID1_H][PID2_L][PID2_H][Report][State][StateVal][Version]
 *********************************************************************/
void CH9350_Send_Ack(ch9350_t *ch9350)
{
    uint8_t ack[CH9350_ACK_FRAME_LEN];

    if ((ch9350 == NULL) || (ch9350->enable == 0))
        return;

    ack[0] = CH9350_FRAME_HEAD1;
    ack[1] = CH9350_FRAME_HEAD2;
    ack[2] = CH9350_OP_ACK;
    ack[3] = (uint8_t)(ch9350->pid1 & 0xFF);
    ack[4] = (uint8_t)(ch9350->pid1 >> 8);
    ack[5] = (uint8_t)(ch9350->pid2 & 0xFF);
    ack[6] = (uint8_t)(ch9350->pid2 >> 8);
    ack[7] = ch9350->ack_report;
    ack[8] = ch9350->work_state;
    ack[9] = 0x00;          // 状态值（默认0，应用可按需设置）
    ack[10] = 0x01;         // 固定值/版本号（默认0x01）

    CH9350_Send_Data(ch9350, ack, CH9350_ACK_FRAME_LEN);
}

/*********************************************************************
 * @fn      CH9350_Set_Work_State
 * @brief   上位机主动切换CH9350工作状态（发送0x40命令）
 * @param   ch9350 - CH9350管理结构体指针
 * @param   state  - 目标工作状态（0x02~0x04，见CH9350_STATE_*）
 * @return  none
 * @note    仅状态2/3/4支持此命令；切换后CH9350会进入对应工作状态
 *********************************************************************/
void CH9350_Set_Work_State(ch9350_t *ch9350, uint8_t state)
{
    uint8_t cmd[4];

    if ((ch9350 == NULL) || (ch9350->enable == 0))
        return;

    cmd[0] = CH9350_FRAME_HEAD1;
    cmd[1] = CH9350_FRAME_HEAD2;
    cmd[2] = CH9350_OP_STATE_SWITCH;
    cmd[3] = state;

    CH9350_Send_Data(ch9350, cmd, 4);
}

/*********************************************************************
 * @fn      CH9350_KeyCode_Name
 * @brief   USB HID键码转按键名称
 * @param   keycode - USB HID键盘键码（0x00~0xFF）
 * @return  按键名称字符串（已知键）或 NULL（未知键）
 * @note    基于USB HID Usage Tables ( Hut1_12v2.pdf ) 键盘/Keypad Page (0x07)
 *********************************************************************/
const char *CH9350_KeyCode_Name(uint8_t keycode)
{
    static const char *const names[256] = {
        [0x00] = "None",
        /* 字母 A-Z */
        [0x04] = "A",  [0x05] = "B",  [0x06] = "C",  [0x07] = "D",
        [0x08] = "E",  [0x09] = "F",  [0x0A] = "G",  [0x0B] = "H",
        [0x0C] = "I",  [0x0D] = "J",  [0x0E] = "K",  [0x0F] = "L",
        [0x10] = "M",  [0x11] = "N",  [0x12] = "O",  [0x13] = "P",
        [0x14] = "Q",  [0x15] = "R",  [0x16] = "S",  [0x17] = "T",
        [0x18] = "U",  [0x19] = "V",  [0x1A] = "W",  [0x1B] = "X",
        [0x1C] = "Y",  [0x1D] = "Z",
        /* 数字 1-0 */
        [0x1E] = "1",  [0x1F] = "2",  [0x20] = "3",  [0x21] = "4",
        [0x22] = "5",  [0x23] = "6",  [0x24] = "7",  [0x25] = "8",
        [0x26] = "9",  [0x27] = "0",
        /* 主键盘符号与功能键 */
        [0x28] = "Enter",      [0x29] = "Esc",        [0x2A] = "Backspace",
        [0x2B] = "Tab",        [0x2C] = "Space",      [0x2D] = "Minus",
        [0x2E] = "Equal",      [0x2F] = "LBracket",   [0x30] = "RBracket",
        [0x31] = "Backslash",  [0x32] = "NonUSHash",  [0x33] = "Semicolon",
        [0x34] = "Quote",      [0x35] = "Grave",      [0x36] = "Comma",
        [0x37] = "Period",     [0x38] = "Slash",      [0x39] = "CapsLock",
        /* F1-F12 */
        [0x3A] = "F1",   [0x3B] = "F2",   [0x3C] = "F3",   [0x3D] = "F4",
        [0x3E] = "F5",   [0x3F] = "F6",   [0x40] = "F7",   [0x41] = "F8",
        [0x42] = "F9",   [0x43] = "F10",  [0x44] = "F11",  [0x45] = "F12",
        /* 编辑/导航键 */
        [0x46] = "PrtSc",      [0x47] = "ScrollLock", [0x48] = "Pause",
        [0x49] = "Insert",     [0x4A] = "Home",       [0x4B] = "PgUp",
        [0x4C] = "Delete",     [0x4D] = "End",        [0x4E] = "PgDn",
        [0x4F] = "Right",      [0x50] = "Left",       [0x51] = "Down",
        [0x52] = "Up",
        /* 小键盘 */
        [0x53] = "NumLock",    [0x54] = "KPDiv",      [0x55] = "KPMul",
        [0x56] = "KPSub",      [0x57] = "KPAdd",      [0x58] = "KPEnter",
        [0x59] = "KP1",  [0x5A] = "KP2",  [0x5B] = "KP3",  [0x5C] = "KP4",
        [0x5D] = "KP5",  [0x5E] = "KP6",  [0x5F] = "KP7",  [0x60] = "KP8",
        [0x61] = "KP9",  [0x62] = "KP0",  [0x63] = "KPPeriod",
        /* 杂项 */
        [0x64] = "NonUSBackslash", [0x65] = "App",      [0x66] = "Power",
        [0x67] = "KPEqual",
        /* F13-F24 */
        [0x68] = "F13",  [0x69] = "F14",  [0x6A] = "F15",  [0x6B] = "F16",
        [0x6C] = "F17",  [0x6D] = "F18",  [0x6E] = "F19",  [0x6F] = "F20",
        [0x70] = "F21",  [0x71] = "F22",  [0x72] = "F23",  [0x73] = "F24",
        /* 媒体/特殊键 */
        [0x7F] = "Mute",       [0x80] = "VolUp",      [0x81] = "VolDown",
        /* 左右修饰键（虽然通常出现在Modifier字节，但也映射） */
        [0xE0] = "LCtrl",  [0xE1] = "LShift", [0xE2] = "LAlt",   [0xE3] = "LGUI",
        [0xE4] = "RCtrl",  [0xE5] = "RShift", [0xE6] = "RAlt",   [0xE7] = "RGUI",
    };

    return names[keycode];
}

/*********************************************************************
 * @fn      CH9350_Has_New_Data
 * @brief   检测是否有新的有效数据
 * @param   ch9350 - CH9350管理结构体指针
 * @return  0-无新数据 1-有新数据
 */
uint8_t CH9350_Has_New_Data(ch9350_t *ch9350)
{
    if ((ch9350 == NULL) || (ch9350->enable == 0))
    {
        return 0;
    }
    return ch9350->frame_ready;
}

/*********************************************************************
 * @fn      CH9350_Clear_Data
 * @brief   清除当前所有数据和状态
 * @param   ch9350 - CH9350管理结构体指针
 * @return  none
 */
void CH9350_Clear_Data(ch9350_t *ch9350)
{
    if (ch9350 == NULL)
        return;

    ch9350->rx_len = 0;
    ch9350->frame_ready = 0;
    ch9350->frame_error = 0;
    ch9350->data_len = 0;
    ch9350->device_type = CH9350_DEV_NONE;
    ch9350->dev_connected = 0;
    ch9350->dev_disconnected = 0;
    memset(ch9350->rx_buffer, 0, CH9350_RX_BUF_MAX_LEN);
    memset(ch9350->current_data, 0, CH9350_DATA_MAX_LEN);
    memset(&ch9350->keyboard_data, 0, sizeof(ch9350_keyboard_t));
    memset(&ch9350->mouse_rel_data, 0, sizeof(ch9350_mouse_rel_t));
    // 注意：prev_* 字段不清零，用于跨帧事件检测
}

/*********************************************************************
 * @fn      CH9350_Process
 * @brief   CH9350主循环轮询入口：超时检测、应答发送、帧解析、printf输出
 * @param   ch9350 - CH9350管理结构体指针
 * @return  none
 * @note    应在主循环中每1ms调用一次
 *********************************************************************/
void CH9350_Process(ch9350_t *ch9350)
{
    uint8_t i;

    if ((ch9350 == NULL) || (ch9350->enable == 0))
        return;

    // 1. 帧接收超时检测：超过10ms未收到新字节且帧未就绪，重置状态
    // 主循环约每1ms调用一次，计数到10即为10ms
    if ((ch9350->rx_len > 0) && (ch9350->frame_ready == 0))
    {
        if (++ch9350->rx_timeout_cnt > CH9350_FRAME_TIMEOUT_MS)
        {
            ch9350->frame_error = 1;
            ch9350->rx_len = 0;
            ch9350->rx_timeout_cnt = 0;
        }
    }

    // 2. 主循环发送应答（避免中断中阻塞发送）
    if (ch9350->ack_pending)
    {
        ch9350->ack_pending = 0;
        CH9350_Send_Ack(ch9350);
    }

    // 3. 帧错误时清理状态
    if (ch9350->frame_error)
    {
        CH9350_Clear_Data(ch9350);
        return;
    }

    // 4. 解析并输出HID信息
    if (CH9350_Has_New_Data(ch9350))
    {
        uint8_t raw_buf[CH9350_RX_BUF_MAX_LEN];
        uint16_t raw_len;
        uint8_t parse_ok;
        uint8_t op_code;

        // 关中断：防止printf期间USART1中断覆盖rx_buffer
        __disable_irq();
        raw_len = ch9350->rx_len;
        memcpy(raw_buf, ch9350->rx_buffer, raw_len);
        parse_ok = CH9350_Parse_Frame(ch9350);
        __enable_irq();

        // 打印原始帧（用raw_buf，因为Parse_Frame已清零rx_buffer）
        op_code = raw_buf[2];
        // printf("[CH9350] RAW | OP:0x%02X | LEN:%d | DATA:", op_code, raw_len);
        // for (i = 0; i < raw_len; i++)
        // {
        //     printf(" %02X", raw_buf[i]);
        // }
        // printf("\r\n");

        if (parse_ok)
        {
            switch (ch9350->device_type)
            {
            case CH9350_DEV_KEYBOARD:
            {
                uint8_t has_curr = (ch9350->keyboard_data.modifier != 0);
                uint8_t has_prev = (ch9350->prev_keyboard_data.modifier != 0);
                for (i = 0; i < 6; i++)
                {
                    if (ch9350->keyboard_data.key_code[i] != 0) has_curr = 1;
                    if (ch9350->prev_keyboard_data.key_code[i] != 0) has_prev = 1;
                }

                if (has_curr)
                {
                    // 有按键：输出 Modifier + 所有非零 KeyCode（已知键显示名称，未知保留十六进制）
                    printf("[CH9350] KB | Mod: 0x%02X | Keys:", ch9350->keyboard_data.modifier);
                    for (i = 0; i < 6; i++)
                    {
                        uint8_t key = ch9350->keyboard_data.key_code[i];
                        if (key)
                        {
                            const char *name = CH9350_KeyCode_Name(key);
                            if (name)
                                printf(" %s", name);
                            else
                                printf(" 0x%02X", key);
                        }
                    }
                    printf("\r\n");

                    /* 转发键盘输入事件给 Display */
                    {
                        uint8_t kb_report[8];
                        kb_report[0] = ch9350->keyboard_data.modifier;
                        kb_report[1] = ch9350->keyboard_data.reserved;
                        memcpy(&kb_report[2], ch9350->keyboard_data.key_code, 6);
                        Display_SendInputEvent(&display_g, INPUT_DEV_KEYBOARD,
                                               kb_report, 8);
                    }
                }
                else if (has_prev)
                {
                    // 从有到无：按键释放
                    printf("[CH9350] KB | Released\r\n");

                    /* 转发键盘释放事件给 Display */
                    {
                        uint8_t kb_report[8] = {0};
                        Display_SendInputEvent(&display_g, INPUT_DEV_KEYBOARD,
                                               kb_report, 8);
                    }
                }
                // 两帧全0：静默，避免空闲刷屏

                ch9350->prev_keyboard_data = ch9350->keyboard_data;
                break;
            }

            case CH9350_DEV_MOUSE_REL:
            {
                uint8_t has_curr = (ch9350->mouse_rel_data.button != 0) ||
                                   (ch9350->mouse_rel_data.x_offset != 0) ||
                                   (ch9350->mouse_rel_data.y_offset != 0) ||
                                   (ch9350->mouse_rel_data.wheel != 0);
                uint8_t has_prev = (ch9350->prev_mouse_rel_data.button != 0) ||
                                   (ch9350->prev_mouse_rel_data.x_offset != 0) ||
                                   (ch9350->prev_mouse_rel_data.y_offset != 0) ||
                                   (ch9350->prev_mouse_rel_data.wheel != 0);

                if (has_curr || has_prev)
                {
                    printf("[CH9350] MS | Btn: 0x%02X | X: %+4d | Y: %+4d | W: %+4d\r\n",
                           ch9350->mouse_rel_data.button,
                           ch9350->mouse_rel_data.x_offset,
                           ch9350->mouse_rel_data.y_offset,
                           ch9350->mouse_rel_data.wheel);

                    /* 转发鼠标输入事件给 Display */
                    {
                        uint8_t ms_report[4];
                        ms_report[0] = ch9350->mouse_rel_data.button;
                        ms_report[1] = (uint8_t)ch9350->mouse_rel_data.x_offset;
                        ms_report[2] = (uint8_t)ch9350->mouse_rel_data.y_offset;
                        ms_report[3] = (uint8_t)ch9350->mouse_rel_data.wheel;
                        Display_SendInputEvent(&display_g, INPUT_DEV_MOUSE,
                                               ms_report, 4);
                    }
                }
                ch9350->prev_mouse_rel_data = ch9350->mouse_rel_data;
                break;
            }

            // 状态2不支持绝对鼠标和多媒体键

            case CH9350_DEV_NONE:
                // 连接/断开事件仍然输出
                if (ch9350->dev_connected)
                {
                    const char *dev_name = "Unknown";
                    uint8_t hid_dev_type = 0;
                    if (ch9350->connected_dev_type == CH9350_DEV_KEYBOARD) {
                        dev_name = "Keyboard";
                        hid_dev_type = HID_DEV_KEYBOARD;
                    } else if (ch9350->connected_dev_type == CH9350_DEV_MOUSE_REL) {
                        dev_name = "Mouse";
                        hid_dev_type = HID_DEV_MOUSE;
                    }
                    printf("[CH9350] Device Connected (%s)\r\n", dev_name);

                    /* 通知 Display 外接 HID 设备连接 */
                    if (hid_dev_type != 0) {
                        Display_SendHidStatus(&display_g, hid_dev_type, 1);
                    }
                }
                else if (ch9350->dev_disconnected)
                {
                    printf("[CH9350] Device Disconnected\r\n");

                    /* 通知 Display 外接 HID 设备断开（键盘和鼠标都通知断开） */
                    Display_SendHidStatus(&display_g, HID_DEV_KEYBOARD, 0);
                    Display_SendHidStatus(&display_g, HID_DEV_MOUSE, 0);
                }
                else if (op_code != CH9350_OP_STATUS_CHG)
                {
                    // 非0x80的控制帧才输出
                    printf("[CH9350] Control Frame | WorkState: %d | Version: %d\r\n",
                           ch9350->work_state, ch9350->version);
                }
                // 0x80状态改变帧：静默处理，只发应答，不printf
                break;

            default:
                printf("[CH9350] Unknown DeviceType: 0x%02X | DataLen: %d | Data:",
                       ch9350->device_type, ch9350->data_len);
                for (i = 0; i < ch9350->data_len; i++)
                {
                    printf(" 0x%02X", ch9350->current_data[i]);
                }
                printf("\r\n");
                break;
            }
        }
        else
        {
            printf("[CH9350] Frame Parse Failed\r\n");
        }

        CH9350_Clear_Data(ch9350);
    }
}
