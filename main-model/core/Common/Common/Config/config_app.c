#include "config_app.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

/* Build filename "<app_name>.json" */
static void app_make_filename(const char *app_name, char *out, uint16_t out_size)
{
    snprintf(out, out_size, "%s.json", app_name);
}

int Config_AppGetInt(const char *app_name, const char *key, int default_val)
{
    char filename[64];
    int val;

    if (!app_name || !key) return default_val;

    app_make_filename(app_name, filename, sizeof(filename));

    /* Use public Config_GetFileInt API */
    int ret = Config_GetFileInt(filename, key, &val);
    if (ret == 0) return val;

    /* File or key doesn't exist — create file and add key with default value */
    if (ret == -1) {
        /* File doesn't exist — create it */
        if (Config_NewFile(filename) == 0) {
            Config_AddFileKeyInt(filename, key, default_val);
        }
    } else if (ret == -2) {
        /* File exists but key doesn't — add it */
        Config_AddFileKeyInt(filename, key, default_val);
    }

    return default_val;
}

int Config_AppSetInt(const char *app_name, const char *key, int value)
{
    char filename[64];
    int ret;

    if (!app_name || !key) return -1;

    app_make_filename(app_name, filename, sizeof(filename));

    /* Try to set existing key */
    ret = Config_SetFileInt(filename, key, value);
    printf("[appcfg] SetFileInt('%s','%s')=%d\r\n", filename, key, ret);
    if (ret == -2) {
        /* Key doesn't exist in file — add it */
        ret = Config_AddFileKeyInt(filename, key, value);
        printf("[appcfg] AddFileKeyInt=%d\r\n", ret);
    } else if (ret == -1) {
        /* File doesn't exist — create it then add key */
        int nr = Config_NewFile(filename);
        printf("[appcfg] NewFile('%s')=%d\r\n", filename, nr);
        if (nr == 0) {
            ret = Config_AddFileKeyInt(filename, key, value);
            printf("[appcfg] AddFileKeyInt=%d\r\n", ret);
        }
    }
    return (ret == 0) ? 0 : -1;
}

int Config_AppGetString(const char *app_name, const char *key,
                        char *out_buf, uint16_t buf_size)
{
    char filename[64];

    if (!app_name || !key || !out_buf || buf_size == 0) return -2;

    app_make_filename(app_name, filename, sizeof(filename));
    return Config_GetFileString(filename, key, out_buf, buf_size);
}

int Config_AppSetString(const char *app_name, const char *key, const char *value)
{
    char filename[64];
    int ret;

    if (!app_name || !key || !value) return -1;

    app_make_filename(app_name, filename, sizeof(filename));

    ret = Config_SetFileString(filename, key, value);
    if (ret == -2) {
        ret = Config_AddFileKeyString(filename, key, value);
    }
    return (ret == 0) ? 0 : -1;
}

int Config_AppListKeys(const char *app_name)
{
    char filename[64];

    if (!app_name) return -1;

    app_make_filename(app_name, filename, sizeof(filename));
    return Config_ListKeys(filename, 1);
}
