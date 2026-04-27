#include "CH378.h"

/************************* CH378 内部命令与状态定义 *************************/
#define MAX_WAIT_CNT          500000
#define MAX_SINGLE_RW_LEN     20480

/************************* 全局变量 *************************/
extern ch378_t ch378_g;
static uint16_t SectorSize = 512;

/************************* 内部静态辅助函数声明 *************************/
static void xWriteCH378Cmd(uint8_t cmd);
static void xWriteCH378Data(uint8_t dat);
static uint8_t xReadCH378Data(void);
static void xEndCH378Cmd(void);
static uint8_t CH378_Wait_Interrupt(void);
static void CH378_Clear_Pending_Int(void);

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
uint8_t CH378_Send_Byte(ch378_t *ch378, uint8_t data)
{
    (void)ch378;
    while (SPI_I2S_GetFlagStatus(CH378_SPI, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(CH378_SPI, data);
    while (SPI_I2S_GetFlagStatus(CH378_SPI, SPI_I2S_FLAG_RXNE) == RESET);
    return (uint8_t)SPI_I2S_ReceiveData(CH378_SPI);
}

uint8_t CH378_Read_Byte(ch378_t *ch378)
{
    return CH378_Send_Byte(ch378, 0xFF);
}

/************************* 对标 EVT 的 HAL 原语 *************************/
static void xWriteCH378Cmd(uint8_t cmd)
{
    CH378_SCS_HIGH();
    CH378_SCS_HIGH();
    CH378_SCS_LOW();
    CH378_Send_Byte(&ch378_g, cmd);
    Delay_Us(2);
}

static void xWriteCH378Data(uint8_t dat)
{
    CH378_Send_Byte(&ch378_g, dat);
}

static uint8_t xReadCH378Data(void)
{
    return CH378_Send_Byte(&ch378_g, 0xFF);
}

static void xEndCH378Cmd(void)
{
    CH378_SCS_HIGH();
    Delay_Us(2);
}

// SPI发送CH378命令+可选参数（公共API，保持自动结束SCS的语义）
void CH378_Send_Cmd(ch378_t *ch378, uint8_t cmd, uint8_t *wbuf, uint8_t wlen)
{
    (void)ch378;
    xWriteCH378Cmd(cmd);
    for (uint8_t i = 0; i < wlen; i++)
    {
        xWriteCH378Data(wbuf[i]);
    }
    xEndCH378Cmd();
}

// 清除CH378可能残留的中断状态
static void CH378_Clear_Pending_Int(void)
{
    if (GPIO_ReadInputDataBit(CH378_INT_PORT, CH378_INT_PIN) == 0)
    {
        xWriteCH378Cmd(CMD01_GET_STATUS);
        (void)xReadCH378Data();
        xEndCH378Cmd();
    }
}

/************************* CH378核心初始化函数 *************************/
void CH378_Init(ch378_t *ch378)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef SPI_InitStructure = {0};

    /* 使能外设时钟 */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA | RCC_HB2Periph_GPIOC | RCC_HB2Periph_SPI1, ENABLE);

    /* SPI引脚配置 */
    GPIO_PinAFConfig(CH378_SPI_MOSI_PORT, GPIO_PinSource7, CH378_SPI_MOSI_AF);
    GPIO_InitStructure.GPIO_Pin = CH378_SPI_MOSI_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH378_SPI_MOSI_PORT, &GPIO_InitStructure);

    GPIO_PinAFConfig(CH378_SPI_MISO_PORT, GPIO_PinSource6, CH378_SPI_MISO_AF);
    GPIO_InitStructure.GPIO_Pin = CH378_SPI_MISO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH378_SPI_MISO_PORT, &GPIO_InitStructure);

    GPIO_PinAFConfig(CH378_SPI_SCK_PORT, GPIO_PinSource5, CH378_SPI_SCK_AF);
    GPIO_InitStructure.GPIO_Pin = CH378_SPI_SCK_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH378_SPI_SCK_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = CH378_SPI_NSS_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(CH378_SPI_NSS_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = CH378_INT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(CH378_INT_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = CH378_RSTI_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(CH378_RSTI_PORT, &GPIO_InitStructure);

    /* SPI主机模式初始化 - Mode 3 (与EVT官方例程一致) */
    SPI_StructInit(&SPI_InitStructure);
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = CH378_SPI_CLOCK;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(CH378_SPI, &SPI_InitStructure);
    SPI_Cmd(CH378_SPI, ENABLE);

    /* 硬件复位CH378 */
    CH378_SCS_HIGH();
    CH378_RSTI_LOW();
    Delay_Ms(300);
    CH378_RSTI_HIGH();
    Delay_Ms(300);

    /* 通讯测试 */
    uint8_t test = 0x57;
    xWriteCH378Cmd(CMD11_CHECK_EXIST);
    xWriteCH378Data(test);
    uint8_t ret = xReadCH378Data();
    xEndCH378Cmd();

    if (ret == (uint8_t)~test)
    {
        printf("CH378 communication test passed\r\n");
        ch378->enable = 1;
    }
    else
    {
        printf("CH378 communication test failed, ret=%02X\r\n", ret);
        ch378->enable = 0;
    }

    ch378->now_device = 0x00;
}

/************************* CH378 芯片信息读取 *************************/

uint8_t CH378_Get_IC_Ver(void)
{
    uint8_t ver;
    xWriteCH378Cmd(CMD01_GET_IC_VER);
    ver = xReadCH378Data();
    xEndCH378Cmd();
    return ver;
}

/************************* 官方 FILE_SYS 移植函数 *************************/

uint32_t CH378Read32bitDat(void)
{
    uint8_t c0, c1, c2, c3;
    c0 = xReadCH378Data();
    c1 = xReadCH378Data();
    c2 = xReadCH378Data();
    c3 = xReadCH378Data();
    xEndCH378Cmd();
    return (c0 | (uint16_t)c1 << 8 | (uint32_t)c2 << 16 | (uint32_t)c3 << 24);
}

uint8_t CH378ReadVar8(uint8_t addr)
{
    uint8_t dat;
    xWriteCH378Cmd(CMD11_READ_VAR8);
    xWriteCH378Data(addr);
    dat = xReadCH378Data();
    xEndCH378Cmd();
    return dat;
}

void CH378WriteVar8(uint8_t addr, uint8_t dat)
{
    xWriteCH378Cmd(CMD20_WRITE_VAR8);
    xWriteCH378Data(addr);
    xWriteCH378Data(dat);
    xEndCH378Cmd();
}

uint32_t CH378ReadVar32(uint8_t addr)
{
    xWriteCH378Cmd(CMD14_READ_VAR32);
    xWriteCH378Data(addr);
    return CH378Read32bitDat();
}

void CH378WriteVar32(uint8_t addr, uint32_t dat)
{
    xWriteCH378Cmd(CMD50_WRITE_VAR32);
    xWriteCH378Data(addr);
    xWriteCH378Data((uint8_t)dat);
    xWriteCH378Data((uint8_t)((uint16_t)dat >> 8));
    xWriteCH378Data((uint8_t)(dat >> 16));
    xWriteCH378Data((uint8_t)(dat >> 24));
    xEndCH378Cmd();
}

uint32_t CH378GetTrueLen(void)
{
    uint8_t c0, c1, c2, c3;
    xWriteCH378Cmd(CMD02_GET_REAL_LEN);
    c0 = xReadCH378Data();
    c1 = xReadCH378Data();
    c2 = xReadCH378Data();
    c3 = xReadCH378Data();
    xEndCH378Cmd();
    return (c0 | (uint16_t)c1 << 8 | (uint32_t)c2 << 16 | (uint32_t)c3 << 24);
}

void CH378SetFileName(uint8_t *PathName)
{
    uint8_t i, c;
    if (PathName == NULL) return;
    xWriteCH378Cmd(CMD10_SET_FILE_NAME);
    for (i = 128; i != 0; --i)
    {
        c = *PathName;
        xWriteCH378Data(c);
        if (c == 0) break;
        PathName++;
    }
    xEndCH378Cmd();
}

uint8_t CH378GetDiskStatus(void)
{
    return CH378ReadVar8(VAR8_DISK_STATUS);
}

uint8_t CH378GetIntStatus(void)
{
    uint8_t s;
    xWriteCH378Cmd(CMD01_GET_STATUS);
    s = xReadCH378Data();
    xEndCH378Cmd();
    return s;
}

static uint8_t CH378_Wait_Interrupt(void)
{
    uint32_t i;
    for (i = 0; i < 5000000; i++)
    {
        if (GPIO_ReadInputDataBit(CH378_INT_PORT, CH378_INT_PIN) == 0)
        {
            return CH378GetIntStatus();
        }
        Delay_Us(3);
    }
    return 0xFF;
}

uint8_t CH378SendCmdWaitInt(uint8_t mCmd)
{
    xWriteCH378Cmd(mCmd);
    xEndCH378Cmd();
    return CH378_Wait_Interrupt();
}

uint8_t CH378SendCmdDatWaitInt(uint8_t mCmd, uint8_t mDat)
{
    xWriteCH378Cmd(mCmd);
    xWriteCH378Data(mDat);
    xEndCH378Cmd();
    return CH378_Wait_Interrupt();
}

uint32_t CH378GetFileSize(void)
{
    return CH378ReadVar32(VAR32_FILE_SIZE);
}

void CH378SetFileSize(uint32_t filesize)
{
    CH378WriteVar32(VAR32_FILE_SIZE, filesize);
}

uint8_t CH378DiskConnect(void)
{
    if (GPIO_ReadInputDataBit(CH378_INT_PORT, CH378_INT_PIN) == 0)
    {
        CH378GetIntStatus();
    }
    return CH378SendCmdWaitInt(CMD0H_DISK_CONNECT);
}

uint8_t CH378DiskReady(void)
{
    return CH378SendCmdWaitInt(CMD0H_DISK_MOUNT);
}

uint16_t CH378ReadReqBlock(uint8_t *buf)
{
    uint16_t len, l;
    xWriteCH378Cmd(CMD00_RD_HOST_REQ_DATA);
    len = xReadCH378Data();
    len |= (uint16_t)xReadCH378Data() << 8;
    Delay_Us(1);
    l = len;
    if (len)
    {
        do
        {
            *buf = xReadCH378Data();
            buf++;
        } while (--l);
    }
    xEndCH378Cmd();
    return len;
}

uint16_t CH378ReadBlock(uint8_t *buf, uint16_t len)
{
    uint16_t l;
    xWriteCH378Cmd(CMD20_RD_HOST_CUR_DATA);
    xWriteCH378Data((uint8_t)len);
    xWriteCH378Data(len >> 8);
    Delay_Us(1);
    if (len)
    {
        l = len;
        do
        {
            *buf = xReadCH378Data();
            buf++;
        } while (--l);
    }
    xEndCH378Cmd();
    return len;
}

uint16_t CH378WriteBlock(uint8_t *buf, uint16_t len)
{
    uint16_t l;
    xWriteCH378Cmd(CMD20_WR_HOST_CUR_DATA);
    xWriteCH378Data((uint8_t)len);
    xWriteCH378Data(len >> 8);
    Delay_Us(1);
    if (len)
    {
        l = len;
        do
        {
            xWriteCH378Data(*buf);
            buf++;
        } while (--l);
    }
    xEndCH378Cmd();
    return len;
}

uint8_t CH378FileOpen(uint8_t *PathName)
{
    CH378SetFileName(PathName);
    return CH378SendCmdWaitInt(CMD0H_FILE_OPEN);
}

uint8_t CH378FileCreate(uint8_t *PathName)
{
    CH378SetFileName(PathName);
    return CH378SendCmdWaitInt(CMD0H_FILE_CREATE);
}

uint8_t CH378DirCreate(uint8_t *PathName)
{
    CH378SetFileName(PathName);
    return CH378SendCmdWaitInt(CMD0H_DIR_CREATE);
}

uint8_t CH378FileErase(uint8_t *PathName)
{
    CH378SetFileName(PathName);
    return CH378SendCmdWaitInt(CMD0H_FILE_ERASE);
}

uint8_t CH378FileClose(uint8_t UpdateSz)
{
    return CH378SendCmdDatWaitInt(CMD1H_FILE_CLOSE, UpdateSz);
}

uint8_t CH378ByteLocate(uint32_t offset)
{
    xWriteCH378Cmd(CMD4H_BYTE_LOCATE);
    xWriteCH378Data((uint8_t)offset);
    xWriteCH378Data((uint8_t)((uint16_t)offset >> 8));
    xWriteCH378Data((uint8_t)(offset >> 16));
    xWriteCH378Data((uint8_t)(offset >> 24));
    xEndCH378Cmd();
    return CH378_Wait_Interrupt();
}

uint8_t CH378ByteRead(uint8_t *buf, uint16_t ReqCount, uint16_t *RealCount)
{
    uint8_t s;
    uint16_t len;

    xWriteCH378Cmd(CMD2H_BYTE_READ);
    xWriteCH378Data((uint8_t)ReqCount);
    xWriteCH378Data((uint8_t)(ReqCount >> 8));
    xEndCH378Cmd();

    if (RealCount) *RealCount = 0;
    s = CH378_Wait_Interrupt();
    if (s == ERR_SUCCESS)
    {
        len = CH378GetTrueLen();
        if (RealCount) *RealCount = len;
        if (len)
        {
            CH378ReadBlock(buf, len);
        }
    }
    return s;
}

uint8_t CH378ByteWrite(uint8_t *buf, uint16_t ReqCount, uint16_t *RealCount)
{
    uint8_t s;

    CH378WriteBlock(buf, ReqCount);

    xWriteCH378Cmd(CMD2H_BYTE_WRITE);
    xWriteCH378Data((uint8_t)ReqCount);
    xWriteCH378Data((uint8_t)(ReqCount >> 8));
    xEndCH378Cmd();
    s = CH378_Wait_Interrupt();
    if (s != ERR_SUCCESS)
    {
        if (RealCount) *RealCount = 0;
    }
    else
    {
        if (RealCount) *RealCount = ReqCount;
    }
    return s;
}

/************************* 模式设置函数 *************************/
uint8_t CH378_SetUsbMode(uint8_t mode)
{
    uint8_t mode_ret;
    xWriteCH378Cmd(CMD11_SET_USB_MODE);
    xWriteCH378Data(mode);
    if (mode == CH378_Device_USB)
        Delay_Ms(50);
    else
        Delay_Ms(50);
    mode_ret = xReadCH378Data();
    xEndCH378Cmd();
    return mode_ret;
}

/************************* 旧版兼容函数 *************************/

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

        CH378_Clear_Pending_Int();

        xWriteCH378Cmd(CMD11_SET_USB_MODE);
        xWriteCH378Data(mode);
        if (device == CH378_Device_USB)
            Delay_Ms(50);
        else
            Delay_Ms(50);

        uint8_t mode_ret = xReadCH378Data();
        xEndCH378Cmd();

        if (mode_ret != CMD_RET_SUCCESS)
        {
            printf("CH378 SET_USB_MODE failed, ret=%02X\r\n", mode_ret);
            ch378->now_device = 0x00;
            return;
        }

        CH378_Clear_Pending_Int();
        int_status = CH378DiskConnect();
        printf("CH378 DISK_CONNECT status=%02X\r\n", int_status);
        if (int_status != ERR_SUCCESS)
        {
            printf("CH378 device not connected\r\n");
            ch378->now_device = 0x00;
            return;
        }

        printf("CH378 wait 200ms for device init...\r\n");
        Delay_Ms(200);

        while (mount_retry < 100)
        {
            CH378_Clear_Pending_Int();
            int_status = CH378DiskReady();
            printf("CH378 DISK_MOUNT try %d, status=%02X\r\n", mount_retry, int_status);
            if (int_status == ERR_SUCCESS)
                break;
            mount_retry++;
            Delay_Ms(50);
        }

        if (int_status == ERR_SUCCESS)
        {
            printf("CH378 device %02X mount success\r\n", device);
            ch378->now_device = device;
        }
        else
        {
            printf("CH378 device %02X mount failed, final status=%02X\r\n", device, int_status);
            uint8_t disk_status = CH378GetDiskStatus();
            printf("CH378 internal disk status=%02X\r\n", disk_status);
            ch378->now_device = 0x00;
        }
    }
}

void CH378_Disk_Capacity(ch378_t *ch378)
{
    (void)ch378;
    uint8_t int_status;

    CH378_Send_Cmd(&ch378_g, CMD0H_DISK_CAPACITY, NULL, 0);
    int_status = CH378_Wait_Interrupt();
    if (int_status == ERR_SUCCESS)
    {
        uint32_t total_sectors = 0;
        uint32_t capacity_bytes = 0;
        uint8_t sec_len_code = 0;
        uint16_t data_len = 0;

        xWriteCH378Cmd(CMD00_RD_HOST_REQ_DATA);
        data_len = xReadCH378Data();
        data_len |= (uint16_t)xReadCH378Data() << 8;

        for (uint16_t i = 0; i < data_len && i < 4; i++)
        {
            total_sectors |= (uint32_t)xReadCH378Data() << (8 * i);
        }
        for (uint16_t i = 4; i < data_len; i++)
        {
            (void)xReadCH378Data();
        }
        xEndCH378Cmd();

        xWriteCH378Cmd(CMD11_READ_VAR8);
        xWriteCH378Data(0x24);
        sec_len_code = xReadCH378Data();
        xEndCH378Cmd();

        uint16_t sector_size = (sec_len_code >= 9 && sec_len_code <= 12) ? (1U << sec_len_code) : 512;
        capacity_bytes = total_sectors * sector_size;

        printf("--- Disk Capacity ---\r\n");
        printf("  Total sectors: %lu\r\n", total_sectors);
        printf("  Sector size: %u bytes\r\n", sector_size);
        printf("  Total capacity: %lu bytes (%lu MB)\r\n", capacity_bytes, capacity_bytes / (1024 * 1024));
    }
    else
    {
        printf("--- Disk Capacity query failed, status=%02X ---\r\n", int_status);
    }
}

/* FAT 目录项结构 (32字节) */
typedef struct _FAT_DIR_INFO {
    uint8_t  DIR_Name[11];      /* 00H, 8.3短文件名，左对齐，不足补空格 */
    uint8_t  DIR_Attr;          /* 0BH, 文件属性 */
    uint8_t  DIR_NTRes;         /* 0CH */
    uint8_t  DIR_CrtTimeTenth;  /* 0DH */
    uint16_t DIR_CrtTime;       /* 0EH */
    uint16_t DIR_CrtDate;       /* 10H */
    uint16_t DIR_LstAccDate;    /* 12H */
    uint16_t DIR_FstClusHI;     /* 14H */
    uint16_t DIR_WrtTime;       /* 16H */
    uint16_t DIR_WrtDate;       /* 18H */
    uint16_t DIR_FstClusLO;     /* 1AH */
    uint32_t DIR_FileSize;      /* 1CH, 文件大小 */
} FAT_DIR_INFO;

/* 将8.3短文件名格式转换为可打印字符串 (name[8]+'.'+ext[3]) */
static void FormatShortName(uint8_t *src, uint8_t *dst)
{
    uint8_t i, j = 0;
    /* 文件名部分 (前8字节，去除尾部空格) */
    for (i = 0; i < 8 && src[i] != ' '; i++)
        dst[j++] = src[i];
    /* 扩展名部分 (后3字节) */
    if (src[8] != ' ')
    {
        dst[j++] = '.';
        for (i = 8; i < 11 && src[i] != ' '; i++)
            dst[j++] = src[i];
    }
    dst[j] = '\0';
}

void CH378_List_Root_Files(ch378_t *ch378)
{
    uint8_t int_status = 0;
    uint8_t name_str[14];       /* 8.3文件名最大 12+1=13 字节 */
    uint8_t dir_buf[32];        /* FAT_DIR_INFO = 32字节 */
    uint16_t name_len = 0;
    uint16_t item_cnt = 0;
    FAT_DIR_INFO *pDir;

    if (ch378->enable != 1 || ch378->now_device == 0x00)
    {
        printf("CH378 not ready\r\n");
        return;
    }

    CH378_Disk_Capacity(ch378);

    printf("--- Root Directory Listing ---\r\n");

    /* 使用官方风格的 CH378FileOpen 枚举根目录 */
    int_status = CH378FileOpen((uint8_t*)"\\*");
    printf("  FILE_OPEN(\\*) status: %02X\r\n", int_status);

    while (int_status == USB_INT_DISK_READ)
    {
        name_len = CH378ReadReqBlock(dir_buf);
        printf("  ReadReqBlock len=%d\r\n", name_len);

        if (name_len == sizeof(FAT_DIR_INFO))
        {
            pDir = (FAT_DIR_INFO *)dir_buf;
            FormatShortName(pDir->DIR_Name, name_str);

            if (pDir->DIR_Attr & 0x10)
                printf("  [DIR]  %s\r\n", name_str);
            else
                printf("  [FILE] %s  (size=%lu)\r\n", name_str, pDir->DIR_FileSize);
            item_cnt++;
        }
        else if (name_len > 0)
        {
            /* 非标准长度，直接按字符串打印 */
            dir_buf[name_len < 32 ? name_len : 31] = '\0';
            printf("  %s\r\n", dir_buf);
            item_cnt++;
        }

        xWriteCH378Cmd(CMD0H_FILE_ENUM_GO);
        xEndCH378Cmd();
        int_status = CH378_Wait_Interrupt();
        printf("  FILE_ENUM_GO status: %02X\r\n", int_status);
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

/************************* 旧版文件操作兼容函数 *************************/

uint8_t CH378_Open_File(ch378_t *ch378, uint8_t *file_name)
{
    uint8_t name_len = 0;
    uint8_t int_status = 0;

    (void)ch378;
    if (ch378_g.enable != 1 || ch378_g.now_device == 0x00)
    {
        printf("CH378 not ready\r\n");
        return ERR_MISS_FILE;
    }

    while (file_name[name_len] != '\0' && name_len < 127)
        name_len++;
    name_len++;

    CH378_Send_Cmd(&ch378_g, CMD10_SET_FILE_NAME, file_name, name_len);
    CH378_Send_Cmd(&ch378_g, CMD0H_FILE_OPEN, NULL, 0);
    int_status = CH378_Wait_Interrupt();

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

void CH378_Close_File(ch378_t *ch378, uint8_t update_len)
{
    uint8_t int_status = 0;
    (void)ch378;
    if (ch378_g.enable != 1) return;
    CH378_Send_Cmd(&ch378_g, CMD1H_FILE_CLOSE, &update_len, 1);
    int_status = CH378_Wait_Interrupt();
    if (int_status == ERR_SUCCESS)
        printf("File close success\r\n");
    else
        printf("File close failed, status: %02X\r\n", int_status);
}

void CH378_Read_File(ch378_t *ch378, uint8_t *file_name, uint8_t *rbuf, uint32_t len)
{
    uint32_t total_read = 0;
    uint16_t real_len = 0;
    uint8_t int_status = 0;

    (void)ch378;
    if (ch378_g.enable != 1 || ch378_g.now_device == 0x00 || rbuf == NULL || len == 0)
    {
        printf("Read file param error\r\n");
        return;
    }
    if (len > CH378_MAX_FILE_SIZE)
    {
        printf("Read file size exceed limit %d bytes\r\n", CH378_MAX_FILE_SIZE);
        return;
    }

    uint8_t open_status = CH378_Open_File(&ch378_g, file_name);
    if (open_status != ERR_SUCCESS && open_status != ERR_OPEN_DIR)
    {
        printf("Read file aborted, open failed\r\n");
        return;
    }

    while (total_read < len)
    {
        uint16_t once_read = (len - total_read) > MAX_SINGLE_RW_LEN ? MAX_SINGLE_RW_LEN : (uint16_t)(len - total_read);
        int_status = CH378ByteRead(rbuf + total_read, once_read, &real_len);
        if (int_status != ERR_SUCCESS)
        {
            printf("Read file failed, status: %02X\r\n", int_status);
            break;
        }
        total_read += real_len;
        if (real_len < once_read)
            break;
    }

    printf("Read file total: %d bytes\r\n", total_read);
    CH378_Close_File(&ch378_g, 1);
}

void CH378_Edit_File(ch378_t *ch378, uint8_t *file_name, uint8_t *wbuf, uint32_t len)
{
    uint32_t total_write = 0;
    uint8_t int_status = 0;
    uint8_t name_len = 0;
    uint16_t real_write = 0;

    (void)ch378;
    if (ch378_g.enable != 1 || ch378_g.now_device == 0x00 || wbuf == NULL || len == 0)
    {
        printf("Write file param error\r\n");
        return;
    }
    if (len > CH378_MAX_FILE_SIZE)
    {
        printf("Write file size exceed limit %d bytes\r\n", CH378_MAX_FILE_SIZE);
        return;
    }

    while (file_name[name_len] != '\0' && name_len < 127)
        name_len++;
    name_len++;

    CH378_Send_Cmd(&ch378_g, CMD10_SET_FILE_NAME, file_name, name_len);
    CH378_Send_Cmd(&ch378_g, CMD0H_FILE_CREATE, NULL, 0);
    int_status = CH378_Wait_Interrupt();
    if (int_status != ERR_SUCCESS)
    {
        printf("Create file failed, status: %02X\r\n", int_status);
        return;
    }

    while (total_write < len)
    {
        uint16_t once_write = (len - total_write) > MAX_SINGLE_RW_LEN ? MAX_SINGLE_RW_LEN : (uint16_t)(len - total_write);
        int_status = CH378ByteWrite(wbuf + total_write, once_write, &real_write);
        if (int_status != ERR_SUCCESS)
        {
            printf("Write file failed, status: %02X\r\n", int_status);
            break;
        }
        total_write += real_write;
    }

    printf("Write file total: %d bytes\r\n", total_write);
    CH378_Close_File(&ch378_g, 1);
}

/************************* 高级文件资源管理器 API *************************/

char ch378_current_path[CH378_MAX_PATH_LEN] = "\\";

/* 辅助：拼接基础路径和名称，生成绝对路径 */
void CH378_Path_Join(const char *base, const char *name, char *out, uint16_t out_len)
{
    uint16_t base_len = strlen(base);
    uint16_t name_len = strlen(name);

    if (base_len >= out_len) {
        out[0] = '\0';
        return;
    }

    strcpy(out, base);

    if (base_len > 0 && out[base_len - 1] != '\\') {
        if (base_len + 1 < out_len) {
            out[base_len] = '\\';
            out[base_len + 1] = '\0';
            base_len++;
        }
    }

    if (base_len + name_len < out_len) {
        strcat(out, name);
    }
}

/* ------------------------------------------------------------------------ */
/* 目录导航 */
/* ------------------------------------------------------------------------ */

uint8_t CH378_Dir_Enter(ch378_t *ch378, const char *dir_name)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;

    if (ch378->enable != 1 || ch378->now_device == 0x00) {
        printf("CH378 not ready\r\n");
        return ERR_DISK_DISCON;
    }

    CH378_Path_Join(ch378_current_path, dir_name, full_path, sizeof(full_path));

    status = CH378FileOpen((uint8_t*)full_path);
    if (status == ERR_OPEN_DIR) {
        strncpy(ch378_current_path, full_path, CH378_MAX_PATH_LEN - 1);
        ch378_current_path[CH378_MAX_PATH_LEN - 1] = '\0';
        return ERR_SUCCESS;
    }

    return status;
}

uint8_t CH378_Dir_Go_Root(ch378_t *ch378)
{
    (void)ch378;
    ch378_current_path[0] = '\\';
    ch378_current_path[1] = '\0';
    return ERR_SUCCESS;
}

uint8_t CH378_Dir_Go_Parent(ch378_t *ch378)
{
    uint16_t len = strlen(ch378_current_path);

    (void)ch378;

    if (len <= 1) {
        return ERR_MISS_DIR;
    }

    /* 去掉末尾的反斜杠 */
    while (len > 0 && ch378_current_path[len - 1] == '\\') {
        ch378_current_path[len - 1] = '\0';
        len--;
    }

    /* 回退到上一级反斜杠 */
    while (len > 0 && ch378_current_path[len - 1] != '\\') {
        len--;
    }

    if (len == 0) {
        ch378_current_path[0] = '\\';
        ch378_current_path[1] = '\0';
    } else {
        ch378_current_path[len] = '\0';
    }

    return ERR_SUCCESS;
}

const char* CH378_Dir_Get_Path(void)
{
    return ch378_current_path;
}

/* ------------------------------------------------------------------------ */
/* 目录列表 */
/* ------------------------------------------------------------------------ */

void CH378_Dir_List(ch378_t *ch378, ch378_dir_callback_t callback)
{
    uint8_t int_status;
    uint8_t dir_buf[32];
    uint8_t name_str[14];
    uint16_t name_len;
    FAT_DIR_INFO *pDir;
    char search_path[CH378_MAX_PATH_LEN];
    uint16_t path_len;

    if (ch378->enable != 1 || ch378->now_device == 0x00) {
        printf("CH378 not ready\r\n");
        return;
    }

    path_len = strlen(ch378_current_path);
    if (path_len >= sizeof(search_path)) {
        return;
    }
    strcpy(search_path, ch378_current_path);

    if (path_len > 0 && search_path[path_len - 1] == '\\') {
        if (path_len + 1 < sizeof(search_path)) {
            strcat(search_path, "*");
        }
    } else {
        if (path_len + 2 < sizeof(search_path)) {
            strcat(search_path, "\\*");
        }
    }

    int_status = CH378FileOpen((uint8_t*)search_path);

    if (int_status != USB_INT_DISK_READ) {
        printf("Dir list open failed, status=%02X\r\n", int_status);
        return;
    }

    while (int_status == USB_INT_DISK_READ) {
        name_len = CH378ReadReqBlock(dir_buf);

        if (name_len == sizeof(FAT_DIR_INFO)) {
            pDir = (FAT_DIR_INFO *)dir_buf;
            FormatShortName(pDir->DIR_Name, name_str);

            /* 跳过 "." 和 ".." 目录项 */
            if (strcmp((char*)name_str, ".") != 0 && strcmp((char*)name_str, "..") != 0) {
                if (callback) {
                    callback((const char*)name_str, (pDir->DIR_Attr & 0x10) ? 1 : 0, pDir->DIR_FileSize);
                }
            }
        }

        xWriteCH378Cmd(CMD0H_FILE_ENUM_GO);
        xEndCH378Cmd();
        int_status = CH378_Wait_Interrupt();
    }
}

/* ------------------------------------------------------------------------ */
/* 文件操作 */
/* ------------------------------------------------------------------------ */

uint8_t CH378_File_Create(ch378_t *ch378, const char *filename)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;

    if (ch378->enable != 1 || ch378->now_device == 0x00) {
        printf("CH378 not ready\r\n");
        return ERR_DISK_DISCON;
    }

    CH378_Path_Join(ch378_current_path, filename, full_path, sizeof(full_path));

    status = CH378FileCreate((uint8_t*)full_path);
    if (status == ERR_SUCCESS) {
        CH378FileClose(0);
    }

    return status;
}

uint8_t CH378_File_Delete(ch378_t *ch378, const char *filename)
{
    char full_path[CH378_MAX_PATH_LEN];

    if (ch378->enable != 1 || ch378->now_device == 0x00) {
        printf("CH378 not ready\r\n");
        return ERR_DISK_DISCON;
    }

    CH378_Path_Join(ch378_current_path, filename, full_path, sizeof(full_path));

    return CH378FileErase((uint8_t*)full_path);
}

uint32_t CH378_File_Read(ch378_t *ch378, const char *filename, uint8_t *buf, uint32_t len)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint32_t total_read = 0;
    uint16_t real_len = 0;
    uint16_t once_read;

    if (ch378->enable != 1 || ch378->now_device == 0x00 || buf == NULL || len == 0) {
        printf("File read param error\r\n");
        return 0;
    }

    CH378_Path_Join(ch378_current_path, filename, full_path, sizeof(full_path));

    status = CH378FileOpen((uint8_t*)full_path);
    if (status != ERR_SUCCESS) {
        printf("File open failed, status=%02X\r\n", status);
        return 0;
    }

    while (total_read < len) {
        once_read = (len - total_read) > MAX_SINGLE_RW_LEN ? MAX_SINGLE_RW_LEN : (uint16_t)(len - total_read);
        status = CH378ByteRead(buf + total_read, once_read, &real_len);
        if (status != ERR_SUCCESS) {
            printf("File read failed, status=%02X\r\n", status);
            break;
        }
        total_read += real_len;
        if (real_len < once_read) {
            break;
        }
    }

    CH378FileClose(0);
    return total_read;
}

uint8_t CH378_File_Write(ch378_t *ch378, const char *filename, const uint8_t *buf, uint32_t len)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint32_t total_write = 0;
    uint16_t once_write;
    uint16_t real_write;

    if (ch378->enable != 1 || ch378->now_device == 0x00 || buf == NULL || len == 0) {
        printf("File write param error\r\n");
        return ERR_PARAMETER_ERROR;
    }

    CH378_Path_Join(ch378_current_path, filename, full_path, sizeof(full_path));

    /* 先尝试打开已有文件，不存在则创建 */
    status = CH378FileOpen((uint8_t*)full_path);
    if (status == ERR_MISS_FILE) {
        status = CH378FileCreate((uint8_t*)full_path);
        if (status != ERR_SUCCESS) {
            printf("File create failed, status=%02X\r\n", status);
            return status;
        }
    } else if (status != ERR_SUCCESS) {
        printf("File open failed, status=%02X\r\n", status);
        return status;
    }

    while (total_write < len) {
        once_write = (len - total_write) > MAX_SINGLE_RW_LEN ? MAX_SINGLE_RW_LEN : (uint16_t)(len - total_write);
        status = CH378ByteWrite((uint8_t*)buf + total_write, once_write, &real_write);
        if (status != ERR_SUCCESS) {
            printf("File write failed, status=%02X\r\n", status);
            CH378FileClose(0);
            return status;
        }
        total_write += real_write;
    }

    CH378FileClose(1);
    return ERR_SUCCESS;
}

uint32_t CH378_File_GetSize(ch378_t *ch378, const char *filename)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint32_t size = 0;

    if (ch378->enable != 1 || ch378->now_device == 0x00) {
        return 0;
    }

    CH378_Path_Join(ch378_current_path, filename, full_path, sizeof(full_path));

    status = CH378FileOpen((uint8_t*)full_path);
    if (status == ERR_SUCCESS) {
        size = CH378GetFileSize();
        CH378FileClose(0);
    }

    return size;
}


