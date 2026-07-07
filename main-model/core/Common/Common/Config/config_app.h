#ifndef __CONFIG_APP_H
#define __CONFIG_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32h417.h"

/* ============================================================================
 * GAME/APP 配置持久化接口
 *
 * 每个 GAME/APP 拥有独立的 JSON 数据文件（存储在 \CONFIG\ 目录下），
 * 文件名为 "<app_name>.json"。
 * 通过 CLI 直通命令 appcfg 可从 Display-1 侧访问。
 *
 * CLI 命令格式：
 *   appcfg get <app> <key>          读取配置值
 *   appcfg set <app> <key> <value>  写入配置值
 *   appcfg list <app>               列出应用所有配置
 * ============================================================================ */

/**
 * @brief  从应用数据文件读取整型配置
 * @param  app_name    应用名称（如 "game_2048"），用作文件名前缀
 * @param  key         键名（支持点分路径，如 "highscore"）
 * @param  default_val 默认值（文件不存在或键不存在时返回）
 * @return 配置值
 */
int Config_AppGetInt(const char *app_name, const char *key, int default_val);

/**
 * @brief  向应用数据文件写入整型配置
 * @param  app_name    应用名称
 * @param  key         键名
 * @param  value       配置值
 * @return 0=成功，-1=文件操作失败，-2=键不存在
 */
int Config_AppSetInt(const char *app_name, const char *key, int value);

/**
 * @brief  读取应用数据文件中的字符串配置
 * @param  app_name    应用名称
 * @param  key         键名
 * @param  out_buf     输出缓冲区（调用者分配）
 * @param  buf_size    缓冲区大小
 * @return 0=成功，-1=文件不存在，-2=键不存在
 */
int Config_AppGetString(const char *app_name, const char *key,
                        char *out_buf, uint16_t buf_size);

/**
 * @brief  向应用数据文件写入字符串配置
 * @param  app_name    应用名称
 * @param  key         键名
 * @param  value       字符串值
 * @return 0=成功，-1=失败
 */
int Config_AppSetString(const char *app_name, const char *key, const char *value);

/**
 * @brief  列出应用数据文件中的所有键（通过 printf 输出）
 * @param  app_name    应用名称
 * @return 0=成功，-1=失败
 */
int Config_AppListKeys(const char *app_name);

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_APP_H */
