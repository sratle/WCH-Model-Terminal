/********************************** (C) COPYRIGHT *******************************
 * File Name          : bt_peripheral.h
 * Description        : BLE Peripheral role: advertising & APP connection
 *******************************************************************************/

#ifndef __BT_PERIPHERAL_H__
#define __BT_PERIPHERAL_H__

#include "CH58x_common.h"
#include "CONFIG.h"

/* Events */
#define BP_START_DEVICE_EVT     0x0001

void BT_Peripheral_Init(uint8_t task_id);
uint16_t BT_Peripheral_ProcessEvent(uint8_t task_id, uint16_t events);

void BT_Peripheral_SetAdvName(const uint8_t *name, uint8_t len);
void BT_Peripheral_SetAdvertising(bool enable);
void BT_Peripheral_Disconnect(void);

uint16_t BT_Peripheral_GetConnHandle(void);
bool BT_Peripheral_IsConnected(void);

#endif /* __BT_PERIPHERAL_H__ */
