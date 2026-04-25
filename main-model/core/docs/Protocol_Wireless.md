# Wireless 通信协议

## 1. 模块概述

- **模块类型编号**：`0x02`
- **模块 ID**：`0x01`

## 2. 操作码定义

| 操作码 | 宏名 | 方向 | 说明 |
| --- | --- | --- | --- |
| `0x51` | `CMD_BT_GET_STATUS` | Core -> CH585F | 查询蓝牙连接状态。 |
| `0x52` | `CMD_BT_SEND_DATA` | Core -> CH585F | 通过蓝牙发送数据。 |
| `0x53` | `CMD_BT_START_OTA` | Core -> CH585F | 启动蓝牙 OTA 升级流程。 |
| `0x54` | `CMD_BT_CONN_EVT` | CH585F -> Core | 蓝牙连接/断开事件。 |
| `0x55` | `CMD_BT_RECV_DATA` | CH585F -> Core | 蓝牙接收到数据上报。 |

## 3. CMD_GET_TYPE 响应

CH585F 模块响应 `CMD_GET_TYPE` 时，`CMD_ACK` 的 DATA 格式如下：

| 字节 | 字段 | 值/说明 |
|------|------|---------|
| DATA[0] | 模块类型编号 | `0x02` |
| DATA[1] | 模块子类型编号 | `0x01`（`MODULE_SUBTYPE_WIRELESS_BT`，标准蓝牙/无线模块） |
| DATA[2] | 硬件版本号 | CH585F 硬件版本 |
| DATA[3] | 固件主版本号 | 蓝牙协议栈或固件主版本 |
| DATA[4] | 固件次版本号 | 蓝牙协议栈或固件次版本 |
| DATA[5..N] | 扩展信息 | 蓝牙版本、支持协议（BLE/Classic）等能力位（可选） |

> 当前 CH585F 仅规划一种实现，子类型固定为 `0x01`（该大类的标准/自身实现），预留 `0x02~0xFF` 用于未来扩展。子类型宏定义见 `protocol.h`。 |
