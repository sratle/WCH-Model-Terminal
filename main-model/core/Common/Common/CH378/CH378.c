#include "CH378.h"

/************************* CH378 内部命令与状态定义 *************************/
// 修正：CMD_CHECK_EXIST 应为 0x06，原 0x05 是 RESET_ALL
#define CMD_CHECK_EXIST       0x06
#define CMD_SET_SDO_INT       0x0B
#define CMD_GET_IC_VER        0x01
#define CMD_RESET_ALL         0x05
#define CMD_GET_STATUS        0x22
#define RD_HOST_REQ_DATA      0x28
#define WR_HOST_CUR_DATA      0x2E
#define SET_FILE_NAME         0x2F
#define DISK_CONNECT          0x30
#define DISK_MOUNT            0x31
#define FILE_OPEN             0x32
#define FILE_CREATE           0x34
#define FILE_CLOSE            0x36
#define FILE_ENUM_GO          0x33
#define BYTE_LOCATE           0x39
#define BYTE_READ             0x3A
#define BYTE_WRITE            0x3C
#define SET_USB_MODE          0x15
#define USB_INT_DISK_READ     0x1D

// CH378 操作状态码
#define ERR_SUCCESS           0x00
#define ERR_MISS_FILE         0x42
#define ERR_OPEN_DIR          0x41   // 修正：原 0x1D 错误，0x1D 是 USB_INT_DISK_READ
#define CMD_TIMEOUT           0xFF
#define CMD_RET_SUCCESS       0x51   // SET_USB_MODE 等命令的返回成功码
#define MAX_WAIT_CNT          500000 // 增大超时：约 5s @ 10us
#define MAX_SINGLE_RW_LEN     20480  // CH378单次最大读写长度(20K RAM)

/************************* 内部静态辅助函数声明 *************************/
static uint8_t CH378_Wait_Interrupt(ch378_t *ch378);

// 对标 EVT HAL 的底层 SPI 事务原语
static void xWriteCH378Cmd(ch378_t *ch378, uint8_t cmd);
static void xWriteCH378Data(ch378_t *ch378, uint8_t dat);
static uint8_t xReadCH378Data(ch378_t *ch378);
static void xEndCH378Cmd(void);

/************************* 底层GPIO操作函数 *************************/
void CH378_RSTI_HIGH(void)
{
    GPIO_SetBits(CH378_RSTI_PORT, CH378_RSTI_PIN);
}

void CH378_RSTI_LOW(void)
{
    GPIO_ResetBits(CH378_RSTI_PORT, CH378_RSTI_PIN);
}

void CH378_SCS_HIGH(void)
{
    GPIO_SetBits(CH378_SPI_NSS_PORT, CH378_SPI_NSS_PIN);
}

void CH378_SCS_LOW(void)
{
    GPIO_ResetBits(CH378_SPI_NSS_PORT, CH378_SPI_NSS_PIN);
}

/************************* SPI底层读写函数 *************************/
// SPI发送单字节，返回接收到的字节
uint8_t CH378_Send_Byte(ch378_t *ch378, uint8_t data)
{
    while (SPI_I2S_GetFlagStatus(CH378_SPI, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(CH378_SPI, data);
    while (SPI_I2S_GetFlagStatus(CH378_SPI, SPI_I2S_FLAG_RXNE) == RESET);
    return (uint8_t)SPI_I2S_ReceiveData(CH378_SPI);
}

// SPI读取单字节
uint8_t CH378_Read_Byte(ch378_t *ch378)
{
    return CH378_Send_Byte(ch378, 0xFF);
}

/************************* 对标 EVT 的 HAL 原语 *************************/
static void xWriteCH378Cmd(ch378_t *ch378, uint8_t cmd)
{
    CH378_SCS_HIGH();           // 确保前一个 SPI 事务结束
    CH378_SCS_HIGH();
    CH378_SCS_LOW();            // 片选有效，开始新事务
    CH378_Send_Byte(ch378, cmd);
    Delay_Us(2);                // 满足手册 TSC 时序要求(最小1.5us)
}

static void xWriteCH378Data(ch378_t *ch378, uint8_t dat)
{
    CH378_Send_Byte(ch378, dat);
}

static uint8_t xReadCH378Data(ch378_t *ch378)
{
    return CH378_Send_Byte(ch378, 0xFF);
}

static void xEndCH378Cmd(void)
{
    CH378_SCS_HIGH();
    Delay_Us(2);                // 满足命令间最小间隔
}

// SPI发送CH378命令+可选参数（公共API，保持自动结束SCS的语义）
void CH378_Send_Cmd(ch378_t *ch378, uint8_t cmd, uint8_t *wbuf, uint8_t wlen)
{
    xWriteCH378Cmd(ch378, cmd);
    for (uint8_t i = 0; i < wlen; i++)
    {
        xWriteCH378Data(ch378, wbuf[i]);
    }
    xEndCH378Cmd();
}

/************************* CH378核心初始化函数 *************************/
void CH378_Init(ch378_t *ch378)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef SPI_InitStructure = {0};

    /* 使能外设时钟 */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA | RCC_HB2Periph_GPIOC | RCC_HB2Periph_SPI1, ENABLE);

    /* SPI引脚配置 */
    // MOSI - 复用推挽输出
    GPIO_PinAFConfig(CH378_SPI_MOSI_PORT, GPIO_PinSource7, CH378_SPI_MOSI_AF);
    GPIO_InitStructure.GPIO_Pin = CH378_SPI_MOSI_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH378_SPI_MOSI_PORT, &GPIO_InitStructure);

    // MISO - 复用推挽输出（EVT STM32 例程使用 AF_PP，主模式下 SPI 外设接管）
    GPIO_PinAFConfig(CH378_SPI_MISO_PORT, GPIO_PinSource6, CH378_SPI_MISO_AF);
    GPIO_InitStructure.GPIO_Pin = CH378_SPI_MISO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH378_SPI_MISO_PORT, &GPIO_InitStructure);

    // SCK - 复用推挽输出
    GPIO_PinAFConfig(CH378_SPI_SCK_PORT, GPIO_PinSource5, CH378_SPI_SCK_AF);
    GPIO_InitStructure.GPIO_Pin = CH378_SPI_SCK_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH378_SPI_SCK_PORT, &GPIO_InitStructure);

    // NSS(SCS) - 普通推挽输出(软件片选，非硬件SPI复用)
    GPIO_InitStructure.GPIO_Pin = CH378_SPI_NSS_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(CH378_SPI_NSS_PORT, &GPIO_InitStructure);

    // INT中断引脚 - 上拉输入（CH378 INT# 为开漏输出，低电平有效）
    GPIO_InitStructure.GPIO_Pin = CH378_INT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(CH378_INT_PORT, &GPIO_InitStructure);

    // RSTI复位引脚 - 推挽输出
    GPIO_InitStructure.GPIO_Pin = CH378_RSTI_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(CH378_RSTI_PORT, &GPIO_InitStructure);

    /* SPI主机模式初始化 */
    SPI_StructInit(&SPI_InitStructure);
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;  // CH378 支持 Mode 0
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;    // 软件控制片选
    SPI_InitStructure.SPI_BaudRatePrescaler = CH378_SPI_CLOCK;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(CH378_SPI, &SPI_InitStructure);
    SPI_Cmd(CH378_SPI, ENABLE);

    /* 硬件复位CH378 */
    CH378_SCS_HIGH();
    CH378_RSTI_LOW();
    Delay_Ms(100);
    CH378_RSTI_HIGH();
    Delay_Ms(100);

    /* 通讯测试：在同一次 SCS 事务中发命令、写测试数据、读返回 */
    uint8_t test = 0x57;
    xWriteCH378Cmd(ch378, CMD_CHECK_EXIST);
    xWriteCH378Data(ch378, test);
    uint8_t ret = xReadCH378Data(ch378);
    xEndCH378Cmd();

    if (ret == (uint8_t)~test)
    {
        printf("CH378 communication test passed\r\n");
        // 配置SDO引脚中断功能，兼容无INT引脚场景
        uint8_t sdo_int_cfg[2] = {0x16, 0x01};
        xWriteCH378Cmd(ch378, CMD_SET_SDO_INT);
        xWriteCH378Data(ch378, sdo_int_cfg[0]);
        xWriteCH378Data(ch378, sdo_int_cfg[1]);
        xEndCH378Cmd();
        ch378->enable = 1;
    }
    else
    {
        printf("CH378 communication test failed, ret=%02X\r\n", ret);
        ch378->enable = 0;
    }
    
    ch378->now_device = 0x00;
}

/************************* 设备模式切换与磁盘初始化 *************************/
void CH378_Device_Select(ch378_t *ch378, uint8_t device)
{
    if (ch378->enable != 1)
    {
        printf("CH378 not initialized\r\n");
        return;
    }

    if (ch378->now_device != device)
    {
        uint8_t mode = device;
        uint8_t mount_retry = 0;
        uint8_t int_status = 0;

        // 切换工作模式：命令+数据+延时+读取返回值，必须在同一次SCS事务中
        xWriteCH378Cmd(ch378, SET_USB_MODE);
        xWriteCH378Data(ch378, mode);
        if (device == CH378_Device_USB)
            Delay_Ms(35); // USB模式切换最长35ms
        else
            Delay_Ms(3);  // SD卡模式切换最长3ms

        uint8_t mode_ret = xReadCH378Data(ch378);
        xEndCH378Cmd();

        if (mode_ret != CMD_RET_SUCCESS)
        {
            printf("CH378 SET_USB_MODE failed, ret=%02X\r\n", mode_ret);
            ch378->now_device = 0x00;
            return;
        }

        // 检测磁盘连接
        CH378_Send_Cmd(ch378, DISK_CONNECT, NULL, 0);
        int_status = CH378_Wait_Interrupt(ch378);
        if (int_status != ERR_SUCCESS)
        {
            printf("CH378 device not connected\r\n");
            ch378->now_device = 0x00;
            return;
        }

        // 挂载磁盘，最多重试5次
        while (mount_retry < 5)
        {
            CH378_Send_Cmd(ch378, DISK_MOUNT, NULL, 0);
            int_status = CH378_Wait_Interrupt(ch378);
            if (int_status == ERR_SUCCESS)
                break;
            mount_retry++;
            Delay_Ms(10);
        }

        if (int_status == ERR_SUCCESS)
        {
            printf("CH378 device %02X mount success\r\n", device);
            ch378->now_device = device;
        }
        else
        {
            printf("CH378 device %02X mount failed\r\n", device);
            ch378->now_device = 0x00;
        }
    }
}

/************************* 文件打开函数 *************************/
uint8_t CH378_Open_File(ch378_t *ch378, uint8_t *file_name)
{
    uint8_t name_len = 0;
    uint8_t int_status = 0;

    if (ch378->enable != 1 || ch378->now_device == 0x00)
    {
        printf("CH378 not ready\r\n");
        return ERR_MISS_FILE;
    }

    // 计算文件名长度(含结束符0)，最大128字节
    while (file_name[name_len] != '\0' && name_len < 127)
        name_len++;
    name_len++; // 包含结束符

    // 设置文件名
    CH378_Send_Cmd(ch378, SET_FILE_NAME, file_name, name_len);
    // 执行打开命令
    CH378_Send_Cmd(ch378, FILE_OPEN, NULL, 0);
    // 等待操作完成
    int_status = CH378_Wait_Interrupt(ch378);

    if (int_status == ERR_SUCCESS)
        printf("File open success\r\n");
    else if (int_status == ERR_MISS_FILE)
        printf("File not found\r\n");
    else if (int_status == ERR_OPEN_DIR)
        printf("Directory open success\r\n");
    else
        printf("File open failed, status: %02X\r\n", int_status);

    return int_status;
}

/************************* 文件关闭函数 *************************/
void CH378_Close_File(ch378_t *ch378, uint8_t update_len)
{
    uint8_t int_status = 0;

    if (ch378->enable != 1)
        return;

    // 发送关闭命令
    CH378_Send_Cmd(ch378, FILE_CLOSE, &update_len, 1);
    int_status = CH378_Wait_Interrupt(ch378);

    if (int_status == ERR_SUCCESS)
        printf("File close success\r\n");
    else
        printf("File close failed, status: %02X\r\n", int_status);
}

/************************* 文件读取函数 *************************/
void CH378_Read_File(ch378_t *ch378, uint8_t *file_name, uint8_t *rbuf, uint32_t len)
{
    uint32_t total_read = 0;
    uint32_t once_read = 0;
    uint16_t real_len = 0;
    uint8_t int_status = 0;
    uint8_t len_buf[2] = {0};

    if (ch378->enable != 1 || ch378->now_device == 0x00 || rbuf == NULL || len == 0)
    {
        printf("Read file param error\r\n");
        return;
    }

    if (len > CH378_MAX_FILE_SIZE)
    {
        printf("Read file size exceed limit %d bytes\r\n", CH378_MAX_FILE_SIZE);
        return;
    }

    // 打开文件并检查返回值
    uint8_t open_status = CH378_Open_File(ch378, file_name);
    if (open_status != ERR_SUCCESS && open_status != ERR_OPEN_DIR)
    {
        printf("Read file aborted, open failed\r\n");
        return;
    }

    // 循环读取，单次最大20480字节
    while (total_read < len)
    {
        once_read = (len - total_read) > MAX_SINGLE_RW_LEN ? MAX_SINGLE_RW_LEN : (len - total_read);
        // 小端格式：低字节在前
        len_buf[0] = once_read & 0xFF;
        len_buf[1] = (once_read >> 8) & 0xFF;

        // 发送字节读命令
        CH378_Send_Cmd(ch378, BYTE_READ, len_buf, 2);
        int_status = CH378_Wait_Interrupt(ch378);
        if (int_status != ERR_SUCCESS)
        {
            printf("Read file failed, status: %02X\r\n", int_status);
            break;
        }

        // 读取实际数据：在同一次 SCS 事务中发命令、读长度、读数据
        xWriteCH378Cmd(ch378, RD_HOST_REQ_DATA);
        real_len = xReadCH378Data(ch378);
        real_len |= (uint16_t)xReadCH378Data(ch378) << 8;

        // 读取实际数据
        for (uint16_t i = 0; i < real_len; i++)
        {
            rbuf[total_read + i] = xReadCH378Data(ch378);
        }
        xEndCH378Cmd();

        total_read += real_len;
        // 读到文件末尾
        if (real_len < once_read)
            break;
    }

    printf("Read file total: %d bytes\r\n", total_read);
    // 关闭文件并更新长度
    CH378_Close_File(ch378, 1);
}

/************************* 文件写入/编辑函数 *************************/
void CH378_Edit_File(ch378_t *ch378, uint8_t *file_name, uint8_t *wbuf, uint32_t len)
{
    uint32_t total_write = 0;
    uint32_t once_write = 0;
    uint8_t int_status = 0;
    uint8_t len_buf[2] = {0};
    uint8_t name_len = 0;

    if (ch378->enable != 1 || ch378->now_device == 0x00 || wbuf == NULL || len == 0)
    {
        printf("Write file param error\r\n");
        return;
    }

    if (len > CH378_MAX_FILE_SIZE)
    {
        printf("Write file size exceed limit %d bytes\r\n", CH378_MAX_FILE_SIZE);
        return;
    }

    // 计算文件名长度(含结束符0)，最大128字节
    while (file_name[name_len] != '\0' && name_len < 127)
        name_len++;
    name_len++; // 包含结束符

    // 新建文件(存在则覆盖)
    CH378_Send_Cmd(ch378, SET_FILE_NAME, file_name, name_len);
    CH378_Send_Cmd(ch378, FILE_CREATE, NULL, 0);
    int_status = CH378_Wait_Interrupt(ch378);
    if (int_status != ERR_SUCCESS)
    {
        printf("Create file failed, status: %02X\r\n", int_status);
        return;
    }

    // 循环写入，单次最大20480字节
    while (total_write < len)
    {
        once_write = (len - total_write) > MAX_SINGLE_RW_LEN ? MAX_SINGLE_RW_LEN : (len - total_write);
        len_buf[0] = once_write & 0xFF;
        len_buf[1] = (once_write >> 8) & 0xFF;

        // 先写入数据到CH378内部缓冲区
        xWriteCH378Cmd(ch378, WR_HOST_CUR_DATA);
        xWriteCH378Data(ch378, len_buf[0]);
        xWriteCH378Data(ch378, len_buf[1]);
        for (uint16_t i = 0; i < once_write; i++)
        {
            xWriteCH378Data(ch378, wbuf[total_write + i]);
        }
        xEndCH378Cmd();

        // 执行字节写命令
        CH378_Send_Cmd(ch378, BYTE_WRITE, len_buf, 2);
        int_status = CH378_Wait_Interrupt(ch378);
        if (int_status != ERR_SUCCESS)
        {
            printf("Write file failed, status: %02X\r\n", int_status);
            break;
        }

        total_write += once_write;
    }

    printf("Write file total: %d bytes\r\n", total_write);
    // 关闭文件并更新长度
    CH378_Close_File(ch378, 1);
}

/************************* 根目录文件枚举函数 *************************/
void CH378_List_Root_Files(ch378_t *ch378)
{
    uint8_t int_status = 0;
    uint8_t name_buf[16]; // 8+3 短文件名 + '\0'
    uint16_t name_len = 0;
    uint16_t item_cnt = 0;

    if (ch378->enable != 1 || ch378->now_device == 0x00)
    {
        printf("CH378 not ready\r\n");
        return;
    }

    printf("--- Root Directory Listing ---\r\n");

    // 设置通配符路径：根目录下所有文件/目录
    uint8_t wildcard[] = "\\*";
    CH378_Send_Cmd(ch378, SET_FILE_NAME, wildcard, sizeof(wildcard));

    // 启动枚举
    CH378_Send_Cmd(ch378, FILE_OPEN, NULL, 0);
    int_status = CH378_Wait_Interrupt(ch378);

    while (int_status == USB_INT_DISK_READ)
    {
        // 读取枚举到的文件名（2字节长度前缀 + 字符串）
        xWriteCH378Cmd(ch378, RD_HOST_REQ_DATA);
        name_len = xReadCH378Data(ch378);
        name_len |= (uint16_t)xReadCH378Data(ch378) << 8;

        if (name_len > 0 && name_len < sizeof(name_buf))
        {
            for (uint16_t i = 0; i < name_len; i++)
            {
                name_buf[i] = xReadCH378Data(ch378);
            }
            name_buf[name_len] = '\0';
            printf("  %s\r\n", name_buf);
            item_cnt++;
        }
        else
        {
            // 长度异常则丢弃数据
            for (uint16_t i = 0; i < name_len; i++)
            {
                (void)xReadCH378Data(ch378);
            }
        }
        xEndCH378Cmd();

        // 继续枚举下一个
        CH378_Send_Cmd(ch378, FILE_ENUM_GO, NULL, 0);
        int_status = CH378_Wait_Interrupt(ch378);
    }

    if (int_status == ERR_MISS_FILE)
    {
        printf("--- Listing complete (%d items) ---\r\n", item_cnt);
    }
    else
    {
        printf("--- Listing ended, status: %02X ---\r\n", int_status);
    }
}

/************************* 内部静态辅助函数实现 *************************/
// 等待CH378中断，返回中断状态，超时返回0xFF
static uint8_t CH378_Wait_Interrupt(ch378_t *ch378)
{
    uint32_t wait_cnt = 0;
    uint8_t int_status = 0;

    // 等待中断引脚拉低(低有效)，或超时
    while (GPIO_ReadInputDataBit(CH378_INT_PORT, CH378_INT_PIN) == 1)
    {
        wait_cnt++;
        if (wait_cnt > MAX_WAIT_CNT)
            return CMD_TIMEOUT;
        Delay_Us(10);
    }

    // 读取中断状态并清除中断：命令与读取在同一次 SCS 事务中
    xWriteCH378Cmd(ch378, CMD_GET_STATUS);
    int_status = xReadCH378Data(ch378);
    xEndCH378Cmd();

    return int_status;
}
