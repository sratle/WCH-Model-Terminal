#include "config.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "CJSON/cJSON.h"
#include "CH378/CH378.h"
#include "CS43131/cs43131.h"
#include "Display/display.h"
#include "Keyboard/keyboard.h"
#include "Power/power.h"
#include "CH585F/ch585f_bt.h"
#include "Submodels/submodels.h"
#include "Protocol/protocol.h"
#include "hardware.h"

/************************* 内部变量 *************************/

static cJSON *config_root = NULL;   /* config.json 的 cJSON 根对象 */
static uint8_t config_dirty = 0;    /* 脏标记：有未写回的变更 */
static uint8_t s_config_applied = 0; /* Config_Apply 至少成功调用过一次 */

/* 前向声明 */
static void Config_SyncFromHardware(void);

extern ch378_t ch378_g;
extern cs43131_t CS43131_g;
extern display_t display_g;
extern keyboard_t keyboard_g;
extern power_t power_g;
extern ch585f_t ch585f_g;
extern submodels_t submodels_g[3];

/************************* 默认配置表 *************************/

const config_default_entry_t config_defaults[] = {
    /* Core (0000) */
    { "0000", "volume",          50  },
    { "0000", "volume_step",     5   },
    { "0000", "screen_timeout",  30  },

    /* Display-LCD (0101) */
    { "0101", "brightness",      80  },
    { "0101", "rotation",        0   },

    /* Display-Eink (0102) */
    { "0102", "brightness",      50  },
    { "0102", "rotation",        0   },

    /* Wireless-BT (0201) */
    { "0201", "discoverable",    0   },

    /* Power (0301) */
    { "0301", "report_interval", 10  },

    /* Keyboard-Main (0401) */
    { "0401", "backlight",       50  },

    /* Keyboard-Game (0402) */
    { "0402", "backlight",       50  },

    /* Keyboard-Music (0403) */
    { "0403", "backlight",       50  },

    /* Submodel-Health (0502) */
    { "0502", "monitor_interval", 10  },

    /* Submodel-RGB (0505) — 0=自定义(rgb.json), 1~3=预设模式 */
    { "0505", "rgb_mode",        1   },
    { "0505", "rgb_color_r",     255 },
    { "0505", "rgb_color_g",     255 },
    { "0505", "rgb_color_b",     0   },
    { "0505", "rgb_brightness",  80  },
    { "0505", "rgb_speed",       50  },

    /* Submodel-TouchRing (0504) */
    { "0504", "sensitivity",     50  },

    /* Submodel-Infrared (0506) */
    { "0506", "detect_threshold", 50  },

    /* Submodel-SubDisplay (0507) */
    { "0507", "brightness",      80  },
    { "0507", "content_mode",   0   },
};

#define CONFIG_DEFAULT_COUNT  (sizeof(config_defaults) / sizeof(config_defaults[0]))

/* 供 CLI 等外部模块访问默认配置表 */
const uint16_t config_default_count = CONFIG_DEFAULT_COUNT;

/************************* 内部辅助函数 *************************/

/**
 * @brief  ASCII 字符串转 UTF-16LE（用于 CH378 LFN 操作）
 */
static uint16_t Config_AsciiToUnicode(const char *ascii, uint8_t *unicode, uint16_t max_len)
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

/**
 * @brief  判断文件名是否为有效的 FAT 短文件名
 *         短文件名：全大写，主名≤8字符，扩展名≤3字符，无小写
 */
static uint8_t Config_IsShortName(const char *name)
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

/**
 * @brief  使用 LFN 方式打开文件
 * @param  path  完整路径
 * @return CH378 状态码
 */
static uint8_t Config_LFN_Open(const char *path)
{
    const char *filename;
    uint8_t unicode_name[512];

    filename = strrchr(path, '\\');
    filename = filename ? filename + 1 : path;

    if (Config_IsShortName(filename)) {
        return CH378FileOpen((uint8_t*)path);
    }

    Config_AsciiToUnicode(filename, unicode_name, sizeof(unicode_name));
    return CH378_Open_Long_Name((uint8_t*)path, unicode_name);
}

/**
 * @brief  使用 LFN 方式创建文件
 * @param  path  完整路径
 * @return CH378 状态码
 */
static uint8_t Config_LFN_Create(const char *path)
{
    const char *filename;
    uint8_t unicode_name[512];

    filename = strrchr(path, '\\');
    filename = filename ? filename + 1 : path;

    if (Config_IsShortName(filename)) {
        return CH378FileCreate((uint8_t*)path);
    }

    Config_AsciiToUnicode(filename, unicode_name, sizeof(unicode_name));
    return CH378_Create_Long_File((uint8_t*)path, unicode_name);
}

/**
 * @brief  使用 LFN 方式删除文件
 * @param  path  完整路径
 * @return CH378 状态码
 */
static uint8_t Config_LFN_Erase(const char *path)
{
    const char *filename;
    uint8_t unicode_name[512];

    filename = strrchr(path, '\\');
    filename = filename ? filename + 1 : path;

    if (Config_IsShortName(filename)) {
        return CH378FileErase((uint8_t*)path);
    }

    Config_AsciiToUnicode(filename, unicode_name, sizeof(unicode_name));
    return CH378_Erase_Long_Name((uint8_t*)path, unicode_name);
}

/**
 * @brief  构建 CONFIG 目录下文件的完整路径
 * @param  filename  文件名（如 "config.json"）
 * @param  out       输出缓冲区
 * @param  out_len   输出缓冲区大小
 */
void Config_MakePath(const char *filename, char *out, uint16_t out_len)
{
    snprintf(out, out_len, "%s\\%s", CONFIG_ROOT_DIR, filename);
}

/**
 * @brief  从 CH378 读取文件全部内容到缓冲区
 * @param  path    SFN 完整路径
 * @param  buf     输出缓冲区
 * @param  buf_size 缓冲区大小
 * @return 实际读取字节数，0 表示失败
 */
static uint32_t Config_ReadFileToBuf(const char *path, uint8_t *buf, uint32_t buf_size)
{
    uint8_t status;
    uint16_t real_len;
    uint32_t total = 0;

    status = Config_LFN_Open(path);
    if (status != ERR_SUCCESS) {
        return 0;
    }

    while (total < buf_size) {
        uint16_t to_read = (buf_size - total > 256) ? 256 : (uint16_t)(buf_size - total);
        status = CH378ByteRead(buf + total, to_read, &real_len);
        if (status != ERR_SUCCESS || real_len == 0) break;
        total += real_len;
        if (real_len < to_read) break;
    }

    CH378FileClose(0);
    return total;
}

/**
 * @brief  将缓冲区数据写入 CH378 文件（覆盖写入）
 * @param  path    SFN 完整路径
 * @param  buf     数据缓冲区
 * @param  len     数据长度
 * @return ERR_SUCCESS 或 CH378 错误码
 */
static uint8_t Config_WriteBufToFile(const char *path, const uint8_t *buf, uint32_t len)
{
    uint8_t status;
    uint16_t real_len;
    uint32_t written = 0;

    /* 先尝试打开已有文件 */
    status = Config_LFN_Open(path);
    if (status == ERR_SUCCESS) {
        /* 文件存在：截断后覆盖写入 */
        CH378SetFileSize(0);
        CH378ByteLocate(0);
    } else if (status == ERR_MISS_FILE) {
        /* 文件不存在：创建新文件 */
        status = Config_LFN_Create(path);
        if (status != ERR_SUCCESS) return status;
    } else {
        return status;
    }

    while (written < len) {
        uint16_t to_write = (len - written > 256) ? 256 : (uint16_t)(len - written);
        status = CH378ByteWrite((uint8_t*)(buf + written), to_write, &real_len);
        if (status != ERR_SUCCESS) {
            CH378FileClose(0);
            return status;
        }
        written += real_len;
    }

    CH378FileClose(1);
    return ERR_SUCCESS;
}

/**
 * @brief  使用默认配置表重建 config_root
 */
static void Config_BuildDefaults(void)
{
    uint16_t i;
    const config_default_entry_t *entry;
    cJSON *module_obj;
    cJSON *item;

    if (config_root) {
        cJSON_Delete(config_root);
        config_root = NULL;
    }

    config_root = cJSON_CreateObject();
    if (!config_root) return;

    /* 添加 version */
    item = cJSON_CreateNumber(CONFIG_VERSION);
    if (item) cJSON_AddItemToObject(config_root, "version", item);

    /* 遍历默认配置表，按 module_key 分组创建对象和键 */
    for (i = 0; i < CONFIG_DEFAULT_COUNT; i++) {
        entry = &config_defaults[i];

        /* 查找或创建模块段 */
        module_obj = cJSON_GetObjectItem(config_root, entry->module_key);
        if (!module_obj) {
            module_obj = cJSON_CreateObject();
            if (module_obj) {
                cJSON_AddItemToObject(config_root, entry->module_key, module_obj);
            }
        }

        /* 添加配置项 */
        if (module_obj) {
            item = cJSON_CreateNumber(entry->default_value);
            if (item) {
                cJSON_AddItemToObject(module_obj, entry->key, item);
            }
        }
    }
}

/**
 * @brief  确保 \CONFIG 目录存在
 * @return ERR_SUCCESS 或 CH378 错误码
 */
static uint8_t Config_EnsureDir(void)
{
    uint8_t status;

    /* 尝试打开目录，如果不存在则创建 */
    status = CH378FileOpen((uint8_t*)CONFIG_ROOT_DIR);
    if (status == ERR_OPEN_DIR) {
        /* 目录已存在 */
        CH378FileClose(0);
        return ERR_SUCCESS;
    }

    if (status == ERR_SUCCESS) {
        /* 是文件而非目录，关闭 */
        CH378FileClose(0);
    }

    /* 尝试创建目录 */
    status = CH378DirCreate((uint8_t*)CONFIG_ROOT_DIR);
    if (status == ERR_SUCCESS || status == ERR_NAME_EXIST) {
        return ERR_SUCCESS;
    }

    printf("[Config] Failed to create %s (status=%02X)\r\n", CONFIG_ROOT_DIR, status);
    return status;
}

/**
 * @brief  按点分路径从 cJSON 对象中查找嵌套键
 * @param  root  JSON 对象
 * @param  key   点分路径，如 "player.score"
 * @return 找到的 cJSON 项，未找到返回 NULL
 */
static cJSON* Config_FindByPath(cJSON *root, const char *key)
{
    char buf[64];
    char *dot;
    cJSON *obj = root;

    strncpy(buf, key, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    while (obj) {
        dot = strchr(buf, '.');
        if (dot) {
            *dot = '\0';
            obj = cJSON_GetObjectItem(obj, buf);
            if (!obj || !cJSON_IsObject(obj)) return NULL;
            /* 继续处理剩余路径 */
            memmove(buf, dot + 1, strlen(dot + 1) + 1);
        } else {
            return cJSON_GetObjectItem(obj, buf);
        }
    }

    return NULL;
}

/**
 * @brief  按点分路径在 cJSON 对象中查找父对象和最终键名
 * @param  root      JSON 根对象
 * @param  key       点分路径
 * @param  out_parent 输出父对象指针
 * @param  out_leaf  输出最终键名（指向 buf 内部）
 * @return 0=成功，-2=路径中间节点不存在
 */
static int Config_FindParentByPath(cJSON *root, const char *key,
                                   cJSON **out_parent, char **out_leaf)
{
    static char buf[64];
    char *dot;
    cJSON *obj = root;

    strncpy(buf, key, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    while (1) {
        dot = strchr(buf, '.');
        if (dot) {
            *dot = '\0';
            obj = cJSON_GetObjectItem(obj, buf);
            if (!obj || !cJSON_IsObject(obj)) return -2;
            memmove(buf, dot + 1, strlen(dot + 1) + 1);
        } else {
            *out_parent = obj;
            *out_leaf = buf;
            return 0;
        }
    }
}

/************************* 初始化与生命周期 *************************/

void Config_Init(void)
{
    char path[CH378_MAX_PATH_LEN];
    uint8_t *file_buf;
    uint32_t file_len;
    uint8_t status;

    /* 检查当前挂载设备是否与配置保存目标一致 */
    if (!Config_IsDeviceMatch()) {
        printf("[Config] Current device (%s) != target (%s), skip loading\r\n",
               (ch378_g.now_device == CH378_Device_USB) ? "USB" :
               (ch378_g.now_device == CH378_Device_TF) ? "SD" : "??",
               Config_GetTargetDeviceName());
        Config_BuildDefaults();
        return;
    }

    /* 确保 \CONFIG 目录存在 */
    status = Config_EnsureDir();
    if (status != ERR_SUCCESS) {
        printf("[Config] Directory check failed, using defaults\r\n");
        Config_BuildDefaults();
        return;
    }

    /* 分配文件读取缓冲区 */
    file_buf = (uint8_t*)malloc(CONFIG_MAX_FILE_SIZE);
    if (!file_buf) {
        printf("[Config] malloc failed, using defaults\r\n");
        Config_BuildDefaults();
        return;
    }

    /* 读取 config.json */
    Config_MakePath(CONFIG_MAIN_FILE, path, sizeof(path));
    file_len = Config_ReadFileToBuf(path, file_buf, CONFIG_MAX_FILE_SIZE - 1);

    if (file_len == 0) {
        /* 文件不存在，使用默认配置（不自动创建文件） */
        printf("[Config] config.json not found, using defaults\r\n");
        Config_BuildDefaults();
    } else {
        /* 解析 JSON */
        file_buf[file_len] = '\0';
        config_root = cJSON_Parse((char*)file_buf);
        if (!config_root) {
            printf("[Config] JSON parse failed, using defaults\r\n");
            Config_BuildDefaults();
        } else {
            printf("[Config] Loaded config.json (%lu bytes)\r\n", file_len);
        }
    }

    free(file_buf);

    /* 将配置应用到各模块 */
    Config_Apply();
}

uint8_t Config_Save(void)
{
    char path[CH378_MAX_PATH_LEN];
    char *json_str;
    uint8_t status;
    uint32_t json_len;
    int needs_ch378_lock = 0;

    /* 当前设备不匹配，不允许保存 */
    if (!Config_IsDeviceMatch()) {
        printf("[Config] Save rejected: current device != target (%s)\r\n",
               Config_GetTargetDeviceName());
        return 0xFE;  /* CONFIG_ERR_DEVICE_MISMATCH */
    }

    if (!config_root) return ERR_SUCCESS;

    /* 保存前从硬件同步运行时状态到 config_root */
    Config_SyncFromHardware();

    /* 无变更则跳过写盘 */
    if (!config_dirty) {
        printf("[Config] No changes\r\n");
        return ERR_SUCCESS;
    }

    json_str = cJSON_PrintUnformatted(config_root);
    if (!json_str) {
        printf("[Config] cJSON_Print failed\r\n");
        return 0xFF;
    }

    json_len = strlen(json_str);
    if (json_len > CONFIG_MAX_FILE_SIZE) {
        printf("[Config] Config too large (%lu bytes)\r\n", json_len);
        cJSON_free(json_str);
        return 0xFF;
    }

    /* 音频并发协调：写盘前预填充音频缓冲区并锁定 CH378 */
    if (Audio_IsStreaming()) {
        Audio_PreFill();
        needs_ch378_lock = 1;
    }
    if (needs_ch378_lock) {
        Audio_CH378_Lock();
    }

    /* 构建 config.json 完整路径 */
    Config_MakePath(CONFIG_MAIN_FILE, path, sizeof(path));

    /* 原子写入：先备份当前 config.json 到 config.bak，再覆写
     * 注意：不调用 Config_Backup() 以避免递归（Config_Backup 内部会调用 Config_Save） */
    {
        char bak_path[CH378_MAX_PATH_LEN];
        uint8_t *bak_buf = (uint8_t*)malloc(CONFIG_MAX_FILE_SIZE);
        if (bak_buf) {
            uint32_t bak_len = Config_ReadFileToBuf(path, bak_buf, CONFIG_MAX_FILE_SIZE);
            if (bak_len > 0) {
                Config_MakePath(CONFIG_BAK_FILE, bak_path, sizeof(bak_path));
                Config_WriteBufToFile(bak_path, bak_buf, bak_len);
            }
            free(bak_buf);
        }
    }

    status = Config_WriteBufToFile(path, (const uint8_t*)json_str, json_len);

    if (needs_ch378_lock) {
        Audio_CH378_Unlock();
    }

    if (status == ERR_SUCCESS) {
        config_dirty = 0;
        printf("[Config] Saved (%lu bytes)\r\n", json_len);
    } else {
        printf("[Config] Save failed (status=%02X)\r\n", status);
    }

    cJSON_free(json_str);
    return status;
}

uint8_t Config_Backup(void)
{
    char src_path[CH378_MAX_PATH_LEN];
    char dst_path[CH378_MAX_PATH_LEN];
    uint8_t *file_buf;
    uint32_t file_len;
    uint8_t status;

    /* 当前设备不匹配，不允许备份 */
    if (!Config_IsDeviceMatch()) return 0xFE;

    /* 先保存当前脏数据 */
    if (config_dirty) {
        status = Config_Save();
        if (status != ERR_SUCCESS && status != 0x04) return status;
    }

    file_buf = (uint8_t*)malloc(CONFIG_MAX_FILE_SIZE);
    if (!file_buf) return 0xFF;

    /* 读取 config.json */
    Config_MakePath(CONFIG_MAIN_FILE, src_path, sizeof(src_path));
    file_len = Config_ReadFileToBuf(src_path, file_buf, CONFIG_MAX_FILE_SIZE);
    if (file_len == 0) {
        free(file_buf);
        printf("[Config] Backup: cannot read config.json\r\n");
        return 0xFF;
    }

    /* 写入 config.bak */
    Config_MakePath(CONFIG_BAK_FILE, dst_path, sizeof(dst_path));
    status = Config_WriteBufToFile(dst_path, file_buf, file_len);

    if (status == ERR_SUCCESS) {
        printf("[Config] Backed up (%lu bytes)\r\n", file_len);
    } else {
        printf("[Config] Backup failed (status=%02X)\r\n", status);
    }

    free(file_buf);
    return status;
}

uint8_t Config_Rollback(void)
{
    char bak_path[CH378_MAX_PATH_LEN];
    uint8_t *file_buf;
    uint32_t file_len;
    uint8_t status;
    cJSON *new_root;

    if (!Config_IsDeviceMatch()) return 0xFE;

    /* 读取 config.bak */
    Config_MakePath(CONFIG_BAK_FILE, bak_path, sizeof(bak_path));
    file_buf = (uint8_t*)malloc(CONFIG_MAX_FILE_SIZE);
    if (!file_buf) return 0xFF;

    file_len = Config_ReadFileToBuf(bak_path, file_buf, CONFIG_MAX_FILE_SIZE - 1);
    if (file_len == 0) {
        free(file_buf);
        printf("[Config] Rollback: config.bak not found\r\n");
        return 0xFF;
    }

    /* 解析 JSON */
    file_buf[file_len] = '\0';
    new_root = cJSON_Parse((char*)file_buf);
    free(file_buf);

    if (!new_root) {
        printf("[Config] Rollback: config.bak parse failed\r\n");
        return 0xFF;
    }

    /* 替换当前配置 */
    if (config_root) cJSON_Delete(config_root);
    config_root = new_root;
    config_dirty = 1;

    /* 保存并应用 */
    status = Config_Save();
    if (status == ERR_SUCCESS) {
        Config_Apply();
        printf("[Config] Rollback from config.bak OK\r\n");
    }

    return status;
}

void Config_ResetDefaults(void)
{
    Config_BuildDefaults();
    config_dirty = 1;
    Config_Save();
    Config_Apply();
    printf("[Config] Reset to defaults\r\n");
}

void Config_Deinit(void)
{
    if (config_root) {
        cJSON_Delete(config_root);
        config_root = NULL;
    }
    config_dirty = 0;
}

/************************* 配置读取（config.json） *************************/

int Config_GetInt(const char *module_key, const char *key, int *out_val)
{
    cJSON *module_obj;
    cJSON *item;

    if (!config_root) return -1;

    module_obj = cJSON_GetObjectItem(config_root, module_key);
    if (!module_obj) return -1;

    item = cJSON_GetObjectItem(module_obj, key);
    if (!item) return -2;

    if (cJSON_IsNumber(item)) {
        *out_val = item->valueint;
        return 0;
    }

    return -2;
}

int Config_GetString(const char *module_key, const char *key, const char **out_val)
{
    cJSON *module_obj;
    cJSON *item;

    if (!config_root) return -1;

    module_obj = cJSON_GetObjectItem(config_root, module_key);
    if (!module_obj) return -1;

    item = cJSON_GetObjectItem(module_obj, key);
    if (!item) return -2;

    if (cJSON_IsString(item)) {
        *out_val = item->valuestring;
        return 0;
    }

    return -2;
}

/************************* 配置写入（config.json） *************************/

int Config_SetInt(const char *module_key, const char *key, int value)
{
    cJSON *module_obj;
    cJSON *item;

    if (!config_root) return -1;

    module_obj = cJSON_GetObjectItem(config_root, module_key);
    if (!module_obj) return -1;

    item = cJSON_GetObjectItem(module_obj, key);
    if (!item) return -2;

    cJSON_SetIntValue(item, value);
    config_dirty = 1;
    return 0;
}

int Config_SetString(const char *module_key, const char *key, const char *value)
{
    cJSON *module_obj;
    cJSON *item;

    if (!config_root) return -1;

    module_obj = cJSON_GetObjectItem(config_root, module_key);
    if (!module_obj) return -1;

    item = cJSON_GetObjectItem(module_obj, key);
    if (!item) return -2;

    if (!cJSON_IsString(item)) return -2;

    char *new_str = cJSON_SetValuestring(item, value);
    if (!new_str) return -2;

    config_dirty = 1;
    return 0;
}

/************************* 数据文件读写（按需加载） *************************/

void* Config_LoadFile(const char *filename)
{
    char path[CH378_MAX_PATH_LEN];
    uint8_t *file_buf;
    uint32_t file_len;
    cJSON *json;

    file_buf = (uint8_t*)malloc(CONFIG_MAX_FILE_SIZE);
    if (!file_buf) return NULL;

    Config_MakePath(filename, path, sizeof(path));
    file_len = Config_ReadFileToBuf(path, file_buf, CONFIG_MAX_FILE_SIZE - 1);

    if (file_len == 0) {
        free(file_buf);
        return NULL;
    }

    file_buf[file_len] = '\0';
    json = cJSON_Parse((char*)file_buf);
    free(file_buf);

    return (void*)json;
}

uint8_t Config_SaveFile(const char *filename, void *json)
{
    char path[CH378_MAX_PATH_LEN];
    char *json_str;
    uint8_t status;

    if (!Config_IsDeviceMatch()) return 0xFE;

    json_str = cJSON_PrintUnformatted((cJSON*)json);
    if (!json_str) return 0xFF;

    Config_MakePath(filename, path, sizeof(path));
    status = Config_WriteBufToFile(path, (const uint8_t*)json_str, strlen(json_str));

    cJSON_free(json_str);
    return status;
}

int Config_GetFileInt(const char *filename, const char *key, int *out_val)
{
    cJSON *json;
    cJSON *item;
    int ret;

    json = (cJSON*)Config_LoadFile(filename);
    if (!json) return -1;

    item = Config_FindByPath(json, key);
    if (!item || !cJSON_IsNumber(item)) {
        cJSON_Delete(json);
        return -2;
    }

    *out_val = item->valueint;
    ret = 0;
    cJSON_Delete(json);
    return ret;
}

int Config_SetFileInt(const char *filename, const char *key, int value)
{
    cJSON *json;
    cJSON *item;
    int ret;

    if (!Config_IsDeviceMatch()) return -3;

    json = (cJSON*)Config_LoadFile(filename);
    if (!json) return -1;

    /* 查找键 */
    item = Config_FindByPath(json, key);
    if (!item) {
        cJSON_Delete(json);
        return -2;
    }

    cJSON_SetIntValue(item, value);

    /* 写回文件 */
    ret = Config_SaveFile(filename, json);
    cJSON_Delete(json);

    return (ret == ERR_SUCCESS) ? 0 : -1;
}

int Config_GetFileString(const char *filename, const char *key, char *out_buf, uint16_t buf_size)
{
    cJSON *json;
    cJSON *item;

    if (!out_buf || buf_size == 0) return -2;

    json = (cJSON*)Config_LoadFile(filename);
    if (!json) return -1;

    item = Config_FindByPath(json, key);
    if (!item || !cJSON_IsString(item)) {
        cJSON_Delete(json);
        return -2;
    }

    /* Copy string to caller-provided buffer before cJSON_Delete frees it */
    strncpy(out_buf, item->valuestring, buf_size - 1);
    out_buf[buf_size - 1] = '\0';
    cJSON_Delete(json);
    return 0;
}

int Config_SetFileString(const char *filename, const char *key, const char *value)
{
    cJSON *json;
    cJSON *item;
    int ret;

    if (!Config_IsDeviceMatch()) return -3;

    json = (cJSON*)Config_LoadFile(filename);
    if (!json) return -1;

    item = Config_FindByPath(json, key);
    if (!item || !cJSON_IsString(item)) {
        cJSON_Delete(json);
        return -2;
    }

    cJSON_SetValuestring(item, value);

    ret = Config_SaveFile(filename, json);
    cJSON_Delete(json);

    return (ret == ERR_SUCCESS) ? 0 : -1;
}

/************************* 配置项与文件创建（显式操作） *************************/

int Config_AddKeyInt(const char *module_key, const char *key, int value)
{
    cJSON *module_obj;
    cJSON *item;

    if (!Config_IsDeviceMatch()) return -3;

    if (!config_root) return -1;

    /* 查找或创建模块段 */
    module_obj = cJSON_GetObjectItem(config_root, module_key);
    if (!module_obj) {
        module_obj = cJSON_CreateObject();
        if (!module_obj) return -1;
        cJSON_AddItemToObject(config_root, module_key, module_obj);
    }

    /* 检查键是否已存在 */
    item = cJSON_GetObjectItem(module_obj, key);
    if (item) return -2;  /* 键已存在 */

    /* 添加新键 */
    item = cJSON_CreateNumber(value);
    if (!item) return -1;
    cJSON_AddItemToObject(module_obj, key, item);

    config_dirty = 1;
    return 0;
}

int Config_AddKeyString(const char *module_key, const char *key, const char *value)
{
    cJSON *module_obj;
    cJSON *item;

    if (!Config_IsDeviceMatch()) return -3;

    if (!config_root) return -1;

    /* 查找或创建模块段 */
    module_obj = cJSON_GetObjectItem(config_root, module_key);
    if (!module_obj) {
        module_obj = cJSON_CreateObject();
        if (!module_obj) return -1;
        cJSON_AddItemToObject(config_root, module_key, module_obj);
    }

    /* 检查键是否已存在 */
    item = cJSON_GetObjectItem(module_obj, key);
    if (item) return -2;

    /* 添加新键 */
    item = cJSON_CreateString(value);
    if (!item) return -1;
    cJSON_AddItemToObject(module_obj, key, item);

    config_dirty = 1;
    return 0;
}

int Config_AddFileKeyInt(const char *filename, const char *key, int value)
{
    cJSON *json;
    cJSON *parent;
    char *leaf;
    cJSON *item;
    int ret;

    if (!Config_IsDeviceMatch()) return -3;

    json = (cJSON*)Config_LoadFile(filename);
    if (!json) return -1;

    /* 查找父对象和叶键名 */
    ret = Config_FindParentByPath(json, key, &parent, &leaf);
    if (ret != 0) {
        cJSON_Delete(json);
        return -2;
    }

    /* 检查键是否已存在 */
    item = cJSON_GetObjectItem(parent, leaf);
    if (item) {
        cJSON_Delete(json);
        return -2;
    }

    /* 添加新键 */
    item = cJSON_CreateNumber(value);
    if (!item) {
        cJSON_Delete(json);
        return -1;
    }
    cJSON_AddItemToObject(parent, leaf, item);

    /* 写回文件 */
    ret = Config_SaveFile(filename, json);
    cJSON_Delete(json);

    return (ret == ERR_SUCCESS) ? 0 : -1;
}

int Config_AddFileKeyString(const char *filename, const char *key, const char *value)
{
    cJSON *json;
    cJSON *parent;
    char *leaf;
    cJSON *item;
    int ret;

    if (!Config_IsDeviceMatch()) return -3;

    json = (cJSON*)Config_LoadFile(filename);
    if (!json) return -1;

    ret = Config_FindParentByPath(json, key, &parent, &leaf);
    if (ret != 0) {
        cJSON_Delete(json);
        return -2;
    }

    item = cJSON_GetObjectItem(parent, leaf);
    if (item) {
        cJSON_Delete(json);
        return -2;
    }

    item = cJSON_CreateString(value);
    if (!item) {
        cJSON_Delete(json);
        return -1;
    }
    cJSON_AddItemToObject(parent, leaf, item);

    ret = Config_SaveFile(filename, json);
    cJSON_Delete(json);

    return (ret == ERR_SUCCESS) ? 0 : -1;
}

int Config_NewFile(const char *filename)
{
    char path[CH378_MAX_PATH_LEN];
    uint8_t status;

    if (!Config_IsDeviceMatch()) return -3;

    /* 不允许创建 config.json */
    if (strcmp(filename, CONFIG_MAIN_FILE) == 0) return -3;

    /* 检查文件是否已存在 */
    Config_MakePath(filename, path, sizeof(path));
    status = Config_LFN_Open(path);
    if (status == ERR_SUCCESS) {
        CH378FileClose(0);
        return -1;  /* 文件已存在 */
    }

    /* 创建文件，写入 "{}" */
    status = Config_WriteBufToFile(path, (const uint8_t*)"{}", 2);
    if (status == ERR_SUCCESS) return 0;

    return -1;
}

int Config_ListKeys(const char *target, uint8_t is_file)
{
    cJSON *obj = NULL;
    cJSON *item;
    uint8_t need_delete = 0;

    if (is_file) {
        /* 数据文件：加载并遍历 */
        obj = (cJSON*)Config_LoadFile(target);
        if (!obj) return -1;
        need_delete = 1;
    } else {
        /* config.json 中的模块段 */
        if (!config_root) return -1;
        obj = cJSON_GetObjectItem(config_root, target);
        if (!obj) return -1;
    }

    cJSON_ArrayForEach(item, obj) {
        printf("%s\r\n", item->string);
    }

    if (need_delete) cJSON_Delete(obj);
    return 0;
}

uint8_t Config_DeleteFile(const char *filename)
{
    char path[CH378_MAX_PATH_LEN];

    if (!Config_IsDeviceMatch()) return 0xFE;

    if (strcmp(filename, CONFIG_MAIN_FILE) == 0) return 0xFF;

    Config_MakePath(filename, path, sizeof(path));
    return Config_LFN_Erase(path);
}

/************************* 运行时状态同步 *************************/

/**
 * @brief  从硬件运行时状态同步到 config_root
 *         在 Config_Save() 时调用，确保运行时变更（如音量调节）被持久化
 * @note   仅同步 Core 自身管理的运行时状态，不涉及外部模块
 */
static void Config_SyncFromHardware(void)
{
    int cfg_vol;

    /* 同步音量：CS43131_g.volume 是当前实际值 */
    if (Config_GetInt("0000", "volume", &cfg_vol) == 0) {
        if (cfg_vol != (int)Audio_GetVolume()) {
            Config_SetInt("0000", "volume", (int)Audio_GetVolume());
        }
    }
}

/************************* 配置应用 *************************/

void Config_Apply(void)
{
    int val;
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint16_t len;
    uint8_t i;

    if (!config_root) return;

    /* 1. Audio: volume */
    if (Config_GetInt("0000", "volume", &val) == 0) {
        Audio_SetVolume((uint8_t)val);
    }

    /* 2. Display: brightness, rotation, screen_timeout (仅当 Display 在线时) */
    for (i = 0; i < HB_MAX_SLOTS; i++) {
        if (hardware_g.hb_slots[i].module_id == MODULE_ID_DISPLAY &&
            hardware_g.hb_slots[i].status == HB_STATUS_ONLINE) {

            const char *disp_key;
            uint8_t type = hardware_g.hb_slots[i].type;
            uint8_t subtype = hardware_g.hb_slots[i].subtype;

            if (type == MODULE_TYPE_DISPLAY && subtype == MODULE_SUBTYPE_DISPLAY_LCD) {
                disp_key = "0101";
            } else if (type == MODULE_TYPE_DISPLAY && subtype == MODULE_SUBTYPE_DISPLAY_EINK) {
                disp_key = "0102";
            } else {
                disp_key = "0101";
            }

            /* brightness */
            if (Config_GetInt(disp_key, "brightness", &val) == 0) {
                len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                         CMD_DISP_SET_BRIGHTNESS,
                                         (uint8_t[]){(uint8_t)val}, 1,
                                         buf, sizeof(buf));
                if (len > 0) Display_Send_Data(&display_g, buf, len);
            }

            /* rotation */
            if (Config_GetInt(disp_key, "rotation", &val) == 0) {
                len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                         CMD_DISP_SET_ROTATION,
                                         (uint8_t[]){(uint8_t)val}, 1,
                                         buf, sizeof(buf));
                if (len > 0) Display_Send_Data(&display_g, buf, len);
            }

            /* screen_timeout */
            if (Config_GetInt("0000", "screen_timeout", &val) == 0) {
                len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                         CMD_DISP_SCREEN_CONTROL,
                                         (uint8_t[]){SCREEN_CTRL_SET_TIMEOUT, (uint8_t)val}, 2,
                                         buf, sizeof(buf));
                if (len > 0) Display_Send_Data(&display_g, buf, len);
            }

            break;
        }
    }

    /* 3. Keyboard: backlight (仅当 Keyboard 在线时) */
    for (i = 0; i < HB_MAX_SLOTS; i++) {
        if (hardware_g.hb_slots[i].module_id == MODULE_ID_KEYBOARD &&
            hardware_g.hb_slots[i].status == HB_STATUS_ONLINE) {

            const char *kbd_key;
            uint8_t subtype = hardware_g.hb_slots[i].subtype;

            if (subtype == MODULE_SUBTYPE_KEYBOARD_MAIN) {
                kbd_key = "0401";
            } else if (subtype == MODULE_SUBTYPE_KEYBOARD_GAME) {
                kbd_key = "0402";
            } else if (subtype == MODULE_SUBTYPE_KEYBOARD_MUSIC) {
                kbd_key = "0403";
            } else {
                kbd_key = "0401";
            }

            if (Config_GetInt(kbd_key, "backlight", &val) == 0) {
                len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_KEYBOARD,
                                         CMD_KBD_SET_BACKLIGHT,
                                         (uint8_t[]){(uint8_t)val}, 1,
                                         buf, sizeof(buf));
                if (len > 0) Keyboard_Send_Data(&keyboard_g, buf, len);
            }

            break;
        }
    }

    /* 4. Power: report_interval (仅当 Power 在线时) */
    for (i = 0; i < HB_MAX_SLOTS; i++) {
        if (hardware_g.hb_slots[i].module_id == MODULE_ID_POWER &&
            hardware_g.hb_slots[i].status == HB_STATUS_ONLINE) {

            if (Config_GetInt("0301", "report_interval", &val) == 0) {
                len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_POWER,
                                         CMD_PWR_SET_REPORT_INTERVAL,
                                         (uint8_t[]){(uint8_t)val}, 1,
                                         buf, sizeof(buf));
                if (len > 0) Power_Send_Data(&power_g, buf, len);
            }

            break;
        }
    }

    /* 5. Wireless: discoverable */
    for (i = 0; i < HB_MAX_SLOTS; i++) {
        if (hardware_g.hb_slots[i].module_id == MODULE_ID_WIRELESS &&
            hardware_g.hb_slots[i].status == HB_STATUS_ONLINE) {

            if (Config_GetInt("0201", "discoverable", &val) == 0) {
                len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_WIRELESS,
                                         CMD_BT_SET_DISCOVERABLE,
                                         (uint8_t[]){(uint8_t)val}, 1,
                                         buf, sizeof(buf));
                if (len > 0) CH585F_Send_Data(&ch585f_g, buf, len);
            }

            break;
        }
    }

    /* 6. Submodel: RGB mode — 写入共享内存，由 V3F 在子模块上线时发送 */
    {
        int val;

        if (Config_GetInt("0505", "rgb_mode", &val) == 0)
            hardware_g.rgb_config.mode = (uint8_t)val;
        if (Config_GetInt("0505", "rgb_color_r", &val) == 0)
            hardware_g.rgb_config.r = (uint8_t)val;
        if (Config_GetInt("0505", "rgb_color_g", &val) == 0)
            hardware_g.rgb_config.g = (uint8_t)val;
        if (Config_GetInt("0505", "rgb_color_b", &val) == 0)
            hardware_g.rgb_config.b = (uint8_t)val;
        if (Config_GetInt("0505", "rgb_brightness", &val) == 0)
            hardware_g.rgb_config.brightness = (uint8_t)val;
        if (Config_GetInt("0505", "rgb_speed", &val) == 0)
            hardware_g.rgb_config.speed = (uint8_t)val;
        hardware_g.rgb_config.pending = 1;

        printf("[Config] RGB: mode=%d R=%d G=%d B=%d bright=%d speed=%d\r\n",
               hardware_g.rgb_config.mode,
               hardware_g.rgb_config.r,
               hardware_g.rgb_config.g,
               hardware_g.rgb_config.b,
               hardware_g.rgb_config.brightness,
               hardware_g.rgb_config.speed);
    }

    hardware_g.config_applied = 1;
    s_config_applied = 1;
}

/************************* 脏标记查询 *************************/

uint8_t Config_IsDirty(void)
{
    return config_dirty;
}

uint8_t Config_IsApplied(void)
{
    return s_config_applied;
}

/************************* 设备匹配检查 *************************/

uint8_t Config_IsDeviceMatch(void)
{
    return (ch378_g.now_device == CONFIG_STORAGE_DEVICE) ? 1 : 0;
}

const char* Config_GetTargetDeviceName(void)
{
    return (CONFIG_STORAGE_DEVICE == CH378_Device_USB) ? "USB" : "SD";
}
