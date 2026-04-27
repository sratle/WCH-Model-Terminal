#include "CLI.h"

#include <string.h>
#include <stdio.h>

#include "all_devices.h"
#include "debug.h"

extern ch378_t ch378_g;

/************************* CLI 命令行接口 *************************/

#define CLI_MAX_ENTRIES     64
#define CLI_MAX_ARGC        8
#define CLI_BUF_SIZE        256

typedef struct {
    char names[CLI_MAX_ENTRIES][14];
    uint8_t is_dir[CLI_MAX_ENTRIES];
    uint8_t count;
} cli_entries_t;

static cli_entries_t g_cli_entries;

/* 命令行参数解析（支持双引号包裹含空格的字符串） */
static uint8_t CLI_ParseArgs(char *buf, char **argv, uint8_t max_argv)
{
    uint8_t argc = 0;
    char *p = buf;

    while (*p && argc < max_argv) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        if (*p == '"') {
            p++;
            argv[argc++] = p;
            while (*p && *p != '"') p++;
            if (*p == '"') *p++ = '\0';
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
    }
    return argc;
}

/* ls 打印回调 */
static void CLI_LsCallback(const char *name, uint8_t is_dir, uint32_t size)
{
    if (is_dir)
        printf("  [DIR]  %s\r\n", name);
    else
        printf("  [FILE] %s  (%lu bytes)\r\n", name, size);
}

/* rm -rf 条目收集回调 */
static void CLI_RmCollectCallback(const char *name, uint8_t is_dir, uint32_t size)
{
    (void)size;
    if (g_cli_entries.count < CLI_MAX_ENTRIES) {
        strncpy(g_cli_entries.names[g_cli_entries.count], name, 13);
        g_cli_entries.names[g_cli_entries.count][13] = '\0';
        g_cli_entries.is_dir[g_cli_entries.count] = is_dir;
        g_cli_entries.count++;
    }
}

/* ------------------------------------------------------------------------ */
/* 各命令实现 */
/* ------------------------------------------------------------------------ */

static void CLI_Cmd_Ls(void)
{
    printf("--- %s ---\r\n", CH378_Dir_Get_Path());
    CH378_Dir_List(&ch378_g, CLI_LsCallback);
}

static void CLI_Cmd_Cd(uint8_t argc, char **argv)
{
    uint8_t status;
    const char *dir;

    if (argc < 2) {
        printf("%s\r\n", CH378_Dir_Get_Path());
        return;
    }

    dir = argv[1];
    if (strcmp(dir, "..") == 0) {
        status = CH378_Dir_Go_Parent(&ch378_g);
    } else if (strcmp(dir, "/") == 0 || strcmp(dir, "\\") == 0) {
        status = CH378_Dir_Go_Root(&ch378_g);
    } else {
        status = CH378_Dir_Enter(&ch378_g, dir);
    }

    if (status == ERR_SUCCESS) {
        printf("%s\r\n", CH378_Dir_Get_Path());
    } else {
        printf("cd: %s: failed (status=%02X)\r\n", dir, status);
    }
}

static void CLI_Cmd_Pwd(void)
{
    printf("%s\r\n", CH378_Dir_Get_Path());
}

static void CLI_Cmd_Mkdir(uint8_t argc, char **argv)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;

    if (argc < 2) {
        printf("Usage: mkdir <dir>\r\n");
        return;
    }

    CH378_Path_Join(ch378_current_path, argv[1], full_path, sizeof(full_path));

    status = CH378DirCreate((uint8_t*)full_path);
    if (status == ERR_SUCCESS) {
        printf("mkdir: created '%s'\r\n", argv[1]);
    } else {
        printf("mkdir: %s: failed (status=%02X)\r\n", argv[1], status);
    }
}

static void CLI_Cmd_Touch(uint8_t argc, char **argv)
{
    uint8_t status;

    if (argc < 2) {
        printf("Usage: touch <file>\r\n");
        return;
    }

    status = CH378_File_Create(&ch378_g, argv[1]);
    if (status == ERR_SUCCESS) {
        printf("touch: created '%s'\r\n", argv[1]);
    } else {
        printf("touch: %s: failed (status=%02X)\r\n", argv[1], status);
    }
}

static void CLI_Cmd_Cat(uint8_t argc, char **argv)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint8_t buf[256];
    uint16_t real_len;
    uint16_t once_read;

    if (argc < 2) {
        printf("Usage: cat <file>\r\n");
        return;
    }

    CH378_Path_Join(ch378_current_path, argv[1], full_path, sizeof(full_path));

    status = CH378FileOpen((uint8_t*)full_path);
    if (status != ERR_SUCCESS) {
        printf("cat: %s: No such file\r\n", argv[1]);
        return;
    }

    while (1) {
        once_read = sizeof(buf);
        status = CH378ByteRead(buf, once_read, &real_len);
        if (status != ERR_SUCCESS || real_len == 0) break;

        for (uint16_t i = 0; i < real_len; i++) {
            printf("%c", buf[i]);
        }
        if (real_len < once_read) break;
    }
    printf("\r\n");
    CH378FileClose(0);
}

static void CLI_Cmd_Echo(uint8_t argc, char **argv)
{
    uint8_t i;
    uint8_t redirect_idx = 0;
    uint8_t text_start = 1;
    uint8_t text_end = argc;

    if (argc < 2) {
        printf("\r\n");
        return;
    }

    /* 查找 > 重定向符号 */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0) {
            redirect_idx = i;
            text_end = i;
            break;
        }
    }

    if (redirect_idx > 0 && redirect_idx + 1 < argc) {
        /* 有重定向，收集文本并写入文件 */
        char text_buf[256];
        uint16_t pos = 0;
        uint8_t first = 1;
        uint8_t status;

        for (i = text_start; i < text_end; i++) {
            uint16_t arg_len = strlen(argv[i]);
            if (!first && pos < sizeof(text_buf) - 1) {
                text_buf[pos++] = ' ';
            }
            first = 0;
            if (pos + arg_len < sizeof(text_buf) - 1) {
                memcpy(text_buf + pos, argv[i], arg_len);
                pos += arg_len;
            }
        }
        text_buf[pos] = '\0';

        status = CH378_File_Write(&ch378_g, argv[redirect_idx + 1], (const uint8_t*)text_buf, pos);
        if (status == ERR_SUCCESS) {
            printf("Wrote %d bytes to %s\r\n", pos, argv[redirect_idx + 1]);
        } else {
            printf("echo: write failed (status=%02X)\r\n", status);
        }
    } else {
        /* 无重定向，直接打印 */
        for (i = 1; i < argc; i++) {
            printf("%s%s", (i > 1) ? " " : "", argv[i]);
        }
        printf("\r\n");
    }
}

/* 递归清空目录内所有文件（不包括目录本身）
 * CH378 的 CMD0H_FILE_ERASE 官方注释明确说明"对目录则等待"，
 * 即不支持删除目录，因此本函数仅删除文件，子目录本身保留。
 */
static void CLI_Cmd_RmRecursive(const char *dir)
{
    uint8_t i;
    char subdirs[CLI_MAX_ENTRIES][14];
    uint8_t subdir_cnt = 0;

    /* 进入目标目录 */
    if (CH378_Dir_Enter(&ch378_g, dir) != ERR_SUCCESS) {
        return;
    }

    /* 收集目录内条目 */
    g_cli_entries.count = 0;
    CH378_Dir_List(&ch378_g, CLI_RmCollectCallback);

    /* 删除所有文件，同时记录子目录名 */
    for (i = 0; i < g_cli_entries.count; i++) {
        if (!g_cli_entries.is_dir[i]) {
            CH378_File_Delete(&ch378_g, g_cli_entries.names[i]);
        } else {
            if (subdir_cnt < CLI_MAX_ENTRIES) {
                strncpy(subdirs[subdir_cnt], g_cli_entries.names[i], 13);
                subdirs[subdir_cnt][13] = '\0';
                subdir_cnt++;
            }
        }
    }

    /* 递归清空子目录 */
    for (i = 0; i < subdir_cnt; i++) {
        CLI_Cmd_RmRecursive(subdirs[i]);
    }

    /* 返回上级 */
    CH378_Dir_Go_Parent(&ch378_g);
}

static void CLI_Cmd_Rm(uint8_t argc, char **argv)
{
    uint8_t status;

    if (argc < 2) {
        printf("Usage: rm <file> or rm -rf <dir>\r\n");
        return;
    }

    if (argc >= 3 && strcmp(argv[1], "-rf") == 0) {
        const char *dir = argv[2];

        /* 递归清空目录内所有文件（包括子目录中的文件） */
        CLI_Cmd_RmRecursive(dir);

        /* 尝试删除目录本身。
         * CH378 的 CMD0H_FILE_ERASE 官方注释明确说明"对目录则等待"，
         * 即不支持删除目录，此步骤预期会失败。 */
        status = CH378FileErase((uint8_t*)dir);
        if (status == ERR_SUCCESS) {
            printf("rm -rf: removed '%s'\r\n", dir);
        } else {
            printf("rm -rf: cleared '%s', but directory itself cannot be removed (CH378 limitation)\r\n", dir);
        }
    } else {
        status = CH378_File_Delete(&ch378_g, argv[1]);
        if (status == ERR_SUCCESS) {
            printf("rm: removed '%s'\r\n", argv[1]);
        } else {
            printf("rm: cannot remove '%s' (status=%02X)\r\n", argv[1], status);
        }
    }
}

/* ------------------------------------------------------------------------ */
/* CLI 主入口 */
/* ------------------------------------------------------------------------ */

void CLI_Init(void)
{
    /* CLI 无需特殊初始化，USART2 RX 中断在 Debug_EnableRxIRQ 中启用 */
}

void CLI_Process(uint8_t *cmd, uint8_t len)
{
    char buf[CLI_BUF_SIZE];
    char *argv[CLI_MAX_ARGC];
    uint8_t argc;

    if (len == 0 || cmd == NULL) return;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;

    memcpy(buf, cmd, len);
    buf[len] = '\0';

    argc = CLI_ParseArgs(buf, argv, CLI_MAX_ARGC);
    if (argc == 0) return;

    if (strcmp(argv[0], "ls") == 0) {
        CLI_Cmd_Ls();
    } else if (strcmp(argv[0], "cd") == 0) {
        CLI_Cmd_Cd(argc, argv);
    } else if (strcmp(argv[0], "pwd") == 0) {
        CLI_Cmd_Pwd();
    } else if (strcmp(argv[0], "mkdir") == 0) {
        CLI_Cmd_Mkdir(argc, argv);
    } else if (strcmp(argv[0], "touch") == 0) {
        CLI_Cmd_Touch(argc, argv);
    } else if (strcmp(argv[0], "cat") == 0) {
        CLI_Cmd_Cat(argc, argv);
    } else if (strcmp(argv[0], "echo") == 0) {
        CLI_Cmd_Echo(argc, argv);
    } else if (strcmp(argv[0], "rm") == 0) {
        CLI_Cmd_Rm(argc, argv);
    } else {
        printf("Unknown command: %s\r\n", argv[0]);
    }
}
