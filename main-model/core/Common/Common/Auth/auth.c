#include "auth.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "Config/config.h"
#include "CJSON/cJSON.h"
#include "CH378/CH378.h"
#include "Display/display.h"
#include "Protocol/protocol.h"

/* ============================================================================
 * 内部数据结构
 * ============================================================================ */

typedef struct {
    char     name[AUTH_NAME_LEN + 1];
    uint32_t pin_hash;
    uint8_t  fp_ids[AUTH_MAX_FP_IDS];
    uint8_t  fp_count;
    uint8_t  nfc_cards[AUTH_MAX_NFC_CARDS][AUTH_NFC_CARD_LEN];
    uint8_t  nfc_count;
} auth_user_t;

typedef struct {
    char     path[AUTH_PATH_LEN + 1];   /* 规范化绝对路径（大写，'\' 分隔） */
    uint32_t acl_mask;                  /* bit i = users[i] 允许访问 */
} auth_folder_t;

typedef struct {
    auth_user_t   users[AUTH_MAX_USERS];
    uint8_t       user_count;
    auth_folder_t folders[AUTH_MAX_FOLDERS];
    uint8_t       folder_count;
    int8_t        current_user;         /* -1 = guest */
    uint8_t       loaded;
} auth_t;

static auth_t auth_g;

/* ============================================================================
 * 工具函数
 * ============================================================================ */

/* PIN 简单哈希：FNV-1a 32bit + 固定盐 */
static uint32_t Auth_PinHash(const char *pin)
{
    uint32_t h = 2166136261u;
    const char *salt = "WCHAUTH:";
    const char *p;

    for (p = salt; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    for (p = pin; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    return h;
}

/* 路径规范化：'/'→'\'，ASCII 大写，去除末尾 '\'（根目录除外） */
static void Auth_NormPath(char *path)
{
    uint16_t len;
    char *p;

    for (p = path; *p; p++) {
        if (*p == '/') *p = '\\';
        else if (*p >= 'a' && *p <= 'z') *p -= ('a' - 'A');
    }
    len = strlen(path);
    while (len > 1 && path[len - 1] == '\\') {
        path[--len] = '\0';
    }
}

/* NFC 卡号：10 位十六进制字符串 <-> 5 字节 */
static int Auth_HexToCard(const char *hex10, uint8_t card[AUTH_NFC_CARD_LEN])
{
    uint8_t i;

    if (strlen(hex10) != 10)
        return -1;
    for (i = 0; i < AUTH_NFC_CARD_LEN; i++) {
        uint8_t hi = (uint8_t)hex10[i * 2];
        uint8_t lo = (uint8_t)hex10[i * 2 + 1];
        uint8_t v;

        hi = (hi >= '0' && hi <= '9') ? hi - '0' :
             (hi >= 'A' && hi <= 'F') ? hi - 'A' + 10 :
             (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10 : 0xFF;
        lo = (lo >= '0' && lo <= '9') ? lo - '0' :
             (lo >= 'A' && lo <= 'F') ? lo - 'A' + 10 :
             (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10 : 0xFF;
        if (hi > 0x0F || lo > 0x0F)
            return -1;
        v = (uint8_t)((hi << 4) | lo);
        card[i] = v;
    }
    return 0;
}

static void Auth_CardToHex(const uint8_t card[AUTH_NFC_CARD_LEN], char out[11])
{
    static const char hexd[] = "0123456789ABCDEF";
    uint8_t i;

    for (i = 0; i < AUTH_NFC_CARD_LEN; i++) {
        out[i * 2]     = hexd[(card[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hexd[card[i] & 0x0F];
    }
    out[10] = '\0';
}

/* ============================================================================
 * 持久化：user.json 加载 / 保存
 * ============================================================================ */

static void Auth_Load(void)
{
    cJSON *root = (cJSON *)Config_LoadFile(AUTH_USER_FILE);
    cJSON *users, *priv, *item;

    memset(&auth_g, 0, sizeof(auth_g));
    auth_g.current_user = -1;

    if (root == NULL) {
        /* 文件不存在：设备匹配时视为空配置（loaded=1），首次保存时创建文件；
         * 设备不匹配时 loaded=0，变更操作被拒绝，路径检查全部放行 */
        if (Config_IsDeviceMatch())
            auth_g.loaded = 1;
        return;
    }

    users = cJSON_GetObjectItem(root, "users");
    if (cJSON_IsArray(users)) {
        cJSON_ArrayForEach(item, users) {
            auth_user_t *u;
            cJSON *sub, *cred;

            if (auth_g.user_count >= AUTH_MAX_USERS)
                break;
            sub = cJSON_GetObjectItem(item, "name");
            if (!cJSON_IsString(sub) || sub->valuestring[0] == '\0')
                continue;

            u = &auth_g.users[auth_g.user_count];
            strncpy(u->name, sub->valuestring, AUTH_NAME_LEN);
            u->name[AUTH_NAME_LEN] = '\0';

            sub = cJSON_GetObjectItem(item, "pin");
            if (cJSON_IsString(sub))
                u->pin_hash = (uint32_t)strtoul(sub->valuestring, NULL, 16);

            cred = cJSON_GetObjectItem(item, "fp");
            if (cJSON_IsArray(cred)) {
                cJSON *id;
                cJSON_ArrayForEach(id, cred) {
                    if (u->fp_count >= AUTH_MAX_FP_IDS)
                        break;
                    if (cJSON_IsNumber(id))
                        u->fp_ids[u->fp_count++] = (uint8_t)id->valueint;
                }
            }

            cred = cJSON_GetObjectItem(item, "nfc");
            if (cJSON_IsArray(cred)) {
                cJSON *hex;
                cJSON_ArrayForEach(hex, cred) {
                    if (u->nfc_count >= AUTH_MAX_NFC_CARDS)
                        break;
                    if (cJSON_IsString(hex) &&
                        Auth_HexToCard(hex->valuestring,
                                       u->nfc_cards[u->nfc_count]) == 0)
                        u->nfc_count++;
                }
            }

            auth_g.user_count++;
        }
    }

    priv = cJSON_GetObjectItem(root, "private");
    if (cJSON_IsArray(priv)) {
        cJSON_ArrayForEach(item, priv) {
            auth_folder_t *f;
            cJSON *sub, *acl, *name;

            if (auth_g.folder_count >= AUTH_MAX_FOLDERS)
                break;
            sub = cJSON_GetObjectItem(item, "path");
            if (!cJSON_IsString(sub) || sub->valuestring[0] == '\0')
                continue;

            f = &auth_g.folders[auth_g.folder_count];
            strncpy(f->path, sub->valuestring, AUTH_PATH_LEN);
            f->path[AUTH_PATH_LEN] = '\0';
            Auth_NormPath(f->path);

            acl = cJSON_GetObjectItem(item, "acl");
            if (cJSON_IsArray(acl)) {
                cJSON_ArrayForEach(name, acl) {
                    int ui;
                    if (!cJSON_IsString(name))
                        continue;
                    ui = Auth_FindUser(name->valuestring);
                    if (ui >= 0)
                        f->acl_mask |= (1u << ui);
                }
            }

            auth_g.folder_count++;
        }
    }

    cJSON_Delete(root);
    auth_g.loaded = 1;
}

uint8_t Auth_Save(void)
{
    cJSON *root, *users, *priv;
    uint8_t i, j;
    uint8_t status;

    if (!auth_g.loaded)
        return AUTH_ERR_SAVE;

    root = cJSON_CreateObject();
    if (root == NULL)
        return AUTH_ERR_SAVE;

    users = cJSON_CreateArray();
    for (i = 0; i < auth_g.user_count; i++) {
        const auth_user_t *u = &auth_g.users[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON *arr;
        char hex[16];

        snprintf(hex, sizeof(hex), "%08lX", (unsigned long)u->pin_hash);
        cJSON_AddStringToObject(obj, "name", u->name);
        cJSON_AddStringToObject(obj, "pin", hex);

        arr = cJSON_CreateArray();
        for (j = 0; j < u->fp_count; j++)
            cJSON_AddItemToArray(arr, cJSON_CreateNumber(u->fp_ids[j]));
        cJSON_AddItemToObject(obj, "fp", arr);

        arr = cJSON_CreateArray();
        for (j = 0; j < u->nfc_count; j++) {
            char card_hex[11];
            Auth_CardToHex(u->nfc_cards[j], card_hex);
            cJSON_AddItemToArray(arr, cJSON_CreateString(card_hex));
        }
        cJSON_AddItemToObject(obj, "nfc", arr);

        cJSON_AddItemToArray(users, obj);
    }
    cJSON_AddItemToObject(root, "users", users);

    priv = cJSON_CreateArray();
    for (i = 0; i < auth_g.folder_count; i++) {
        const auth_folder_t *f = &auth_g.folders[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();

        cJSON_AddStringToObject(obj, "path", f->path);
        for (j = 0; j < auth_g.user_count; j++) {
            if (f->acl_mask & (1u << j))
                cJSON_AddItemToArray(arr,
                                     cJSON_CreateString(auth_g.users[j].name));
        }
        cJSON_AddItemToObject(obj, "acl", arr);
        cJSON_AddItemToArray(priv, obj);
    }
    cJSON_AddItemToObject(root, "private", priv);

    status = Config_SaveFile(AUTH_USER_FILE, root);
    cJSON_Delete(root);

    return (status == ERR_SUCCESS) ? AUTH_OK : AUTH_ERR_SAVE;
}

/* ============================================================================
 * 生命周期
 * ============================================================================ */

void Auth_Init(void)
{
    Auth_Load();
}

uint8_t Auth_IsLoaded(void)
{
    return auth_g.loaded;
}

/* ============================================================================
 * 登录会话
 * ============================================================================ */

int8_t Auth_CurrentUser(void)
{
    return auth_g.current_user;
}

const char *Auth_CurrentUserName(void)
{
    if (auth_g.current_user < 0)
        return "";
    return auth_g.users[auth_g.current_user].name;
}

int Auth_FindUser(const char *name)
{
    uint8_t i;

    for (i = 0; i < auth_g.user_count; i++) {
        if (strcmp(auth_g.users[i].name, name) == 0)
            return i;
    }
    return -1;
}

/* 设置当前用户并推送 Display */
static void Auth_SetCurrent(int8_t idx)
{
    if (auth_g.current_user == idx)
        return;
    auth_g.current_user = idx;
    Auth_ReportStatusToDisplay();
}

int Auth_Login(const char *name, const char *pin)
{
    int idx = Auth_FindUser(name);

    if (idx < 0)
        return AUTH_ERR_NOT_FOUND;
    if (auth_g.users[idx].pin_hash != Auth_PinHash(pin))
        return AUTH_ERR_AUTH;

    Auth_SetCurrent((int8_t)idx);
    return idx;
}

void Auth_Logout(void)
{
    Auth_SetCurrent(-1);
}

int Auth_LoginByFp(uint8_t fp_id)
{
    uint8_t i, j;

    for (i = 0; i < auth_g.user_count; i++) {
        for (j = 0; j < auth_g.users[i].fp_count; j++) {
            if (auth_g.users[i].fp_ids[j] == fp_id) {
                Auth_SetCurrent((int8_t)i);
                return i;
            }
        }
    }
    return -1;
}

int Auth_LoginByNfc(const uint8_t card[AUTH_NFC_CARD_LEN])
{
    uint8_t i, j;

    for (i = 0; i < auth_g.user_count; i++) {
        for (j = 0; j < auth_g.users[i].nfc_count; j++) {
            if (memcmp(auth_g.users[i].nfc_cards[j], card,
                       AUTH_NFC_CARD_LEN) == 0) {
                Auth_SetCurrent((int8_t)i);
                return i;
            }
        }
    }
    return -1;
}

/* ============================================================================
 * 路径访问检查
 * ============================================================================ */

uint8_t Auth_CheckPath(const char *abs_path)
{
    char norm[AUTH_PATH_LEN + 1];
    uint8_t i;

    if (!auth_g.loaded || auth_g.folder_count == 0)
        return 1;
    if (abs_path == NULL || abs_path[0] == '\0')
        return 1;

    strncpy(norm, abs_path, AUTH_PATH_LEN);
    norm[AUTH_PATH_LEN] = '\0';
    Auth_NormPath(norm);

    for (i = 0; i < auth_g.folder_count; i++) {
        uint16_t flen = strlen(auth_g.folders[i].path);

        if (strncmp(norm, auth_g.folders[i].path, flen) == 0 &&
            (norm[flen] == '\0' || norm[flen] == '\\')) {
            /* 命中私密文件夹：guest 一律拒绝 */
            if (auth_g.current_user < 0)
                return 0;
            return (auth_g.folders[i].acl_mask >> auth_g.current_user) & 1u;
        }
    }
    return 1;
}

/* ============================================================================
 * 用户管理
 * ============================================================================ */

int Auth_UserAdd(const char *name, const char *pin)
{
    auth_user_t *u;
    uint16_t name_len;

    if (!auth_g.loaded)
        return AUTH_ERR_SAVE;
    if (name == NULL || pin == NULL)
        return AUTH_ERR_BAD_PARAM;
    name_len = strlen(name);
    if (name_len == 0 || name_len > AUTH_NAME_LEN)
        return AUTH_ERR_BAD_PARAM;
    if (Auth_FindUser(name) >= 0)
        return AUTH_ERR_EXISTS;
    if (auth_g.user_count >= AUTH_MAX_USERS)
        return AUTH_ERR_FULL;

    u = &auth_g.users[auth_g.user_count];
    memset(u, 0, sizeof(*u));
    strcpy(u->name, name);
    u->pin_hash = Auth_PinHash(pin);
    auth_g.user_count++;

    return (Auth_Save() == AUTH_OK) ? (int)(auth_g.user_count - 1)
                                    : AUTH_ERR_SAVE;
}

int Auth_UserDel(const char *name)
{
    int idx = Auth_FindUser(name);
    uint8_t i;

    if (idx < 0)
        return AUTH_ERR_NOT_FOUND;

    /* 从所有文件夹 ACL 中移除该用户，并将高位用户位下移 */
    for (i = 0; i < auth_g.folder_count; i++) {
        uint32_t low  = auth_g.folders[i].acl_mask & ((1u << idx) - 1u);
        uint32_t high = auth_g.folders[i].acl_mask >> (idx + 1);
        auth_g.folders[i].acl_mask = low | (high << idx);
    }

    /* 压缩用户表 */
    for (i = (uint8_t)idx; i + 1 < auth_g.user_count; i++)
        auth_g.users[i] = auth_g.users[i + 1];
    auth_g.user_count--;

    /* 修正当前登录索引 */
    if (auth_g.current_user == idx)
        Auth_SetCurrent(-1);
    else if (auth_g.current_user > idx)
        auth_g.current_user--;

    return (Auth_Save() == AUTH_OK) ? AUTH_OK : AUTH_ERR_SAVE;
}

int Auth_UserSetPin(const char *name, const char *pin)
{
    int idx = Auth_FindUser(name);

    if (idx < 0)
        return AUTH_ERR_NOT_FOUND;
    if (pin == NULL || pin[0] == '\0')
        return AUTH_ERR_BAD_PARAM;

    auth_g.users[idx].pin_hash = Auth_PinHash(pin);
    return (Auth_Save() == AUTH_OK) ? AUTH_OK : AUTH_ERR_SAVE;
}

int Auth_BindFp(const char *name, uint8_t fp_id, uint8_t bind)
{
    int idx = Auth_FindUser(name);
    auth_user_t *u;
    uint8_t i;

    if (idx < 0)
        return AUTH_ERR_NOT_FOUND;
    u = &auth_g.users[idx];

    if (bind) {
        uint8_t j;

        for (i = 0; i < u->fp_count; i++)
            if (u->fp_ids[i] == fp_id)
                return AUTH_ERR_EXISTS;
        /* 唯一性：一个指纹 ID 只能绑定到一个用户 */
        for (i = 0; i < auth_g.user_count; i++) {
            if (i == (uint8_t)idx)
                continue;
            for (j = 0; j < auth_g.users[i].fp_count; j++)
                if (auth_g.users[i].fp_ids[j] == fp_id)
                    return AUTH_ERR_CONFLICT;
        }
        if (u->fp_count >= AUTH_MAX_FP_IDS)
            return AUTH_ERR_FULL;
        u->fp_ids[u->fp_count++] = fp_id;
    } else {
        for (i = 0; i < u->fp_count; i++) {
            if (u->fp_ids[i] == fp_id)
                break;
        }
        if (i >= u->fp_count)
            return AUTH_ERR_NOT_FOUND;
        for (; i + 1 < u->fp_count; i++)
            u->fp_ids[i] = u->fp_ids[i + 1];
        u->fp_count--;
    }

    return (Auth_Save() == AUTH_OK) ? AUTH_OK : AUTH_ERR_SAVE;
}

int Auth_BindNfc(const char *name, const char *hex10, uint8_t bind)
{
    int idx = Auth_FindUser(name);
    auth_user_t *u;
    uint8_t card[AUTH_NFC_CARD_LEN];
    uint8_t i;

    if (idx < 0)
        return AUTH_ERR_NOT_FOUND;
    if (Auth_HexToCard(hex10, card) != 0)
        return AUTH_ERR_BAD_PARAM;
    u = &auth_g.users[idx];

    if (bind) {
        uint8_t j;

        for (i = 0; i < u->nfc_count; i++)
            if (memcmp(u->nfc_cards[i], card, AUTH_NFC_CARD_LEN) == 0)
                return AUTH_ERR_EXISTS;
        /* 唯一性：一张 NFC 卡只能绑定到一个用户 */
        for (i = 0; i < auth_g.user_count; i++) {
            if (i == (uint8_t)idx)
                continue;
            for (j = 0; j < auth_g.users[i].nfc_count; j++)
                if (memcmp(auth_g.users[i].nfc_cards[j], card,
                           AUTH_NFC_CARD_LEN) == 0)
                    return AUTH_ERR_CONFLICT;
        }
        if (u->nfc_count >= AUTH_MAX_NFC_CARDS)
            return AUTH_ERR_FULL;
        memcpy(u->nfc_cards[u->nfc_count++], card, AUTH_NFC_CARD_LEN);
    } else {
        for (i = 0; i < u->nfc_count; i++) {
            if (memcmp(u->nfc_cards[i], card, AUTH_NFC_CARD_LEN) == 0)
                break;
        }
        if (i >= u->nfc_count)
            return AUTH_ERR_NOT_FOUND;
        for (; i + 1 < u->nfc_count; i++)
            memcpy(u->nfc_cards[i], u->nfc_cards[i + 1], AUTH_NFC_CARD_LEN);
        u->nfc_count--;
    }

    return (Auth_Save() == AUTH_OK) ? AUTH_OK : AUTH_ERR_SAVE;
}

/* ============================================================================
 * 私密文件夹管理
 * ============================================================================ */

static int Auth_FindFolder(const char *norm_path)
{
    uint8_t i;

    for (i = 0; i < auth_g.folder_count; i++) {
        if (strcmp(auth_g.folders[i].path, norm_path) == 0)
            return i;
    }
    return -1;
}

int Auth_PrivateAdd(const char *abs_path, uint32_t acl_mask)
{
    auth_folder_t *f;
    char norm[AUTH_PATH_LEN + 1];

    if (!auth_g.loaded)
        return AUTH_ERR_SAVE;
    if (abs_path == NULL || abs_path[0] != '\\')
        return AUTH_ERR_BAD_PARAM;

    strncpy(norm, abs_path, AUTH_PATH_LEN);
    norm[AUTH_PATH_LEN] = '\0';
    Auth_NormPath(norm);

    if (Auth_FindFolder(norm) >= 0)
        return AUTH_ERR_EXISTS;
    if (auth_g.folder_count >= AUTH_MAX_FOLDERS)
        return AUTH_ERR_FULL;

    f = &auth_g.folders[auth_g.folder_count];
    strcpy(f->path, norm);
    f->acl_mask = acl_mask;
    auth_g.folder_count++;

    return (Auth_Save() == AUTH_OK) ? AUTH_OK : AUTH_ERR_SAVE;
}

int Auth_PrivateRemove(const char *abs_path)
{
    char norm[AUTH_PATH_LEN + 1];
    int idx;
    uint8_t i;

    if (abs_path == NULL)
        return AUTH_ERR_BAD_PARAM;

    strncpy(norm, abs_path, AUTH_PATH_LEN);
    norm[AUTH_PATH_LEN] = '\0';
    Auth_NormPath(norm);

    idx = Auth_FindFolder(norm);
    if (idx < 0)
        return AUTH_ERR_NOT_FOUND;

    for (i = (uint8_t)idx; i + 1 < auth_g.folder_count; i++)
        auth_g.folders[i] = auth_g.folders[i + 1];
    auth_g.folder_count--;

    return (Auth_Save() == AUTH_OK) ? AUTH_OK : AUTH_ERR_SAVE;
}

/* ============================================================================
 * 遍历接口
 * ============================================================================ */

uint8_t Auth_UserCount(void)
{
    return auth_g.user_count;
}

const char *Auth_UserName(uint8_t idx)
{
    if (idx >= auth_g.user_count)
        return "";
    return auth_g.users[idx].name;
}

uint8_t Auth_UserFpCount(uint8_t idx)
{
    if (idx >= auth_g.user_count)
        return 0;
    return auth_g.users[idx].fp_count;
}

uint8_t Auth_UserFpId(uint8_t idx, uint8_t i)
{
    if (idx >= auth_g.user_count || i >= auth_g.users[idx].fp_count)
        return 0;
    return auth_g.users[idx].fp_ids[i];
}

uint8_t Auth_UserNfcCount(uint8_t idx)
{
    if (idx >= auth_g.user_count)
        return 0;
    return auth_g.users[idx].nfc_count;
}

void Auth_UserNfcHex(uint8_t idx, uint8_t i, char out[11])
{
    if (idx >= auth_g.user_count || i >= auth_g.users[idx].nfc_count) {
        out[0] = '\0';
        return;
    }
    Auth_CardToHex(auth_g.users[idx].nfc_cards[i], out);
}

uint8_t Auth_FolderCount(void)
{
    return auth_g.folder_count;
}

const char *Auth_FolderPath(uint8_t idx)
{
    if (idx >= auth_g.folder_count)
        return "";
    return auth_g.folders[idx].path;
}

uint32_t Auth_FolderAclMask(uint8_t idx)
{
    if (idx >= auth_g.folder_count)
        return 0;
    return auth_g.folders[idx].acl_mask;
}

/* ============================================================================
 * Display 状态推送
 * ============================================================================ */

void Auth_ReportStatusToDisplay(void)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t data[2 + AUTH_NAME_LEN];
    uint8_t name_len = 0;
    uint16_t frame_len;

    if (display_ptr == NULL || !display_ptr->type_received)
        return;

    data[0] = CMD_DISP_EXT_USER_STATUS;
    data[1] = (auth_g.current_user >= 0) ? 1 : 0;
    if (auth_g.current_user >= 0) {
        const char *name = auth_g.users[auth_g.current_user].name;
        name_len = (uint8_t)strlen(name);
        if (name_len > AUTH_NAME_LEN)
            name_len = AUTH_NAME_LEN;
        memcpy(&data[2], name, name_len);
    }

    frame_len = Protocol_PackFrame(MODULE_ID_CORE, MODULE_ID_DISPLAY,
                                   CMD_DISP_EXTENSION,
                                   data, (uint8_t)(2 + name_len),
                                   buf, sizeof(buf));
    if (frame_len > 0)
        Display_Send_Data(display_ptr, buf, frame_len);
}
