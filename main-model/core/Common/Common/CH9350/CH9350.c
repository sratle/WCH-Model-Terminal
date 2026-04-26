#include "CH9350.h"

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

    // 校验结构体指针
    if (ch9350 == NULL)
        return;
    g_ch9350_dev = ch9350;

    // 结构体初始化清零
    memset(ch9350, 0, sizeof(ch9350_t));

    // 开启外设时钟
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA, ENABLE);
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_USART1, ENABLE);

    // TX引脚复用配置（推挽输出）
    GPIO_PinAFConfig(CH9350_UART_TX_PORT, GPIO_PinSource9, CH9350_UART_TX_AF);
    GPIO_InitStructure.GPIO_Pin = CH9350_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH9350_UART_TX_PORT, &GPIO_InitStructure);

    // RX引脚复用配置（浮空输入）
    GPIO_PinAFConfig(CH9350_UART_RX_PORT, GPIO_PinSource10, CH9350_UART_RX_AF);
    GPIO_InitStructure.GPIO_Pin = CH9350_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(CH9350_UART_RX_PORT, &GPIO_InitStructure);

    // USART核心参数配置（必须与CH9350硬件配置一致）
    USART_InitStructure.USART_BaudRate = CH9350_UART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(CH9350_UART, &USART_InitStructure);

    // 开启串口接收中断（RXNE：接收数据非空中断）
    USART_ITConfig(CH9350_UART, USART_IT_RXNE, ENABLE);

    // CH32H417 NVIC配置：使用PFIC内联函数，无NVIC_InitTypeDef结构体
    NVIC_SetPriority(CH9350_UART_IRQn, 2 << 4); // 优先级2（根据系统调整）
    NVIC_EnableIRQ(CH9350_UART_IRQn);

    // 使能串口
    USART_Cmd(CH9350_UART, ENABLE);
    ch9350->enable = 1;

    // 初始化应答帧默认值
    ch9350->ack_report = 0x00;
    ch9350->pid1 = 0x0000;
    ch9350->pid2 = 0x0000;
    ch9350->work_state = CH9350_STATE_2;  // 默认状态2（透传模式）
}

/*********************************************************************
 * @fn      CH9350_Send_Data
 * @brief   CH9350串口数据发送函数
 * @param   ch9350 - CH9350管理结构体指针
 * @param   data - 待发送数据缓冲区
 * @param   length - 待发送数据长度
 * @return  none
 */
void CH9350_Send_Data(ch9350_t *ch9350, uint8_t *data, uint16_t length)
{
    uint16_t i = 0;
    if ((ch9350 == NULL) || (ch9350->enable == 0) || (data == NULL) || (length == 0))
    {
        return;
    }

    for (i = 0; i < length; i++)
    {
        // 等待发送数据寄存器为空
        while (USART_GetFlagStatus(CH9350_UART, USART_FLAG_TXE) == RESET)
            ;
        USART_SendData(CH9350_UART, data[i]);
    }
    // 等待发送完成
    while (USART_GetFlagStatus(CH9350_UART, USART_FLAG_TC) == RESET)
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
        return;

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
            // 状态2/3/4 数据帧（固定长度）
            case CH9350_OP_KEYBOARD:
                expect_len = 11;
                break; // 57 AB 01 + 8字节键盘数据
            case CH9350_OP_MOUSE_REL:
                expect_len = 7;
                break; // 57 AB 02 + 4字节鼠标数据
            case CH9350_OP_MOUSE_ABS:
                expect_len = 10;
                break; // 57 AB 04 + 7字节鼠标数据

            // 状态0/1 数据帧（变长：第4字节为后续数据长度）
            case CH9350_OP_KEY_DATA0:
            case CH9350_OP_KEY_DATA1:
                if (ch9350->rx_len >= 4)
                {
                    expect_len = 4 + ch9350->rx_buffer[3];
                }
                break;

            // 状态0/1 命令帧（固定长度）
            case CH9350_OP_STATUS_REQ:      // 0x82: 57 AB 82 + 1状态 + 1A* = 5字节
                expect_len = 5;
                break;
            case CH9350_OP_WORK_STATE_CHG:  // 0x85: 57 AB 85 + 1状态 = 4字节
                expect_len = 4;
                break;
            case CH9350_OP_RESET_DELAY:     // 0x84: 57 AB 84 = 3字节
            case CH9350_OP_DEV_DISCONNECT:  // 0x86: 57 AB 86 = 3字节
            case CH9350_OP_GET_VERSION:     // 0x87: 57 AB 87 = 3字节
            case CH9350_OP_TIMEOUT_CFG:     // 0x89: 57 AB 89 = 3字节
                expect_len = 3;
                break;

            // 状态0/1 设备连接帧（变长：第4字节ID，第5-6字节Payload长度）
            case CH9350_OP_DEV_CONNECT:     // 0x81: 57 AB 81 + 1ID + 2长度 + N负载 + 2ID + 1校验
                if (ch9350->rx_len >= 6)
                {
                    uint16_t payload_len = ch9350->rx_buffer[4] | (ch9350->rx_buffer[5] << 8);
                    expect_len = 9 + payload_len;  // 3头码 + 1ID + 2长度 + payload + 2ID + 1校验
                }
                break;

            // 状态2/3/4 命令帧（固定长度）
            case CH9350_OP_STATUS_CHG:      // 0x80: 57 AB 80 + 1状态 = 4字节
                expect_len = 4;
                break;

            default:
                // 未知操作码：不自动判断长度，依赖主循环超时或后续帧头同步
                break;
            }

            // 接收长度匹配，标记帧就绪
            if ((expect_len > 0) && (ch9350->rx_len == expect_len))
            {
                ch9350->frame_ready = 1;
            }
        }

        // 注意：RXNE标志已由USART_ReceiveData()自动清除，无需手动调用USART_ClearITPendingBit
    }
}

/*********************************************************************
 * @fn      USART1_IRQHandler
 * @brief   串口1中断服务函数（必须与使用的串口号对应）
 * @return  none
 */
void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void)
{
    CH9350_UART_IRQ_Handler(g_ch9350_dev);
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

    // 状态3/4 绝对鼠标数据帧
    case CH9350_OP_MOUSE_ABS:
        ch9350->device_type = CH9350_DEV_MOUSE_ABS;
        ch9350->data_len = 7;
        memcpy(ch9350->current_data, &ch9350->rx_buffer[3], 7);
        ch9350->mouse_abs_data.report_id = ch9350->current_data[0];
        ch9350->mouse_abs_data.button = ch9350->current_data[1];
        ch9350->mouse_abs_data.x_pos = (uint16_t)(ch9350->current_data[2] | (ch9350->current_data[3] << 8));
        ch9350->mouse_abs_data.y_pos = (uint16_t)(ch9350->current_data[4] | (ch9350->current_data[5] << 8));
        ch9350->mouse_abs_data.wheel = (int8_t)ch9350->current_data[6];
        parse_success = 1;
        break;

    // 状态0/1 通用键值帧
    case CH9350_OP_KEY_DATA0:
    case CH9350_OP_KEY_DATA1:
    {
        uint8_t data_field_len = ch9350->rx_buffer[3];  // 标识+键值+序列号+校验总长度
        uint8_t id_byte = ch9350->rx_buffer[4];         // 标识字节
        uint8_t calc_sum = 0;
        uint8_t recv_sum;
        uint8_t i;

        // 数据域至少包含标识(1)+键值(n)+序列号(1)+校验(1)，最小为4
        if (data_field_len < 3)
        {
            ch9350->frame_error = 1;
            break;
        }

        // 校验检查：键值+序列号的累加和（手册7.2.4.11）
        // 校验字节是数据域最后一个字节
        recv_sum = ch9350->rx_buffer[4 + data_field_len - 1];
        // 键值起始=rx_buffer[5]，到序列号结束=rx_buffer[4+data_field_len-2]
        for (i = 5; i < (4 + data_field_len - 1); i++)
        {
            calc_sum += ch9350->rx_buffer[i];
        }
        if (calc_sum != recv_sum)
        {
            ch9350->frame_error = 1;
            break;
        }

        // 拷贝有效数据到current_data（标识+键值+序列号，不含校验字节）
        ch9350->data_len = data_field_len - 1;
        memcpy(ch9350->current_data, &ch9350->rx_buffer[4], ch9350->data_len);

        // 解析设备类型（标识字节Bit5&4，手册7.2.4.11）
        uint8_t dev_type = (id_byte & CH9350_ID_DEV_MASK) >> 4;
        if (dev_type == 0x01)
            ch9350->device_type = CH9350_DEV_KEYBOARD;
        else if (dev_type == 0x02)
            ch9350->device_type = CH9350_DEV_MOUSE_REL;
        else if (dev_type == 0x03)
            ch9350->device_type = CH9350_DEV_MULTIMEDIA;
        else
            ch9350->device_type = CH9350_DEV_NONE;

        parse_success = 1;
        break;
    }

    // 设备连接命令（状态0/1）
    case CH9350_OP_DEV_CONNECT:
        ch9350->dev_connected = 1;
        ch9350->dev_disconnected = 0;
        ch9350->device_type = CH9350_DEV_NONE;
        parse_success = 1;
        break;

    // 设备断开命令（状态1）
    case CH9350_OP_DEV_DISCONNECT:
        ch9350->dev_connected = 0;
        ch9350->dev_disconnected = 1;
        ch9350->device_type = CH9350_DEV_NONE;
        // 清空已缓存的设备数据
        memset(&ch9350->keyboard_data, 0, sizeof(ch9350_keyboard_t));
        memset(&ch9350->mouse_rel_data, 0, sizeof(ch9350_mouse_rel_t));
        memset(&ch9350->mouse_abs_data, 0, sizeof(ch9350_mouse_abs_t));
        parse_success = 1;
        break;

    // 状态请求命令（状态0/1，需应答）
    case CH9350_OP_STATUS_REQ:
        if (ch9350->rx_len >= 4)
        {
            // 最后1字节0xA*，高4位固定，低4位为ID状态值
            ch9350->work_state = ch9350->rx_buffer[3] & 0x0F;
        }
        CH9350_Send_Ack(ch9350);
        parse_success = 1;
        break;

    // 工作状态改变命令（状态0/1）
    case CH9350_OP_WORK_STATE_CHG:
        if (ch9350->rx_len >= 4)
        {
            ch9350->work_state = ch9350->rx_buffer[3];
        }
        parse_success = 1;
        break;

    // 复位延迟命令（状态1）
    case CH9350_OP_RESET_DELAY:
        parse_success = 1;
        break;

    // 获取版本号命令（状态0/1/2/3/4）
    case CH9350_OP_GET_VERSION:
        // 手册说明只发送一次，可不应答；版本信息通常在后续数据中
        ch9350->version = ch9350->rx_buffer[3];  // 若存在则记录
        parse_success = 1;
        break;

    // 状态改变命令（状态2/3/4，需应答）
    case CH9350_OP_STATUS_CHG:
        if (ch9350->rx_len >= 4)
        {
            // 状态值低4位为report ID，高4位为100/101状态值
            ch9350->work_state = ch9350->rx_buffer[3] & 0x0F;
        }
        CH9350_Send_Ack(ch9350);
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
    memset(ch9350->rx_buffer, 0, CH9350_RX_BUF_MAX_LEN);
    memset(ch9350->current_data, 0, CH9350_DATA_MAX_LEN);
    memset(&ch9350->keyboard_data, 0, sizeof(ch9350_keyboard_t));
    memset(&ch9350->mouse_rel_data, 0, sizeof(ch9350_mouse_rel_t));
    memset(&ch9350->mouse_abs_data, 0, sizeof(ch9350_mouse_abs_t));
}