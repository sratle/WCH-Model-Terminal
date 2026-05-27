#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"

/* ============================================================================
 * 编译配置宏
 * ============================================================================ */

/* 配置文件保存的目标存储设备：CH378_Device_USB 或 CH378_Device_TF
 * 仅当当前挂载设备与此一致时，才执行配置的加载/保存操作 */
#define CONFIG_STORAGE_DEVICE   CH378_Device_USB

/* 持久化根目录 */
#define CONFIG_ROOT_DIR         "\\CONFIG"

/* 系统配置文件名（LFN，支持小写） */
#define CONFIG_MAIN_FILE        "config.json"

/* 备份配置文件名 */
#define CONFIG_BAK_FILE         "config.bak"

/* 单个 JSON 文件最大尺寸（字节），受限于 RAM */
#define CONFIG_MAX_FILE_SIZE    4096

/* 配置版本号，用于格式升级兼容 */
#define CONFIG_VERSION          1

/* ============================================================================
 * 辅助宏
 * ============================================================================ */

/* 将模块类型和子类型组合为配置键 "TTSS" */
#define CONFIG_MODULE_KEY(type, subtype) \
    ((const char[]){ \
        "0123456789ABCDEF"[((type) >> 4) & 0x0F], \
        "0123456789ABCDEF"[(type) & 0x0F], \
        "0123456789ABCDEF"[((subtype) >> 4) & 0x0F], \
        "0123456789ABCDEF"[(subtype) & 0x0F], \
        '\0' \
    })

/* Core 配置键 */
#define CONFIG_CORE_KEY   "0000"

/* ============================================================================
 * API：初始化与生命周期
 * ============================================================================ */

/**
 * @brief  初始化配置系统
 *         - 选择存储设备并挂载
 *         - 确保 \CONFIG 目录存在
 *         - 加载 config.json（仅此文件：不存在时创建默认配置）
 *         - 将配置应用到各模块
 */
void Config_Init(void);

/**
 * @brief  将脏配置写回 config.json
 * @return ERR_SUCCESS 或 CH378 错误码；CH378 忙时返回 0x04 (PROTO_ERR_BUSY)
 */
uint8_t Config_Save(void);

/**
 * @brief  将当前 config.json 备份为 config.bak（覆盖已有备份）
 * @return ERR_SUCCESS 或 CH378 错误码
 */
uint8_t Config_Backup(void);

/**
 * @brief  从 config.bak 恢复配置（回滚）
 * @return ERR_SUCCESS 或 CH378 错误码；0xFE=设备不匹配
 */
uint8_t Config_Rollback(void);

/**
 * @brief  恢复默认配置并保存
 */
void Config_ResetDefaults(void);

/**
 * @brief  反初始化，释放 cJSON 对象
 */
void Config_Deinit(void);

/* ============================================================================
 * API：配置读取（config.json）
 * ============================================================================ */

/**
 * @brief  从 config.json 的指定模块段读取整数
 * @param  module_key  模块键名，如 "0000"、"0101"
 * @param  key         配置项键名，如 "volume"
 * @param  out_val     输出配置值
 * @return 0=成功，-1=模块键不存在，-2=配置项不存在
 */
int Config_GetInt(const char *module_key, const char *key, int *out_val);

/**
 * @brief  从 config.json 的指定模块段读取字符串
 * @param  module_key  模块键名
 * @param  key         配置项键名
 * @param  out_val     输出配置值字符串指针（cJSON 内部指针，无需释放）
 * @return 0=成功，-1=模块键不存在，-2=配置项不存在
 */
int Config_GetString(const char *module_key, const char *key, const char **out_val);

/* ============================================================================
 * API：配置写入（config.json）
 * ============================================================================ */

/**
 * @brief  向 config.json 的指定模块段写入整数
 * @return 0=成功，-1=模块键不存在，-2=配置项不存在（不会自动创建）
 */
int Config_SetInt(const char *module_key, const char *key, int value);

/**
 * @brief  向 config.json 的指定模块段写入字符串
 * @return 0=成功，-1=模块键不存在，-2=配置项不存在（不会自动创建）
 */
int Config_SetString(const char *module_key, const char *key, const char *value);

/* ============================================================================
 * API：数据文件读写（按需加载）
 * ============================================================================ */

/**
 * @brief  从 CONFIG 目录读取 JSON 文件并解析
 * @param  filename  文件名，如 "game.json"
 * @return cJSON 对象指针（调用者负责 cJSON_Delete）；文件不存在返回 NULL
 */
void* Config_LoadFile(const char *filename);

/**
 * @brief  将 cJSON 对象序列化并写入 CONFIG 目录（覆盖已有文件）
 * @param  filename  文件名
 * @param  json      cJSON 对象指针
 * @return ERR_SUCCESS 或 CH378 错误码
 */
uint8_t Config_SaveFile(const char *filename, void *json);

/**
 * @brief  从指定 JSON 文件中读取一个整数配置项
 * @param  filename  文件名
 * @param  key       配置项键名（支持点分路径，如 "player.score"）
 * @param  out_val   输出配置值
 * @return 0=成功，-1=文件不存在，-2=键不存在
 */
int Config_GetFileInt(const char *filename, const char *key, int *out_val);

/**
 * @brief  向指定 JSON 文件中写入一个整数配置项
 * @return 0=成功，-1=文件不存在，-2=键不存在（不会自动创建）
 */
int Config_SetFileInt(const char *filename, const char *key, int value);

/**
 * @brief  从指定 JSON 文件中读取一个字符串配置项
 * @return 0=成功，-1=文件不存在，-2=键不存在
 */
int Config_GetFileString(const char *filename, const char *key, const char **out_val);

/**
 * @brief  向指定 JSON 文件中写入一个字符串配置项
 * @return 0=成功，-1=文件不存在，-2=键不存在（不会自动创建）
 */
int Config_SetFileString(const char *filename, const char *key, const char *value);

/* ============================================================================
 * API：配置项与文件创建（显式操作）
 * ============================================================================ */

/**
 * @brief  在 config.json 的指定模块段中新增一个整数配置项
 * @return 0=成功，-2=键已存在
 */
int Config_AddKeyInt(const char *module_key, const char *key, int value);

/**
 * @brief  在 config.json 的指定模块段中新增一个字符串配置项
 * @return 0=成功，-2=键已存在
 */
int Config_AddKeyString(const char *module_key, const char *key, const char *value);

/**
 * @brief  在指定数据文件中新增一个整数配置项
 * @return 0=成功，-1=文件不存在，-2=键已存在
 */
int Config_AddFileKeyInt(const char *filename, const char *key, int value);

/**
 * @brief  在指定数据文件中新增一个字符串配置项
 * @return 0=成功，-1=文件不存在，-2=键已存在
 */
int Config_AddFileKeyString(const char *filename, const char *key, const char *value);

/**
 * @brief  在 CONFIG 目录下创建一个空的 JSON 数据文件（内容为 {}）
 * @param  filename  文件名（不允许为 config.json）
 * @return 0=成功，-1=文件已存在，-3=文件名为 config.json
 */
int Config_NewFile(const char *filename);

/**
 * @brief  列出配置中的所有键名（基于实际 JSON 数据，非预设表）
 * @param  target  模块键名（如 "0101"）或数据文件名（如 "game.json"）
 * @param  is_file 0=模块键名，1=数据文件名
 * @return 0=成功，-1=模块/文件不存在
 */
int Config_ListKeys(const char *target, uint8_t is_file);

/**
 * @brief  删除 CONFIG 目录下的数据文件（支持 LFN）
 * @param  filename  文件名（不允许为 config.json）
 * @return ERR_SUCCESS 或 CH378 错误码；0xFE=设备不匹配
 */
uint8_t Config_DeleteFile(const char *filename);

/* ============================================================================
 * API：配置应用
 * ============================================================================ */

/**
 * @brief  将当前配置应用到各硬件模块
 * @note   在 Config_Init() 末尾和 Config_ResetDefaults() 后调用
 */
void Config_Apply(void);

/* ============================================================================
 * API：脏标记查询
 * ============================================================================ */

/**
 * @brief  查询配置是否有未保存的变更
 * @return 1=有脏数据，0=无
 */
uint8_t Config_IsDirty(void);

/**
 * @brief  检查当前挂载设备是否与配置保存目标一致
 * @return 1=设备匹配可操作，0=设备不匹配
 */
uint8_t Config_IsDeviceMatch(void);

/**
 * @brief  获取配置保存目标设备的名称字符串
 * @return "USB" 或 "SD"
 */
const char* Config_GetTargetDeviceName(void);

/**
 * @brief  配置主循环处理（在 V5F 主循环中调用）
 *         处理 V3F→V5F 跨核配置请求
 */
void Config_Process(void);

/**
 * @brief  构建 CONFIG 目录下文件的完整 SFN 路径
 * @param  filename  文件名
 * @param  out       输出缓冲区
 * @param  out_len   输出缓冲区大小
 */
void Config_MakePath(const char *filename, char *out, uint16_t out_len);

/**
 * @brief  默认配置表条目类型（供 CLI 遍历使用）
 */
typedef struct {
    const char *module_key;
    const char *key;
    int default_value;
} config_default_entry_t;

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_H */
