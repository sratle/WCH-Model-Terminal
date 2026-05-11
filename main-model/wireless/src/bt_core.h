/********************************** (C) COPYRIGHT *******************************
 * File Name          : bt_core.h
 * Description        : BLE core + protocol bridge + SPI uplink/downlink
 *******************************************************************************/

#ifndef __BT_CORE_H__
#define __BT_CORE_H__

#include "CH58x_common.h"
#include "CONFIG.h"
#include "protocol.h"

/* Bluetooth work modes */
typedef enum {
    BT_MODE_OFF = 0,
    BT_MODE_HID_HOST,
    BT_MODE_PERIPHERAL,
    BT_MODE_DUAL
} bt_mode_t;

/* Global status */
typedef struct {
    bt_mode_t mode;
    uint8_t   is_scanning;
    uint8_t   hid_connected;
    uint8_t   app_connected;
    uint8_t   conn_dev_num;
    uint8_t   main_conn_mac[6];
} bt_status_t;

extern bt_status_t g_bt_status;

/* Init */
void BT_Core_Init(void);

/* Mode */
void BT_Core_SetMode(bt_mode_t mode);

/* Status query */
void BT_Core_GetStatus(uint8_t *out_buf, uint8_t *out_len);

/* Protocol frame handler (called from polling loop) */
void BT_Core_ProcessCmd(const proto_frame_t *frame);

/* Event notify -> pack into SPI TX queue */
void BT_Core_NotifyEvent(uint8_t evt_cmd, const uint8_t *data, uint8_t len);

/* APP connection callbacks (called from bt_peripheral) */
void BT_Core_OnAppConnected(uint16_t conn_handle);
void BT_Core_OnAppDisconnected(void);

/* APP data callbacks (called from bt_app_profile) */
void BT_Core_OnAppData(const uint8_t *data, uint16_t len);

/* Send data to APP via BLE notify */
bStatus_t BT_Core_SendToApp(const uint8_t *data, uint16_t len);

/* Main polling loop: parse SPI RX, dispatch commands */
void BT_Core_Poll(void);

#endif /* __BT_CORE_H__ */
