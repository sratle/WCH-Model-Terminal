#include "CH378.h"

/************************* CH378 内部命令与状态定义 *************************/
// 仅在本文件内使用，不修改头文件，保证依赖文件兼容性
#define CMD_CHECK_EXIST       0x05
#define CMD_SET_SDO_INT       0x0B
#define CMD_GET_IC_VER        0x01
#define CMD_RESET_ALL         0x04
#define CMD_GET_STATUS        0x22
#define RD_HOST_REQ_DATA      0x28
#define WR_HOST_CUR_DATA      0x2E
#define SET_FILE_NAME         0x2F
#define DISK_CONNECT          0x30
#define DISK_MOUNT            0x31
#define FILE_OPEN             0x32
#define FILE_CREATE           0x34
#define FILE_CLOSE            0x36
#define BYTE_LOCATE           0x39
#define BYTE_READ             0x3A
#define BYTE_WRITE            0x3C
#define SET_USB_MODE          0x15

// CH378 操作状态码
#define ERR_SUCCESS           0x00
#define ERR_MISS_FILE         0x42
#define ERR_OPEN_DIR          0x1D
#define CMD_TIMEOUT           0xFF
#define MAX_WAIT_CNT          20000  // 命令等待超时计数
#define MAX_SINGLE_RW_LEN     20480  // CH378单次最大读写长度(20K RAM)

/************************* 内部静态辅助函数声明 *************************/
static uint8_t CH378_Wait_Interrupt(ch378_t *ch378);

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

// SPI发送CH378命令+可选参数
void CH378_Send_Cmd(ch378_t *ch378, uint8_t cmd, uint8_t *wbuf, uint8_t wlen)
{
    CH378_SCS_LOW();
    CH378_Send_Byte(ch378, cmd); // 首字节固定为命令码
    Delay_Us(1); // 满足手册TSC时序要求(最小0.6us)
    for (uint8_t i = 0; i < wlen; i++)
    {
        CH378_Send_Byte(ch378, wbuf[i]);
        Delay_Us(1); // 满足手册TSD时序要求(最小0.3us)
    }
    CH378_SCS_HIGH();
    Delay_Us(2); // 满足命令间最小间隔1.5us
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

    // MISO - 浮空输入
    GPIO_PinAFConfig(CH378_SPI_MISO_PORT, GPIO_PinSource6, CH378_SPI_MISO_AF);
    GPIO_InitStructure.GPIO_Pin = CH378_SPI_MISO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
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

    // INT中断引脚 - 浮空输入
    GPIO_InitStructure.GPIO_Pin = CH378_INT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
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
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;  // 匹配CH378 SPI模式0
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;    // 软件控制片选
    SPI_InitStructure.SPI_BaudRatePrescaler = CH378_SPI_CLOCK; // 匹配头文件时钟配置
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB; // CH378要求高位在前
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(CH378_SPI, &SPI_InitStructure);
    SPI_Cmd(CH378_SPI, ENABLE);

    /* 硬件复位CH378 */
    CH378_SCS_HIGH(); // 片选默认拉高
    CH378_RSTI_LOW();
    Delay_Ms(100);
    CH378_RSTI_HIGH();
    Delay_Ms(100); // 等待内部复位完成

    /* 通讯测试 */
    uint8_t test = 0x57;
    CH378_Send_Cmd(ch378, CMD_CHECK_EXIST, &test, 1);
    CH378_SCS_LOW();
    uint8_t ret = CH378_Read_Byte(ch378);
    CH378_SCS_HIGH();

    if (ret == (uint8_t)~test)
    {
        printf("CH378 communication test passed\r\n");
        // 配置SDO引脚中断功能，兼容无INT引脚场景
        uint8_t sdo_int_cfg[2] = {0x16, 0x01};
        CH378_Send_Cmd(ch378, CMD_SET_SDO_INT, sdo_int_cfg, 2);
        ch378->enable = 1;
    }
    else
    {
        printf("CH378 communication test failed\r\n");
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

        // 切换工作模式
        CH378_Send_Cmd(ch378, SET_USB_MODE, &mode, 1);
        if (device == CH378_Device_USB)
            Delay_Ms(35); // USB模式切换最长35ms
        else
            Delay_Ms(3);  // SD卡模式切换最长1ms

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
void CH378_Open_File(ch378_t *ch378, uint8_t *file_name)
{
    uint8_t name_len = 0;
    uint8_t int_status = 0;

    if (ch378->enable != 1 || ch378->now_device == 0x00)
    {
        printf("CH378 not ready\r\n");
        return;
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
}

/************************* 文件关闭函数 *************************/
void CH378_Close_File(ch378_t *ch378, uint8_t *file_name)
{
    uint8_t update_len = 0x01; // 默认允许更新文件长度
    uint8_t int_status = 0;

    if (ch378->enable != 1)
        return;

    // 发送关闭命令，参数1=更新文件长度
    CH378_Send_Cmd(ch378, FILE_CLOSE, &update_len, 1);
    int_status = CH378_Wait_Interrupt(ch378);

    if (int_status == ERR_SUCCESS)
        printf("File close success\r\n");
    else
        printf("File close failed, status: %02X\r\n", int_status);
}

/************************* 文件读取函数 *************************/
void CH378_Read_File(ch378_t *ch378, uint8_t *file_name, uint8_t *rbuf, uint16_t len)
{
    uint16_t total_read = 0;
    uint16_t once_read = 0;
    uint16_t real_len = 0;
    uint8_t int_status = 0;
    uint8_t len_buf[2] = {0};

    if (ch378->enable != 1 || ch378->now_device == 0x00 || rbuf == NULL || len == 0)
    {
        printf("Read file param error\r\n");
        return;
    }

    // 打开文件
    CH378_Open_File(ch378, file_name);

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

        // 读取实际数据长度
        CH378_Send_Cmd(ch378, RD_HOST_REQ_DATA, NULL, 0);
        CH378_SCS_LOW();
        real_len = CH378_Read_Byte(ch378);
        real_len |= (uint16_t)CH378_Read_Byte(ch378) << 8;

        // 读取实际数据
        for (uint16_t i = 0; i < real_len; i++)
        {
            rbuf[total_read + i] = CH378_Read_Byte(ch378);
        }
        CH378_SCS_HIGH();

        total_read += real_len;
        // 读到文件末尾
        if (real_len < once_read)
            break;
    }

    printf("Read file total: %d bytes\r\n", total_read);
    // 关闭文件
    CH378_Close_File(ch378, file_name);
}

/************************* 文件写入/编辑函数 *************************/
void CH378_Edit_File(ch378_t *ch378, uint8_t *file_name, uint8_t *wbuf, uint16_t len)
{
    uint16_t total_write = 0;
    uint16_t once_write = 0;
    uint8_t int_status = 0;
    uint8_t len_buf[2] = {0};

    if (ch378->enable != 1 || ch378->now_device == 0x00 || wbuf == NULL || len == 0)
    {
        printf("Write file param error\r\n");
        return;
    }

    // 新建文件(存在则覆盖)
    CH378_Send_Cmd(ch378, SET_FILE_NAME, file_name, strlen((char*)file_name)+1);
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
        CH378_SCS_LOW();
        CH378_Send_Byte(ch378, WR_HOST_CUR_DATA);
        Delay_Us(1);
        CH378_Send_Byte(ch378, len_buf[0]);
        CH378_Send_Byte(ch378, len_buf[1]);
        for (uint16_t i = 0; i < once_write; i++)
        {
            CH378_Send_Byte(ch378, wbuf[total_write + i]);
        }
        CH378_SCS_HIGH();
        Delay_Us(2);

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
    CH378_Close_File(ch378, file_name);
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

    // 读取中断状态并清除中断
    CH378_Send_Cmd(ch378, CMD_GET_STATUS, NULL, 0);
    CH378_SCS_LOW();
    int_status = CH378_Read_Byte(ch378);
    CH378_SCS_HIGH();

    return int_status;
}
