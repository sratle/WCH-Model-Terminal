/********************************** (C) COPYRIGHT *******************************
 * File Name          : gattprofile.h
 * Description        : WCH Wireless CLI GATT Profile
 *                      Service: 0xFFE0
 *                      CLI_RX (Write): 0xFFF1  - APP -> CH585F
 *                      CLI_TX (Notify): 0xFFF2 - CH585F -> APP
 *********************************************************************************/

#ifndef GATTPROFILE_H
#define GATTPROFILE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************
 * CONSTANTS
 */

/* Profile Parameters */
#define WCHCLI_CHAR_RX          0   /* Write - APP -> CH585F */
#define WCHCLI_CHAR_TX          1   /* Notify - CH585F -> APP */

/* WCH Wireless CLI Service UUID */
#define WCHCLI_SERV_UUID        0xFFE0

/* Characteristic UUIDs */
#define WCHCLI_CHAR_RX_UUID     0xFFF1
#define WCHCLI_CHAR_TX_UUID     0xFFF2

/* Service bit field */
#define WCHCLI_SERVICE          0x00000001

/* Max characteristic length (negotiated MTU, up to 247) */
#define WCHCLI_CHAR_RX_LEN      247
#define WCHCLI_CHAR_TX_LEN      247

/*********************************************************************
 * TYPEDEFS
 */

/* Callback when a characteristic value has been written */
typedef void (*wchcliProfileChange_t)(uint8_t paramID, uint8_t *pValue, uint16_t len);

typedef struct
{
    wchcliProfileChange_t pfnWchCliProfileChange;
} wchcliProfileCBs_t;

/*********************************************************************
 * API FUNCTIONS
 */

extern bStatus_t WchCliProfile_AddService(uint32_t services);
extern bStatus_t WchCliProfile_RegisterAppCBs(wchcliProfileCBs_t *appCallbacks);
extern bStatus_t WchCliProfile_SetParameter(uint8_t param, uint8_t len, void *value);
extern bStatus_t WchCliProfile_GetParameter(uint8_t param, void *value);
extern bStatus_t WchCliProfile_Notify(uint16_t connHandle, attHandleValueNoti_t *pNoti);

#ifdef __cplusplus
}
#endif

#endif /* GATTPROFILE_H */
