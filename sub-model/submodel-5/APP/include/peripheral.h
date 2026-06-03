/********************************** (C) COPYRIGHT *******************************
 * File Name          : peripheral.h
 * Description        : BLE Peripheral App + SPI Bridge for WCH CLI
 *********************************************************************************/

#ifndef PERIPHERAL_H
#define PERIPHERAL_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 * CONSTANTS
 */

/* Peripheral Task Events */
#define SBP_START_DEVICE_EVT    0x0001
#define SBP_PERIODIC_EVT        0x0002
#define SBP_READ_RSSI_EVT       0x0004
#define SBP_PARAM_UPDATE_EVT    0x0008
#define SBP_PHY_UPDATE_EVT      0x0010
#define SBP_SPI_PROCESS_EVT     0x0020  /* SPI RX data to process */
#define SBP_SPI_TX_READY_EVT    0x0040  /* SPI TX data ready */
#define SBP_BLE_NOTIFY_EVT      0x0080  /* BLE Notify pending */

/*********************************************************************
 * MACROS
 */

typedef struct
{
    uint16_t connHandle;
    uint16_t connInterval;
    uint16_t connSlaveLatency;
    uint16_t connTimeout;
} peripheralConnItem_t;

/*********************************************************************
 * FUNCTIONS
 */

extern void Peripheral_Init(void);
extern uint16_t Peripheral_ProcessEvent(uint8_t task_id, uint16_t events);

/* Send data to APP via BLE Notify (called from SPI/protocol callbacks) */
extern bStatus_t Peripheral_SendToApp(const uint8_t *data, uint16_t len);

/* Check if APP is connected and Notify is enabled */
extern uint8_t Peripheral_IsAppConnected(void);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* PERIPHERAL_H */
