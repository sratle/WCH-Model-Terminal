#include "test.h"

/* 高级文件资源管理器 API 演示回调 */
static void List_Callback(const char *name, uint8_t is_dir, uint32_t size)
{
    if (is_dir)
        printf("  [DIR]  %s\r\n", name);
    else
        printf("  [FILE] %s  (size=%lu)\r\n", name, size);
}

void test_CH378(void)
{
    uint8_t status;
    uint16_t i;
    /* 使用官方 EVT 风格的初始化流程，逐步调试 */
    printf("\r\n========== CH378 Step-by-Step Test ==========\r\n");

    /* Step 0: 读取 CH378 芯片版本 */
    {
        uint8_t ic_ver = CH378_Get_IC_Ver();
        printf("[Step 0] CH378 IC Version: 0x%02X\r\n", ic_ver);
    }

    /* Step 1: 设置 USB 模式为 SD 卡模式 (0x04) */
    printf("[Step 1] SET_USB_MODE to 0x04 (SD enabled)...\r\n");
    {
        uint8_t mode_ret = CH378_SetUsbMode(CH378_Device_USB);
        printf("  SET_USB_MODE return: 0x%02X (expect 0x51)\r\n", mode_ret);
        if (mode_ret != CMD_RET_SUCCESS)
        {
            printf("  ERROR: Mode set failed!\r\n");
        }
    }

    /* Step 2: 循环检测 DISK_CONNECT */
    printf("[Step 2] DISK_CONNECT detection...\r\n");
    for (i = 0; i < 50; i++)
    {
        status = CH378DiskConnect();
        printf("  try %d, status=0x%02X\r\n", i, status);
        if (status == ERR_SUCCESS)
        {
            printf("  DISK_CONNECT success!\r\n");
            break;
        }
        Delay_Ms(100);
    }
    if (status != ERR_SUCCESS)
    {
        printf("  ERROR: DISK_CONNECT failed after 50 retries!\r\n");
    }

    /* Step 3: 延时等待设备稳定 */
    printf("[Step 3] Wait 200ms for device stabilization...\r\n");
    Delay_Ms(200);

    /* Step 4: 循环检测 DISK_MOUNT / DISK_READY */
    printf("[Step 4] DISK_MOUNT (CH378DiskReady) loop...\r\n");
    for (i = 0; i < 100; i++)
    {
        status = CH378DiskReady();
        uint8_t disk_status = CH378GetDiskStatus();
        printf("  try %d, DISK_MOUNT=0x%02X, disk_status=0x%02X\r\n", i, status, disk_status);

        if (status == ERR_SUCCESS)
        {
            printf("  DISK_MOUNT success!\r\n");
            break;
        }
        else if (status == ERR_DISK_DISCON)
        {
            printf("  Device disconnected during mount!\r\n");
            break;
        }

        /* 官方例程逻辑：只要 disk_status >= DEF_DISK_MOUNTED 且重试 >= 5 次，也算成功 */
        if (disk_status >= DEF_DISK_MOUNTED && i >= 5)
        {
            printf("  DISK_MOUNT implicit success (disk_status >= 0x03)!\r\n");
            break;
        }

        Delay_Ms(50);
    }

    /* Step 5: 最终检查磁盘状态 */
    {
        uint8_t final_status = CH378GetDiskStatus();
        printf("[Step 5] Final disk status: 0x%02X\r\n", final_status);
        if (final_status < DEF_DISK_MOUNTED)
        {
            printf("  ERROR: Disk not mounted!\r\n");
        }
        ch378_g.now_device = CH378_Device_USB;  // 标记设备已就绪
    }

    /* Step 6: 查询并打印容量 */
    printf("[Step 6] Query disk capacity...\r\n");
    CH378_Disk_Capacity(&ch378_g);

    /* Step 7: 列出根目录文件 */
    printf("[Step 7] List root files...\r\n");
    CH378_List_Root_Files(&ch378_g);

    /* Step 8: 高级文件资源管理器 API 演示 */
    printf("[Step 8] File Manager API demo...\r\n");
    printf("  Current path: %s\r\n", CH378_Dir_Get_Path());

    printf("  --- List current dir (advanced API) ---\r\n");
    CH378_Dir_List(&ch378_g, List_Callback);

    /* 文件读写测试 */
    {
        uint8_t test_buf[64] = "Hello CH378 from File Manager API!";
        uint32_t test_len = strlen((char*)test_buf);
        uint32_t read_len = 0;
        uint32_t file_size = 0;

        CH378_Dir_Enter(&ch378_g, "DOC");
        printf("  Current path: %s\r\n", CH378_Dir_Get_Path());
        CH378_Dir_List(&ch378_g, List_Callback);

        // printf("  --- File write test ---\r\n");
        // status = CH378_File_Write(&ch378_g, "TEST.TXT", test_buf, test_len);
        // printf("  Write TEST.TXT status=%02X\r\n", status);

        // printf("  --- File size query ---\r\n");
        // file_size = CH378_File_GetSize(&ch378_g, "TEST.TXT");
        // printf("  TEST.TXT size=%lu\r\n", file_size);

        // printf("  --- File read test ---\r\n");
        // memset(test_buf, 0, sizeof(test_buf));
        // read_len = CH378_File_Read(&ch378_g, "TEST.TXT", test_buf, sizeof(test_buf));
        // printf("  Read TEST.TXT: %lu bytes, content=%s\r\n", read_len, test_buf);

        // printf("  --- File delete test ---\r\n");
        // status = CH378_File_Delete(&ch378_g, "TEST.TXT");
        // printf("  Delete TEST.TXT status=%02X\r\n", status);
    }

    /* Step 9: CLI 命令行接口演示 */
    // printf("[Step 9] CLI demo...\r\n");
    // {
    //     uint8_t cli_cmd[128];
    //     uint8_t cli_len;

    //     cli_len = sprintf((char*)cli_cmd, "ls");
    //     printf("$ %s\r\n", cli_cmd);
    //     CH378_CLI(&ch378_g, cli_cmd, cli_len);

    //     cli_len = sprintf((char*)cli_cmd, "mkdir TESTDIR");
    //     printf("$ %s\r\n", cli_cmd);
    //     CH378_CLI(&ch378_g, cli_cmd, cli_len);

    //     cli_len = sprintf((char*)cli_cmd, "cd TESTDIR");
    //     printf("$ %s\r\n", cli_cmd);
    //     CH378_CLI(&ch378_g, cli_cmd, cli_len);

    //     cli_len = sprintf((char*)cli_cmd, "echo Hello CH378 CLI > test.txt");
    //     printf("$ %s\r\n", cli_cmd);
    //     CH378_CLI(&ch378_g, cli_cmd, cli_len);

    //     cli_len = sprintf((char*)cli_cmd, "cat test.txt");
    //     printf("$ %s\r\n", cli_cmd);
    //     CH378_CLI(&ch378_g, cli_cmd, cli_len);

    //     cli_len = sprintf((char*)cli_cmd, "ls");
    //     printf("$ %s\r\n", cli_cmd);
    //     CH378_CLI(&ch378_g, cli_cmd, cli_len);

    //     cli_len = sprintf((char*)cli_cmd, "cd ..");
    //     printf("$ %s\r\n", cli_cmd);
    //     CH378_CLI(&ch378_g, cli_cmd, cli_len);

    //     cli_len = sprintf((char*)cli_cmd, "rm -rf TESTDIR");
    //     printf("$ %s\r\n", cli_cmd);
    //     CH378_CLI(&ch378_g, cli_cmd, cli_len);

    //     cli_len = sprintf((char*)cli_cmd, "ls");
    //     printf("$ %s\r\n", cli_cmd);
    //     CH378_CLI(&ch378_g, cli_cmd, cli_len);
    // }
    // printf("========== CLI Demo End ==========\r\n\r\n");

    printf("========== Test End ==========>>>\r\n");
    printf("> ");
}
