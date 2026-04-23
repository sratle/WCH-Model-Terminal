# Keyboard 模块通信协议

## 1. 模块概述

- **模块类型编号**：`0x04`
- **模块 ID**：`0x20`（预留范围 `0x20~0x22`）
- **物理接口**：UART3
- **主要交互核心**：V3F
- **说明**：V3F 直接管理键盘模块的 UART 中断与协议解析。

## 2. 操作码定义

| 操作码 | 宏名 | 方向 | 说明 |
| --- | --- | --- | --- |
| `0x21` | `CMD_KBD_GET_LAYOUT` | Core -> Keyboard | 查询当前键位布局。 |
| `0x22` | `CMD_KBD_SET_BACKLIGHT` | Core -> Keyboard | 设置键盘背光。DATA[0] = 模式/亮度。 |
| `0x23` | `CMD_KBD_KEY_DOWN` | Keyboard -> Core | 按键按下事件。DATA[0..5] = 键码数组。 |
| `0x24` | `CMD_KBD_KEY_UP` | Keyboard -> Core | 按键释放事件。 |
| `0x25` | `CMD_KBD_FN_COMBO` | Keyboard -> Core | Fn 组合键事件上报。 |

## 3. CMD_GET_TYPE 响应

Keyboard 模块响应 `CMD_GET_TYPE` 时，`CMD_ACK` 的 DATA 格式如下：

| 字节 | 字段 | 值/说明 |
|------|------|---------|
| DATA[0] | 模块类型编号 | `0x04` |
| DATA[1] | 模块子类型编号 | 见下表 |
| DATA[2] | 硬件版本号 | 键盘硬件版本 |
| DATA[3] | 固件主版本号 | 键盘固件主版本 |
| DATA[4] | 固件次版本号 | 键盘固件次版本 |
| DATA[5..N] | 扩展信息 | 键位布局支持、背光能力等（可选） |

**Keyboard 子类型定义**

> 代码宏定义：`protocol.h` → `MODULE_SUBTYPE_KEYBOARD_*`

| 子类型编号 | 宏名 | 实现名称 | 说明 |
|------------|------|----------|------|
| `0x00` | `MODULE_SUBTYPE_KEYBOARD_RESERVED` | 保留 | 系统保留 |
| `0x01` | `MODULE_SUBTYPE_KEYBOARD_MAIN` | Main Keyboard | 主键盘（标准全功能键盘） |
| `0x02` | `MODULE_SUBTYPE_KEYBOARD_GAME` | Game Keyboard | 游戏键盘（侧重方向键与宏定义） |
| `0x03` | `MODULE_SUBTYPE_KEYBOARD_MUSIC` | Music Keyboard | 音乐键盘（侧重媒体控制与合成器映射） |
| `0x04~0xFF` | — | 预留 | 未来扩展 |
