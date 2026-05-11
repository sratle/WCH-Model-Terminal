/********************************** (C) COPYRIGHT *******************************
 * File Name          : bt_app_profile.h
 * Description        : Custom GATT Profile for APP CLI data channel
 *******************************************************************************/

#ifndef __BT_APP_PROFILE_H__
#define __BT_APP_PROFILE_H__

#include "CH58x_common.h"
#include "CONFIG.h"

#define APP_PROFILE_SERV_UUID       0xFFF0
#define APP_PROFILE_CHAR_TX_UUID    0xFFF1   /* CH585F -> APP (Notify) */
#define APP_PROFILE_CHAR_RX_UUID    0xFFF2   /* APP -> CH585F (Write)  */

#define APP_PROFILE_CHAR_TX_LEN     247      /* Max ATT_MTU - 3, room for long data */
#define APP_PROFILE_CHAR_RX_LEN     247

/* Callback when APP writes data to RX characteristic */
typedef void (*app_profile_rx_cb_t)(const uint8_t *data, uint16_t len);

bStatus_t AppProfile_AddService(void);
void AppProfile_RegisterRxCb(app_profile_rx_cb_t cb);
bStatus_t AppProfile_SendData(uint16_t conn_handle, const uint8_t *data, uint16_t len);

#endif /* __BT_APP_PROFILE_H__ */
