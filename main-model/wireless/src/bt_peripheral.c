/********************************** (C) COPYRIGHT *******************************
 * File Name          : bt_peripheral.c
 *******************************************************************************/

#include "bt_peripheral.h"
#include "bt_app_profile.h"
#include "bt_core.h"

/*********************************************************************
 * CONSTANTS
 */
#define DEFAULT_ADVERTISING_INTERVAL        80   /* 50ms (units of 625us) */
#define DEFAULT_DISCOVERABLE_MODE           GAP_ADTYPE_FLAGS_GENERAL
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL   20   /* 25ms */
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL   100  /* 125ms */
#define DEFAULT_DESIRED_SLAVE_LATENCY       0
#define DEFAULT_DESIRED_CONN_TIMEOUT        100  /* 1s */
#define WCH_COMPANY_ID                      0x07D7

/*********************************************************************
 * LOCAL VARIABLES
 */
static uint8_t  g_bp_task_id = INVALID_TASK_ID;
static uint16_t g_bp_conn_handle = GAP_CONNHANDLE_INIT;
static uint16_t g_bp_mtu = ATT_MTU_SIZE;

static uint8_t g_scanRspData[] = {
    0x0F,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'W', 'C', 'H', '-', 'T', 'e', 'r', 'm', 'i', 'n', 'a', 'l', '\0',
    0x05,
    GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
    LO_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
    HI_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
    LO_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL),
    HI_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL),
    0x02,
    GAP_ADTYPE_POWER_LEVEL,
    0
};

static uint8_t g_advertData[] = {
    0x02,
    GAP_ADTYPE_FLAGS,
    DEFAULT_DISCOVERABLE_MODE | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
    0x03,
    GAP_ADTYPE_16BIT_MORE,
    LO_UINT16(APP_PROFILE_SERV_UUID),
    HI_UINT16(APP_PROFILE_SERV_UUID)
};

static uint8_t g_attDeviceName[GAP_DEVICE_NAME_LEN] = "WCH-Terminal";

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void BP_StateNotificationCB(gapRole_States_t newState, gapRoleEvent_t *pEvent);
static void BP_ParamUpdateCB(uint16_t connHandle, uint16_t connInterval,
                             uint16_t connSlaveLatency, uint16_t connTimeout);
static void BP_ProcessTMOSMsg(tmos_event_hdr_t *pMsg);

/*********************************************************************
 * CALLBACKS
 */
static gapRolesCBs_t BP_PeripheralCBs = {
    BP_StateNotificationCB,
    NULL,
    BP_ParamUpdateCB
};

static gapRolesBroadcasterCBs_t BP_BroadcasterCBs = {
    NULL,
    NULL
};

static gapBondCBs_t BP_BondMgrCBs = {
    NULL,
    NULL
};

/*********************************************************************
 * @fn      BT_Peripheral_Init
 */
void BT_Peripheral_Init(uint8_t task_id)
{
    g_bp_task_id = task_id;

    /* Setup GAP Peripheral Role */
    {
        uint8_t  initial_advertising_enable = TRUE;
        uint16_t desired_min_interval = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
        uint16_t desired_max_interval = DEFAULT_DESIRED_MAX_CONN_INTERVAL;

        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initial_advertising_enable);
        GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(g_scanRspData), g_scanRspData);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(g_advertData), g_advertData);
        GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(uint16_t), &desired_min_interval);
        GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(uint16_t), &desired_max_interval);
    }

    /* Advertising interval */
    {
        uint16_t advInt = DEFAULT_ADVERTISING_INTERVAL;
        GAP_SetParamValue(TGAP_DISC_ADV_INT_MIN, advInt);
        GAP_SetParamValue(TGAP_DISC_ADV_INT_MAX, advInt);
    }

    /* Bond Manager: Just Works, no MITM */
    {
        uint32_t passkey = 0;
        uint8_t  pairMode = GAPBOND_PAIRING_MODE_WAIT_FOR_REQ;
        uint8_t  mitm = FALSE;
        uint8_t  bonding = TRUE;
        uint8_t  ioCap = GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT;
        GAPBondMgr_SetParameter(GAPBOND_PERI_DEFAULT_PASSCODE, sizeof(uint32_t), &passkey);
        GAPBondMgr_SetParameter(GAPBOND_PERI_PAIRING_MODE, sizeof(uint8_t), &pairMode);
        GAPBondMgr_SetParameter(GAPBOND_PERI_MITM_PROTECTION, sizeof(uint8_t), &mitm);
        GAPBondMgr_SetParameter(GAPBOND_PERI_IO_CAPABILITIES, sizeof(uint8_t), &ioCap);
        GAPBondMgr_SetParameter(GAPBOND_PERI_BONDING_ENABLED, sizeof(uint8_t), &bonding);
    }

    /* GATT attributes */
    GGS_AddService(GATT_ALL_SERVICES);
    GATTServApp_AddService(GATT_ALL_SERVICES);
    AppProfile_AddService();

    /* Device name */
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, g_attDeviceName);

    /* Broadcaster callbacks */
    GAPRole_BroadcasterSetCB(&BP_BroadcasterCBs);

    /* Start device later via event */
    tmos_set_event(g_bp_task_id, BP_START_DEVICE_EVT);
}

/*********************************************************************
 * @fn      BT_Peripheral_ProcessEvent
 */
uint16_t BT_Peripheral_ProcessEvent(uint8_t task_id, uint16_t events)
{
    if(events & SYS_EVENT_MSG)
    {
        uint8_t *pMsg;
        if((pMsg = tmos_msg_receive(task_id)) != NULL)
        {
            BP_ProcessTMOSMsg((tmos_event_hdr_t *)pMsg);
            tmos_msg_deallocate(pMsg);
        }
        return (events ^ SYS_EVENT_MSG);
    }

    if(events & BP_START_DEVICE_EVT)
    {
        GAPRole_PeripheralStartDevice(task_id, &BP_BondMgrCBs, &BP_PeripheralCBs);
        return (events ^ BP_START_DEVICE_EVT);
    }

    return 0;
}

/*********************************************************************
 * @fn      BT_Peripheral_SetAdvName
 */
void BT_Peripheral_SetAdvName(const uint8_t *name, uint8_t len)
{
    uint8_t copyLen = (len < (GAP_DEVICE_NAME_LEN - 1)) ? len : (GAP_DEVICE_NAME_LEN - 1);
    tmos_memset(g_attDeviceName, 0, GAP_DEVICE_NAME_LEN);
    tmos_memcpy(g_attDeviceName, name, copyLen);
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, g_attDeviceName);

    /* Rebuild scan response data */
    uint8_t idx = 0;
    g_scanRspData[idx++] = 1 + copyLen;
    g_scanRspData[idx++] = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
    tmos_memcpy(&g_scanRspData[idx], name, copyLen);
    idx += copyLen;
    g_scanRspData[idx++] = 0x05;
    g_scanRspData[idx++] = GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE;
    g_scanRspData[idx++] = LO_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL);
    g_scanRspData[idx++] = HI_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL);
    g_scanRspData[idx++] = LO_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL);
    g_scanRspData[idx++] = HI_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL);
    g_scanRspData[idx++] = 0x02;
    g_scanRspData[idx++] = GAP_ADTYPE_POWER_LEVEL;
    g_scanRspData[idx++] = 0;
    GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, idx, g_scanRspData);
}

/*********************************************************************
 * @fn      BT_Peripheral_SetAdvertising
 */
void BT_Peripheral_SetAdvertising(bool enable)
{
    uint8_t advertising_enable = enable ? TRUE : FALSE;
    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &advertising_enable);
}

/*********************************************************************
 * @fn      BT_Peripheral_Disconnect
 */
void BT_Peripheral_Disconnect(void)
{
    if(g_bp_conn_handle != GAP_CONNHANDLE_INIT)
    {
        GAPRole_TerminateLink(g_bp_conn_handle);
    }
}

/*********************************************************************
 * @fn      BT_Peripheral_GetConnHandle
 */
uint16_t BT_Peripheral_GetConnHandle(void)
{
    return g_bp_conn_handle;
}

/*********************************************************************
 * @fn      BT_Peripheral_IsConnected
 */
bool BT_Peripheral_IsConnected(void)
{
    return (g_bp_conn_handle != GAP_CONNHANDLE_INIT);
}

/*********************************************************************
 * @fn      BP_StateNotificationCB
 */
static void BP_StateNotificationCB(gapRole_States_t newState, gapRoleEvent_t *pEvent)
{
    switch(newState & GAPROLE_STATE_ADV_MASK)
    {
        case GAPROLE_STARTED:
            PRINT("BT Peripheral started\n");
            break;

        case GAPROLE_ADVERTISING:
            PRINT("BT Advertising..\n");
            break;

        case GAPROLE_CONNECTED:
            if(pEvent->gap.opcode == GAP_LINK_ESTABLISHED_EVENT)
            {
                g_bp_conn_handle = pEvent->linkCmpl.connectionHandle;
                g_bp_mtu = ATT_MTU_SIZE;
                PRINT("BT APP connected, handle=%x\n", g_bp_conn_handle);
                BT_Core_OnAppConnected(g_bp_conn_handle);
            }
            break;

        case GAPROLE_WAITING:
            if(pEvent->gap.opcode == GAP_LINK_TERMINATED_EVENT)
            {
                PRINT("BT APP disconnected, reason=%x\n", pEvent->linkTerminate.reason);
                g_bp_conn_handle = GAP_CONNHANDLE_INIT;
                BT_Core_OnAppDisconnected();
                /* Restart advertising */
                BT_Peripheral_SetAdvertising(TRUE);
            }
            break;

        default:
            break;
    }
}

/*********************************************************************
 * @fn      BP_ParamUpdateCB
 */
static void BP_ParamUpdateCB(uint16_t connHandle, uint16_t connInterval,
                             uint16_t connSlaveLatency, uint16_t connTimeout)
{
    (void)connHandle;
    (void)connInterval;
    (void)connSlaveLatency;
    (void)connTimeout;
}

/*********************************************************************
 * @fn      BP_ProcessTMOSMsg
 */
static void BP_ProcessTMOSMsg(tmos_event_hdr_t *pMsg)
{
    switch(pMsg->event)
    {
        case GAP_MSG_EVENT:
            break;

        case GATT_MSG_EVENT:
        {
            gattMsgEvent_t *pMsgEvent = (gattMsgEvent_t *)pMsg;
            if(pMsgEvent->method == ATT_MTU_UPDATED_EVENT)
            {
                g_bp_mtu = pMsgEvent->msg.exchangeMTUReq.clientRxMTU;
                PRINT("BT MTU updated: %d\n", g_bp_mtu);
            }
            break;
        }

        default:
            break;
    }
}
