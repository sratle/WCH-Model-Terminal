# Submodels 模块通信协议

## 1. 模块概述

- **模块类型编号**：`0x05`
- **模块 ID**：`0x40` ~ `0x42`（预留范围 `0x40~0x49`）
- **物理接口**：UART6 / UART7 / UART8
- **主要交互核心**：V3F
- **说明**：V3F 直接管理各配件槽的 UART 中断与协议解析。每个 Submodel 独立占用一路 UART，协议格式相同，通过模块 ID 区分物理接口。

## 2. 操作码定义

| 操作码    | 宏名                 | 方向               | 说明                                   |
| ------ | ------------------ | ---------------- | ------------------------------------ |
| `0x41` | `CMD_SUB_GET_TYPE` | Core -> Submodel | 查询配件类型（与通用 `CMD_GET_TYPE` 语义相同，可复用）。 |
| `0x42` | `CMD_SUB_GET_DATA` | Core -> Submodel | 读取配件传感器/状态数据。                        |
| `0x43` | `CMD_SUB_SET_DATA` | Core -> Submodel | 向配件写入控制数据。                           |
| `0x44` | `CMD_SUB_EVT_DATA` | Submodel -> Core | 配件数据主动上报。                            |

## 3. CMD_GET_TYPE 响应

Submodel 模块响应 `CMD_GET_TYPE` 时，`CMD_ACK` 的 DATA 格式如下：

| 字节         | 字段      | 值/说明              |
| ---------- | ------- | ----------------- |
| DATA[0]    | 模块类型编号  | `0x05`（通用配件类型）    |
| DATA[1]    | 模块子类型编号 | 见下表               |
| DATA[2]    | 硬件版本号   | 配件硬件版本            |
| DATA[3]    | 固件主版本号  | 配件固件主版本           |
| DATA[4]    | 固件次版本号  | 配件固件次版本           |
| DATA[5..N] | 扩展信息    | 传感器类型、数据格式版本等（可选） |

**Submodel 子类型定义**

> 代码宏定义：`protocol.h` → `MODULE_SUBTYPE_SUBMODEL_*`

| 子类型编号 | 宏名 | 实现名称 | 说明 |
| ---------- | ---- | -------- | ---- |
| `0x00` | `MODULE_SUBTYPE_SUBMODEL_RESERVED` | 保留 | 系统保留 |
| `0x01` | `MODULE_SUBTYPE_SUBMODEL_FINGERPRINT` | Fingerprint | 指纹识别模块 |
| `0x02` | `MODULE_SUBTYPE_SUBMODEL_HEALTH` | Health Monitor | 健康监测模块 |
| `0x03` | `MODULE_SUBTYPE_SUBMODEL_NFC` | NFC | NFC 近场通信模块 |
| `0x04` | `MODULE_SUBTYPE_SUBMODEL_TOUCH_RING` | Touch Ring | 触摸环模块 |
| `0x05` | `MODULE_SUBTYPE_SUBMODEL_RGB` | RGB Lights | RGB 灯效控制模块 |
| `0x06` | `MODULE_SUBTYPE_SUBMODEL_INFRARED` | Infrared | 红外测距/遥控模块 |
| `0x07` | `MODULE_SUBTYPE_SUBMODEL_SUB_DISPLAY` | Sub Display | 副屏显示模块 |
| `0x08~0xFF` | — | 预留 | 未来扩展 |
