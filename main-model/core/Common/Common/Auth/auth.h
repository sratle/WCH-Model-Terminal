#ifndef __AUTH_H
#define __AUTH_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"

/* ============================================================================
 * 用户系统与私密文件夹访问控制
 *
 * 数据源：\CONFIG\user.json（Config 数据文件机制）
 *   {
 *     "users":   [{"name":"Alice","pin":"8hex","fp":[1,2],"nfc":["0A1A3BAC20"]}],
 *     "private": [{"path":"\\PRIVATE\\DIARY","acl":["Alice","Bob"]}]
 *   }
 *
 * 用户是唯一实体，指纹 ID / NFC 卡号只是绑定在用户身上的凭据。
 * 登录状态持久保持（无会话超时），仅 logout 或其他用户登录时切换。
 * 私密文件夹为软门禁：CLI 层路径前缀拦截，guest 一律拒绝。
 * ============================================================================ */

#define AUTH_USER_FILE       "user.json"

#define AUTH_MAX_USERS       8
#define AUTH_NAME_LEN        16
#define AUTH_MAX_FP_IDS      8
#define AUTH_MAX_NFC_CARDS   4
#define AUTH_NFC_CARD_LEN    5
#define AUTH_MAX_FOLDERS     8
#define AUTH_PATH_LEN        96

/* 错误码 */
#define AUTH_OK              0
#define AUTH_ERR_NOT_FOUND   (-1)   /* 用户/文件夹/凭据不存在 */
#define AUTH_ERR_EXISTS      (-2)   /* 用户/文件夹已存在 */
#define AUTH_ERR_FULL        (-3)   /* 超出容量上限 */
#define AUTH_ERR_BAD_PARAM   (-4)   /* 参数非法 */
#define AUTH_ERR_AUTH        (-5)   /* PIN 校验失败 */
#define AUTH_ERR_NO_USER     (-6)   /* 未登录（guest） */
#define AUTH_ERR_SAVE        (-7)   /* 落盘失败 */
#define AUTH_ERR_CONFLICT    (-8)   /* 凭据已绑定到其他用户 */

/* 初始化：从 user.json 加载缓存（需在 Config_Init 之后调用） */
void Auth_Init(void);

/* 缓存是否已加载（设备不匹配或文件缺失时为 0，所有路径检查放行） */
uint8_t Auth_IsLoaded(void);

/* 当前登录用户索引，-1 = guest 未登录 */
int8_t Auth_CurrentUser(void);

/* 当前登录用户名，guest 返回 "" */
const char *Auth_CurrentUserName(void);

/* 按名查找用户，返回索引或 -1 */
int Auth_FindUser(const char *name);

/* PIN 登录（CLI 唯一登录途径）。成功返回用户索引 */
int Auth_Login(const char *name, const char *pin);

/* 登出（回到 guest） */
void Auth_Logout(void);

/* 指纹/NFC 凭据登录（任何时候生效，相当于快速切换用户）。
 * 命中返回用户索引，未命中返回 -1（不改变当前登录状态） */
int Auth_LoginByFp(uint8_t fp_id);
int Auth_LoginByNfc(const uint8_t card[AUTH_NFC_CARD_LEN]);

/* 路径访问检查。
 * @param  abs_path  绝对路径（'\' 开头，大小写不敏感）
 * @return 1=允许访问，0=拒绝（命中私密文件夹且当前用户不在 ACL） */
uint8_t Auth_CheckPath(const char *abs_path);

/* ---- 用户管理（变更后立即落盘） ---- */
int Auth_UserAdd(const char *name, const char *pin);
int Auth_UserDel(const char *name);
int Auth_UserSetPin(const char *name, const char *pin);
int Auth_BindFp(const char *name, uint8_t fp_id, uint8_t bind);
int Auth_BindNfc(const char *name, const char *hex10, uint8_t bind);

/* ---- 私密文件夹管理 ----
 * acl_mask: bit i 对应用户索引 i；Auth_PrivateAdd 时按当前用户表快照 */
int Auth_PrivateAdd(const char *abs_path, uint32_t acl_mask);
int Auth_PrivateRemove(const char *abs_path);

/* ---- 遍历（供 CLI 列表显示） ---- */
uint8_t     Auth_UserCount(void);
const char *Auth_UserName(uint8_t idx);
uint8_t     Auth_UserFpCount(uint8_t idx);
uint8_t     Auth_UserFpId(uint8_t idx, uint8_t i);
uint8_t     Auth_UserNfcCount(uint8_t idx);
void        Auth_UserNfcHex(uint8_t idx, uint8_t i, char out[11]);

uint8_t     Auth_FolderCount(void);
const char *Auth_FolderPath(uint8_t idx);
uint32_t    Auth_FolderAclMask(uint8_t idx);

/* 将当前登录状态推送给 Display（USER_STATUS 扩展帧，发送即忘） */
void Auth_ReportStatusToDisplay(void);

#ifdef __cplusplus
}
#endif

#endif /* __AUTH_H */
