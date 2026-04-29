#ifndef __CH378_H
#define __CH378_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"

/* CH378 SPI clock definition */
#define CH378_SPI_CLOCK SPI_BaudRatePrescaler_Mode4
#define CH378_SPI SPI1

/* CH378 gpio definitions SPI1 */
#define CH378_SPI_MOSI_PORT GPIOA
#define CH378_SPI_MOSI_PIN GPIO_Pin_7
#define CH378_SPI_MOSI_AF GPIO_AF5

#define CH378_SPI_MISO_PORT GPIOA
#define CH378_SPI_MISO_PIN GPIO_Pin_6
#define CH378_SPI_MISO_AF GPIO_AF5

#define CH378_SPI_SCK_PORT GPIOA
#define CH378_SPI_SCK_PIN GPIO_Pin_5
#define CH378_SPI_SCK_AF GPIO_AF5

#define CH378_SPI_NSS_PORT GPIOA
#define CH378_SPI_NSS_PIN GPIO_Pin_4

/* CH378 interrupt and reset pins */
#define CH378_INT_PORT GPIOC
#define CH378_INT_PIN GPIO_Pin_4
#define CH378_RSTI_PORT GPIOC
#define CH378_RSTI_PIN GPIO_Pin_5

/* ======================================================================== */
/* CH378 Commands (from CH378INC.H) */
/* ======================================================================== */
#define CMD01_GET_IC_VER           0x01
#define CMD11_CHECK_EXIST          0x06
#define CMD11_SET_USB_MODE         0x15
#define CMD01_GET_STATUS           0x22
#define CMD11_READ_VAR8            0x0A
#define CMD20_WRITE_VAR8           0x0B
#define CMD14_READ_VAR32           0x0C
#define CMD50_WRITE_VAR32          0x0D
#define CMD02_GET_REAL_LEN         0x0E
#define CMD01_TEST_CONNECT         0x16
#define CMD10_SET_FILE_NAME        0x2F
#define CMD0H_DISK_CONNECT         0x30
#define CMD0H_DISK_MOUNT           0x31
#define CMD0H_FILE_OPEN            0x32
#define CMD0H_FILE_ENUM_GO         0x33
#define CMD0H_FILE_CREATE          0x34
#define CMD0H_FILE_ERASE           0x35
#define CMD1H_FILE_CLOSE           0x36
#define CMD1H_DIR_INFO_READ        0x37
#define CMD1H_DIR_INFO_SAVE        0x38
#define CMD4H_BYTE_LOCATE          0x39
#define CMD2H_BYTE_READ            0x3A
#define CMD2H_BYTE_WRITE           0x3C
#define CMD0H_DISK_CAPACITY        0x3E
#define CMD0H_DISK_QUERY           0x3F
#define CMD0H_DIR_CREATE           0x40
#define CMD0H_FILE_QUERY           0x55
#define CMD0H_FILE_MODIFY          0x57
#define CMD50_SET_FILE_SIZE        0x0D
#define CMD40_RD_HOST_OFS_DATA     0x26
#define CMD20_RD_HOST_CUR_DATA     0x27
#define CMD00_RD_HOST_REQ_DATA     0x28
#define CMD40_WR_HOST_OFS_DATA     0x2D
#define CMD20_WR_HOST_CUR_DATA     0x2E

/* Long Filename Commands */
#define CMD10_SET_LONG_FILE_NAME   0x60
#define CMD10_GET_LONG_FILE_NAME   0x61
#define CMD0H_LONG_FILE_CREATE     0x62
#define CMD0H_LONG_DIR_CREATE      0x63
#define CMD1H_GET_SHORT_FILE_NAME  0x65
#define CMD0H_LONG_FILE_OPEN       0x66
#define CMD0H_LONG_FILE_ERASE      0x67

/* ======================================================================== */
/* Status Codes (from CH378INC.H) */
/* ======================================================================== */
#define ERR_SUCCESS                0x00
#define ERR_MISS_FILE              0x42
#define ERR_OPEN_DIR               0x41
#define ERR_MISS_DIR               0xB3
#define ERR_DISK_DISCON            0x82
#define ERR_TYPE_ERROR             0x92
#define ERR_BPB_ERROR              0xA1
#define ERR_FOUND_NAME             0x43
#define ERR_PARAMETER_ERROR        0x03
#define ERR_LONG_NAME_ERR          0x49
#define ERR_NAME_EXIST             0x4A

#define USB_INT_SUCCESS            0x14
#define USB_INT_CONNECT            0x15
#define USB_INT_DISCONNECT         0x16
#define USB_INT_USB_READY          0x18
#define USB_INT_DISK_READ          0x1D
#define USB_INT_DISK_WRITE         0x1E
#define USB_INT_DISK_ERR           0x1F

#define CMD_RET_SUCCESS            0x51
#define CMD_RET_ABORT              0x5F

/* ======================================================================== */
/* Disk Status (from CH378INC.H) */
/* ======================================================================== */
#define DEF_DISK_UNKNOWN           0x00
#define DEF_DISK_DISCONN           0x01
#define DEF_DISK_CONNECT           0x02
#define DEF_DISK_MOUNTED           0x03
#define DEF_DISK_READY             0x10

/* ======================================================================== */
/* Variable Addresses (from CH378INC.H) */
/* ======================================================================== */
#define VAR8_DISK_STATUS           0x21
#define VAR8_DISK_FAT              0x22
#define VAR8_DISK_SEC_LEN          0x24
#define VAR32_FILE_SIZE            0x68

/* ======================================================================== */
/* Device Modes */
/* ======================================================================== */
#define CH378_Device_TF            0x04   /* SD card host mode / enabled */
#define CH378_Device_USB           0x07   /* USB host mode / enabled */

#define CH378_MAX_FILE_SIZE        (400 * 1024)
#define CH378_MAX_PATH_LEN          260

/* ======================================================================== */
/* Type Definitions */
/* ======================================================================== */
typedef struct {
    uint8_t enable;
    uint8_t now_device;
} ch378_t;

/* ======================================================================== */
/* Low-level GPIO / SPI Operations */
/* ======================================================================== */
void CH378_RSTI_HIGH(void);
void CH378_RSTI_LOW(void);
void CH378_SCS_HIGH(void);
void CH378_SCS_LOW(void);
uint8_t CH378_Send_Byte(ch378_t *ch378, uint8_t data);
uint8_t CH378_Read_Byte(ch378_t *ch378);

/* ======================================================================== */
/* Initialization */
/* ======================================================================== */
void CH378_Init(ch378_t *ch378);
uint8_t CH378_SetUsbMode(uint8_t mode);
void CH378_Send_Cmd(ch378_t *ch378, uint8_t cmd, uint8_t *wbuf, uint8_t wlen);
uint8_t CH378_Get_IC_Ver(void);

/* ======================================================================== */
/* Official FILE_SYS API (ported from EVT) */
/* ======================================================================== */
uint8_t CH378ReadVar8(uint8_t addr);
void    CH378WriteVar8(uint8_t addr, uint8_t dat);
uint32_t CH378ReadVar32(uint8_t addr);
void    CH378WriteVar32(uint8_t addr, uint32_t dat);
uint8_t CH378GetDiskStatus(void);
uint8_t CH378GetIntStatus(void);
uint8_t CH378DiskConnect(void);
uint8_t CH378DiskReady(void);
void    CH378SetFileName(uint8_t *PathName);
uint8_t CH378FileOpen(uint8_t *PathName);
uint8_t CH378FileCreate(uint8_t *PathName);
uint8_t CH378DirCreate(uint8_t *PathName);
uint8_t CH378FileErase(uint8_t *PathName);
uint8_t CH378FileClose(uint8_t UpdateSz);
uint8_t CH378ByteLocate(uint32_t offset);
uint8_t CH378ByteRead(uint8_t *buf, uint16_t ReqCount, uint16_t *RealCount);
uint8_t CH378ByteWrite(uint8_t *buf, uint16_t ReqCount, uint16_t *RealCount);
uint16_t CH378ReadReqBlock(uint8_t *buf);
uint16_t CH378ReadBlock(uint8_t *buf, uint16_t len);
uint16_t CH378WriteBlock(uint8_t *buf, uint16_t len);
uint32_t CH378GetFileSize(void);
uint32_t CH378GetTrueLen(void);

/* ======================================================================== */
/* Legacy / High-level API */
/* ======================================================================== */
void CH378_Device_Select(ch378_t *ch378, uint8_t device);
void CH378_Disk_Capacity(ch378_t *ch378);
void CH378_List_Root_Files(ch378_t *ch378);

/* ======================================================================== */
/* 高级文件资源管理器 API */
/* ======================================================================== */

/* 目录列表回调：name=短文件名, is_dir=1表示目录, size=文件大小(目录为0) */
typedef void (*ch378_dir_callback_t)(const char *name, uint8_t is_dir, uint32_t size);

/* 目录导航 */
uint8_t CH378_Dir_Enter(ch378_t *ch378, const char *dir_name);
uint8_t CH378_Dir_Go_Root(ch378_t *ch378);
uint8_t CH378_Dir_Go_Parent(ch378_t *ch378);
const char* CH378_Dir_Get_Path(void);

/* 目录列表 */
void CH378_Dir_List(ch378_t *ch378, ch378_dir_callback_t callback);

/* 文件操作 */
uint8_t CH378_File_Create(ch378_t *ch378, const char *filename);
uint8_t CH378_File_Delete(ch378_t *ch378, const char *filename);
uint32_t CH378_File_Read(ch378_t *ch378, const char *filename, uint8_t *buf, uint32_t len);
uint8_t CH378_File_Write(ch378_t *ch378, const char *filename, const uint8_t *buf, uint32_t len);
uint32_t CH378_File_GetSize(ch378_t *ch378, const char *filename);

/* ======================================================================== */
/* 路径工具（供 CLI 等外部模块使用） */
/* ======================================================================== */
extern char ch378_current_path[];
void CH378_Path_Join(const char *base, const char *name, char *out, uint16_t out_len);

/* ======================================================================== */
/* 磁盘/文件信息查询 */
/* ======================================================================== */
uint32_t CH378_Disk_Query_FreeSectors(void);
uint8_t  CH378_File_Query(uint8_t *buf);

/* ======================================================================== */
/* 目录项读写 */
/* ======================================================================== */
uint8_t CH378_Dir_Info_Read(uint8_t index);
uint8_t CH378_Dir_Info_Save(uint8_t index);
uint8_t CH378_Dir_Info_Write(uint8_t index, const uint8_t *buf);

/* ======================================================================== */
/* 文件信息修改 */
/* ======================================================================== */
uint8_t CH378_File_Modify(uint32_t filesize, uint16_t filedate, uint16_t filetime, uint8_t fileattr);

/* ======================================================================== */
/* 长文件名支持 (Long Filename Extension) */
/* ======================================================================== */
uint8_t CH378_Set_Long_File_Name(const uint8_t *long_name, uint16_t len);
uint8_t CH378_Get_Long_Name(const uint8_t *path, uint8_t *long_name);
uint8_t CH378_Get_Short_Name(const uint8_t *path, const uint8_t *long_name, uint8_t *short_name);
uint8_t CH378_Create_Long_File(const uint8_t *path, const uint8_t *long_name);
uint8_t CH378_Create_Long_Dir(const uint8_t *path, const uint8_t *long_name);
uint8_t CH378_Open_Long_Name(const uint8_t *path, const uint8_t *long_name);
uint8_t CH378_Erase_Long_Name(const uint8_t *path, const uint8_t *long_name);

#endif
