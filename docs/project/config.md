# 设置/数据持久化系统设计

## 1. 概述

本系统为 Core 提供统一的设置持久化与数据文件管理能力，基于 CH378 文件管理芯片将 JSON 文件存储在 TF 卡或 U 盘上。系统支持：

- **启动时加载**：系统配置（`config.json`）在 V5F 初始化阶段加载到内存
- **按需加载**：数据文件（游戏存档、电子书进度等）仅在应用请求时读取
- **宏切换存储设备**：通过 `config.h` 宏定义切换 TF 卡 / U 盘
- **CLI 调试**：通过串口 CLI 命令查看和修改配置

**核心原则——配置不可动态扩展**：

- 所有配置文件和配置项必须由开发者手动预先创建（通过 CLI `touch`/`echo` 或在 TF 卡上直接编辑）
- `config get` 访问不存在的文件或键 → 返回 `Error: not found`
- `config set` 写入不存在的文件或键 → 返回 `Error: not found`，不会自动创建
- 唯一例外：`Config_Init()` 首次运行时若 `config.json` 不存在，会创建包含所有预定义模块段和键的默认配置文件

---

## 2. 存储布局

### 2.1 目录结构

CH378 下挂存储介质根目录下的 `\CONFIG` 目录为持久化根路径：

```
\CONFIG\
├── config.json           # 系统配置（启动时加载）
├── game.json             # 游戏数据（按需加载）
├── ebook.json            # 电子书进度（按需加载）
├── piano.json            # 电子琴数据（按需加载）
└── ...                   # 其他应用数据文件
```

### 2.2 config.json 结构

`config.json` 是系统核心配置文件，按**模块类型-子类型**（`TTSSTT`，即协议中 `module_identity_t` 的 `type` + `subtype` 各两位十六进制）组织顶层键：

```json
{
  "version": 1,
  "0000": {
    "volume": 50,
    "audio_mode": 0,
    "screen_timeout": 30
  },
  "0101": {
    "brightness": 80,
    "rotation": 0
  },
  "0102": {
    "brightness": 50,
    "rotation": 0
  },
  "0201": {
    "discoverable": 0
  },
  "0301": {
    "charge_policy": 0,
    "output_policy": 0,
    "alarm_threshold": 15
  },
  "0401": {
    "backlight": 50
  },
  "0402": {
    "backlight": 50
  },
  "0403": {
    "backlight": 50
  },
  "0501": {},
  "0505": {
    "rgb_mode": 0
  }
}
```

#### 键名规则

| 键名 | 含义 | 对应模块 |
|------|------|----------|
| `0000` | Core 系统配置 | Core（type=0x00, subtype=0x00） |
| `TTSS` | 模块配置 | type=0xTT, subtype=0xSS |

模块类型-子类型编码对照：

| 编码 | 模块 | 说明 |
|------|------|------|
| `0000` | Core | 核心系统配置 |
| `0101` | Display-LCD | LCD 显示模块 |
| `0102` | Display-Eink | 墨水屏显示模块 |
| `0201` | Wireless-BT | 蓝牙无线模块 |
| `0301` | Power-Standard | 标准供电模块 |
| `0401` | Keyboard-Main | 主键盘 |
| `0402` | Keyboard-Game | 游戏键盘 |
| `0403` | Keyboard-Music | 音乐键盘 |
| `0501` | Submodel-Fingerprint | 指纹识别 |
| `0502` | Submodel-Health | 健康监测 |
| `0503` | Submodel-NFC | NFC 读卡 |
| `0504` | Submodel-TouchRing | 触摸圆环/旋钮 |
| `0505` | Submodel-RGB | RGB 点阵 |
| `0506` | Submodel-Infrared | 红外测距 |
| `0507` | Submodel-SubDisplay | 副屏显示 |

> 编码规则：高两位 = `MODULE_TYPE_xxx`，低两位 = `MODULE_SUBTYPE_xxx`，均以两位十六进制表示。

### 2.3 数据文件结构

数据文件（如 `game.json`、`ebook.json`）由各应用自行定义内部结构，Config 模块仅提供通用的 JSON 文件读写 API，不做结构约束。示例：

```json
// game.json
{
  "high_score": 1000,
  "current_level": 5,
  "save_slot_1": "base64_data..."
}
```

```json
// ebook.json
{
  "current_book": "\\DOC\\novel.txt",
  "position": 4096,
  "bookmarks": [1024, 8192, 32768]
}
```

---

## 3. 宏定义与编译配置

在 `config.h` 中通过宏定义控制存储行为：

```c
/* 存储设备选择：CH378_Device_TF 或 CH378_Device_USB */
#define CONFIG_STORAGE_DEVICE   CH378_Device_TF

/* 持久化根目录（CH378 短文件名路径） */
#define CONFIG_ROOT_DIR         "\\CONFIG"

/* 系统配置文件名 */
#define CONFIG_MAIN_FILE        "config.json"

/* 单个 JSON 文件最大尺寸（字节），受限于 RAM */
#define CONFIG_MAX_FILE_SIZE    4096

/* 配置版本号，用于格式升级兼容 */
#define CONFIG_VERSION          1
```

切换存储设备只需修改 `CONFIG_STORAGE_DEVICE` 宏，Config 模块在初始化时会自动调用 `CH378_Device_Select()` 切换到指定设备。

---

## 4. 数据结构

### 4.1 内存中的配置缓存

`config.json` 在启动时整体加载到内存，运行时修改内存中的 cJSON 对象，保存时序列化写回文件：

```c
/* config.c 内部变量 */
static cJSON *config_root = NULL;   /* config.json 的 cJSON 根对象 */
static uint8_t config_dirty = 0;    /* 脏标记：有未写回的变更 */
```

不使用独立的 C 结构体缓存配置——直接操作 cJSON 对象，避免结构体与 JSON 之间的同步负担。各模块通过 `Config_Get*` / `Config_Set*` API 访问配置值，内部从 `config_root` 中按路径查找。

### 4.2 模块键名辅助宏

```c
/* 将模块类型和子类型组合为配置键 "TTSS" */
#define CONFIG_MODULE_KEY(type, subtype) \
    ((const char[]){ \
        '0' + ((type) >> 4), '0' + ((type) & 0x0F), \
        '0' + ((subtype) >> 4), '0' + ((subtype) & 0x0F), \
        '\0' \
    })

/* Core 配置键 */
#define CONFIG_CORE_KEY   "0000"
```

> 注：由于模块类型和子类型均为 0x00~0x05 范围，高位始终为 0，因此直接用 `'0' + value` 即可生成对应的十六进制字符。若未来类型值超过 0x09，需改用标准 hex 转换。

---

## 5. API 设计

### 5.1 初始化与生命周期

```c
/**
 * @brief  初始化配置系统
 *         - 选择存储设备并挂载
 *         - 确保 \CONFIG 目录存在
 *         - 加载 config.json（仅此文件：不存在时创建默认配置）
 *         - 将配置应用到各模块
 * @note   其他数据文件不会自动创建，需手动预先建立
 */
void Config_Init(void);

/**
 * @brief  将脏配置写回 config.json
 * @return ERR_SUCCESS 或 CH378 错误码
 */
uint8_t Config_Save(void);

/**
 * @brief  将当前 config.json 备份为 config.bak（覆盖已有备份）
 * @return ERR_SUCCESS 或 CH378 错误码
 */
uint8_t Config_Backup(void);

/**
 * @brief  恢复默认配置并保存
 */
void Config_ResetDefaults(void);

/**
 * @brief  反初始化，释放 cJSON 对象
 */
void Config_Deinit(void);
```

### 5.2 配置读取（config.json）

```c
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
```

### 5.3 配置写入（config.json）

```c
/**
 * @brief  向 config.json 的指定模块段写入整数
 * @param  module_key  模块键名
 * @param  key         配置项键名
 * @param  value       配置值
 * @return 0=成功，-1=模块键不存在，-2=配置项不存在（不会自动创建）
 */
int Config_SetInt(const char *module_key, const char *key, int value);

/**
 * @brief  向 config.json 的指定模块段写入字符串
 * @param  module_key  模块键名
 * @param  key         配置项键名
 * @param  value       配置值（会被 cJSON 内部复制）
 * @return 0=成功，-1=模块键不存在，-2=配置项不存在（不会自动创建）
 */
int Config_SetString(const char *module_key, const char *key, const char *value);
```

> `Config_Set*` 函数仅修改已存在的配置项，不会自动创建新的模块段或键。修改成功后标记 dirty，不会立即写盘。需显式调用 `Config_Save()` 或由主循环定期保存。

### 5.4 数据文件读写（按需加载）

```c
/**
 * @brief  从 CONFIG 目录读取 JSON 文件并解析
 * @param  filename  文件名，如 "game.json"
 * @return cJSON 对象指针（调用者负责 cJSON_Delete）；文件不存在返回 NULL
 */
cJSON* Config_LoadFile(const char *filename);

/**
 * @brief  将 cJSON 对象序列化并写入 CONFIG 目录（覆盖已有文件）
 * @param  filename  文件名
 * @param  json      cJSON 对象指针
 * @return ERR_SUCCESS 或 CH378 错误码
 * @note   文件不存在时会创建，但调用者应确保文件已预先存在
 */
uint8_t Config_SaveFile(const char *filename, cJSON *json);

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
 * @param  filename  文件名
 * @param  key       配置项键名（支持点分路径）
 * @param  value     配置值
 * @return 0=成功，-1=文件不存在，-2=键不存在（不会自动创建）
 * @note   会读取→修改→写回整个文件
 */
int Config_SetFileInt(const char *filename, const char *key, int value);

/**
 * @brief  从指定 JSON 文件中读取一个字符串配置项
 * @param  filename  文件名
 * @param  key       配置项键名
 * @param  out_val   输出配置值字符串指针
 * @return 0=成功，-1=文件不存在，-2=键不存在
 */
int Config_GetFileString(const char *filename, const char *key, const char **out_val);

/**
 * @brief  向指定 JSON 文件中写入一个字符串配置项
 * @param  filename  文件名
 * @param  key       配置项键名
 * @param  value     配置值
 * @return 0=成功，-1=文件不存在，-2=键不存在（不会自动创建）
 */
int Config_SetFileString(const char *filename, const char *key, const char *value);
```

### 5.5 配置项与文件创建（显式操作）

```c
/**
 * @brief  在 config.json 的指定模块段中新增一个整数配置项
 * @param  module_key  模块键名（不存在则自动创建空段）
 * @param  key         配置项键名（已存在则返回错误）
 * @param  value       初始值
 * @return 0=成功，-2=键已存在
 */
int Config_AddKeyInt(const char *module_key, const char *key, int value);

/**
 * @brief  在 config.json 的指定模块段中新增一个字符串配置项
 * @param  module_key  模块键名（不存在则自动创建空段）
 * @param  key         配置项键名（已存在则返回错误）
 * @param  value       初始值
 * @return 0=成功，-2=键已存在
 */
int Config_AddKeyString(const char *module_key, const char *key, const char *value);

/**
 * @brief  在指定数据文件中新增一个整数配置项
 * @param  filename  文件名
 * @param  key       配置项键名（已存在则返回错误）
 * @param  value     初始值
 * @return 0=成功，-1=文件不存在，-2=键已存在
 */
int Config_AddFileKeyInt(const char *filename, const char *key, int value);

/**
 * @brief  在指定数据文件中新增一个字符串配置项
 * @param  filename  文件名
 * @param  key       配置项键名（已存在则返回错误）
 * @param  value     初始值
 * @return 0=成功，-1=文件不存在，-2=键已存在
 */
int Config_AddFileKeyString(const char *filename, const char *key, const char *value);

/**
 * @brief  在 CONFIG 目录下创建一个空的 JSON 数据文件（内容为 {}）
 * @param  filename  文件名（不允许为 config.json）
 * @return 0=成功，-1=文件已存在，-3=文件名为 config.json
 */
int Config_NewFile(const char *filename);
```

### 5.6 配置应用

```c
/**
 * @brief  将当前配置应用到各硬件模块
 *         - Audio_SetVolume(config_g.audio.volume)
 *         - Display_Send brightness/rotation
 *         - Keyboard_Send backlight
 *         - Power_Send charge_policy 等
 * @note   在 Config_Init() 末尾和 Config_ResetDefaults() 后调用
 */
void Config_Apply(void);
```

---

## 6. CLI 命令设计

### 6.1 命令总览

```
config get                                 获取 Core 系统配置
config get <module>                        获取指定模块的全部配置
config get <file.json>                     获取指定数据文件的全部配置
config get <file.json> <key>               获取指定数据文件中某配置项的值
config set <key> <value>                   设置 Core 配置项
config set <module> <key> <value>          设置指定模块的配置项
config set <file.json> <key> <value>       设置指定数据文件中的配置项
config addkey <module> <key> <value>       在 config.json 的指定模块段中新增配置项
config addkey <file.json> <key> <value>    在指定数据文件中新增配置项
config newfile <file.json>                 在 CONFIG 目录下创建空 JSON 数据文件
config save                                保存配置到存储设备
config backup                              备份 config.json 为 config.bak
config reset                               恢复默认配置并保存
config ls                                  列出 CONFIG 目录下所有文件
config rm <file.json>                      删除数据文件
```

### 6.2 参数解析规则

第一个参数的判断优先级：

1. **以 `.json` 结尾** → 视为文件名，操作对象为 `\CONFIG\<file>`
2. **匹配 4 位十六进制（`[0-9a-fA-F]{4}`）** → 视为模块键名，操作对象为 `config.json` 中对应段
3. **无参数** → 默认操作 Core 配置（等价于模块键 `0000`）

### 6.3 命令详细说明

#### `config get`

获取 Core 系统配置，输出 `key:value` 格式，每行一项：

```
> config get
volume:50
audio_mode:0
screen_timeout:30
```

#### `config get <module>`

获取指定模块的全部配置，`<module>` 为 4 位十六进制模块键名：

```
> config get 0101
brightness:80
rotation:0

> config get 0505
rgb_mode:0
```

若模块键不存在，输出 `Error: module not found`。

#### `config get <file.json>`

获取指定数据文件的全部配置：

```
> config get game.json
high_score:1000
current_level:5
save_slot_1:base64_data...
```

若文件不存在，输出 `Error: file not found`。

#### `config get <file.json> <key>`

获取数据文件中指定配置项的值，仅输出原始值（不带 key: 前缀）：

```
> config get game.json high_score
1000
```

支持点分路径访问嵌套键：

```
> config get game.json player.score
1000
```

若键不存在，输出 `Error: key not found`。

#### `config set <key> <value>`

设置 Core 配置项（键必须已存在）：

```
> config set volume 80
OK

> config set screen_timeout 60
OK

> config set unknown_key 1
Error: key not found
```

#### `config set <module> <key> <value>`

设置指定模块的配置项（模块键和配置项必须已存在）：

```
> config set 0101 brightness 90
OK

> config set 0505 rgb_mode 3
OK

> config set 0505 unknown_key 1
Error: key not found

> config set 9999 brightness 90
Error: module not found
```

#### `config set <file.json> <key> <value>`

设置数据文件中的配置项（文件和键必须已存在，读取→修改→写回）：

```
> config set game.json high_score 2000
OK

> config set game.json unknown_key 1
Error: key not found

> config set missing.json high_score 100
Error: file not found
```

#### `config save`

将脏配置写回存储设备：

```
> config save
Saved (128 bytes)

> config save
No changes
```

#### `config backup`

将当前 `config.json` 备份为 `config.bak`（覆盖已有备份）：

```
> config backup
Backed up (256 bytes)

> config backup
Backed up (256 bytes)
```

#### `config reset`

恢复默认配置并保存：

```
> config reset
Reset to defaults and saved
```

#### `config ls`

列出 CONFIG 目录下所有文件：

```
> config ls
config.json       256
game.json         128
ebook.json         64
```

#### `config rm <file.json>`

删除数据文件（不允许删除 `config.json`）：

```
> config rm game.json
Deleted

> config rm config.json
Cannot delete config.json
```

#### `config addkey <module> <key> <value>`

在 `config.json` 的指定模块段中新增一个配置项（显式创建，键必须不存在）：

```
> config addkey 0000 language 1
OK

> config addkey 0505 effect_speed 10
OK

> config addkey 0101 brightness 80
Error: key already exists
```

若模块键不存在，自动创建该模块段（空对象），然后添加键。

#### `config addkey <file.json> <key> <value>`

在指定数据文件中新增一个配置项（文件必须已存在，键必须不存在）：

```
> config addkey game.json player_name "Alice"
OK

> config addkey game.json high_score 500
Error: key already exists

> config addkey missing.json level 1
Error: file not found
```

#### `config newfile <file.json>`

在 CONFIG 目录下创建一个空的 JSON 数据文件（`{}`），文件必须不存在：

```
> config newfile game.json
Created

> config newfile game.json
Error: file already exists

> config newfile config.json
Error: cannot create config.json
```

---

## 7. 初始化流程

在 `Hardware_V5F_Init()` 中，CH378 初始化完成后调用 `Config_Init()`：

```
Hardware_V5F_Init()
  ├── CS43131_init()
  ├── CH378_Init()
  ├── CH378_Device_Select(TF)          ← 已有
  ├── Config_Init()                    ← 新增
  │     ├── CH378_Device_Select(CONFIG_STORAGE_DEVICE)  ← 按宏切换
  │     ├── 确保 \CONFIG 目录存在
  │     ├── 读取 config.json
  │     │     ├── 文件不存在 → 创建包含所有预定义模块段和键的默认配置并保存
  │     │     └── 文件存在 → cJSON_Parse 解析
  │     │           ├── 解析成功 → 加载到 config_root
  │     │           └── 解析失败 → printf 警告，使用默认配置重建并保存
  │     └── Config_Apply() → 将配置应用到各模块
  ├── CLI_Init()
  ├── CH585F_Init()
  └── Display_Init()
```

### 配置应用顺序

`Config_Apply()` 按以下顺序将配置下发到各模块：

1. **Audio**：`Config_GetInt("0000", "volume", &val); Audio_SetVolume(val);`
2. **Display**：发送 `CMD_DISP_SET_BRIGHTNESS`、`CMD_DISP_SET_ROTATION`
3. **Keyboard**：发送 `CMD_KBD_SET_BACKLIGHT`
4. **Power**：发送 `CMD_PWR_SET_CHARGE_POLICY`、`CMD_PWR_SET_OUTPUT_POLICY`、`CMD_PWR_SET_ALARM_THRESHOLD`
5. **Wireless**：发送 `CMD_BT_SET_DISCOVERABLE`

> 配置应用仅在对应模块在线时执行（检查 `hardware_g.hb_slots` 状态）。

---

## 8. 运行时配置变更流程

### 8.1 V5F 侧变更

```
用户操作 / Display 请求 / CLI 命令
  → Config_SetInt("0101", "brightness", 90)
  → 返回 0（成功）或 -1/-2（模块/键不存在）
  → 成功时 config_dirty = 1
  → 主循环中检查 dirty，调用 Config_Save()
  → CH378_File_Write 写回 config.json
```

### 8.2 V3F 侧变更

V3F 无法直接访问 CH378，需通过跨核机制通知 V5F：

```
V3F 收到模块配置变更事件
  → 设置共享标志位（hardware_g 中的 config_request）
  → V5F 主循环检测到标志位
  → V5F 执行 Config_Set*()
     → 成功 → Config_Save()，清除标志位
     → 失败（模块/键不存在）→ 清除标志位，printf 警告
```

共享标志位定义在 `hardware.h` 的 `hardware_t` 中扩展：

```c
typedef struct {
    /* ... 已有字段 ... */
    volatile uint8_t config_request;    /* V3F 请求 V5F 保存配置的标志 */
    uint8_t  config_module_key[5];      /* 请求保存的模块键名 "TTSS\0" */
    uint8_t  config_key[16];            /* 请求保存的配置项键名 */
    int32_t  config_value;              /* 请求保存的配置值 */
} hardware_t;
```

### 8.3 CH378 互斥

WAV 流式播放期间 CH378 被占用（`Audio_IsStreaming()` 返回 1），此时 `Config_Save()` 应跳过并保留 dirty 标记，待播放结束后再写回：

```c
uint8_t Config_Save(void)
{
    if (!config_dirty) return ERR_SUCCESS;
    if (Audio_IsStreaming()) return PROTO_ERR_BUSY;  /* 延迟保存 */
    /* ... 实际写盘 ... */
    config_dirty = 0;
    return ERR_SUCCESS;
}
```

V5F 主循环中增加定期保存检查：

```c
/* V5F main loop */
while (1) {
    Display_Process(&display_g);
    Audio_Process();
    Debug_CLI_Process();
    CH585F_BT_Poll();
    Hardware_Heartbeat();

    /* 定期检查配置保存需求 */
    if (config_dirty || hardware_g.config_request) {
        Config_Save();
    }

    Delay_Ms(1);
}
```

---

## 9. 默认配置定义

```c
/* config.c 内部 */

/* 各模块的默认配置表 */
typedef struct {
    const char *module_key;     /* "0000", "0101", ... */
    const char *key;            /* "volume", "brightness", ... */
    int default_value;          /* 默认值 */
} config_default_entry_t;

static const config_default_entry_t config_defaults[] = {
    /* Core (0000) */
    { "0000", "volume",          50  },
    { "0000", "audio_mode",      0   },
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
    { "0301", "charge_policy",   0   },
    { "0301", "output_policy",   0   },
    { "0301", "alarm_threshold", 15  },

    /* Keyboard-Main (0401) */
    { "0401", "backlight",       50  },

    /* Keyboard-Game (0402) */
    { "0402", "backlight",       50  },

    /* Keyboard-Music (0403) */
    { "0403", "backlight",       50  },

    /* Submodel-RGB (0505) */
    { "0505", "rgb_mode",        0   },
};

#define CONFIG_DEFAULT_COUNT  (sizeof(config_defaults) / sizeof(config_defaults[0]))
```

`Config_ResetDefaults()` 遍历此表重建 `config_root`。

---

## 10. 文件组织

```
Common/Common/Config/
├── config.c              # 配置系统实现
└── config.h              # 配置系统头文件（含宏定义与 API 声明）
```

集成点：
- `all_devices.h` 中添加 `#include "Config/config.h"`
- `hardware.c` 中 `Hardware_V5F_Init()` 的 CH378 初始化后调用 `Config_Init()`
- `hardware.h` 的 `hardware_t` 中扩展跨核配置请求字段
- `CLI/cli.c` 中添加 `config` 命令处理

---

## 11. 内存与性能评估

| 项目 | 估算 |
|------|------|
| config.json 文件大小 | ~500 字节（当前配置项） |
| cJSON 解析内存开销 | ~2KB（节点 + 字符串副本） |
| cJSON_PrintUnformatted 输出 | ~500 字节（需临时分配） |
| 数据文件读取缓冲区 | 最大 `CONFIG_MAX_FILE_SIZE`（4KB） |
| 栈需求 | `Config_Save()` 中路径拼接等约需 300 字节栈 |

> cJSON 使用标准 `malloc`/`free`，在嵌入式环境中需确保堆空间充足（建议至少 8KB 空闲堆）。若内存紧张，可考虑使用 `cJSON_InitHooks()` 接入自定义内存池。
