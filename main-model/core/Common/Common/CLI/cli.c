#include "CLI.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "CH378/CH378.h"
#include "CS43131/cs43131.h"
#include "Display/display.h"
#include "Protocol/protocol.h"
#include "Config/config.h"
#include "CJSON/cJSON.h"
#include "hardware.h"
#include "Submodels/submodels.h"
#include "Keyboard/keyboard.h"
#include "debug.h"

extern ch378_t ch378_g;

/************************* CLI 命令行接口 *************************/

#define CLI_MAX_ENTRIES     64
#define CLI_MAX_ARGC        8
#define CLI_BUF_SIZE        256

typedef struct {
    char names[CLI_MAX_ENTRIES][14];
    uint8_t is_dir[CLI_MAX_ENTRIES];
    uint32_t size[CLI_MAX_ENTRIES];
    uint8_t count;
} cli_entries_t;

static cli_entries_t g_cli_entries;
static uint8_t g_tree_depth;
static const char *g_find_pattern;
static uint8_t g_find_found;
static uint8_t g_lfn_buf[512];

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

/* rm -rf / tree / du / find 条目收集回调 */
static void CLI_RmCollectCallback(const char *name, uint8_t is_dir, uint32_t size)
{
    if (g_cli_entries.count < CLI_MAX_ENTRIES) {
        strncpy(g_cli_entries.names[g_cli_entries.count], name, 13);
        g_cli_entries.names[g_cli_entries.count][13] = '\0';
        g_cli_entries.is_dir[g_cli_entries.count] = is_dir;
        g_cli_entries.size[g_cli_entries.count] = size;
        g_cli_entries.count++;
    }
}

/* ------------------------------------------------------------------------ */
/* 各命令实现 */
/* ------------------------------------------------------------------------ */

/* ASCII 转 UTF-16LE，双 0 结尾 */
static uint16_t CLI_AsciiToUnicode(const char *ascii, uint8_t *unicode, uint16_t max_len)
{
    uint16_t i = 0;
    while (*ascii && i < max_len - 2) {
        unicode[i++] = (uint8_t)*ascii++;
        unicode[i++] = 0;
    }
    unicode[i++] = 0;
    unicode[i++] = 0;
    return i;
}

/* 从路径中提取文件名部分（最后一个 \ 或 / 之后的内容）
 * 支持绝对路径和相对路径，两种分隔符均可。 */
static const char *CLI_ExtractFilename(const char *path)
{
    const char *sep_bs = strrchr(path, '\\');
    const char *sep_fs = strrchr(path, '/');
    if (sep_bs && sep_fs)
        return (sep_bs > sep_fs) ? sep_bs + 1 : sep_fs + 1;
    if (sep_bs) return sep_bs + 1;
    if (sep_fs) return sep_fs + 1;
    return path;
}

/* 判断文件名是否为有效的 FAT 短文件名 */
static uint8_t CLI_IsShortName(const char *name)
{
    uint8_t has_dot = 0;
    uint8_t len = strlen(name);
    uint8_t i;

    if (len > 12) return 0;

    for (i = 0; i < len; i++) {
        char c = name[i];
        if (c == '.') {
            if (has_dot) return 0;
            has_dot = 1;
        } else if (c >= 'a' && c <= 'z') {
            return 0;
        } else if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '~' || c == '-')) {
            return 0;
        }
    }

    if (has_dot) {
        const char *dot = strchr(name, '.');
        if ((uint8_t)(dot - name) > 8) return 0;
        if (strlen(dot + 1) > 3) return 0;
    } else {
        if (len > 8) return 0;
    }

    return 1;
}

/* 长文件名操作辅助：分离路径和文件名，执行 LFN 操作 */
static uint8_t CLI_LFN_Operation(const char *full_path, uint8_t op)
{
    const char *filename;
    uint8_t unicode_name[512];
    uint8_t status;

    /* 分离文件名用于生成 UTF-16LE LFN；完整路径直接传给 CH378，
     * 由固件自行解析父目录与目标文件名 */
    filename = strrchr(full_path, '\\');
    if (filename) {
        filename++;
    } else {
        filename = full_path;
    }

    /* ASCII 转 UTF-16LE */
    (void)CLI_AsciiToUnicode(filename, unicode_name, sizeof(unicode_name));

    switch (op) {
        case 0:  /* open */
            status = CH378_Open_Long_Name((uint8_t*)full_path, unicode_name);
            break;
        case 1:  /* create file */
            status = CH378_Create_Long_File((uint8_t*)full_path, unicode_name);
            break;
        case 2:  /* create dir */
            status = CH378_Create_Long_Dir((uint8_t*)full_path, unicode_name);
            break;
        case 3:  /* erase */
            status = CH378_Erase_Long_Name((uint8_t*)full_path, unicode_name);
            break;
        default:
            return ERR_PARAMETER_ERROR;
    }

    return status;
}

static void CLI_Cmd_Ls(void)
{
    uint8_t i;
    char display_name[256];

    printf("--- %s ---\r\n", CH378_Dir_Get_Path());

    g_cli_entries.count = 0;
    CH378_Dir_List(&ch378_g, CLI_RmCollectCallback);

    for (i = 0; i < g_cli_entries.count; i++) {
        char full_path[CH378_MAX_PATH_LEN];

        CH378_Path_Join(ch378_current_path_sfn, g_cli_entries.names[i], full_path, sizeof(full_path));

        /* 获取长文件名（LFN）；CH378 CMD10_GET_LONG_FILE_NAME 返回的 lfn_len 精确可靠，
         * 但数据末尾不含 00 00 终止符，需严格按返回长度解析。 */
        {
            uint8_t status;
            uint8_t lfn_status;
            uint16_t j;
            uint16_t lfn_len = 0;
            uint8_t lfn_ok = 0;

            display_name[0] = '\0';

            status = CH378FileOpen((uint8_t*)full_path);
            if (status == ERR_SUCCESS || status == ERR_OPEN_DIR) {
                memset(g_lfn_buf, 0, sizeof(g_lfn_buf));
                lfn_status = CH378SendCmdWaitInt(CMD10_GET_LONG_FILE_NAME);
                if (lfn_status == ERR_SUCCESS) {
                    lfn_len = CH378ReadReqBlock(g_lfn_buf);
                    /* CH378 返回的 LFN 为 UTF-16LE，长度必须是偶数且在合理范围 */
                    if (lfn_len >= 2 && lfn_len <= 512 && (lfn_len % 2) == 0) {
                        for (j = 0; j < 255 && (j * 2 + 1) < lfn_len; j++) {
                            display_name[j] = g_lfn_buf[j * 2];
                        }
                        display_name[j] = '\0';
                        if (display_name[0] != '\0')
                            lfn_ok = 1;
                    }
                }
                CH378FileClose(0);
            }

            if (lfn_ok)
                goto use_lfn;
        }
        {
            strncpy(display_name, g_cli_entries.names[i], 13);
            display_name[13] = '\0';
        }
use_lfn:;

        if (g_cli_entries.is_dir[i])
            printf("  [DIR]  %s\r\n", display_name);
        else
            printf("  [FILE] %s  (%lu bytes)\r\n", display_name, g_cli_entries.size[i]);
    }
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
        const char *cwd = CH378_Dir_Get_Path();
        printf("%s\r\n", cwd);
        /* CWD notification tag: parsed by WCH Terminal App for path sync.
         * When cd is initiated by another client (Display/UART CLI),
         * the App receives this as unsolicited CLI output and syncs its path. */
        printf("[CWD] %s\r\n", cwd);
        /* Push CWD notification to Display for path synchronization */
        Display_SendCWDNotify(display_ptr, cwd);
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

    CH378_Path_Join(ch378_current_path_sfn, argv[1], full_path, sizeof(full_path));

    if (CLI_IsShortName(CLI_ExtractFilename(argv[1]))) {
        status = CH378DirCreate((uint8_t*)full_path);
    } else {
        status = CLI_LFN_Operation(full_path, 2);
    }

    if (status == ERR_SUCCESS) {
        printf("mkdir: created '%s'\r\n", argv[1]);
    } else {
        printf("mkdir: %s: failed (status=%02X)\r\n", argv[1], status);
    }
}

static void CLI_Cmd_Touch(uint8_t argc, char **argv)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;

    if (argc < 2) {
        printf("Usage: touch <file>\r\n");
        return;
    }

    CH378_Path_Join(ch378_current_path_sfn, argv[1], full_path, sizeof(full_path));

    if (CLI_IsShortName(CLI_ExtractFilename(argv[1]))) {
        status = CH378FileCreate((uint8_t*)full_path);
        if (status == ERR_SUCCESS) {
            CH378FileClose(1);
        }
    } else {
        status = CLI_LFN_Operation(full_path, 1);
        if (status == ERR_SUCCESS) {
            CH378FileClose(1);
        }
    }

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

    CH378_Path_Join(ch378_current_path_sfn, argv[1], full_path, sizeof(full_path));

    if (CLI_IsShortName(CLI_ExtractFilename(argv[1]))) {
        status = CH378FileOpen((uint8_t*)full_path);
    } else {
        status = CLI_LFN_Operation(full_path, 0);
    }
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

static uint8_t CLI_File_Append(const char *filename, const uint8_t *data, uint16_t len)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint16_t real_len;
    uint32_t fsize;

    CH378_Path_Join(ch378_current_path_sfn, filename, full_path, sizeof(full_path));

    status = CH378FileOpen((uint8_t*)full_path);
    if (status == ERR_MISS_FILE) {
        status = CH378FileCreate((uint8_t*)full_path);
        if (status != ERR_SUCCESS) return status;
        status = CH378FileOpen((uint8_t*)full_path);
    }
    if (status != ERR_SUCCESS) return status;

    fsize = CH378GetFileSize();
    if (fsize > 0) {
        status = CH378ByteLocate(fsize);
        if (status != ERR_SUCCESS) {
            CH378FileClose(0);
            return status;
        }
    }

    status = CH378ByteWrite((uint8_t*)data, len, &real_len);
    CH378FileClose(1);
    return status;
}

static void CLI_Cmd_Echo(uint8_t argc, char **argv)
{
    uint8_t i;
    uint8_t redirect_idx = 0;
    uint8_t append_mode = 0;
    uint8_t text_start = 1;
    uint8_t text_end = argc;

    if (argc < 2) {
        printf("\r\n");
        return;
    }

    /* 查找 > 或 >> 重定向符号 */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">>") == 0) {
            redirect_idx = i;
            text_end = i;
            append_mode = 1;
            break;
        } else if (strcmp(argv[i], ">") == 0) {
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

        if (append_mode) {
            status = CLI_File_Append(argv[redirect_idx + 1], (const uint8_t*)text_buf, pos);
            if (status == ERR_SUCCESS) {
                printf("Appended %d bytes to %s\r\n", pos, argv[redirect_idx + 1]);
            } else {
                printf("echo: append failed (status=%02X)\r\n", status);
            }
        } else {
            char full_path[CH378_MAX_PATH_LEN];
            CH378_Path_Join(ch378_current_path_sfn, argv[redirect_idx + 1], full_path, sizeof(full_path));
            status = CH378FileCreate((uint8_t*)full_path);
            if (status == ERR_SUCCESS) {
                CH378FileClose(1);
                status = CH378FileOpen((uint8_t*)full_path);
                if (status == ERR_SUCCESS) {
                    status = CH378ByteWrite((uint8_t*)text_buf, pos, NULL);
                    CH378FileClose(1);
                }
            }
            if (status == ERR_SUCCESS) {
                printf("Wrote %d bytes to %s\r\n", pos, argv[redirect_idx + 1]);
            } else {
                printf("echo: write failed (status=%02X)\r\n", status);
            }
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
    char full_path[CH378_MAX_PATH_LEN];

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
            CH378_Path_Join(ch378_current_path_sfn, g_cli_entries.names[i], full_path, sizeof(full_path));
            CH378FileErase((uint8_t*)full_path);
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

/* ------------------------------------------------------------------------ */
static uint8_t s_write_file_open = 0;
static uint32_t s_write_total_bytes = 0;

/* write 命令：多帧文件写入（支持最大 20KB）                                    */
/*   write "path" content   — 单次写入（覆盖）                               */
/*   write -s "path" data   — 开始写入：创建/截断文件 + 写第一块             */
/*   write -a data          — 追加写入：追加数据到已打开的文件               */
/*   write -e data          — 结束写入：追加最后一块 + 关闭文件             */
/*   write -e               — 结束写入：关闭已打开的文件（无数据）           */
/* raw_buf 是 CLI_ParseArgs 之前的原始命令副本，数据提取基于此缓冲区。        */
/* 每帧写入后立即关闭文件（与 echo 相同模式），确保数据落盘。                  */

#define WRITE_MAX_FILE_SIZE  (20 * 1024)  /* 20KB limit */

/* 在原始缓冲区中定位 "path" 的起止位置以及其后的数据起始偏移 */
static uint8_t cli_write_find_path(const char *raw, uint8_t raw_len,
                                    uint8_t search_from,
                                    char *path_out, uint8_t path_max,
                                    uint8_t *data_start_out)
{
    uint8_t i = search_from;
    uint8_t p1, p2, plen;
    while (i < raw_len && raw[i] != '"') i++;
    if (i >= raw_len) return 0;
    p1 = i + 1;
    i = p1;
    while (i < raw_len && raw[i] != '"') i++;
    if (i >= raw_len) return 0;
    p2 = i;
    plen = p2 - p1;
    if (plen == 0 || plen >= path_max) return 0;
    memcpy(path_out, raw + p1, plen);
    path_out[plen] = '\0';
    *data_start_out = p2 + 2;
    return 1;
}

/* 保存多帧写入的目标路径 */
static char s_write_path[128];

static void CLI_Cmd_Write(uint8_t argc, char **argv, char *raw_buf, uint8_t raw_len)
{
    uint8_t status;
    char full_path[CH378_MAX_PATH_LEN];
    char path[128];
    uint8_t data_start;
    uint8_t prefix_len;

    if (argc < 2) {
        printf("Usage: write \"path\" <data>\r\n");
        printf("       write -s \"path\" <data>  (start multi-frame)\r\n");
        printf("       write -a <data>          (append chunk)\r\n");
        printf("       write -e [data]          (end + close)\r\n");
        return;
    }

    /* --- Multi-frame start: write -s "path" data --- */
    if (strcmp(argv[1], "-s") == 0) {
        if (argc < 3) {
            printf("write: -s requires file path\r\n");
            return;
        }
        if (!cli_write_find_path(raw_buf, raw_len, 8, path, sizeof(path), &data_start)) {
            printf("write: cannot parse path\r\n");
            return;
        }
        /* Save path for subsequent -a/-e frames */
        strncpy(s_write_path, path, sizeof(s_write_path) - 1);
        s_write_path[sizeof(s_write_path) - 1] = '\0';

        CH378_Path_Join(ch378_current_path_sfn, path, full_path, sizeof(full_path));
        /* Create (truncate) then open, write, close — same pattern as echo */
        status = CH378FileCreate((uint8_t*)full_path);
        if (status != ERR_SUCCESS) {
            printf("write: cannot create '%s' (status=%02X)\r\n", path, status);
            return;
        }
        CH378FileClose(1);
        status = CH378FileOpen((uint8_t*)full_path);
        if (status != ERR_SUCCESS) {
            printf("write: cannot open '%s' (status=%02X)\r\n", path, status);
            return;
        }
        s_write_file_open = 1;
        s_write_total_bytes = 0;

        if (data_start < raw_len) {
            uint16_t data_len = raw_len - data_start;
            if (data_len > 0 && data_len <= WRITE_MAX_FILE_SIZE) {
                uint16_t real_len = 0;
                status = CH378ByteWrite((uint8_t*)(raw_buf + data_start), data_len, &real_len);
                if (status == ERR_SUCCESS)
                    s_write_total_bytes += real_len;
            }
        }
        /* Close file immediately to flush data to disk */
        CH378FileClose(1);
        printf("write: started '%s' (%lu bytes)\r\n", path, (unsigned long)s_write_total_bytes);
        return;
    }

    /* --- Multi-frame append: write -a data --- */
    if (strcmp(argv[1], "-a") == 0) {
        if (!s_write_file_open) {
            printf("write: no active write session\r\n");
            return;
        }
        prefix_len = 9;  /* "write -a " */
        if (raw_len > prefix_len && s_write_total_bytes < WRITE_MAX_FILE_SIZE) {
            uint16_t data_len = raw_len - prefix_len;
            uint32_t remaining = WRITE_MAX_FILE_SIZE - s_write_total_bytes;
            if (data_len > remaining) data_len = (uint16_t)remaining;

            /* Open file, seek to end, write, close */
            CH378_Path_Join(ch378_current_path_sfn, s_write_path, full_path, sizeof(full_path));
            status = CH378FileOpen((uint8_t*)full_path);
            if (status != ERR_SUCCESS) {
                printf("write: reopen failed (status=%02X)\r\n", status);
                return;
            }
            if (s_write_total_bytes > 0) {
                CH378ByteLocate(s_write_total_bytes);
            }
            {
                uint16_t real_len = 0;
                status = CH378ByteWrite((uint8_t*)(raw_buf + prefix_len), data_len, &real_len);
                if (status == ERR_SUCCESS)
                    s_write_total_bytes += real_len;
            }
            CH378FileClose(1);
        }
        printf("write: appended (%lu bytes total)\r\n", (unsigned long)s_write_total_bytes);
        return;
    }

    /* --- Multi-frame end: write -e [data] --- */
    if (strcmp(argv[1], "-e") == 0) {
        if (!s_write_file_open) {
            printf("write: no active write session\r\n");
            return;
        }
        prefix_len = 9;  /* "write -e " */
        if (raw_len > prefix_len && s_write_total_bytes < WRITE_MAX_FILE_SIZE) {
            uint16_t data_len = raw_len - prefix_len;
            uint32_t remaining = WRITE_MAX_FILE_SIZE - s_write_total_bytes;
            if (data_len > remaining) data_len = (uint16_t)remaining;

            CH378_Path_Join(ch378_current_path_sfn, s_write_path, full_path, sizeof(full_path));
            status = CH378FileOpen((uint8_t*)full_path);
            if (status != ERR_SUCCESS) {
                printf("write: reopen failed (status=%02X)\r\n", status);
                s_write_file_open = 0;
                return;
            }
            if (s_write_total_bytes > 0) {
                CH378ByteLocate(s_write_total_bytes);
            }
            {
                uint16_t real_len = 0;
                CH378ByteWrite((uint8_t*)(raw_buf + prefix_len), data_len, &real_len);
                s_write_total_bytes += real_len;
            }
            CH378FileClose(1);
        }
        s_write_file_open = 0;
        printf("write: done (%lu bytes total)\r\n", (unsigned long)s_write_total_bytes);
        return;
    }

    /* --- Single-frame write: write "path" content --- */
    {
        if (!cli_write_find_path(raw_buf, raw_len, 6, path, sizeof(path), &data_start)) {
            printf("write: cannot parse path\r\n");
            return;
        }
        CH378_Path_Join(ch378_current_path_sfn, path, full_path, sizeof(full_path));
        status = CH378FileCreate((uint8_t*)full_path);
        if (status != ERR_SUCCESS) {
            printf("write: cannot create '%s' (status=%02X)\r\n", path, status);
            return;
        }
        CH378FileClose(1);
        status = CH378FileOpen((uint8_t*)full_path);
        if (status != ERR_SUCCESS) {
            printf("write: cannot open '%s' (status=%02X)\r\n", path, status);
            return;
        }
        if (data_start < raw_len) {
            uint16_t data_len = raw_len - data_start;
            if (data_len > WRITE_MAX_FILE_SIZE) data_len = WRITE_MAX_FILE_SIZE;
            if (data_len > 0) {
                CH378ByteWrite((uint8_t*)(raw_buf + data_start), data_len, NULL);
            }
        }
        CH378FileClose(1);
        printf("Wrote to %s\r\n", path);
    }
}

static void CLI_Cmd_Rm(uint8_t argc, char **argv)
{
    uint8_t status;

    if (argc < 2) {
        printf("Usage: rm <file> or rm -rf <dir>\r\n");
        return;
    }

    if (argc >= 3 && strcmp(argv[1], "-rf") == 0) {
        char full_path[CH378_MAX_PATH_LEN];
        const char *dir = argv[2];

        CH378_Path_Join(ch378_current_path_sfn, dir, full_path, sizeof(full_path));

        /* 递归清空目录内所有文件（包括子目录中的文件） */
        CLI_Cmd_RmRecursive(dir);

        /* 尝试删除目录本身。
         * CH378 的 CMD0H_FILE_ERASE 官方注释明确说明"对目录则等待"，
         * 即不支持删除目录，此步骤预期会失败。 */
        status = CH378FileErase((uint8_t*)full_path);
        if (status == ERR_SUCCESS) {
            printf("rm -rf: removed '%s'\r\n", dir);
        } else {
            printf("rm -rf: cleared '%s', but directory itself cannot be removed (CH378 limitation)\r\n", dir);
        }
    } else {
        char full_path[CH378_MAX_PATH_LEN];
        CH378_Path_Join(ch378_current_path_sfn, argv[1], full_path, sizeof(full_path));

        if (CLI_IsShortName(CLI_ExtractFilename(argv[1]))) {
            status = CH378FileErase((uint8_t*)full_path);
        } else {
            status = CLI_LFN_Operation(full_path, 3);
        }
        if (status == ERR_SUCCESS) {
            printf("rm: removed '%s'\r\n", argv[1]);
        } else {
            printf("rm: cannot remove '%s' (status=%02X)\r\n", argv[1], status);
        }
    }
}

/* ---- 设备/Display 交互命令 ---- */

/* 辅助：通过 Display UART 发送一帧命令给 Display */
static void CLI_SendToDisplay(uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint16_t len;

    len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY, cmd,
                             data, data_len, buf, sizeof(buf));
    if (len > 0)
        Display_Send_Data(&display_g, buf, len);
}

static void CLI_Cmd_Lsdev(void)
{
    uint8_t i;
    const char *names[HB_MAX_SLOTS] = {
        "Display", "Keyboard", "Power", "Submodel1", "Submodel2", "Submodel3"
    };

    printf("Module Status:\r\n");
    for (i = 0; i < HB_MAX_SLOTS; i++)
    {
        const char *status_str;
        switch (hardware_g.hb_slots[i].status)
        {
            case HB_STATUS_ONLINE:  status_str = "ONLINE";  break;
            case HB_STATUS_OFFLINE: status_str = "OFFLINE"; break;
            default:                status_str = "UNKNOWN"; break;
        }
        printf("  %-10s  %s  type=0x%02X  subtype=0x%02X  miss=%d\r\n",
               names[i], status_str,
               hardware_g.hb_slots[i].type,
               hardware_g.hb_slots[i].subtype,
               hardware_g.hb_slots[i].miss_count);
    }
}

static uint8_t CLI_IsSubDispOnline(void)
{
    uint8_t i;
    for (i = 3; i < HB_MAX_SLOTS; i++)
    {
        if (hardware_g.hb_slots[i].subtype == MODULE_SUBTYPE_SUBMODEL_SUB_DISPLAY &&
            hardware_g.hb_slots[i].status == HB_STATUS_ONLINE)
            return 1;
    }
    return 0;
}

static void CLI_Cmd_Bmp(uint8_t argc, char **argv)
{
    char path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint16_t real_len;
    uint32_t file_size;
    uint32_t total_read = 0;
    uint8_t buf[64];
    uint8_t send_to_sub = 0;

    if (argc < 2) {
        printf("Usage: bmp get <filename> [sub]\r\n");
        printf("       bmp ls\r\n");
        return;
    }

    /* bmp ls: 列出 BMP 文件夹中的文件 */
    if (strcmp(argv[1], "ls") == 0) {
        char saved_path[CH378_MAX_PATH_LEN];
        char saved_sfn[CH378_MAX_PATH_LEN];
        uint8_t i;

        /* 保存当前工作目录 */
        strncpy(saved_path, ch378_current_path, CH378_MAX_PATH_LEN);
        saved_path[CH378_MAX_PATH_LEN - 1] = '\0';
        strncpy(saved_sfn, ch378_current_path_sfn, CH378_MAX_PATH_LEN);
        saved_sfn[CH378_MAX_PATH_LEN - 1] = '\0';

        /* 进入根目录下的 BMP 目录（确保从根目录查找） */
        CH378_Dir_Go_Root(&ch378_g);
        status = CH378_Dir_Enter(&ch378_g, "BMP");
        if (status != ERR_SUCCESS) {
            printf("BMP directory not found\r\n");
            return;
        }

        /* 收集目录条目 */
        g_cli_entries.count = 0;
        CH378_Dir_List(&ch378_g, CLI_RmCollectCallback);

        printf("--- \\BMP ---\r\n");
        for (i = 0; i < g_cli_entries.count; i++) {
            if (g_cli_entries.is_dir[i])
                printf("  [DIR]  %s\r\n", g_cli_entries.names[i]);
            else
                printf("  [FILE] %s  (%lu bytes)\r\n", g_cli_entries.names[i], g_cli_entries.size[i]);
        }
        if (g_cli_entries.count == 0)
            printf("  (empty)\r\n");

        /* 恢复原工作目录 */
        strncpy(ch378_current_path, saved_path, CH378_MAX_PATH_LEN);
        strncpy(ch378_current_path_sfn, saved_sfn, CH378_MAX_PATH_LEN);
        return;
    }

    /* bmp get <filename> [sub] */
    if (strcmp(argv[1], "get") != 0 || argc < 3) {
        printf("Usage: bmp get <filename> [sub]\r\n");
        printf("       bmp ls\r\n");
        return;
    }

    /* 检查是否有 "sub" 参数 */
    if (argc >= 4 && strcmp(argv[3], "sub") == 0) {
        if (CLI_IsSubDispOnline()) {
            send_to_sub = 1;
        } else {
            printf("SubDisplay not online\r\n");
        }
    }

    snprintf(path, sizeof(path), "\\BMP\\%s.BMP", argv[2]);

    status = CH378FileOpen((uint8_t *)path);
    if (status != ERR_SUCCESS) {
        printf("File not found: %s\r\n", path);
        return;
    }

    file_size = CH378GetFileSize();
    printf("BMP: %s  size=%lu bytes\r\n", argv[2], file_size);

    if (send_to_sub) {
        /* 发送到 SubDisplay：只验证文件可读，不输出 hex dump */
        CH378FileClose(0);
        hardware_g.subdisp_req.cmd = SUBDISP_CMD_SEND_BMP;
        strncpy((char *)hardware_g.subdisp_req.filename, argv[2], SUBDISP_FILENAME_MAX_LEN - 1);
        hardware_g.subdisp_req.filename[SUBDISP_FILENAME_MAX_LEN - 1] = '\0';
        hardware_g.subdisp_req.pending = 1;
        printf("Send to SubDisplay (pending)\r\n");
        return;
    }

    /* 仅查看模式：读取并输出文件内容（十六进制） */
    while (total_read < file_size) {
        uint16_t to_read = (file_size - total_read > sizeof(buf)) ? sizeof(buf) : (uint16_t)(file_size - total_read);
        uint16_t i;

        status = CH378ByteRead(buf, to_read, &real_len);
        if (status != ERR_SUCCESS || real_len == 0)
            break;

        for (i = 0; i < real_len; i++)
            printf("%02X ", buf[i]);
        total_read += real_len;
    }
    printf("\r\n");

    CH378FileClose(0);
}

static void CLI_Cmd_Lsstatus(void)
{
    audio_state_t audio_st;
    uint8_t vol;
    uint32_t play_time;
    uint8_t i;
    const char *names[HB_MAX_SLOTS] = {
        "Display", "Keyboard", "Power", "Submodel1", "Submodel2", "Submodel3"
    };

    printf("=== System Status ===\r\n");

    /* 音频状态 */
    audio_st = Audio_GetState();
    vol = Audio_GetVolume();
    play_time = Audio_GetPlayTime_ms();
    {
        const char *st_str;
        switch (audio_st) {
            case AUDIO_STATE_IDLE:    st_str = "IDLE";    break;
            case AUDIO_STATE_PLAYING: st_str = "PLAYING"; break;
            case AUDIO_STATE_PAUSED:  st_str = "PAUSED";  break;
            case AUDIO_STATE_STOPPED: st_str = "STOPPED"; break;
            default:                  st_str = "UNKNOWN"; break;
        }
        printf("Audio:     %s  vol=%d  time=%lus\r\n",
               st_str, vol, play_time / 1000);
    }

    /* 电量 */
    printf("Battery:   N/A\r\n");

    /* 蓝牙 */
    printf("Bluetooth: N/A\r\n");

    /* 屏幕亮度 */
    {
        int brightness = 0;
        Config_GetInt("0101", "brightness", &brightness);
        printf("Display:   brightness=%d\r\n", brightness);
    }

    /* 键盘背光 */
    {
        int kbd_backlight = 0;
        Config_GetInt("0401", "backlight", &kbd_backlight);
        printf("Keyboard:  backlight=%d\r\n", kbd_backlight);
    }

    /* CH378 */
    printf("CH378:     device=%s  enable=%d\r\n",
           (ch378_g.now_device == CH378_Device_USB) ? "USB" :
           (ch378_g.now_device == CH378_Device_TF) ? "SD" : "None",
           ch378_g.enable);

    /* CH9350 */
    printf("CH9350:    hid_connected=%d\r\n", ch9350_g.dev_connected);

    /* RGB */
    printf("RGB:       mode=%d  brightness=%d\r\n",
           hardware_g.rgb_config.mode,
           hardware_g.rgb_config.brightness);

    /* 配置 */
    printf("Config:    applied=%d\r\n", Config_IsApplied());

    /* 模块列表 */
    printf("\r\nModules:\r\n");
    for (i = 0; i < HB_MAX_SLOTS; i++)
    {
        const char *status_str;
        switch (hardware_g.hb_slots[i].status)
        {
            case HB_STATUS_ONLINE:  status_str = "ONLINE";  break;
            case HB_STATUS_OFFLINE: status_str = "OFFLINE"; break;
            default:                status_str = "UNKNOWN"; break;
        }
        printf("  %-10s  %s  type=0x%02X  subtype=0x%02X  miss=%d\r\n",
               names[i], status_str,
               hardware_g.hb_slots[i].type,
               hardware_g.hb_slots[i].subtype,
               hardware_g.hb_slots[i].miss_count);
    }
}

static void CLI_Cmd_Subdisp(uint8_t argc, char **argv)
{
    if (argc < 2) {
        printf("Usage:\r\n");
        printf("  subdisp mode <0|1>       Switch display mode (0=status, 1=image)\r\n");
        printf("  subdisp refresh status   Send current status to SubDisplay\r\n");
        return;
    }

    if (!CLI_IsSubDispOnline()) {
        printf("SubDisplay not online\r\n");
        return;
    }

    if (strcmp(argv[1], "mode") == 0) {
        if (argc < 3) {
            printf("Usage: subdisp mode <0|1>\r\n");
            return;
        }
        int mode = atoi(argv[2]);
        if (mode < 0 || mode > 1) {
            printf("Invalid mode: %d (must be 0 or 1)\r\n", mode);
            return;
        }
        hardware_g.subdisp_req.cmd = SUBDISP_CMD_SET_MODE;
        hardware_g.subdisp_req.mode = (uint8_t)mode;
        hardware_g.subdisp_req.pending = 1;
        printf("SubDisplay mode: %s (pending)\r\n",
               mode == 0 ? "Status" : "Image");
    }
    else if (strcmp(argv[1], "refresh") == 0) {
        if (argc < 3 || strcmp(argv[2], "status") != 0) {
            printf("Usage: subdisp refresh status\r\n");
            return;
        }
        hardware_g.subdisp_req.cmd = SUBDISP_CMD_REFRESH_STATUS;
        hardware_g.subdisp_req.pending = 1;
        printf("Send status to SubDisplay (pending)\r\n");
    }
    else {
        printf("Unknown subdisp command: %s\r\n", argv[1]);
    }
}

static void CLI_Cmd_Light(uint8_t argc, char **argv)
{
    int val;
    uint8_t brightness;

    if (argc < 2) {
        printf("Usage: light <0-100>\r\n");
        return;
    }

    val = atoi(argv[1]);
    if (val < 0) val = 0;
    if (val > 100) val = 100;

    brightness = (uint8_t)val;
    CLI_SendToDisplay(CMD_DISP_SET_BRIGHTNESS, &brightness, 1);
    printf("Set brightness: %d%%\r\n", brightness);
}

static void CLI_Cmd_Note(uint8_t argc, char **argv)
{
    uint8_t data[128];
    uint8_t priority = 0;
    uint16_t text_len;
    uint8_t i;

    if (argc < 2) {
        printf("Usage: note <text>\r\n");
        return;
    }

    /* 拼接所有参数为文本内容 */
    {
        char text_buf[110];
        uint16_t pos = 0;
        for (i = 1; i < argc && pos < sizeof(text_buf) - 1; i++) {
            if (i > 1 && pos < sizeof(text_buf) - 1) {
                text_buf[pos++] = ' ';
            }
            uint16_t alen = strlen(argv[i]);
            if (pos + alen >= sizeof(text_buf))
                alen = sizeof(text_buf) - 1 - pos;
            memcpy(&text_buf[pos], argv[i], alen);
            pos += alen;
        }
        text_buf[pos] = '\0';
        text_len = pos;

        /* DATA[0]=priority, DATA[1..16]=title(16字节补0), DATA[17..N]=content */
        data[0] = priority;
        memset(&data[1], 0, 16);
        {
            const char *title = "Notice";
            uint8_t tlen = strlen(title);
            if (tlen > 16) tlen = 16;
            memcpy(&data[1], title, tlen);
        }
        if (text_len > sizeof(data) - 17)
            text_len = sizeof(data) - 17;
        memcpy(&data[17], text_buf, text_len);
    }

    CLI_SendToDisplay(CMD_DISP_SHOW_NOTICE, data, 17 + text_len);
    printf("Notice sent\r\n");
}

static void CLI_Cmd_Mouse(uint8_t argc, char **argv)
{
    uint8_t data[5];
    int x, y;
    uint8_t buttons = 0;

    if (argc < 4) {
        printf("Usage: mouse <left/right/none> <x> <y>\r\n");
        return;
    }

    if (strcmp(argv[1], "left") == 0)
        buttons = 0x01;
    else if (strcmp(argv[1], "right") == 0)
        buttons = 0x02;
    else if (strcmp(argv[1], "none") == 0)
        buttons = 0x00;
    else {
        printf("Usage: mouse <left/right/none> <x> <y>\r\n");
        return;
    }

    x = atoi(argv[2]);
    y = atoi(argv[3]);
    if (x < -128) x = -128; if (x > 127) x = 127;
    if (y < -128) y = -128; if (y > 127) y = 127;

    /* 鼠标报告: DATA[0]=0x01(mouse), DATA[1]=buttons, DATA[2]=dx, DATA[3]=dy, DATA[4]=wheel */
    data[0] = INPUT_DEV_MOUSE;
    data[1] = buttons;
    data[2] = (int8_t)x;
    data[3] = (int8_t)y;
    data[4] = 0;

    CLI_SendToDisplay(CMD_DISP_INPUT_EVENT, data, 5);

    /* 发送按钮释放事件 */
    if (buttons != 0) {
        data[1] = 0x00;
        data[2] = 0;
        data[3] = 0;
        CLI_SendToDisplay(CMD_DISP_INPUT_EVENT, data, 5);
    }

    printf("Mouse: btn=%s dx=%d dy=%d\r\n", argv[1], x, y);
}

static void CLI_Cmd_Roll(uint8_t argc, char **argv)
{
    uint8_t data[5];
    int delta;

    if (argc < 2) {
        printf("Usage: roll <delta>\r\n");
        return;
    }

    delta = atoi(argv[1]);
    if (delta < -128) delta = -128;
    if (delta > 127) delta = 127;

    /* 鼠标报告: buttons=0, dx=0, dy=0, wheel=delta */
    data[0] = INPUT_DEV_MOUSE;
    data[1] = 0x00;
    data[2] = 0;
    data[3] = 0;
    data[4] = (int8_t)delta;

    CLI_SendToDisplay(CMD_DISP_INPUT_EVENT, data, 5);
    printf("Roll: %d\r\n", delta);
}

/* 键名 -> USB HID Usage ID 映射表 */
static uint8_t CLI_KeyNameToHID(const char *name)
{
    /* 单字符: 0-9, a-z, 基本标点（不含需要Shift的符号） */
    if (strlen(name) == 1)
    {
        char c = name[0];
        if (c >= 'a' && c <= 'z') return 0x04 + (c - 'a');
        if (c >= 'A' && c <= 'Z') return 0x04 + (c - 'A');
        if (c >= '1' && c <= '9') return 0x1E + (c - '1');
        if (c == '0')             return 0x27;
        if (c == ' ')             return 0x2C;
        if (c == '.')             return 0x37;
        if (c == ',')             return 0x36;
        if (c == ';')             return 0x33;
        if (c == '/')             return 0x38;
        if (c == '\\')            return 0x31;
        if (c == '[')             return 0x2F;
        if (c == ']')             return 0x30;
        if (c == '-')             return 0x2D;
        if (c == '=')             return 0x2E;
        if (c == '`')             return 0x35;
        if (c == '\'')            return 0x34;
        return 0x00;
    }

    /* 特殊键名 */
    if (strcmp(name, "SPACE") == 0)   return 0x2C;
    if (strcmp(name, "ENTER") == 0)   return 0x28;
    if (strcmp(name, "UP") == 0)      return 0x52;
    if (strcmp(name, "DOWN") == 0)    return 0x51;
    if (strcmp(name, "LEFT") == 0)    return 0x50;
    if (strcmp(name, "RIGHT") == 0)   return 0x4F;
    if (strcmp(name, "ESC") == 0)     return 0x29;
    if (strcmp(name, "BACKSPACE") == 0) return 0x2A;
    if (strcmp(name, "TAB") == 0)     return 0x2B;
    if (strcmp(name, "DEL") == 0)     return 0x4C;
    if (strcmp(name, "HOME") == 0)    return 0x4A;
    if (strcmp(name, "END") == 0)     return 0x4D;
    if (strcmp(name, "PGUP") == 0)    return 0x4B;
    if (strcmp(name, "PGDN") == 0)    return 0x4E;

    return 0x00;
}

/* ---- music 命令：控制音乐键盘 (Keyboard-3) ---- */
static void CLI_Cmd_Music(uint8_t argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: music <start|stop|status>\r\n");
        printf("  start  - 启动 Keyboard-3 事件定时上报\r\n");
        printf("  stop   - 停止 Keyboard-3 事件定时上报\r\n");
        printf("  status - 查询音乐键盘状态\r\n");
        return;
    }

    if (strcmp(argv[1], "start") == 0) {
        if (Keyboard_Music_Start()) {
            printf("Music keyboard started\r\n");
        } else {
            printf("Error: music keyboard not available (Keyboard-3 not connected?)\r\n");
        }
    } else if (strcmp(argv[1], "stop") == 0) {
        if (Keyboard_Music_Stop()) {
            printf("Music keyboard stopped\r\n");
        } else {
            printf("Error: music keyboard not available\r\n");
        }
    } else if (strcmp(argv[1], "status") == 0) {
        printf("Music keyboard: %s\r\n",
               Keyboard_Music_IsActive() ? "ACTIVE" : "INACTIVE");
    } else {
        printf("music: unknown subcommand '%s' (use start/stop/status)\r\n", argv[1]);
    }
}

static void CLI_Cmd_Keyboard(uint8_t argc, char **argv)
{
    uint8_t data[9];
    uint8_t hid_code;

    if (argc < 2) {
        printf("Usage: keyboard <key>\r\n");
        printf("  Keys: a-z 0-9 SPACE ENTER UP DOWN LEFT RIGHT\r\n");
        return;
    }

    hid_code = CLI_KeyNameToHID(argv[1]);
    if (hid_code == 0x00) {
        printf("Unknown key: %s\r\n", argv[1]);
        return;
    }

    /* 键盘报告: DATA[0]=0x00(keyboard), DATA[1]=modifier, DATA[2]=reserved,
     *           DATA[3]=keycode, DATA[4..8]=0 */
    data[0] = INPUT_DEV_KEYBOARD;
    data[1] = 0x00; /* 无修饰键 */
    data[2] = 0x00;
    data[3] = hid_code;
    data[4] = 0x00;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    data[8] = 0x00;

    /* 发送按下事件 */
    CLI_SendToDisplay(CMD_DISP_INPUT_EVENT, data, 9);

    /* 发送释放事件（所有键码清零） */
    data[3] = 0x00;
    CLI_SendToDisplay(CMD_DISP_INPUT_EVENT, data, 9);

    printf("Key: 0x%02X (%s)\r\n", hid_code, argv[1]);
}

static void CLI_Cmd_Help(uint8_t argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "d") == 0) {
        /* Detailed help */
        printf("Available commands (detailed):\r\n\r\n");
        printf("  ls              List directory contents (with LFN)\r\n");
        printf("  cd <dir>        Change directory\r\n");
        printf("  pwd             Print working directory\r\n");
        printf("  mkdir <dir>     Create directory (supports LFN)\r\n");
        printf("  touch <file>    Create empty file (supports LFN)\r\n");
        printf("  cat <file>      Display file contents (supports LFN)\r\n");
        printf("  echo [text]     Print text or write to file (use > or >>)\r\n");
        printf("  write \"path\" <data>  Write data to file (supports multi-frame)\r\n");
        printf("  rm <file>       Remove file (supports LFN)\r\n");
        printf("  rm -rf <dir>    Clear directory recursively\r\n");
        printf("  cp <src> <dst>  Copy file (supports LFN)\r\n");
        printf("  mv <old> <new>  Rename file (short names only)\r\n");
        printf("  hexdump <file>  Display file in hex format (supports LFN)\r\n");
        printf("  head <file> [n] Show first n bytes of file (supports LFN)\r\n");
        printf("  tail <file> [n] Show last n bytes of file (supports LFN)\r\n");
        printf("  tree [dir]      List directory tree\r\n");
        printf("  du <dir>        Show directory total size\r\n");
        printf("  find <pattern>  Find files matching pattern\r\n");
        printf("  df              Show disk capacity\r\n");
        printf("  free            Show disk free space\r\n");
        printf("  device [usb|sd] Show or switch device mode\r\n");
        printf("  stat <file>     Show file status (supports LFN)\r\n");
        printf("  chmod <file> <attr>  Change file attributes (supports LFN)\r\n");
        printf("  ver             Show CH378 firmware version\r\n");
        printf("  vol <0-100>     Set playback volume\r\n");
        printf("  play <file> [ch]  Play WAV file (ch=0|1, default ch0 stops all)\r\n");
        printf("  pause [ch]      Pause playback (no ch=all)\r\n");
        printf("  resume [ch]     Resume playback (no ch=all)\r\n");
        printf("  stop [ch]       Stop playback, cannot resume (no ch=all)\r\n");
        printf("  playst          Show playback status\r\n");
        printf("  lsdev           List module status (online/offline/type)\r\n");
        printf("  bmp get <file> [sub]  Read BMP file (hex dump, sub=send to SubDisplay)\r\n");
        printf("  lsstatus        Show system status (audio/battery/ble/modules)\r\n");
        printf("  subdisp mode <0|1>  Switch SubDisplay mode (0=status, 1=image)\r\n");
        printf("  subdisp refresh status  Send current status to SubDisplay\r\n");
        printf("  light <0-255>   Set display brightness\r\n");
        printf("  note <text>     Show notice popup on display\r\n");
        printf("  mouse <L/R/none> <dx> <dy>  Send mouse click event\r\n");
        printf("  roll <delta>    Send mouse scroll event\r\n");
        printf("  keyboard <key>  Send keyboard event (a-z 0-9 SPACE ENTER UP DOWN LEFT RIGHT)\r\n");
        printf("  music <start|stop|status>  Control music keyboard (Keyboard-3)\r\n");
        printf("  config get [module|file.json [key]]  Get config value\r\n");
        printf("  config getkey [module|file.json]  List all key names\r\n");
        printf("  config set <key|module key|file.json key> <value>  Set config value\r\n");
        printf("  config addkey <module|file.json> <key> <value>  Add new config key\r\n");
        printf("  config newfile <file.json>  Create data file\r\n");
        printf("  config save     Save config to file\r\n");
        printf("  config backup   Backup config\r\n");
        printf("  config rollback Rollback config from backup\r\n");
        printf("  config reset    Reset to defaults\r\n");
        printf("  config ls       List CONFIG directory\r\n");
        printf("  config rm <file.json>  Remove data file\r\n");
        printf("  speaker <on|off> [left|right]  Control speaker output\r\n");
        printf("  rgb mode <mode> <r> <g> <b> <brightness> <speed>  Set RGB mode\r\n");
        printf("  rgb refresh json  Load rgb.json and queue custom frames\r\n");
        printf("  rgb status      Show RGB status\r\n");
        printf("  fp register     Start fingerprint enrollment\r\n");
        printf("  fp del <ID>     Delete fingerprint by ID\r\n");
        printf("  fp ls           List fingerprints and count\r\n");
        printf("  fp count        Query fingerprint count\r\n");
        printf("  fp config led <func> <color> [speed]  Set LED effect\r\n");
        printf("  fp config sec <level:1-3>  Set security level\r\n");
        printf("  fp set <id> <name>  Set fingerprint name (id=decimal, fp.json)\r\n");
        printf("  fp get <id>     Get fingerprint name\r\n");
        printf("  fp names        List all fingerprint names\r\n");
        printf("  nfc set <hex_id> <name>  Set NFC card name (10-hex-digit id, nfc.json)\r\n");
        printf("  nfc get <hex_id>  Get NFC card name\r\n");
        printf("  nfc ls          List all NFC card names\r\n");
        printf("  ir start        Start IR ranging (submodel-6)\r\n");
        printf("  ir stop         Stop IR ranging (submodel-6)\r\n");
        printf("  clear           Clear screen\r\n");
        printf("  help            Show command list\r\n");
        printf("  help d          Show detailed help\r\n");
    } else {
        /* Brief command list */
        printf("Commands:\r\n");
        printf("  ls, cd <dir>, pwd, mkdir <dir>, touch <file>, cat <file>\r\n");
        printf("  echo [text], write \"path\" <data>, rm <file>, rm -rf <dir>\r\n");
        printf("  cp <src> <dst>, mv <old> <new>, hexdump <file>\r\n");
        printf("  head <file> [n], tail <file> [n], tree [dir], du <dir>\r\n");
        printf("  find <pattern>, df, free, device [usb|sd]\r\n");
        printf("  stat <file>, chmod <file> <attr>, ver\r\n");
        printf("  play <file> [ch], vol <0-100>, pause [ch], resume [ch], stop [ch], playst\r\n");
        printf("  music <start|stop|status>\r\n");
        printf("  lsdev, bmp get <file> [sub], lsstatus\r\n");
        printf("  subdisp mode <0|1>, subdisp refresh status\r\n");
        printf("  light <0-255>, note <text>\r\n");
        printf("  mouse <L/R/none> <dx> <dy>, roll <delta>, keyboard <key>\r\n");
        printf("  config get|set|addkey|newfile|save|backup|rollback|reset|ls|rm\r\n");
        printf("  speaker <on|off> [left|right]\r\n");
        printf("  rgb mode|refresh|status\r\n");
        printf("  fp register|del <ID>|ls|count|config [led|sec]|set|get|names\r\n");
        printf("  nfc set <hex_id> <name>|get <hex_id>|ls\r\n");
        printf("  ir start|stop\r\n");
        printf("  clear, help [d]\r\n");
    }
}

static void CLI_Cmd_Clear(void)
{
    printf("\x1B[2J\x1B[H");
}

static void CLI_Cmd_Vol(uint8_t argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: vol <0-100>\r\n");
        return;
    }
    int vol = atoi(argv[1]);
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    Audio_SetVolume((uint8_t)vol);
    printf("Volume: %d\r\n", vol);
}

static void CLI_Cmd_Pause(uint8_t argc, char **argv)
{
    if (argc >= 2) {
        int ch = atoi(argv[1]);
        if (ch < 0 || ch >= AUDIO_MAX_CHANNELS) {
            printf("pause: invalid channel %d (0~%d)\r\n", ch, AUDIO_MAX_CHANNELS - 1);
            return;
        }
        Audio_ChannelPause((uint8_t)ch);
        printf("Paused ch%d\r\n", ch);
    } else {
        Audio_Pause();
        printf("Paused all\r\n");
    }
}

static void CLI_Cmd_Resume(uint8_t argc, char **argv)
{
    if (argc >= 2) {
        int ch = atoi(argv[1]);
        if (ch < 0 || ch >= AUDIO_MAX_CHANNELS) {
            printf("resume: invalid channel %d (0~%d)\r\n", ch, AUDIO_MAX_CHANNELS - 1);
            return;
        }
        Audio_ChannelResume((uint8_t)ch);
        printf("Resumed ch%d\r\n", ch);
    } else {
        Audio_Resume();
        printf("Resumed all\r\n");
    }
}

static void CLI_Cmd_Stop(uint8_t argc, char **argv)
{
    if (argc >= 2) {
        int ch = atoi(argv[1]);
        if (ch < 0 || ch >= AUDIO_MAX_CHANNELS) {
            printf("stop: invalid channel %d (0~%d)\r\n", ch, AUDIO_MAX_CHANNELS - 1);
            return;
        }
        Audio_ChannelStop((uint8_t)ch);
        printf("Stopped ch%d\r\n", ch);
    } else {
        Audio_PlayStop();
        printf("Stopped all\r\n");
    }
}

static void CLI_Cmd_Playst(void)
{
    const char *track = Audio_GetCurrentTrackName();
    uint32_t time_ms = Audio_GetPlayTime_ms();
    uint32_t sec = time_ms / 1000;
    uint32_t min = sec / 60;
    sec = sec % 60;
    audio_state_t st = Audio_GetState();
    speaker_channel_t spk = Speaker_GetState();

    const char *state_str;
    switch (st) {
        case AUDIO_STATE_PLAYING: state_str = "Playing"; break;
        case AUDIO_STATE_PAUSED:  state_str = "Paused";  break;
        case AUDIO_STATE_STOPPED: state_str = "Stopped"; break;
        default:                  state_str = "Idle";    break;
    }

    printf("State:  %s\r\n", state_str);
    printf("Track:  %s\r\n", (track && track[0]) ? track : "(none)");
    printf("Time:   %02lu:%02lu\r\n", min, sec);
    printf("Volume: %d\r\n", Audio_GetVolume());
    printf("Speaker: L=%s R=%s\r\n",
           (spk & SPEAKER_CHANNEL_LEFT) ? "ON" : "OFF",
           (spk & SPEAKER_CHANNEL_RIGHT) ? "ON" : "OFF");
}

static void CLI_Cmd_Ver(void)
{
    uint8_t ver = CH378_Get_IC_Ver();
    printf("CH378 IC Version: 0x%02X\r\n", ver);
}

static void CLI_Cmd_Df(void)
{
    CH378_Disk_Capacity(&ch378_g);
}

static void CLI_Cmd_Device(uint8_t argc, char **argv)
{
    if (argc < 2) {
        printf("Current device: %s\r\n",
               (ch378_g.now_device == CH378_Device_USB) ? "USB" :
               (ch378_g.now_device == CH378_Device_TF) ? "SD" : "Unknown");
        return;
    }

    if (strcmp(argv[1], "usb") == 0) {
        CH378_Device_Select(&ch378_g, CH378_Device_USB);
    } else if (strcmp(argv[1], "sd") == 0) {
        CH378_Device_Select(&ch378_g, CH378_Device_TF);
    } else {
        printf("Usage: device [usb|sd]\r\n");
        return;
    }
    /* Device switch resets CWD to root — notify Display and BLE App */
    const char *new_cwd = CH378_Dir_Get_Path();
    printf("[CWD] %s\r\n", new_cwd);
    Display_SendCWDNotify(display_ptr, new_cwd);
}

static void CLI_Cmd_Hexdump(uint8_t argc, char **argv)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint8_t buf[256];
    uint16_t real_len;
    uint16_t once_read;
    uint32_t offset = 0;
    uint16_t i;

    if (argc < 2) {
        printf("Usage: hexdump <file>\r\n");
        return;
    }

    CH378_Path_Join(ch378_current_path_sfn, argv[1], full_path, sizeof(full_path));

    if (CLI_IsShortName(CLI_ExtractFilename(argv[1]))) {
        status = CH378FileOpen((uint8_t*)full_path);
    } else {
        status = CLI_LFN_Operation(full_path, 0);
    }
    if (status != ERR_SUCCESS) {
        printf("hexdump: %s: No such file\r\n", argv[1]);
        return;
    }

    while (1) {
        once_read = sizeof(buf);
        status = CH378ByteRead(buf, once_read, &real_len);
        if (status != ERR_SUCCESS || real_len == 0) break;

        for (i = 0; i < real_len; i++) {
            if ((i % 16) == 0) {
                printf("\r\n%08lX  ", offset + i);
            }
            printf("%02X ", buf[i]);
            if ((i % 16) == 7) {
                printf(" ");
            }
        }
        offset += real_len;
        if (real_len < once_read) break;
    }
    printf("\r\n");
    CH378FileClose(0);
}

static void CLI_Cmd_Head(uint8_t argc, char **argv)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint8_t buf[256];
    uint16_t real_len;
    uint16_t once_read;
    uint32_t remain;
    uint16_t i;
    uint32_t n = 256;

    if (argc < 2) {
        printf("Usage: head <file> [n_bytes]\r\n");
        return;
    }
    if (argc >= 3) {
        n = (uint32_t)atoi(argv[2]);
        if (n == 0) n = 256;
    }

    CH378_Path_Join(ch378_current_path_sfn, argv[1], full_path, sizeof(full_path));

    if (CLI_IsShortName(CLI_ExtractFilename(argv[1]))) {
        status = CH378FileOpen((uint8_t*)full_path);
    } else {
        status = CLI_LFN_Operation(full_path, 0);
    }
    if (status != ERR_SUCCESS) {
        printf("head: %s: No such file\r\n", argv[1]);
        return;
    }

    remain = n;
    while (remain > 0) {
        once_read = (remain > sizeof(buf)) ? sizeof(buf) : (uint16_t)remain;
        status = CH378ByteRead(buf, once_read, &real_len);
        if (status != ERR_SUCCESS || real_len == 0) break;

        for (i = 0; i < real_len; i++) {
            printf("%c", buf[i]);
        }
        remain -= real_len;
    }
    printf("\r\n");
    CH378FileClose(0);
}

static void CLI_Cmd_Tail(uint8_t argc, char **argv)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint8_t buf[256];
    uint16_t real_len;
    uint32_t file_size;
    uint32_t offset;
    uint32_t n = 256;
    uint16_t i;

    if (argc < 2) {
        printf("Usage: tail <file> [n_bytes]\r\n");
        return;
    }
    if (argc >= 3) {
        n = (uint32_t)atoi(argv[2]);
        if (n == 0) n = 256;
    }

    CH378_Path_Join(ch378_current_path_sfn, argv[1], full_path, sizeof(full_path));

    if (CLI_IsShortName(CLI_ExtractFilename(argv[1]))) {
        status = CH378FileOpen((uint8_t*)full_path);
    } else {
        status = CLI_LFN_Operation(full_path, 0);
    }
    if (status != ERR_SUCCESS) {
        printf("tail: %s: No such file\r\n", argv[1]);
        return;
    }

    file_size = CH378GetFileSize();
    if (n > file_size) n = file_size;
    offset = file_size - n;

    status = CH378ByteLocate(offset);
    if (status != ERR_SUCCESS) {
        printf("tail: seek failed\r\n");
        CH378FileClose(0);
        return;
    }

    while (n > 0) {
        uint16_t once_read = (n > sizeof(buf)) ? sizeof(buf) : (uint16_t)n;
        status = CH378ByteRead(buf, once_read, &real_len);
        if (status != ERR_SUCCESS || real_len == 0) break;

        for (i = 0; i < real_len; i++) {
            printf("%c", buf[i]);
        }
        n -= real_len;
    }
    printf("\r\n");
    CH378FileClose(0);
}

static void CLI_Cmd_Cp(uint8_t argc, char **argv)
{
    char src_path[CH378_MAX_PATH_LEN];
    char dst_path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint8_t buf[256];
    uint16_t real_len;
    uint32_t offset = 0;
    uint8_t src_lfn, dst_lfn;

    if (argc < 3) {
        printf("Usage: cp <src> <dst>\r\n");
        return;
    }

    CH378_Path_Join(ch378_current_path_sfn, argv[1], src_path, sizeof(src_path));
    CH378_Path_Join(ch378_current_path_sfn, argv[2], dst_path, sizeof(dst_path));

    src_lfn = !CLI_IsShortName(CLI_ExtractFilename(argv[1]));
    dst_lfn = !CLI_IsShortName(CLI_ExtractFilename(argv[2]));

    /* Open source */
    if (src_lfn) {
        status = CLI_LFN_Operation(src_path, 0);
    } else {
        status = CH378FileOpen((uint8_t*)src_path);
    }
    if (status != ERR_SUCCESS) {
        printf("cp: %s: No such file\r\n", argv[1]);
        return;
    }
    CH378FileClose(0);

    /* Remove destination if exists */
    if (dst_lfn) {
        CLI_LFN_Operation(dst_path, 3);
    } else {
        CH378FileErase((uint8_t*)dst_path);
    }

    /* Create destination */
    if (dst_lfn) {
        status = CLI_LFN_Operation(dst_path, 1);
    } else {
        status = CH378FileCreate((uint8_t*)dst_path);
    }
    if (status != ERR_SUCCESS) {
        printf("cp: cannot create '%s'\r\n", argv[2]);
        return;
    }
    CH378FileClose(1);

    /* Copy loop */
    while (1) {
        /* Read from source */
        if (src_lfn) {
            status = CLI_LFN_Operation(src_path, 0);
        } else {
            status = CH378FileOpen((uint8_t*)src_path);
        }
        if (status != ERR_SUCCESS) break;

        if (offset > 0) {
            status = CH378ByteLocate(offset);
            if (status != ERR_SUCCESS) {
                CH378FileClose(0);
                break;
            }
        }

        status = CH378ByteRead(buf, sizeof(buf), &real_len);
        CH378FileClose(0);
        if (status != ERR_SUCCESS || real_len == 0) break;

        /* Write to destination */
        if (dst_lfn) {
            status = CLI_LFN_Operation(dst_path, 0);
        } else {
            status = CH378FileOpen((uint8_t*)dst_path);
        }
        if (status != ERR_SUCCESS) {
            printf("cp: write error on '%s'\r\n", argv[2]);
            break;
        }

        if (offset > 0) {
            status = CH378ByteLocate(offset);
            if (status != ERR_SUCCESS) {
                CH378FileClose(0);
                break;
            }
        }

        status = CH378ByteWrite(buf, real_len, &real_len);
        CH378FileClose(1);
        if (status != ERR_SUCCESS) {
            printf("cp: write error\r\n");
            break;
        }

        offset += real_len;
        if (real_len < sizeof(buf)) break;
    }

    printf("cp: copied '%s' -> '%s'\r\n", argv[1], argv[2]);
}

/* Tree/Du/Find helpers */
static void CLI_TreePrint(const char *name, uint8_t is_dir, uint32_t size)
{
    uint8_t i;
    for (i = 0; i < g_tree_depth; i++) {
        printf("  ");
    }
    if (is_dir) {
        printf("[DIR]  %s\r\n", name);
    } else {
        printf("[FILE] %s  (%lu bytes)\r\n", name, size);
    }
}

static void CLI_TreeRecursive(const char *dir, uint8_t depth)
{
    char saved_path[CH378_MAX_PATH_LEN];
    uint8_t i;
    char subdirs[CLI_MAX_ENTRIES][14];
    uint8_t subdir_cnt = 0;

    strcpy(saved_path, CH378_Dir_Get_Path());

    if (CH378_Dir_Enter(&ch378_g, dir) != ERR_SUCCESS) {
        return;
    }

    g_tree_depth = depth;
    g_cli_entries.count = 0;
    CH378_Dir_List(&ch378_g, CLI_RmCollectCallback);

    for (i = 0; i < g_cli_entries.count; i++) {
        CLI_TreePrint(g_cli_entries.names[i], g_cli_entries.is_dir[i], 0);
        if (g_cli_entries.is_dir[i]) {
            if (subdir_cnt < CLI_MAX_ENTRIES) {
                strncpy(subdirs[subdir_cnt], g_cli_entries.names[i], 13);
                subdirs[subdir_cnt][13] = '\0';
                subdir_cnt++;
            }
        }
    }

    for (i = 0; i < subdir_cnt; i++) {
        CLI_TreeRecursive(subdirs[i], depth + 1);
    }

    CH378_Dir_Go_Parent(&ch378_g);
}

static void CLI_Cmd_Tree(uint8_t argc, char **argv)
{
    const char *dir = ".";
    uint8_t i;
    char subdirs[CLI_MAX_ENTRIES][14];
    uint8_t subdir_cnt = 0;

    if (argc >= 2) dir = argv[1];

    printf("%s\r\n", CH378_Dir_Get_Path());
    if (strcmp(dir, ".") == 0) {
        /* List current directory tree */
        g_tree_depth = 0;
        g_cli_entries.count = 0;
        CH378_Dir_List(&ch378_g, CLI_RmCollectCallback);
        for (i = 0; i < g_cli_entries.count; i++) {
            CLI_TreePrint(g_cli_entries.names[i], g_cli_entries.is_dir[i], 0);
            if (g_cli_entries.is_dir[i]) {
                if (subdir_cnt < CLI_MAX_ENTRIES) {
                    strncpy(subdirs[subdir_cnt], g_cli_entries.names[i], 13);
                    subdirs[subdir_cnt][13] = '\0';
                    subdir_cnt++;
                }
            }
        }
        for (i = 0; i < subdir_cnt; i++) {
            CLI_TreeRecursive(subdirs[i], 1);
        }
    } else {
        CLI_TreeRecursive(dir, 0);
    }
}

static uint32_t CLI_DuRecursive(const char *dir)
{
    char saved_path[CH378_MAX_PATH_LEN];
    uint8_t i;
    char subdirs[CLI_MAX_ENTRIES][14];
    uint8_t subdir_cnt = 0;
    uint32_t total = 0;
    uint8_t entered = 0;

    strcpy(saved_path, CH378_Dir_Get_Path());

    if (strcmp(dir, ".") != 0) {
        if (CH378_Dir_Enter(&ch378_g, dir) != ERR_SUCCESS) {
            return 0;
        }
        entered = 1;
    }

    g_cli_entries.count = 0;
    CH378_Dir_List(&ch378_g, CLI_RmCollectCallback);

    for (i = 0; i < g_cli_entries.count; i++) {
        if (!g_cli_entries.is_dir[i]) {
            total += CH378_File_GetSize(&ch378_g, g_cli_entries.names[i]);
        } else {
            if (subdir_cnt < CLI_MAX_ENTRIES) {
                strncpy(subdirs[subdir_cnt], g_cli_entries.names[i], 13);
                subdirs[subdir_cnt][13] = '\0';
                subdir_cnt++;
            }
        }
    }

    for (i = 0; i < subdir_cnt; i++) {
        total += CLI_DuRecursive(subdirs[i]);
    }

    if (entered) {
        CH378_Dir_Go_Parent(&ch378_g);
    }
    return total;
}

static void CLI_Cmd_Du(uint8_t argc, char **argv)
{
    const char *dir = ".";
    uint32_t total;

    if (argc >= 2) dir = argv[1];

    if (strcmp(dir, ".") == 0) {
        total = CLI_DuRecursive(".");
    } else {
        total = CLI_DuRecursive(dir);
    }

    printf("%lu bytes  %s\r\n", total, CH378_Dir_Get_Path());
}

static void CLI_FindCallback(const char *name, uint8_t is_dir, uint32_t size)
{
    if (strstr(name, g_find_pattern) != NULL) {
        if (is_dir)
            printf("  [DIR]  %s\r\n", name);
        else
            printf("  [FILE] %s  (%lu bytes)\r\n", name, size);
        g_find_found = 1;
    }
}

static void CLI_FindRecursive(const char *dir)
{
    char saved_path[CH378_MAX_PATH_LEN];
    uint8_t i;
    char subdirs[CLI_MAX_ENTRIES][14];
    uint8_t subdir_cnt = 0;
    uint8_t entered = 0;

    strcpy(saved_path, CH378_Dir_Get_Path());

    if (strcmp(dir, ".") != 0) {
        if (CH378_Dir_Enter(&ch378_g, dir) != ERR_SUCCESS) {
            return;
        }
        entered = 1;
    }

    CH378_Dir_List(&ch378_g, CLI_FindCallback);

    g_cli_entries.count = 0;
    CH378_Dir_List(&ch378_g, CLI_RmCollectCallback);
    for (i = 0; i < g_cli_entries.count; i++) {
        if (g_cli_entries.is_dir[i]) {
            if (subdir_cnt < CLI_MAX_ENTRIES) {
                strncpy(subdirs[subdir_cnt], g_cli_entries.names[i], 13);
                subdirs[subdir_cnt][13] = '\0';
                subdir_cnt++;
            }
        }
    }

    for (i = 0; i < subdir_cnt; i++) {
        CLI_FindRecursive(subdirs[i]);
    }

    if (entered) {
        CH378_Dir_Go_Parent(&ch378_g);
    }
}

static void CLI_Cmd_Find(uint8_t argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: find <pattern>\r\n");
        return;
    }

    g_find_pattern = argv[1];
    g_find_found = 0;

    printf("--- Searching for '%s' ---\r\n", argv[1]);
    CLI_FindRecursive(".");
    if (!g_find_found) {
        printf("  (no matches)\r\n");
    }
}

static void CLI_Cmd_Free(void)
{
    uint32_t free_sectors = CH378_Disk_Query_FreeSectors();
    uint16_t sector_size = (uint16_t)CH378ReadVar8(VAR8_DISK_SEC_LEN) << 8;
    uint32_t free_bytes;

    if (sector_size == 0) sector_size = 512;
    free_bytes = free_sectors * sector_size;

    printf("Free space: %lu sectors x %u bytes\r\n", free_sectors, sector_size);
    printf("           = %lu bytes (%lu MB)\r\n", free_bytes, free_bytes / (1024 * 1024));
}

static void CLI_Cmd_Stat(uint8_t argc, char **argv)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint8_t buf[16];
    uint32_t size;
    uint16_t fdate, ftime;
    uint8_t attr;
    uint16_t year, month, day, hour, minute, second;

    if (argc < 2) {
        printf("Usage: stat <file>\r\n");
        return;
    }

    CH378_Path_Join(ch378_current_path_sfn, argv[1], full_path, sizeof(full_path));

    if (CLI_IsShortName(CLI_ExtractFilename(argv[1]))) {
        status = CH378FileOpen((uint8_t*)full_path);
    } else {
        status = CLI_LFN_Operation(full_path, 0);
    }
    if (status != ERR_SUCCESS) {
        printf("stat: %s: No such file\r\n", argv[1]);
        return;
    }

    status = CH378_File_Query(buf);
    CH378FileClose(0);

    if (status != ERR_SUCCESS) {
        printf("stat: query failed (status=%02X)\r\n", status);
        return;
    }

    size = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    fdate = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
    ftime = (uint16_t)buf[6] | ((uint16_t)buf[7] << 8);
    attr = buf[8];

    /* Decode FAT date/time */
    day   = fdate & 0x1F;
    month = (fdate >> 5) & 0x0F;
    year  = 1980 + ((fdate >> 9) & 0x7F);
    second = (ftime & 0x1F) * 2;
    minute = (ftime >> 5) & 0x3F;
    hour   = (ftime >> 11) & 0x1F;

    printf("  File: %s\r\n", argv[1]);
    printf("  Size: %lu bytes\r\n", size);
    printf("  Date: %04u-%02u-%02u\r\n", year, month, day);
    printf("  Time: %02u:%02u:%02u\r\n", hour, minute, second);
    printf("  Attr: 0x%02X", attr);
    if (attr & 0x01) printf(" R");
    if (attr & 0x02) printf(" H");
    if (attr & 0x04) printf(" S");
    if (attr & 0x08) printf(" V");
    if (attr & 0x10) printf(" D");
    if (attr & 0x20) printf(" A");
    printf("\r\n");
}

static void CLI_Cmd_Mv(uint8_t argc, char **argv)
{
    char src_path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint8_t dir_buf[32];
    uint16_t len;
    uint8_t i;

    if (argc < 3) {
        printf("Usage: mv <oldname> <newname>\r\n");
        return;
    }

    /* mv 目前只支持短文件名重命名 */
    if (!CLI_IsShortName(CLI_ExtractFilename(argv[1])) || !CLI_IsShortName(CLI_ExtractFilename(argv[2]))) {
        printf("mv: long filename rename not supported, use cp + rm instead\r\n");
        return;
    }

    /* 检查是否跨目录 */
    if (strchr(argv[2], '\\') != NULL || strchr(argv[2], '/') != NULL) {
        printf("mv: cross-directory move not supported\r\n");
        return;
    }

    CH378_Path_Join(ch378_current_path_sfn, argv[1], src_path, sizeof(src_path));

    /* 打开源文件/目录 */
    status = CH378FileOpen((uint8_t*)src_path);
    if (status != ERR_SUCCESS) {
        printf("mv: %s: No such file or directory\r\n", argv[1]);
        return;
    }

    /* 读取目录项信息 */
    status = CH378_Dir_Info_Read(0xFF);
    if (status != ERR_SUCCESS) {
        CH378FileClose(0);
        printf("mv: read dir info failed (status=%02X)\r\n", status);
        return;
    }

    /* 读取目录项数据 */
    len = CH378ReadReqBlock(dir_buf);
    CH378FileClose(0);

    if (len < 32) {
        printf("mv: read dir info failed (len=%d)\r\n", len);
        return;
    }

    /* 修改文件名 (DIR_Name[0..10]) */
    memset(&dir_buf[0], ' ', 11);
    for (i = 0; i < 8 && argv[2][i] && argv[2][i] != '.'; i++) {
        dir_buf[i] = argv[2][i];
    }
    char *dot = strchr(argv[2], '.');
    if (dot) {
        for (i = 0; i < 3 && dot[i + 1]; i++) {
            dir_buf[8 + i] = dot[i + 1];
        }
    }

    /* 写回目录项 */
    status = CH378FileOpen((uint8_t*)src_path);
    if (status != ERR_SUCCESS) {
        printf("mv: reopen failed\r\n");
        return;
    }

    status = CH378_Dir_Info_Write(0xFF, dir_buf);
    CH378FileClose(0);

    if (status == ERR_SUCCESS) {
        printf("mv: renamed '%s' -> '%s'\r\n", argv[1], argv[2]);
    } else {
        printf("mv: rename failed (status=%02X)\r\n", status);
    }
}

static void CLI_Cmd_Chmod(uint8_t argc, char **argv)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint8_t attr;

    if (argc < 3) {
        printf("Usage: chmod <file> <attr_hex>\r\n");
        printf("  attr: bit0=R bit1=H bit2=S bit3=V bit4=D bit5=A\r\n");
        return;
    }

    attr = (uint8_t)strtol(argv[2], NULL, 16);

    CH378_Path_Join(ch378_current_path_sfn, argv[1], full_path, sizeof(full_path));

    if (CLI_IsShortName(CLI_ExtractFilename(argv[1]))) {
        status = CH378FileOpen((uint8_t*)full_path);
    } else {
        status = CLI_LFN_Operation(full_path, 0);
    }
    if (status != ERR_SUCCESS) {
        printf("chmod: %s: No such file\r\n", argv[1]);
        return;
    }

    status = CH378_File_Modify(0xFFFFFFFF, 0xFFFF, 0xFFFF, attr);
    CH378FileClose(0);

    if (status == ERR_SUCCESS) {
        printf("chmod: '%s' -> 0x%02X\r\n", argv[1], attr);
    } else {
        printf("chmod: failed (status=%02X)\r\n", status);
    }
}

/* ------------------------------------------------------------------------ */
/* config 命令实现 */
/* ------------------------------------------------------------------------ */

/* 判断字符串是否为 4 位十六进制模块键名 */
static uint8_t CLI_IsModuleKey(const char *s)
{
    uint8_t i;
    if (strlen(s) != 4) return 0;
    for (i = 0; i < 4; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return 0;
    }
    return 1;
}

/* 判断字符串是否以 .json 结尾（不区分大小写） */
static uint8_t CLI_IsJsonFile(const char *s)
{
    uint16_t len = strlen(s);
    if (len < 6) return 0;  /* 至少 "a.json" */
    return (s[len-5] == '.' &&
            (s[len-4] == 'j' || s[len-4] == 'J') &&
            (s[len-3] == 's' || s[len-3] == 'S') &&
            (s[len-2] == 'o' || s[len-2] == 'O') &&
            (s[len-1] == 'n' || s[len-1] == 'N'));
}

/* config get 子命令 */
static void CLI_Cmd_Config_Get(uint8_t argc, char **argv)
{
    /* config get — 获取 Core 配置 */
    if (argc < 3) {
        /* 无参数或仅 "config get"：显示 Core 配置 */
        const char *module_key = CONFIG_CORE_KEY;
        {
            /* 遍历默认配置表中属于 Core 的键 */
            extern const config_default_entry_t config_defaults[];
            extern const uint16_t config_default_count;
            uint16_t i;
            int val;
            for (i = 0; i < config_default_count; i++) {
                if (strcmp(config_defaults[i].module_key, module_key) == 0) {
                    if (Config_GetInt(module_key, config_defaults[i].key, &val) == 0) {
                        printf("%s:%d\r\n", config_defaults[i].key, val);
                    }
                }
            }
        }
        return;
    }

    /* config get <module> [key] — 获取指定模块配置 */
    if (CLI_IsModuleKey(argv[2])) {
        const char *module_key = argv[2];

        /* config get <module> <key> — 返回单个值 */
        if (argc >= 4) {
            int val;
            const char *str_val;
            if (Config_GetInt(module_key, argv[3], &val) == 0) {
                printf("%d\r\n", val);
            } else if (Config_GetString(module_key, argv[3], &str_val) == 0) {
                printf("%s\r\n", str_val);
            } else {
                printf("Error: key not found\r\n");
            }
            return;
        }

        /* config get <module> — 列出模块全部键值 */
        {
            extern const config_default_entry_t config_defaults[];
            extern const uint16_t config_default_count;
            uint16_t i;
            int val;
            uint8_t found = 0;

            for (i = 0; i < config_default_count; i++) {
                if (strcmp(config_defaults[i].module_key, module_key) == 0) {
                    found = 1;
                    if (Config_GetInt(module_key, config_defaults[i].key, &val) == 0) {
                        printf("%s:%d\r\n", config_defaults[i].key, val);
                    }
                }
            }
            if (!found) {
                printf("Error: module not found\r\n");
            }
        }
        return;
    }

    /* config get <file.json> — 获取数据文件全部配置 */
    if (CLI_IsJsonFile(argv[2])) {
        cJSON *json = (cJSON*)Config_LoadFile(argv[2]);
        if (!json) {
            printf("Error: file not found\r\n");
            return;
        }

        if (argc >= 4) {
            /* config get <file.json> <key> — 获取指定键 */
            cJSON *item = cJSON_GetObjectItem(json, argv[3]);
            if (!item) {
                printf("Error: key not found\r\n");
                cJSON_Delete(json);
                return;
            }
            if (cJSON_IsNumber(item)) {
                printf("%d\r\n", item->valueint);
            } else if (cJSON_IsString(item)) {
                printf("%s\r\n", item->valuestring);
            } else {
                char *str = cJSON_PrintUnformatted(item);
                if (str) { printf("%s\r\n", str); cJSON_free(str); }
            }
        } else {
            /* 显示全部键值 */
            cJSON *item;
            cJSON_ArrayForEach(item, json) {
                if (cJSON_IsNumber(item)) {
                    printf("%s:%d\r\n", item->string, item->valueint);
                } else if (cJSON_IsString(item)) {
                    printf("%s:%s\r\n", item->string, item->valuestring);
                } else {
                    char *str = cJSON_PrintUnformatted(item);
                    if (str) { printf("%s:%s\r\n", item->string, str); cJSON_free(str); }
                }
            }
        }
        cJSON_Delete(json);
        return;
    }

    /* config get <key> — 获取 Core 单个配置值 */
    {
        int val;
        const char *str_val;
        if (Config_GetInt(CONFIG_CORE_KEY, argv[2], &val) == 0) {
            printf("%d\r\n", val);
        } else if (Config_GetString(CONFIG_CORE_KEY, argv[2], &str_val) == 0) {
            printf("%s\r\n", str_val);
        } else {
            printf("Error: key not found\r\n");
        }
    }
}

/* config set 子命令 */
static void CLI_Cmd_Config_Set(uint8_t argc, char **argv)
{
    /* config set <key> <value> — 设置 Core 配置 */
    if (argc >= 4 && !CLI_IsModuleKey(argv[2]) && !CLI_IsJsonFile(argv[2])) {
        int val = atoi(argv[3]);
        int ret = Config_SetInt(CONFIG_CORE_KEY, argv[2], val);
        if (ret == 0) {
            /* 音量需立即同步硬件，否则 Config_SyncFromHardware 在 save 时会用旧硬件值覆盖 cJSON */
            if (strcmp(argv[2], "volume") == 0)
                Audio_SetVolume((uint8_t)val);
            printf("OK\r\n");
        }
        else if (ret == -1) printf("Error: module not found\r\n");
        else printf("Error: key not found\r\n");
        return;
    }

    /* config set <module> <key> <value> — 设置模块配置 */
    if (argc >= 5 && CLI_IsModuleKey(argv[2])) {
        int val = atoi(argv[4]);
        int ret = Config_SetInt(argv[2], argv[3], val);
        if (ret == 0) printf("OK\r\n");
        else if (ret == -1) printf("Error: module not found\r\n");
        else printf("Error: key not found\r\n");
        return;
    }

    /* config set <file.json> <key> <value> — 设置数据文件配置 */
    if (argc >= 5 && CLI_IsJsonFile(argv[2])) {
        int val = atoi(argv[4]);
        int ret = Config_SetFileInt(argv[2], argv[3], val);
        if (ret == 0) printf("OK\r\n");
        else if (ret == -1) printf("Error: file not found\r\n");
        else printf("Error: key not found\r\n");
        return;
    }

    printf("Usage: config set <key> <value>\r\n");
    printf("       config set <module> <key> <value>\r\n");
    printf("       config set <file.json> <key> <value>\r\n");
}

/* config addkey 子命令 */
static void CLI_Cmd_Config_AddKey(uint8_t argc, char **argv)
{
    /* config addkey <module> <key> <value> */
    if (argc >= 5 && CLI_IsModuleKey(argv[2])) {
        int val = atoi(argv[4]);
        int ret = Config_AddKeyInt(argv[2], argv[3], val);
        if (ret == 0) printf("OK\r\n");
        else if (ret == -2) printf("Error: key already exists\r\n");
        else printf("Error: failed\r\n");
        return;
    }

    /* config addkey <file.json> <key> <value> */
    if (argc >= 5 && CLI_IsJsonFile(argv[2])) {
        int val = atoi(argv[4]);
        int ret = Config_AddFileKeyInt(argv[2], argv[3], val);
        if (ret == 0) printf("OK\r\n");
        else if (ret == -1) printf("Error: file not found\r\n");
        else if (ret == -2) printf("Error: key already exists\r\n");
        else printf("Error: failed\r\n");
        return;
    }

    printf("Usage: config addkey <module> <key> <value>\r\n");
    printf("       config addkey <file.json> <key> <value>\r\n");
}

/* config 主命令 */
static void CLI_Cmd_Config(uint8_t argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: config <get|getkey|set|addkey|newfile|save|backup|rollback|reset|ls|rm> ...\r\n");
        return;
    }

    /* 写操作需要设备匹配检查 */
    if (strcmp(argv[1], "set") == 0 ||
        strcmp(argv[1], "addkey") == 0 ||
        strcmp(argv[1], "newfile") == 0 ||
        strcmp(argv[1], "save") == 0 ||
        strcmp(argv[1], "backup") == 0 ||
        strcmp(argv[1], "rollback") == 0 ||
        strcmp(argv[1], "reset") == 0 ||
        strcmp(argv[1], "rm") == 0) {
        if (!Config_IsDeviceMatch()) {
            printf("Error: current device != config target (%s), switch device first\r\n",
                   Config_GetTargetDeviceName());
            return;
        }
    }

    if (strcmp(argv[1], "get") == 0) {
        CLI_Cmd_Config_Get(argc, argv);
    } else if (strcmp(argv[1], "getkey") == 0) {
        if (argc < 3) {
            /* config getkey — 列出 Core 所有键名 */
            if (Config_ListKeys(CONFIG_CORE_KEY, 0) != 0)
                printf("Error: core module not found\r\n");
        } else if (CLI_IsModuleKey(argv[2])) {
            /* config getkey <module> — 列出模块所有键名 */
            if (Config_ListKeys(argv[2], 0) != 0)
                printf("Error: module not found\r\n");
        } else if (CLI_IsJsonFile(argv[2])) {
            /* config getkey <file.json> — 列出数据文件所有键名 */
            if (Config_ListKeys(argv[2], 1) != 0)
                printf("Error: file not found\r\n");
        } else {
            /* config getkey <key> — 当作 Core 键名尝试 */
            if (Config_ListKeys(argv[2], 0) != 0)
                printf("Error: module not found\r\n");
        }
    } else if (strcmp(argv[1], "set") == 0) {
        CLI_Cmd_Config_Set(argc, argv);
    } else if (strcmp(argv[1], "addkey") == 0) {
        CLI_Cmd_Config_AddKey(argc, argv);
    } else if (strcmp(argv[1], "newfile") == 0) {
        if (argc < 3) {
            printf("Usage: config newfile <file.json>\r\n");
            return;
        }
        int ret = Config_NewFile(argv[2]);
        if (ret == 0) printf("Created\r\n");
        else if (ret == -1) printf("Error: file already exists\r\n");
        else if (ret == -3) printf("Error: cannot create config.json\r\n");
        else printf("Error: failed\r\n");
    } else if (strcmp(argv[1], "save") == 0) {
        uint8_t ret = Config_Save();
        if (ret == ERR_SUCCESS && !Config_IsDirty()) {
            /* 检查是否真的有变更被保存 */
            printf("No changes\r\n");
        } else if (ret == ERR_SUCCESS) {
            printf("Saved\r\n");
        } else if (ret == 0x04) {
            printf("Busy (audio streaming), will save later\r\n");
        } else {
            printf("Error: save failed (status=%02X)\r\n", ret);
        }
    } else if (strcmp(argv[1], "backup") == 0) {
        uint8_t ret = Config_Backup();
        if (ret == ERR_SUCCESS) printf("Backed up\r\n");
        else printf("Error: backup failed (status=%02X)\r\n", ret);
    } else if (strcmp(argv[1], "rollback") == 0) {
        uint8_t ret = Config_Rollback();
        if (ret == ERR_SUCCESS) printf("Rolled back\r\n");
        else if (ret == 0xFE) printf("Error: device mismatch\r\n");
        else printf("Error: rollback failed (status=%02X)\r\n", ret);
    } else if (strcmp(argv[1], "reset") == 0) {
        Config_ResetDefaults();
        printf("Reset to defaults and saved\r\n");
    } else if (strcmp(argv[1], "ls") == 0) {
        /* 列出 CONFIG 目录下所有文件 */
        char saved_path[CH378_MAX_PATH_LEN];
        uint8_t status;
        uint8_t i;

        /* 保存当前路径 */
        strncpy(saved_path, ch378_current_path_sfn, sizeof(saved_path) - 1);
        saved_path[sizeof(saved_path) - 1] = '\0';

        /* 从根目录进入 CONFIG 目录（确保从根目录查找） */
        CH378_Dir_Go_Root(&ch378_g);
        status = CH378_Dir_Enter(&ch378_g, "CONFIG");
        if (status != ERR_SUCCESS) {
            printf("Error: cannot enter CONFIG dir\r\n");
            return;
        }

        g_cli_entries.count = 0;
        CH378_Dir_List(&ch378_g, CLI_RmCollectCallback);

        for (i = 0; i < g_cli_entries.count; i++) {
            printf("%-16s %lu\r\n", g_cli_entries.names[i], g_cli_entries.size[i]);
        }

        /* 恢复原路径 */
        CH378_Dir_Go_Root(&ch378_g);
        if (saved_path[0] != '\0' && strcmp(saved_path, "\\") != 0) {
            /* 简单恢复：重新进入原路径 */
            /* 注意：完整路径恢复需要逐级进入，这里简化处理 */
        }
    } else if (strcmp(argv[1], "rm") == 0) {
        if (argc < 3) {
            printf("Usage: config rm <file.json>\r\n");
            return;
        }
        if (strcmp(argv[2], CONFIG_MAIN_FILE) == 0) {
            printf("Cannot delete config.json\r\n");
            return;
        }
        {
            uint8_t status = Config_DeleteFile(argv[2]);
            if (status == ERR_SUCCESS) printf("Deleted\r\n");
            else printf("Error: delete failed (status=%02X)\r\n", status);
        }
    } else {
        printf("Unknown config subcommand: %s\r\n", argv[1]);
    }
}

/* ---- IR 激光测距命令 ----
 * ir start   开始测距（发送 CMD_SUB_SET_MODE SUB=0x01 到 submodel-6）
 * ir stop    停止测距（发送 CMD_SUB_SET_MODE SUB=0x02 到 submodel-6）
 */
static void CLI_Cmd_Ir(uint8_t argc, char **argv)
{
    submodels_t *ir;

    if (argc < 2) {
        printf("Usage: ir <start|stop>\r\n");
        return;
    }

    ir = Submodels_FindIRSlot();
    if (ir == NULL) {
        printf("ir: no IR submodel online\r\n");
        return;
    }

    if (strcmp(argv[1], "start") == 0) {
        if (Submodels_IR_StartRanging(ir))
            printf("ir: start ranging sent\r\n");
        else
            printf("ir: failed to send start command\r\n");
    } else if (strcmp(argv[1], "stop") == 0) {
        if (Submodels_IR_StopRanging(ir))
            printf("ir: stop ranging sent\r\n");
        else
            printf("ir: failed to send stop command\r\n");
    } else {
        printf("ir: unknown subcommand '%s' (start|stop)\r\n", argv[1]);
    }
}

/* ---- NFC 名称管理命令 ----
 * nfc set <hex_id> <name>   设置 NFC 卡名称 (ID 为 10 位十六进制，写入 nfc.json)
 * nfc get <hex_id>          获取 NFC 卡名称
 * nfc ls                    列出所有已存储的 NFC 卡
 */
static void CLI_Cmd_Nfc(uint8_t argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: nfc set <hex_id> <name>\r\n");
        printf("       nfc get <hex_id>\r\n");
        printf("       nfc ls\r\n");
        return;
    }

    if (!Config_IsDeviceMatch()) {
        printf("Error: current device != config target (%s)\r\n", Config_GetTargetDeviceName());
        return;
    }

    if (strcmp(argv[1], "set") == 0) {
        cJSON *json;
        cJSON *item;
        uint8_t status;

        if (argc < 4) {
            printf("Usage: nfc set <hex_id> <name>\r\n");
            printf("  hex_id: 10 hex digits (e.g. 0A1A3BAC20)\r\n");
            return;
        }

        /* 校验 hex_id 长度 (5 字节 = 10 个十六进制字符) */
        if (strlen(argv[2]) != 10) {
            printf("Error: ID must be 10 hex digits\r\n");
            return;
        }

        json = (cJSON*)Config_LoadFile("nfc.json");
        if (!json) {
            /* 文件不存在，创建新的 */
            json = cJSON_CreateObject();
            if (!json) { printf("Error: malloc failed\r\n"); return; }
        }

        item = cJSON_GetObjectItem(json, argv[2]);
        if (item) {
            /* 已存在：更新 */
            cJSON_SetValuestring(item, argv[3]);
        } else {
            /* 不存在：新增 */
            item = cJSON_CreateString(argv[3]);
            if (!item) { cJSON_Delete(json); printf("Error: malloc failed\r\n"); return; }
            cJSON_AddItemToObject(json, argv[2], item);
        }

        status = Config_SaveFile("nfc.json", json);
        cJSON_Delete(json);

        if (status == ERR_SUCCESS) printf("OK\r\n");
        else printf("Error: save failed (status=%02X)\r\n", status);
        return;
    }

    if (strcmp(argv[1], "get") == 0) {
        cJSON *json;
        cJSON *item;

        if (argc < 3) {
            printf("Usage: nfc get <hex_id>\r\n");
            return;
        }

        json = (cJSON*)Config_LoadFile("nfc.json");
        if (!json) { printf("Error: nfc.json not found\r\n"); return; }

        item = cJSON_GetObjectItem(json, argv[2]);
        if (item && cJSON_IsString(item)) {
            printf("%s\r\n", item->valuestring);
        } else {
            printf("Not found\r\n");
        }
        cJSON_Delete(json);
        return;
    }

    if (strcmp(argv[1], "ls") == 0) {
        cJSON *json;
        cJSON *item;

        json = (cJSON*)Config_LoadFile("nfc.json");
        if (!json) { printf("nfc.json not found\r\n"); return; }

        cJSON_ArrayForEach(item, json) {
            if (cJSON_IsString(item))
                printf("%s: %s\r\n", item->string, item->valuestring);
        }
        cJSON_Delete(json);
        return;
    }

    printf("Unknown nfc subcommand: %s\r\n", argv[1]);
}


static void CLI_Cmd_Play(uint8_t argc, char **argv)
{
    char full_path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint8_t header[512];
    uint16_t real_len;
    wav_info_t info;
    uint8_t ch = 0;

    if (argc < 2) {
        printf("Usage: play <wav_file> [channel]\r\n");
        printf("  No channel arg: play on ch0 (stop all first)\r\n");
        printf("  channel 0 or 1: play on specified channel\r\n");
        printf("  e.g. play song.wav        (ch0, stops ch1 too)\r\n");
        printf("       play bgm.wav 1       (ch1, does not stop ch0)\r\n");
        return;
    }

    /* Parse optional channel argument */
    if (argc >= 3) {
        int parsed = atoi(argv[2]);
        if (parsed < 0 || parsed >= AUDIO_MAX_CHANNELS) {
            printf("play: invalid channel %d (0~%d)\r\n", parsed, AUDIO_MAX_CHANNELS - 1);
            return;
        }
        ch = (uint8_t)parsed;
    }

    /* Default (no channel arg): stop all channels first */
    if (argc < 3) {
        Audio_PlayStop();
    } else {
        /* Explicit channel: only stop that channel */
        Audio_ChannelStop(ch);
    }

    /* CH378 单句柄：打开新文件会隐式关闭其他通道的文件，
     * 必须显式清除其 file_open 标记 */
    {
        uint8_t j;
        for (j = 0; j < AUDIO_MAX_CHANNELS; j++) {
            if (j != ch && CS43131_g.channels[j].file_open) {
                CS43131_g.channels[j].file_open = 0;
            }
        }
    }

    /* 支持绝对路径（以 \ 或 / 开头）和相对路径 */
    CH378_Path_Join(ch378_current_path_sfn, argv[1], full_path, sizeof(full_path));

    /* 打开文件（支持长文件名） */
    if (CLI_IsShortName(CLI_ExtractFilename(argv[1]))) {
        status = CH378FileOpen((uint8_t*)full_path);
    } else {
        status = CLI_LFN_Operation(full_path, 0); /* 0 = open */
    }
    if (status != ERR_SUCCESS) {
        printf("play: cannot open '%s' (status=%02X)\r\n", argv[1], status);
        return;
    }

    /* 读取 WAV 头 */
    status = CH378ByteRead(header, 512, &real_len);
    if (status != ERR_SUCCESS || real_len < 44) {
        printf("play: failed to read header\r\n");
        CH378FileClose(0);
        return;
    }

    /* 解析头 */
    if (Audio_ParseWAVHeader(header, &info) != 0) {
        CH378FileClose(0);
        return;
    }

    /* 定位到 data chunk 起始位置（读完 header 后 CH378 指针在 512，需要回退到 data_offset） */
    status = CH378ByteLocate(info.data_offset);
    if (status != ERR_SUCCESS) {
        printf("play: seek failed (status=%02X)\r\n", status);
        CH378FileClose(0);
        return;
    }

    /* 不关闭文件：直接交给音频引擎继续顺序读取。
     * CH378 保持文件打开状态，避免二次 open 时路径解析失败 (0x42)。 */

    /* 启动流式播放 */
    if (Audio_ChannelStart(ch, full_path, &info) != 0) {
        printf("play: failed to start channel %d\r\n", ch);
        CH378FileClose(0);
        return;
    }
    /* 标记文件已由 play 命令打开，Audio_Process 直接顺序读取 */
    CS43131_g.channels[ch].file_open = 1;
    if (ch == 0) {
        Audio_SetCurrentTrack(argv[1]);
    } else {
        strncpy(CS43131_g.channels[ch].track_name, argv[1],
                sizeof(CS43131_g.channels[ch].track_name) - 1);
        CS43131_g.channels[ch].track_name[sizeof(CS43131_g.channels[ch].track_name) - 1] = '\0';
    }
    {
        uint32_t byte_rate = info.sample_rate * info.num_channels * (info.bits_per_sample / 8);
        uint32_t duration_sec = byte_rate > 0 ? info.data_size / byte_rate : 0;
        uint32_t dur_min = duration_sec / 60;
        uint32_t dur_s = duration_sec % 60;
        printf("Playing ch%d: %s\r\n", ch, argv[1]);
        printf("Duration: %02lu:%02lu\r\n", dur_min, dur_s);
        printf("Volume: %d\r\n", Audio_GetVolume());
    }
}

static void CLI_Cmd_Speaker(uint8_t argc, char **argv)
{
    speaker_channel_t ch;
    speaker_channel_t state;

    if (argc < 2) {
        state = Speaker_GetState();
        printf("Speaker: L=%s R=%s\r\n",
               (state & SPEAKER_CHANNEL_LEFT) ? "ON" : "OFF",
               (state & SPEAKER_CHANNEL_RIGHT) ? "ON" : "OFF");
        printf("Usage: speaker <on|off> [left|right]\r\n");
        return;
    }

    if (argc >= 3) {
        if (strcmp(argv[2], "left") == 0)
            ch = SPEAKER_CHANNEL_LEFT;
        else if (strcmp(argv[2], "right") == 0)
            ch = SPEAKER_CHANNEL_RIGHT;
        else {
            printf("speaker: unknown channel '%s' (use left/right)\r\n", argv[2]);
            return;
        }
    } else {
        ch = SPEAKER_CHANNEL_BOTH;
    }

    if (strcmp(argv[1], "on") == 0) {
        Speaker_On(ch);
        printf("Speaker %s ON\r\n",
               ch == SPEAKER_CHANNEL_LEFT  ? "left" :
               ch == SPEAKER_CHANNEL_RIGHT ? "right" : "both");
    } else if (strcmp(argv[1], "off") == 0) {
        Speaker_Off(ch);
        printf("Speaker %s OFF\r\n",
               ch == SPEAKER_CHANNEL_LEFT  ? "left" :
               ch == SPEAKER_CHANNEL_RIGHT ? "right" : "both");
    } else {
        printf("speaker: unknown action '%s' (use on/off)\r\n", argv[1]);
    }
}

/* ------------------------------------------------------------------------ */
/*  RGB Submodel Commands                                                   */
/*  Note: CLI runs on V5F, but submodel UART is on V3F.                    */
/*  Commands write to shared memory (hardware_g) and V3F picks them up.    */
/* ------------------------------------------------------------------------ */

static void CLI_Cmd_Fp(uint8_t argc, char **argv)
{
    submodels_t *fp;

    if (argc < 2) {
        printf("Usage: fp <register|del|ls|count|config|set|get|names> [args]\r\n");
        return;
    }

    /* ---- fp.json 名称管理（不依赖硬件模块在线） ---- */

    /* fp set <id> <name> — 设置指纹名称 (ID 为十进制序号) */
    if (strcmp(argv[1], "set") == 0) {
        cJSON *json;
        cJSON *item;
        uint8_t status;

        if (!Config_IsDeviceMatch()) {
            printf("Error: current device != config target (%s)\r\n", Config_GetTargetDeviceName());
            return;
        }
        if (argc < 4) {
            printf("Usage: fp set <id> <name>\r\n");
            printf("  id: decimal index (e.g. 0, 1, 5)\r\n");
            return;
        }

        json = (cJSON*)Config_LoadFile("fp.json");
        if (!json) {
            json = cJSON_CreateObject();
            if (!json) { printf("Error: malloc failed\r\n"); return; }
        }

        item = cJSON_GetObjectItem(json, argv[2]);
        if (item) {
            cJSON_SetValuestring(item, argv[3]);
        } else {
            item = cJSON_CreateString(argv[3]);
            if (!item) { cJSON_Delete(json); printf("Error: malloc failed\r\n"); return; }
            cJSON_AddItemToObject(json, argv[2], item);
        }

        status = Config_SaveFile("fp.json", json);
        cJSON_Delete(json);

        if (status == ERR_SUCCESS) printf("OK\r\n");
        else printf("Error: save failed (status=%02X)\r\n", status);
        return;
    }

    /* fp get <id> — 获取指纹名称 */
    if (strcmp(argv[1], "get") == 0) {
        cJSON *json;
        cJSON *item;

        if (argc < 3) {
            printf("Usage: fp get <id>\r\n");
            return;
        }

        json = (cJSON*)Config_LoadFile("fp.json");
        if (!json) { printf("Error: fp.json not found\r\n"); return; }

        item = cJSON_GetObjectItem(json, argv[2]);
        if (item && cJSON_IsString(item)) {
            printf("%s\r\n", item->valuestring);
        } else {
            printf("Not found\r\n");
        }
        cJSON_Delete(json);
        return;
    }

    /* fp names — 列出所有指纹名称 */
    if (strcmp(argv[1], "names") == 0) {
        cJSON *json;
        cJSON *item;

        json = (cJSON*)Config_LoadFile("fp.json");
        if (!json) { printf("fp.json not found\r\n"); return; }

        cJSON_ArrayForEach(item, json) {
            if (cJSON_IsString(item))
                printf("%s: %s\r\n", item->string, item->valuestring);
        }
        cJSON_Delete(json);
        return;
    }

    /* ---- 以下命令需要指纹硬件模块在线 ---- */
    fp = Submodels_FindFpSlot();
    if (fp == NULL) {
        printf("fp: fingerprint module not found\r\n");
        return;
    }

    if (strcmp(argv[1], "register") == 0) {
        if (Submodels_FP_EnrollStart(fp))
            printf("fp: enrollment started, place finger on sensor\r\n");
        else
            printf("fp: failed to start enrollment\r\n");
    }
    else if (strcmp(argv[1], "del") == 0) {
        if (argc < 3) {
            printf("Usage: fp del <ID>\r\n");
            return;
        }
        uint8_t id = (uint8_t)atoi(argv[2]);
        if (Submodels_FP_Delete(fp, id))
            printf("fp: delete request sent (ID=%d)\r\n", id);
        else
            printf("fp: failed to send delete request\r\n");
    }
    else if (strcmp(argv[1], "ls") == 0) {
        if (Submodels_FP_QueryList(fp))
            printf("fp: query list sent\r\n");
        else
            printf("fp: failed to query\r\n");
    }
    else if (strcmp(argv[1], "count") == 0) {
        if (Submodels_FP_QueryCount(fp))
            printf("fp: query count sent\r\n");
        else
            printf("fp: failed to query count\r\n");
    }
    else if (strcmp(argv[1], "config") == 0) {
        if (argc < 3) {
            printf("Usage: fp config led <func> <color> [speed]\r\n");
            printf("       fp config sec <level:1-3>\r\n");
            printf("LED func: 1=breath 2=flicker 3=on 4=off 5=gradual_on 6=gradual_off 7=horse\r\n");
            printf("LED color: 0=off 1=blue 2=green 3=cyan 4=red 5=magenta 6=yellow 7=white\r\n");
            return;
        }

        if (strcmp(argv[2], "led") == 0) {
            if (argc < 5) {
                printf("Usage: fp config led <func> <color> [speed]\r\n");
                return;
            }
            uint8_t func  = (uint8_t)atoi(argv[3]);
            uint8_t color = (uint8_t)atoi(argv[4]);
            uint8_t speed = (argc >= 6) ? (uint8_t)atoi(argv[5]) : 0;
            if (Submodels_FP_SetLED(fp, func, color, speed))
                printf("fp: LED config sent (func=%d color=%d speed=%d)\r\n", func, color, speed);
            else
                printf("fp: failed to set LED\r\n");
        }
        else if (strcmp(argv[2], "sec") == 0) {
            if (argc < 4) {
                printf("Usage: fp config sec <level:1-3>\r\n");
                return;
            }
            uint8_t level = (uint8_t)atoi(argv[3]);
            if (level < 1 || level > 3) {
                printf("fp: invalid security level %d (1-3)\r\n", level);
                return;
            }
            if (Submodels_FP_SetSecurity(fp, level))
                printf("fp: security level set to %d\r\n", level);
            else
                printf("fp: failed to set security level\r\n");
        }
        else {
            printf("fp: unknown config option '%s' (led/sec)\r\n", argv[2]);
        }
    }
    else {
        printf("fp: unknown subcommand '%s'\r\n", argv[1]);
    }
}

static void CLI_Cmd_Rgb(uint8_t argc, char **argv)
{
    if (argc < 2) {
        printf("Usage:\r\n");
        printf("  rgb mode <mode> <r> <g> <b> <brightness> <speed>\r\n");
        printf("  rgb refresh json\r\n");
        printf("  rgb status\r\n");
        printf("Modes: 0=custom, 1=solid, 2=breathing, 3=marquee\r\n");
        return;
    }

    if (strcmp(argv[1], "mode") == 0) {
        /* rgb mode <mode> <r> <g> <b> <brightness> <speed> */
        uint8_t mode, r, g, b, brightness, speed;

        if (argc < 8) {
            printf("Usage: rgb mode <mode> <r> <g> <b> <brightness> <speed>\r\n");
            return;
        }

        mode       = (uint8_t)atoi(argv[2]);
        r          = (uint8_t)atoi(argv[3]);
        g          = (uint8_t)atoi(argv[4]);
        b          = (uint8_t)atoi(argv[5]);
        brightness = (uint8_t)atoi(argv[6]);
        speed      = (uint8_t)atoi(argv[7]);

        if (mode > 3) {
            printf("rgb: invalid mode %d (0-3)\r\n", mode);
            return;
        }

        /* Write to shared memory; V3F heartbeat will send to submodel */
        hardware_g.rgb_config.mode = mode;
        hardware_g.rgb_config.r = r;
        hardware_g.rgb_config.g = g;
        hardware_g.rgb_config.b = b;
        hardware_g.rgb_config.brightness = brightness;
        hardware_g.rgb_config.speed = speed;
        hardware_g.rgb_config.pending = 1;

        printf("rgb: mode=%d R=%d G=%d B=%d bright=%d speed=%d (pending)\r\n",
               mode, r, g, b, brightness, speed);
    }
    else if (strcmp(argv[1], "refresh") == 0) {
        /* rgb refresh json - load rgb.json and queue custom frames for V3F */
        cJSON *rgb_json;
        cJSON *frames_arr, *fc_item, *fi_item;
        int frame_count, frame_interval;
        uint8_t frame_idx;

        if (argc < 3 || strcmp(argv[2], "json") != 0) {
            printf("Usage: rgb refresh json\r\n");
            return;
        }

        /* Load rgb.json from CONFIG directory */
        rgb_json = (cJSON *)Config_LoadFile("rgb.json");
        if (rgb_json == NULL) {
            printf("rgb: rgb.json not found or parse failed\r\n");
            return;
        }

        /* Parse frame_count and frame_interval */
        fc_item = cJSON_GetObjectItem(rgb_json, "frame_count");
        fi_item = cJSON_GetObjectItem(rgb_json, "frame_interval");
        if (!fc_item || !cJSON_IsNumber(fc_item)) {
            printf("rgb: invalid frame_count in rgb.json\r\n");
            cJSON_Delete(rgb_json);
            return;
        }
        frame_count = fc_item->valueint;
        frame_interval = fi_item ? fi_item->valueint : 100;

        if (frame_count <= 0 || frame_count > RGB_MAX_CUSTOM_FRAMES) {
            printf("rgb: frame_count %d out of range (1-%d)\r\n",
                   frame_count, RGB_MAX_CUSTOM_FRAMES);
            cJSON_Delete(rgb_json);
            return;
        }

        /* Get frames array */
        frames_arr = cJSON_GetObjectItem(rgb_json, "frames");
        if (!frames_arr || !cJSON_IsArray(frames_arr)) {
            printf("rgb: missing or invalid 'frames' array\r\n");
            cJSON_Delete(rgb_json);
            return;
        }

        /* Write frame data to shared memory for V3F to send */
        for (frame_idx = 0; frame_idx < frame_count; frame_idx++) {
            cJSON *frame = cJSON_GetArrayItem(frames_arr, frame_idx);
            int led_idx;

            if (!frame || !cJSON_IsArray(frame) ||
                cJSON_GetArraySize(frame) < RGB_LED_COUNT) {
                printf("rgb: frame %d invalid or incomplete\r\n", frame_idx);
                continue;
            }

            /* Convert JSON frame to flat RGB888 byte array in shared memory */
            for (led_idx = 0; led_idx < RGB_LED_COUNT; led_idx++) {
                cJSON *led = cJSON_GetArrayItem(frame, led_idx);
                if (led && cJSON_IsArray(led) && cJSON_GetArraySize(led) >= 3) {
                    hardware_g.rgb_frame.frame_data[frame_idx][led_idx * 3 + 0] =
                        (uint8_t)cJSON_GetArrayItem(led, 0)->valueint;
                    hardware_g.rgb_frame.frame_data[frame_idx][led_idx * 3 + 1] =
                        (uint8_t)cJSON_GetArrayItem(led, 1)->valueint;
                    hardware_g.rgb_frame.frame_data[frame_idx][led_idx * 3 + 2] =
                        (uint8_t)cJSON_GetArrayItem(led, 2)->valueint;
                } else {
                    hardware_g.rgb_frame.frame_data[frame_idx][led_idx * 3 + 0] = 0;
                    hardware_g.rgb_frame.frame_data[frame_idx][led_idx * 3 + 1] = 0;
                    hardware_g.rgb_frame.frame_data[frame_idx][led_idx * 3 + 2] = 0;
                }
            }
            printf("rgb: queued frame %d/%d\r\n", frame_idx + 1, frame_count);
        }

        /* Set transfer parameters in shared memory */
        hardware_g.rgb_frame.frame_count = (uint8_t)frame_count;
        hardware_g.rgb_frame.frame_interval = (uint16_t)frame_interval;
        hardware_g.rgb_frame.next_frame_idx = 0;
        hardware_g.rgb_frame.pending = 1;

        /* Also set mode to custom (0), preserving current brightness/speed/color */
        {
            uint8_t prev_r = hardware_g.rgb_config.r;
            uint8_t prev_g = hardware_g.rgb_config.g;
            uint8_t prev_b = hardware_g.rgb_config.b;
            uint8_t prev_bright = hardware_g.rgb_config.brightness;
            uint8_t prev_speed = hardware_g.rgb_config.speed;
            hardware_g.rgb_config.mode = 0;
            hardware_g.rgb_config.r = prev_r;
            hardware_g.rgb_config.g = prev_g;
            hardware_g.rgb_config.b = prev_b;
            hardware_g.rgb_config.brightness = prev_bright;
            hardware_g.rgb_config.speed = prev_speed;
            hardware_g.rgb_config.pending = 1;
        }

        printf("rgb: %d frames queued, interval=%dms\r\n",
               frame_count, frame_interval);

        cJSON_Delete(rgb_json);
    }
    else if (strcmp(argv[1], "status") == 0) {
        /* rgb status - print current shared memory config */
        printf("rgb: mode=%d R=%d G=%d B=%d bright=%d speed=%d pending=%d\r\n",
               hardware_g.rgb_config.mode,
               hardware_g.rgb_config.r,
               hardware_g.rgb_config.g,
               hardware_g.rgb_config.b,
               hardware_g.rgb_config.brightness,
               hardware_g.rgb_config.speed,
               hardware_g.rgb_config.pending);
        printf("rgb: frame_pending=%d frame_count=%d interval=%dms\r\n",
               hardware_g.rgb_frame.pending,
               hardware_g.rgb_frame.frame_count,
               hardware_g.rgb_frame.frame_interval);
    }
    else {
        printf("rgb: unknown subcommand '%s'\r\n", argv[1]);
    }
}

/* CLI 主入口 */
/* ------------------------------------------------------------------------ */

void CLI_Init(void)
{
    /* CLI 无需特殊初始化，USART2 RX 中断在 Debug_EnableRxIRQ 中启用 */
}

/* NOTE: 音频引擎使用顺序读取（保持文件打开），CLI 命令执行前需要
 * 通过 Audio_CH378_Lock 关闭音频文件句柄，执行后音频引擎会自动
 * reopen + seek 恢复播放。 */
void CLI_Process(uint8_t *cmd, uint8_t len)
{
    char buf[CLI_BUF_SIZE];
    char raw_buf[CLI_BUF_SIZE];  /* pristine copy for commands that need unmodified data */
    char *argv[CLI_MAX_ARGC];
    uint8_t argc;

    if (len == 0 || cmd == NULL) return;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;

    memcpy(buf, cmd, len);
    buf[len] = '\0';
    memcpy(raw_buf, cmd, len);
    raw_buf[len] = '\0';

    argc = CLI_ParseArgs(buf, argv, CLI_MAX_ARGC);
    if (argc == 0) return;

    /* Lock CH378: close audio streaming files before CLI uses CH378 */
    Audio_CH378_Lock();

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
    } else if (strcmp(argv[0], "write") == 0) {
        CLI_Cmd_Write(argc, argv, raw_buf, len);
    } else if (strcmp(argv[0], "rm") == 0) {
        CLI_Cmd_Rm(argc, argv);
    } else if (strcmp(argv[0], "help") == 0) {
        CLI_Cmd_Help(argc, argv);
    } else if (strcmp(argv[0], "clear") == 0) {
        CLI_Cmd_Clear();
    } else if (strcmp(argv[0], "ver") == 0) {
        CLI_Cmd_Ver();
    } else if (strcmp(argv[0], "df") == 0) {
        CLI_Cmd_Df();
    } else if (strcmp(argv[0], "device") == 0) {
        CLI_Cmd_Device(argc, argv);
    } else if (strcmp(argv[0], "hexdump") == 0) {
        CLI_Cmd_Hexdump(argc, argv);
    } else if (strcmp(argv[0], "head") == 0) {
        CLI_Cmd_Head(argc, argv);
    } else if (strcmp(argv[0], "tail") == 0) {
        CLI_Cmd_Tail(argc, argv);
    } else if (strcmp(argv[0], "cp") == 0) {
        CLI_Cmd_Cp(argc, argv);
    } else if (strcmp(argv[0], "tree") == 0) {
        CLI_Cmd_Tree(argc, argv);
    } else if (strcmp(argv[0], "du") == 0) {
        CLI_Cmd_Du(argc, argv);
    } else if (strcmp(argv[0], "find") == 0) {
        CLI_Cmd_Find(argc, argv);
    } else if (strcmp(argv[0], "free") == 0) {
        CLI_Cmd_Free();
    } else if (strcmp(argv[0], "stat") == 0) {
        CLI_Cmd_Stat(argc, argv);
    } else if (strcmp(argv[0], "mv") == 0 || strcmp(argv[0], "rename") == 0) {
        CLI_Cmd_Mv(argc, argv);
    } else if (strcmp(argv[0], "chmod") == 0) {
        CLI_Cmd_Chmod(argc, argv);
    } else if (strcmp(argv[0], "play") == 0) {
        CLI_Cmd_Play(argc, argv);
    } else if (strcmp(argv[0], "vol") == 0) {
        CLI_Cmd_Vol(argc, argv);
    } else if (strcmp(argv[0], "pause") == 0) {
        CLI_Cmd_Pause(argc, argv);
    } else if (strcmp(argv[0], "resume") == 0) {
        CLI_Cmd_Resume(argc, argv);
    } else if (strcmp(argv[0], "stop") == 0) {
        CLI_Cmd_Stop(argc, argv);
    } else if (strcmp(argv[0], "playst") == 0) {
        CLI_Cmd_Playst();
    } else if (strcmp(argv[0], "lsdev") == 0) {
        CLI_Cmd_Lsdev();
    } else if (strcmp(argv[0], "bmp") == 0) {
        CLI_Cmd_Bmp(argc, argv);
    } else if (strcmp(argv[0], "lsstatus") == 0) {
        CLI_Cmd_Lsstatus();
    } else if (strcmp(argv[0], "subdisp") == 0) {
        CLI_Cmd_Subdisp(argc, argv);
    } else if (strcmp(argv[0], "light") == 0) {
        CLI_Cmd_Light(argc, argv);
    } else if (strcmp(argv[0], "note") == 0) {
        CLI_Cmd_Note(argc, argv);
    } else if (strcmp(argv[0], "mouse") == 0) {
        CLI_Cmd_Mouse(argc, argv);
    } else if (strcmp(argv[0], "roll") == 0) {
        CLI_Cmd_Roll(argc, argv);
    } else if (strcmp(argv[0], "keyboard") == 0) {
        CLI_Cmd_Keyboard(argc, argv);
    } else if (strcmp(argv[0], "music") == 0) {
        CLI_Cmd_Music(argc, argv);
    } else if (strcmp(argv[0], "config") == 0) {
        CLI_Cmd_Config(argc, argv);
    } else if (strcmp(argv[0], "speaker") == 0) {
        CLI_Cmd_Speaker(argc, argv);
    } else if (strcmp(argv[0], "rgb") == 0) {
        CLI_Cmd_Rgb(argc, argv);
    } else if (strcmp(argv[0], "fp") == 0) {
        CLI_Cmd_Fp(argc, argv);
    } else if (strcmp(argv[0], "nfc") == 0) {
        CLI_Cmd_Nfc(argc, argv);
    } else if (strcmp(argv[0], "ir") == 0) {
        CLI_Cmd_Ir(argc, argv);
    } else {
        printf("Unknown command: %s\r\n", argv[0]);
    }

    /* Unlock CH378: audio engine will reopen files and resume streaming */
    Audio_CH378_Unlock();
}
