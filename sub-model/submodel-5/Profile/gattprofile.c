/********************************** (C) COPYRIGHT *******************************
 * File Name          : gattprofile.c
 * Description        : WCH Wireless CLI GATT Profile
 *                      Service 0xFFE0 with CLI_RX (0xFFF1, Write)
 *                      and CLI_TX (0xFFF2, Notify)
 *********************************************************************************/

#include "CONFIG.h"
#include "gattprofile.h"

/*********************************************************************
 * MACROS
 */

/* Position of CLI_TX value in attribute array */
#define WCHCLI_CHAR_TX_VALUE_POS    5

/*********************************************************************
 * GLOBAL VARIABLES
 */

const uint8_t wchCliServUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(WCHCLI_SERV_UUID), HI_UINT16(WCHCLI_SERV_UUID)};

const uint8_t wchCliCharRxUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(WCHCLI_CHAR_RX_UUID), HI_UINT16(WCHCLI_CHAR_RX_UUID)};

const uint8_t wchCliCharTxUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(WCHCLI_CHAR_TX_UUID), HI_UINT16(WCHCLI_CHAR_TX_UUID)};

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

static wchcliProfileCBs_t *wchCliProfile_AppCBs = NULL;

/* Profile Attributes - variables */

/* WCH CLI Service attribute */
static const gattAttrType_t wchCliService = {ATT_BT_UUID_SIZE, wchCliServUUID};

/* CLI RX Characteristic Properties */
static uint8_t wchCliCharRxProps = GATT_PROP_WRITE | GATT_PROP_WRITE_NO_RSP;

/* CLI RX Value (buffer for written data) */
static uint8_t wchCliCharRx[WCHCLI_CHAR_RX_LEN] = {0};

/* CLI RX User Description */
static uint8_t wchCliCharRxUserDesp[] = "CLI RX";

/* CLI TX Characteristic Properties */
static uint8_t wchCliCharTxProps = GATT_PROP_NOTIFY;

/* CLI TX Value */
static uint8_t wchCliCharTx[WCHCLI_CHAR_TX_LEN] = {0};

/* CLI TX Client Characteristic Configuration */
static gattCharCfg_t wchCliCharTxConfig[PERIPHERAL_MAX_CONNECTION];

/* CLI TX User Description */
static uint8_t wchCliCharTxUserDesp[] = "CLI TX";

/*********************************************************************
 * Profile Attributes - Table
 */

static gattAttribute_t wchCliAttrTbl[] = {
    /* WCH CLI Service */
    {
        {ATT_BT_UUID_SIZE, primaryServiceUUID},
        GATT_PERMIT_READ,
        0,
        (uint8_t *)&wchCliService
    },

    /* CLI RX Characteristic Declaration */
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &wchCliCharRxProps
    },

    /* CLI RX Characteristic Value */
    {
        {ATT_BT_UUID_SIZE, wchCliCharRxUUID},
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        wchCliCharRx
    },

    /* CLI RX User Description */
    {
        {ATT_BT_UUID_SIZE, charUserDescUUID},
        GATT_PERMIT_READ,
        0,
        wchCliCharRxUserDesp
    },

    /* CLI TX Characteristic Declaration */
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &wchCliCharTxProps
    },

    /* CLI TX Characteristic Value */
    {
        {ATT_BT_UUID_SIZE, wchCliCharTxUUID},
        0,
        0,
        wchCliCharTx
    },

    /* CLI TX CCCD */
    {
        {ATT_BT_UUID_SIZE, clientCharCfgUUID},
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8_t *)wchCliCharTxConfig
    },

    /* CLI TX User Description */
    {
        {ATT_BT_UUID_SIZE, charUserDescUUID},
        GATT_PERMIT_READ,
        0,
        wchCliCharTxUserDesp
    },
};

/*********************************************************************
 * LOCAL FUNCTIONS
 */

static bStatus_t wchCliProfile_ReadAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                          uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                          uint16_t maxLen, uint8_t method);
static bStatus_t wchCliProfile_WriteAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                           uint8_t *pValue, uint16_t len, uint16_t offset,
                                           uint8_t method);
static void wchCliProfile_HandleConnStatusCB(uint16_t connHandle, uint8_t changeType);

/*********************************************************************
 * PROFILE CALLBACKS
 */

gattServiceCBs_t wchCliProfileCBs = {
    wchCliProfile_ReadAttrCB,
    wchCliProfile_WriteAttrCB,
    NULL
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

bStatus_t WchCliProfile_AddService(uint32_t services)
{
    uint8_t status = SUCCESS;

    GATTServApp_InitCharCfg(INVALID_CONNHANDLE, wchCliCharTxConfig);
    linkDB_Register(wchCliProfile_HandleConnStatusCB);

    if (services & WCHCLI_SERVICE) {
        status = GATTServApp_RegisterService(wchCliAttrTbl,
                                             GATT_NUM_ATTRS(wchCliAttrTbl),
                                             GATT_MAX_ENCRYPT_KEY_SIZE,
                                             &wchCliProfileCBs);
    }

    return status;
}

bStatus_t WchCliProfile_RegisterAppCBs(wchcliProfileCBs_t *appCallbacks)
{
    if (appCallbacks) {
        wchCliProfile_AppCBs = appCallbacks;
        return SUCCESS;
    }
    return bleAlreadyInRequestedMode;
}

bStatus_t WchCliProfile_SetParameter(uint8_t param, uint8_t len, void *value)
{
    bStatus_t ret = SUCCESS;

    switch (param) {
        case WCHCLI_CHAR_RX:
            if (len <= WCHCLI_CHAR_RX_LEN) {
                tmos_memcpy(wchCliCharRx, value, len);
            } else {
                ret = bleInvalidRange;
            }
            break;

        case WCHCLI_CHAR_TX:
            if (len <= WCHCLI_CHAR_TX_LEN) {
                tmos_memcpy(wchCliCharTx, value, len);
            } else {
                ret = bleInvalidRange;
            }
            break;

        default:
            ret = INVALIDPARAMETER;
            break;
    }

    return ret;
}

bStatus_t WchCliProfile_GetParameter(uint8_t param, void *value)
{
    bStatus_t ret = SUCCESS;

    switch (param) {
        case WCHCLI_CHAR_RX:
            tmos_memcpy(value, wchCliCharRx, WCHCLI_CHAR_RX_LEN);
            break;

        case WCHCLI_CHAR_TX:
            tmos_memcpy(value, wchCliCharTx, WCHCLI_CHAR_TX_LEN);
            break;

        default:
            ret = INVALIDPARAMETER;
            break;
    }

    return ret;
}

bStatus_t WchCliProfile_Notify(uint16_t connHandle, attHandleValueNoti_t *pNoti)
{
    uint16_t value = GATTServApp_ReadCharCfg(connHandle, wchCliCharTxConfig);

    if (value & GATT_CLIENT_CFG_NOTIFY) {
        pNoti->handle = wchCliAttrTbl[WCHCLI_CHAR_TX_VALUE_POS].handle;
        return GATT_Notification(connHandle, pNoti, FALSE);
    }
    return bleIncorrectMode;
}

/*********************************************************************
 * LOCAL FUNCTIONS
 */

static bStatus_t wchCliProfile_ReadAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                          uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                          uint16_t maxLen, uint8_t method)
{
    bStatus_t status = SUCCESS;
    uint16_t uuid;

    if (offset > 0) {
        return ATT_ERR_ATTR_NOT_LONG;
    }

    if (pAttr->type.len == ATT_BT_UUID_SIZE) {
        uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);

        switch (uuid) {
            case WCHCLI_CHAR_RX_UUID:
                *pLen = (maxLen > WCHCLI_CHAR_RX_LEN) ? WCHCLI_CHAR_RX_LEN : maxLen;
                tmos_memcpy(pValue, pAttr->pValue, *pLen);
                break;

            case WCHCLI_CHAR_TX_UUID:
                *pLen = (maxLen > WCHCLI_CHAR_TX_LEN) ? WCHCLI_CHAR_TX_LEN : maxLen;
                tmos_memcpy(pValue, pAttr->pValue, *pLen);
                break;

            default:
                *pLen = 0;
                status = ATT_ERR_ATTR_NOT_FOUND;
                break;
        }
    } else {
        *pLen = 0;
        status = ATT_ERR_INVALID_HANDLE;
    }

    return status;
}

static bStatus_t wchCliProfile_WriteAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                                           uint8_t *pValue, uint16_t len, uint16_t offset,
                                           uint8_t method)
{
    bStatus_t status = SUCCESS;
    uint8_t notifyApp = 0xFF;
    uint16_t uuid;

    if (gattPermitAuthorWrite(pAttr->permissions)) {
        return ATT_ERR_INSUFFICIENT_AUTHOR;
    }

    if (pAttr->type.len == ATT_BT_UUID_SIZE) {
        uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);

        switch (uuid) {
            case WCHCLI_CHAR_RX_UUID:
                if (offset == 0) {
                    if (len <= WCHCLI_CHAR_RX_LEN) {
                        tmos_memcpy(wchCliCharRx, pValue, len);
                        notifyApp = WCHCLI_CHAR_RX;
                    } else {
                        status = ATT_ERR_INVALID_VALUE_SIZE;
                    }
                } else {
                    status = ATT_ERR_ATTR_NOT_LONG;
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
    } else {
        status = ATT_ERR_INVALID_HANDLE;
    }

    if ((notifyApp != 0xFF) && wchCliProfile_AppCBs &&
        wchCliProfile_AppCBs->pfnWchCliProfileChange) {
        wchCliProfile_AppCBs->pfnWchCliProfileChange(notifyApp, pValue, len);
    }

    return status;
}

static void wchCliProfile_HandleConnStatusCB(uint16_t connHandle, uint8_t changeType)
{
    if (connHandle != LOOPBACK_CONNHANDLE) {
        if ((changeType == LINKDB_STATUS_UPDATE_REMOVED) ||
            ((changeType == LINKDB_STATUS_UPDATE_STATEFLAGS) &&
             (!linkDB_Up(connHandle)))) {
            GATTServApp_InitCharCfg(connHandle, wchCliCharTxConfig);
        }
    }
}

/*********************************************************************
*********************************************************************/
