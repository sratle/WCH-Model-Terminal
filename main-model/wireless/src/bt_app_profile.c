/********************************** (C) COPYRIGHT *******************************
 * File Name          : bt_app_profile.c
 *******************************************************************************/

#include "bt_app_profile.h"

/* UUIDs */
static const uint8_t appProfileServUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(APP_PROFILE_SERV_UUID), HI_UINT16(APP_PROFILE_SERV_UUID)};

static const uint8_t appProfileCharTxUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(APP_PROFILE_CHAR_TX_UUID), HI_UINT16(APP_PROFILE_CHAR_TX_UUID)};

static const uint8_t appProfileCharRxUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(APP_PROFILE_CHAR_RX_UUID), HI_UINT16(APP_PROFILE_CHAR_RX_UUID)};

/* Service definition */
static const gattAttrType_t appProfileService = {ATT_BT_UUID_SIZE, appProfileServUUID};

/* TX Characteristic: Notify (CH585F -> APP) */
static uint8_t appProfileCharTxProps = GATT_PROP_NOTIFY;
static uint8_t appProfileCharTx[APP_PROFILE_CHAR_TX_LEN] = {0};
static gattCharCfg_t appProfileCharTxConfig[PERIPHERAL_MAX_CONNECTION];
static uint8_t appProfileCharTxUserDesp[] = "APP Data TX (Notify)";

/* RX Characteristic: Write (APP -> CH585F) */
static uint8_t appProfileCharRxProps = GATT_PROP_WRITE;
static uint8_t appProfileCharRx[APP_PROFILE_CHAR_RX_LEN] = {0};
static uint8_t appProfileCharRxUserDesp[] = "APP Data RX (Write)";

/* Callback */
static app_profile_rx_cb_t appProfile_RxCb = NULL;

/* Attribute table */
static gattAttribute_t appProfileAttrTbl[] = {
    /* Service */
    {
        {ATT_BT_UUID_SIZE, primaryServiceUUID},
        GATT_PERMIT_READ,
        0,
        (uint8_t *)&appProfileService
    },
    /* TX Characteristic Declaration */
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &appProfileCharTxProps
    },
    /* TX Characteristic Value */
    {
        {ATT_BT_UUID_SIZE, appProfileCharTxUUID},
        0,
        0,
        appProfileCharTx
    },
    /* TX Client Characteristic Configuration */
    {
        {ATT_BT_UUID_SIZE, clientCharCfgUUID},
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8_t *)appProfileCharTxConfig
    },
    /* TX User Description */
    {
        {ATT_BT_UUID_SIZE, charUserDescUUID},
        GATT_PERMIT_READ,
        0,
        appProfileCharTxUserDesp
    },
    /* RX Characteristic Declaration */
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &appProfileCharRxProps
    },
    /* RX Characteristic Value */
    {
        {ATT_BT_UUID_SIZE, appProfileCharRxUUID},
        GATT_PERMIT_WRITE,
        0,
        appProfileCharRx
    },
    /* RX User Description */
    {
        {ATT_BT_UUID_SIZE, charUserDescUUID},
        GATT_PERMIT_READ,
        0,
        appProfileCharRxUserDesp
    },
};

/* Service callbacks */
static bStatus_t appProfile_ReadAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                       uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                       uint16_t maxLen, uint8_t method);
static bStatus_t appProfile_WriteAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                        uint8_t *pValue, uint16_t len, uint16_t offset,
                                        uint8_t method);
static void appProfile_HandleConnStatusCB(uint16_t connHandle, uint8_t changeType);

static gattServiceCBs_t appProfileCBs = {
    appProfile_ReadAttrCB,
    appProfile_WriteAttrCB,
    NULL
};

/*********************************************************************
 * @fn      AppProfile_AddService
 */
bStatus_t AppProfile_AddService(void)
{
    GATTServApp_InitCharCfg(INVALID_CONNHANDLE, appProfileCharTxConfig);
    linkDB_Register(appProfile_HandleConnStatusCB);
    return GATTServApp_RegisterService(appProfileAttrTbl,
                                       GATT_NUM_ATTRS(appProfileAttrTbl),
                                       GATT_MAX_ENCRYPT_KEY_SIZE,
                                       &appProfileCBs);
}

/*********************************************************************
 * @fn      AppProfile_RegisterRxCb
 */
void AppProfile_RegisterRxCb(app_profile_rx_cb_t cb)
{
    appProfile_RxCb = cb;
}

/*********************************************************************
 * @fn      AppProfile_SendData
 */
bStatus_t AppProfile_SendData(uint16_t conn_handle, const uint8_t *data, uint16_t len)
{
    uint16_t value = GATTServApp_ReadCharCfg(conn_handle, appProfileCharTxConfig);
    if(!(value & GATT_CLIENT_CFG_NOTIFY))
        return bleIncorrectMode;

    if(len > APP_PROFILE_CHAR_TX_LEN)
    {
        /* Data too large for buffer */
        return INVALIDPARAMETER;
    }

    attHandleValueNoti_t noti;
    noti.len = len;
    noti.pValue = GATT_bm_alloc(conn_handle, ATT_HANDLE_VALUE_NOTI, len, NULL, 0);
    if(noti.pValue == NULL)
        return bleMemAllocError;

    tmos_memcpy(noti.pValue, data, len);
    noti.handle = appProfileAttrTbl[2].handle; /* TX value attribute handle */

    bStatus_t status = GATT_Notification(conn_handle, &noti, FALSE);
    if(status != SUCCESS)
    {
        GATT_bm_free((gattMsg_t *)&noti, ATT_HANDLE_VALUE_NOTI);
    }
    return status;
}

/*********************************************************************
 * @fn      appProfile_ReadAttrCB
 */
static bStatus_t appProfile_ReadAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                       uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                       uint16_t maxLen, uint8_t method)
{
    bStatus_t status = SUCCESS;
    if(offset > 0)
        return ATT_ERR_ATTR_NOT_LONG;

    if(pAttr->type.len == ATT_BT_UUID_SIZE)
    {
        uint16_t uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);
        switch(uuid)
        {
            case APP_PROFILE_CHAR_TX_UUID:
                *pLen = MIN(maxLen, APP_PROFILE_CHAR_TX_LEN);
                tmos_memcpy(pValue, pAttr->pValue, *pLen);
                break;
            default:
                *pLen = 0;
                status = ATT_ERR_ATTR_NOT_FOUND;
                break;
        }
    }
    else
    {
        *pLen = 0;
        status = ATT_ERR_INVALID_HANDLE;
    }
    return status;
}

/*********************************************************************
 * @fn      appProfile_WriteAttrCB
 */
static bStatus_t appProfile_WriteAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                        uint8_t *pValue, uint16_t len, uint16_t offset,
                                        uint8_t method)
{
    bStatus_t status = SUCCESS;

    if(pAttr->type.len == ATT_BT_UUID_SIZE)
    {
        uint16_t uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);
        switch(uuid)
        {
            case APP_PROFILE_CHAR_RX_UUID:
                if(offset == 0)
                {
                    if(len > APP_PROFILE_CHAR_RX_LEN)
                        status = ATT_ERR_INVALID_VALUE_SIZE;
                }
                else
                {
                    status = ATT_ERR_ATTR_NOT_LONG;
                }
                if(status == SUCCESS)
                {
                    tmos_memcpy(pAttr->pValue, pValue, len);
                    if(appProfile_RxCb)
                    {
                        appProfile_RxCb(pValue, len);
                    }
                }
                break;

            case GATT_CLIENT_CHAR_CFG_UUID:
                status = GATTServApp_ProcessCCCWriteReq(connHandle, pAttr, pValue, len,
                                                        offset, GATT_CLIENT_CFG_NOTIFY);
                break;

            default:
                status = ATT_ERR_ATTR_NOT_FOUND;
                break;
        }
    }
    else
    {
        status = ATT_ERR_INVALID_HANDLE;
    }
    return status;
}

/*********************************************************************
 * @fn      appProfile_HandleConnStatusCB
 */
static void appProfile_HandleConnStatusCB(uint16_t connHandle, uint8_t changeType)
{
    if(connHandle != LOOPBACK_CONNHANDLE)
    {
        if((changeType == LINKDB_STATUS_UPDATE_REMOVED) ||
           ((changeType == LINKDB_STATUS_UPDATE_STATEFLAGS) && (!linkDB_Up(connHandle))))
        {
            GATTServApp_InitCharCfg(connHandle, appProfileCharTxConfig);
        }
    }
}
