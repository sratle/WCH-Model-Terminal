/********************************** (C) COPYRIGHT *******************************
 * File Name          : peripheral.c
 * Description        : BLE Peripheral App + SPI Bridge for WCH CLI
 *                      - Handles BLE connection, advertising, MTU
 *                      - Bridges APP protocol frames <-> SPI protocol frames
 *                      - Forwards CLI data between APP and Core
 *********************************************************************************/

#include "CONFIG.h"
#include "devinfoservice.h"
#include "gattprofile.h"
#include "peripheral.h"
#include "protocol.h"
#include "spi_slave.h"

/*********************************************************************
 * MACROS
 */

#define SBP_PERIODIC_EVT_PERIOD              1600
#define SBP_READ_RSSI_EVT_PERIOD             3200
#define SBP_PARAM_UPDATE_DELAY               6400
#define SBP_PHY_UPDATE_DELAY                 2400
#define DEFAULT_ADVERTISING_INTERVAL         80
#define DEFAULT_DISCOVERABLE_MODE            GAP_ADTYPE_FLAGS_GENERAL
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL    6
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL    100
#define DEFAULT_DESIRED_SLAVE_LATENCY        0
#define DEFAULT_DESIRED_CONN_TIMEOUT         100
#define WCH_COMPANY_ID                       0x07D7

/* APP Protocol Constants */
#define APP_PROTO_HDR_SIZE      5
#define APP_PROTO_TYPE_CLI      0x01
#define APP_PROTO_TYPE_CTRL     0x02
#define APP_PROTO_TYPE_HB       0xFF
#define APP_PROTO_FLAG_SOF      0x01
#define APP_PROTO_FLAG_EOF      0x02
#define APP_PROTO_FLAG_DIR_UP   0x04

/* Reassembly buffer for APP -> Core */
#define APP_RX_REASM_BUF_SIZE   2048

/* SPI TX buffer for Core -> APP forwarding */
#define SPI_FWD_BUF_SIZE        512

/*********************************************************************
 * LOCAL VARIABLES
 */

static uint8_t  Peripheral_TaskID = INVALID_TASK_ID;

static uint8_t  scanRspData[] = {
    0x0d,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'W', 'C', 'H', '-', 'T', 'e', 'r', 'm', 'i', 'n', 'a', 'l',
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

static uint8_t  advertData[] = {
    0x02,
    GAP_ADTYPE_FLAGS,
    DEFAULT_DISCOVERABLE_MODE | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
    0x03,
    GAP_ADTYPE_16BIT_MORE,
    LO_UINT16(WCHCLI_SERV_UUID),
    HI_UINT16(WCHCLI_SERV_UUID)
};

static uint8_t  attDeviceName[GAP_DEVICE_NAME_LEN] = "WCH-Terminal";

static peripheralConnItem_t peripheralConnList;
static uint16_t peripheralMTU = ATT_MTU_SIZE;

/* APP RX reassembly */
static uint8_t  s_app_rx_reasm[APP_RX_REASM_BUF_SIZE];
static uint16_t s_app_rx_reasm_len = 0;
static uint8_t  s_app_rx_expect_seq = 0;
static uint8_t  s_app_rx_in_reasm = 0;

/* BLE Notify sequence counter (Core -> APP) */
static uint8_t  s_ble_tx_seq = 0;

/* SPI forwarding buffer */
static uint8_t  s_spi_fwd_buf[SPI_FWD_BUF_SIZE];
static uint16_t s_spi_fwd_len = 0;

/*********************************************************************
 * LOCAL FUNCTIONS
 */

static void PrintHex(const char *prefix, const uint8_t *data, uint16_t len);

static void Peripheral_ProcessTMOSMsg(tmos_event_hdr_t *pMsg);
static void peripheralStateNotificationCB(gapRole_States_t newState, gapRoleEvent_t *pEvent);
static void performPeriodicTask(void);
static void wchCliProfileChangeCB(uint8_t paramID, uint8_t *pValue, uint16_t len);
static void peripheralParamUpdateCB(uint16_t connHandle, uint16_t connInterval,
                                    uint16_t connSlaveLatency, uint16_t connTimeout);
static void peripheralInitConnItem(peripheralConnItem_t *peripheralConnList);
static void peripheralRssiCB(uint16_t connHandle, int8_t rssi);

/* Protocol / SPI callbacks */
static void OnProtocolStdFrame(const proto_frame_t *frame);
static void OnProtocolStreamChunk(const uint8_t *chunk, uint16_t chunk_len,
                                   uint8_t is_final, uint8_t flags);
static void ProcessSpiRxData(void);
static void ProcessBleToSpi(const uint8_t *app_data, uint16_t len);

/* APP protocol helpers */
static void AppProtoResetReasm(void);
static void AppProtoHandleFrame(const uint8_t *frame, uint16_t frame_len);
static bStatus_t AppProtoSendNotify(const uint8_t *payload, uint16_t payload_len,
                                     uint8_t flags);

/*********************************************************************
 * PROFILE CALLBACKS
 */

static gapRolesCBs_t Peripheral_PeripheralCBs = {
    peripheralStateNotificationCB,
    peripheralRssiCB,
    peripheralParamUpdateCB
};

static gapRolesBroadcasterCBs_t Broadcaster_BroadcasterCBs = {
    NULL,
    NULL
};

static gapBondCBs_t Peripheral_BondMgrCBs = {
    NULL,
    NULL,
    NULL
};

static wchcliProfileCBs_t Peripheral_WchCliProfileCBs = {
    wchCliProfileChangeCB
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

void Peripheral_Init(void)
{
    Peripheral_TaskID = TMOS_ProcessEventRegister(Peripheral_ProcessEvent);

    /* Setup GAP Peripheral Role */
    {
        uint8_t  initial_advertising_enable = TRUE;
        uint16_t desired_min_interval = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
        uint16_t desired_max_interval = DEFAULT_DESIRED_MAX_CONN_INTERVAL;

        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initial_advertising_enable);
        GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
        GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(uint16_t), &desired_min_interval);
        GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(uint16_t), &desired_max_interval);
    }

    {
        uint16_t advInt = DEFAULT_ADVERTISING_INTERVAL;
        GAP_SetParamValue(TGAP_DISC_ADV_INT_MIN, advInt);
        GAP_SetParamValue(TGAP_DISC_ADV_INT_MAX, advInt);
        GAP_SetParamValue(TGAP_ADV_SCAN_REQ_NOTIFY, ENABLE);
    }

    /* Setup Bond Manager (Just Works pairing) */
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

    /* Initialize GATT attributes */
    GGS_AddService(GATT_ALL_SERVICES);
    GATTServApp_AddService(GATT_ALL_SERVICES);
    DevInfo_AddService();
    WchCliProfile_AddService(WCHCLI_SERVICE);

    /* Set device name */
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, attDeviceName);

    /* Init connection item */
    peripheralInitConnItem(&peripheralConnList);

    /* Register profile callbacks */
    WchCliProfile_RegisterAppCBs(&Peripheral_WchCliProfileCBs);
    GAPRole_BroadcasterSetCB(&Broadcaster_BroadcasterCBs);

    /* Init protocol layer */
    Protocol_Init();
    Protocol_RegisterCallbacks(OnProtocolStdFrame, OnProtocolStreamChunk);

    /* Init SPI slave */
    SPI_Slave_Init();

    /* Start device */
    tmos_set_event(Peripheral_TaskID, SBP_START_DEVICE_EVT);

    /* Start periodic SPI processing */
    tmos_start_task(Peripheral_TaskID, SBP_SPI_PROCESS_EVT, 10);
}

uint16_t Peripheral_ProcessEvent(uint8_t task_id, uint16_t events)
{
    if (events & SYS_EVENT_MSG) {
        uint8_t *pMsg;
        if ((pMsg = tmos_msg_receive(Peripheral_TaskID)) != NULL) {
            Peripheral_ProcessTMOSMsg((tmos_event_hdr_t *)pMsg);
            tmos_msg_deallocate(pMsg);
        }
        return (events ^ SYS_EVENT_MSG);
    }

    if (events & SBP_START_DEVICE_EVT) {
        GAPRole_PeripheralStartDevice(Peripheral_TaskID, &Peripheral_BondMgrCBs,
                                      &Peripheral_PeripheralCBs);
        return (events ^ SBP_START_DEVICE_EVT);
    }

    if (events & SBP_PERIODIC_EVT) {
        if (SBP_PERIODIC_EVT_PERIOD) {
            tmos_start_task(Peripheral_TaskID, SBP_PERIODIC_EVT, SBP_PERIODIC_EVT_PERIOD);
        }
        performPeriodicTask();
        return (events ^ SBP_PERIODIC_EVT);
    }

    if (events & SBP_PARAM_UPDATE_EVT) {
        GAPRole_PeripheralConnParamUpdateReq(peripheralConnList.connHandle,
                                             DEFAULT_DESIRED_MIN_CONN_INTERVAL,
                                             DEFAULT_DESIRED_MAX_CONN_INTERVAL,
                                             DEFAULT_DESIRED_SLAVE_LATENCY,
                                             DEFAULT_DESIRED_CONN_TIMEOUT,
                                             Peripheral_TaskID);
        return (events ^ SBP_PARAM_UPDATE_EVT);
    }

    if (events & SBP_PHY_UPDATE_EVT) {
        GAPRole_UpdatePHY(peripheralConnList.connHandle, 0,
                          GAP_PHY_BIT_LE_2M, GAP_PHY_BIT_LE_2M, 0);
        return (events ^ SBP_PHY_UPDATE_EVT);
    }

    if (events & SBP_READ_RSSI_EVT) {
        GAPRole_ReadRssiCmd(peripheralConnList.connHandle);
        tmos_start_task(Peripheral_TaskID, SBP_READ_RSSI_EVT, SBP_READ_RSSI_EVT_PERIOD);
        return (events ^ SBP_READ_RSSI_EVT);
    }

    if (events & SBP_SPI_PROCESS_EVT) {
        ProcessSpiRxData();
        tmos_start_task(Peripheral_TaskID, SBP_SPI_PROCESS_EVT, 5);
        return (events ^ SBP_SPI_PROCESS_EVT);
    }

    if (events & SBP_BLE_NOTIFY_EVT) {
        /* Process pending BLE notifications if any */
        return (events ^ SBP_BLE_NOTIFY_EVT);
    }

    return 0;
}

bStatus_t Peripheral_SendToApp(const uint8_t *data, uint16_t len)
{
    attHandleValueNoti_t noti;
    bStatus_t status;

    if (peripheralConnList.connHandle == GAP_CONNHANDLE_INIT) {
        PRINT("[BLE TX] Fail: not connected\n");
        return bleNotConnected;
    }
    if (len > (peripheralMTU - 3)) {
        PRINT("[BLE TX] Fail: too large %d > %d\n", len, peripheralMTU - 3);
        return bleInvalidRange;
    }

    noti.len = len;
    noti.pValue = GATT_bm_alloc(peripheralConnList.connHandle, ATT_HANDLE_VALUE_NOTI, noti.len, NULL, 0);
    if (noti.pValue == NULL) {
        PRINT("[BLE TX] Fail: GATT_bm_alloc failed\n");
        return FAILURE;
    }

    tmos_memcpy(noti.pValue, data, noti.len);
    status = WchCliProfile_Notify(peripheralConnList.connHandle, &noti);
    if (status != SUCCESS) {
        GATT_bm_free((gattMsg_t *)&noti, ATT_HANDLE_VALUE_NOTI);
        PRINT("[BLE TX] Fail: Notify err=%02X\n", status);
        return FAILURE;
    }

    PrintHex("[BLE TX] OK", data, len);
    return SUCCESS;
}

uint8_t Peripheral_IsAppConnected(void)
{
    return (peripheralConnList.connHandle != GAP_CONNHANDLE_INIT);
}

/*********************************************************************
 * LOCAL FUNCTIONS - BLE GAP / Connection
 *********************************************************************/

static void peripheralInitConnItem(peripheralConnItem_t *pConn)
{
    pConn->connHandle = GAP_CONNHANDLE_INIT;
    pConn->connInterval = 0;
    pConn->connSlaveLatency = 0;
    pConn->connTimeout = 0;
}

static void Peripheral_ProcessGAPMsg(gapRoleEvent_t *pEvent)
{
    switch (pEvent->gap.opcode) {
        case GAP_SCAN_REQUEST_EVENT:
            // PRINT("Scan req from %x:%x:%x:%x:%x:%x\n",
            //       pEvent->scanReqEvt.scannerAddr[0], pEvent->scanReqEvt.scannerAddr[1],
            //       pEvent->scanReqEvt.scannerAddr[2], pEvent->scanReqEvt.scannerAddr[3],
            //       pEvent->scanReqEvt.scannerAddr[4], pEvent->scanReqEvt.scannerAddr[5]);
            break;

        case GAP_PHY_UPDATE_EVENT:
            PRINT("PHY update Rx:%x Tx:%x\n",
                  pEvent->linkPhyUpdate.connRxPHYS, pEvent->linkPhyUpdate.connTxPHYS);
            break;

        default:
            break;
    }
}

static void Peripheral_ProcessTMOSMsg(tmos_event_hdr_t *pMsg)
{
    switch (pMsg->event) {
        case GAP_MSG_EVENT:
            Peripheral_ProcessGAPMsg((gapRoleEvent_t *)pMsg);
            break;

        case GATT_MSG_EVENT: {
            gattMsgEvent_t *pMsgEvent = (gattMsgEvent_t *)pMsg;
            if (pMsgEvent->method == ATT_MTU_UPDATED_EVENT) {
                peripheralMTU = pMsgEvent->msg.exchangeMTUReq.clientRxMTU;
                PRINT("MTU updated: %d\n", peripheralMTU);
            }
            break;
        }

        default:
            break;
    }
}

static void Peripheral_LinkEstablished(gapRoleEvent_t *pEvent)
{
    gapEstLinkReqEvent_t *event = (gapEstLinkReqEvent_t *)pEvent;

    if (peripheralConnList.connHandle != GAP_CONNHANDLE_INIT) {
        GAPRole_TerminateLink(pEvent->linkCmpl.connectionHandle);
        PRINT("Connection max, reject\n");
    } else {
        peripheralConnList.connHandle = event->connectionHandle;
        peripheralConnList.connInterval = event->connInterval;
        peripheralConnList.connSlaveLatency = event->connLatency;
        peripheralConnList.connTimeout = event->connTimeout;
        peripheralMTU = ATT_MTU_SIZE;

        tmos_start_task(Peripheral_TaskID, SBP_PERIODIC_EVT, SBP_PERIODIC_EVT_PERIOD);
        tmos_start_task(Peripheral_TaskID, SBP_PARAM_UPDATE_EVT, SBP_PARAM_UPDATE_DELAY);
        tmos_start_task(Peripheral_TaskID, SBP_READ_RSSI_EVT, SBP_READ_RSSI_EVT_PERIOD);

        PRINT("Connected, handle=%x mtu=%d\n", event->connectionHandle, peripheralMTU);
    }
}

static void Peripheral_LinkTerminated(gapRoleEvent_t *pEvent)
{
    gapTerminateLinkEvent_t *event = (gapTerminateLinkEvent_t *)pEvent;

    if (event->connectionHandle == peripheralConnList.connHandle) {
        peripheralConnList.connHandle = GAP_CONNHANDLE_INIT;
        peripheralConnList.connInterval = 0;
        peripheralConnList.connSlaveLatency = 0;
        peripheralConnList.connTimeout = 0;

        tmos_stop_task(Peripheral_TaskID, SBP_PERIODIC_EVT);
        tmos_stop_task(Peripheral_TaskID, SBP_READ_RSSI_EVT);

        /* Restart advertising */
        {
            uint8_t advertising_enable = TRUE;
            GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &advertising_enable);
        }

        PRINT("Disconnected, reason=%x\n", event->reason);
    }
}

static void peripheralStateNotificationCB(gapRole_States_t newState, gapRoleEvent_t *pEvent)
{
    switch (newState & GAPROLE_STATE_ADV_MASK) {
        case GAPROLE_STARTED:
            PRINT("GAP started\n");
            break;

        case GAPROLE_ADVERTISING:
            if (pEvent->gap.opcode == GAP_LINK_TERMINATED_EVENT) {
                Peripheral_LinkTerminated(pEvent);
                PRINT("Advertising (after disconnect)\n");
            } else if (pEvent->gap.opcode == GAP_MAKE_DISCOVERABLE_DONE_EVENT) {
                PRINT("Advertising\n");
            }
            break;

        case GAPROLE_CONNECTED:
            if (pEvent->gap.opcode == GAP_LINK_ESTABLISHED_EVENT) {
                Peripheral_LinkEstablished(pEvent);
                PRINT("Connected\n");
            }
            break;

        case GAPROLE_WAITING:
            if (pEvent->gap.opcode == GAP_LINK_TERMINATED_EVENT) {
                Peripheral_LinkTerminated(pEvent);
                PRINT("Disconnected, waiting\n");
            } else if (pEvent->gap.opcode == GAP_END_DISCOVERABLE_DONE_EVENT) {
                PRINT("Waiting for advertising\n");
            }
            break;

        case GAPROLE_ERROR:
            PRINT("GAP error\n");
            break;

        default:
            break;
    }
}

static void peripheralRssiCB(uint16_t connHandle, int8_t rssi)
{
    PRINT("RSSI -%d dB\n", -rssi);
}

static void peripheralParamUpdateCB(uint16_t connHandle, uint16_t connInterval,
                                    uint16_t connSlaveLatency, uint16_t connTimeout)
{
    if (connHandle == peripheralConnList.connHandle) {
        peripheralConnList.connInterval = connInterval;
        peripheralConnList.connSlaveLatency = connSlaveLatency;
        peripheralConnList.connTimeout = connTimeout;
        PRINT("Param update: int=%x\n", connInterval);
    }
}

static void performPeriodicTask(void)
{
    /* Nothing to do periodically for now */
}

/*********************************************************************
 * LOCAL FUNCTIONS - BLE Profile Callback (APP writes data)
 *********************************************************************/

static void PrintHex(const char *prefix, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    PRINT("%s [%d]: ", prefix, len);
    for (i = 0; i < len && i < 64; i++) {
        PRINT("%02X ", data[i]);
    }
    if (len > 64) {
        PRINT("...");
    }
    PRINT("\n");
}

static void wchCliProfileChangeCB(uint8_t paramID, uint8_t *pValue, uint16_t len)
{
    if (paramID == WCHCLI_CHAR_RX) {
        /* APP wrote data to CLI_RX characteristic */
        PrintHex("[BLE RX]", pValue, len);
        AppProtoHandleFrame(pValue, len);
    }
}

/*********************************************************************
 * LOCAL FUNCTIONS - APP Protocol Handling (BLE side)
 *********************************************************************/

static void AppProtoResetReasm(void)
{
    s_app_rx_reasm_len = 0;
    s_app_rx_in_reasm = 0;
}

static void AppProtoHandleFrame(const uint8_t *frame, uint16_t frame_len)
{
    uint8_t type, flags, seq, payload_len;
    const uint8_t *payload;
    uint8_t is_sof, is_eof;

    PRINT("[APP] Parse frame: len=%d\n", frame_len);

    if (frame_len < APP_PROTO_HDR_SIZE) {
        PRINT("[APP] Frame too short, drop\n");
        return; /* Too short */
    }

    type = frame[0];
    flags = frame[1];
    seq = frame[2];
    payload_len = frame[3];
    /* frame[4] = RSV, ignore */

    if (frame_len < (APP_PROTO_HDR_SIZE + payload_len)) {
        return; /* Malformed */
    }

    payload = frame + APP_PROTO_HDR_SIZE;
    is_sof = (flags & APP_PROTO_FLAG_SOF) ? 1 : 0;
    is_eof = (flags & APP_PROTO_FLAG_EOF) ? 1 : 0;

    if (type == APP_PROTO_TYPE_HB) {
        /* Heartbeat: echo back immediately */
        uint8_t hb_rsp[5] = {APP_PROTO_TYPE_HB, APP_PROTO_FLAG_SOF | APP_PROTO_FLAG_EOF | APP_PROTO_FLAG_DIR_UP, seq, 0, 0};
        bStatus_t st = Peripheral_SendToApp(hb_rsp, 5);
        PRINT("[APP] HB rsp seq=%02X status=%02X\n", seq, st);
        return;
    }

    if (type != APP_PROTO_TYPE_CLI) {
        return; /* Only handle CLI data for now */
    }

    if (is_sof) {
        AppProtoResetReasm();
        s_app_rx_in_reasm = 1;
        s_app_rx_expect_seq = seq;
    }

    if (!s_app_rx_in_reasm) {
        return; /* Dropping orphan fragment */
    }

    /* Check sequence continuity (optional, can be relaxed) */
    if (seq != s_app_rx_expect_seq) {
        PRINT("SEQ mismatch: expected %x, got %x\n", s_app_rx_expect_seq, seq);
        AppProtoResetReasm();
        return;
    }
    s_app_rx_expect_seq++;

    /* Append payload to reassembly buffer */
    if ((s_app_rx_reasm_len + payload_len) <= APP_RX_REASM_BUF_SIZE) {
        tmos_memcpy(s_app_rx_reasm + s_app_rx_reasm_len, payload, payload_len);
        s_app_rx_reasm_len += payload_len;
    } else {
        PRINT("Reassembly overflow\n");
        AppProtoResetReasm();
        return;
    }

    if (is_eof) {
        /* Complete message received, forward to Core via SPI */
        PRINT("[APP] Reasm complete: %d bytes -> SPI\n", s_app_rx_reasm_len);
        PrintHex("[APP] Reasm data", s_app_rx_reasm, s_app_rx_reasm_len);
        ProcessBleToSpi(s_app_rx_reasm, s_app_rx_reasm_len);
        AppProtoResetReasm();
    }
}

static bStatus_t AppProtoSendNotify(const uint8_t *payload, uint16_t payload_len,
                                     uint8_t flags)
{
    uint8_t frame[256];
    uint16_t frame_len;
    uint8_t max_payload;
    bStatus_t status = SUCCESS;

    PRINT("[BRIDGE] SPI->BLE: payload=%d bytes flags=%02X\n", payload_len, flags);
    PrintHex("[BRIDGE] SPI->BLE data", payload, payload_len);

    max_payload = (peripheralMTU > 8) ? (peripheralMTU - 8) : 12;

    if (payload_len <= max_payload) {
        /* Single frame */
        frame[0] = APP_PROTO_TYPE_CLI;
        frame[1] = flags | APP_PROTO_FLAG_SOF | APP_PROTO_FLAG_EOF | APP_PROTO_FLAG_DIR_UP;
        frame[2] = s_ble_tx_seq++;
        frame[3] = payload_len;
        frame[4] = 0;
        tmos_memcpy(frame + 5, payload, payload_len);
        frame_len = 5 + payload_len;

        status = Peripheral_SendToApp(frame, frame_len);
    } else {
        /* Multi-frame: split payload */
        uint16_t offset = 0;
        uint8_t first = 1;

        while (offset < payload_len) {
            uint16_t chunk = payload_len - offset;
            uint8_t f;

            if (chunk > max_payload) {
                chunk = max_payload;
            }

            if (first && (offset + chunk >= payload_len)) {
                f = APP_PROTO_FLAG_SOF | APP_PROTO_FLAG_EOF | APP_PROTO_FLAG_DIR_UP;
            } else if (first) {
                f = APP_PROTO_FLAG_SOF | APP_PROTO_FLAG_DIR_UP;
            } else if (offset + chunk >= payload_len) {
                f = APP_PROTO_FLAG_EOF | APP_PROTO_FLAG_DIR_UP;
            } else {
                f = APP_PROTO_FLAG_DIR_UP;
            }

            frame[0] = APP_PROTO_TYPE_CLI;
            frame[1] = f;
            frame[2] = s_ble_tx_seq++;
            frame[3] = chunk;
            frame[4] = 0;
            tmos_memcpy(frame + 5, payload + offset, chunk);
            frame_len = 5 + chunk;

            status = Peripheral_SendToApp(frame, frame_len);
            if (status != SUCCESS) {
                break;
            }

            offset += chunk;
            first = 0;
        }
    }

    return status;
}

/*********************************************************************
 * LOCAL FUNCTIONS - SPI Bridge
 *********************************************************************/

static void ProcessBleToSpi(const uint8_t *app_data, uint16_t len)
{
    uint8_t data_buf[PROTO_DATA_MAX_LEN];
    uint8_t out_buf[PROTO_FRAME_MAX_LEN];
    uint16_t spi_len;
    uint8_t flags = CLI_FLAG_SOF | CLI_FLAG_EOF;

    PRINT("[BRIDGE] BLE->SPI: %d bytes\n", len);
    PrintHex("[BRIDGE] BLE->SPI data", app_data, len);

    /* Build SPI protocol frame: CMD_BT_EXT_CLI_DATA */
    /* DATA = [0x07][FLAGS][CLI_data...] */

    if ((len + 2) <= PROTO_DATA_MAX_LEN) {
        /* Single standard frame */
        data_buf[0] = CMD_BT_EXT_CLI_DATA;
        data_buf[1] = flags;
        tmos_memcpy(data_buf + 2, app_data, len);

        spi_len = Protocol_PackFrame(MODULE_ID_WIRELESS, MODULE_ID_CORE,
                                      CMD_BT_EXT_BASE, data_buf, len + 2,
                                      out_buf, sizeof(out_buf));
        if (spi_len > 0) {
            SPI_Slave_EnqueueTx(out_buf, spi_len);
            PrintHex("[SPI TX] CLI_DATA", out_buf, spi_len);
            tmos_set_event(Peripheral_TaskID, SBP_SPI_TX_READY_EVT);
        }
    } else {
        /* Multi-frame: split into chunks */
        uint16_t offset = 0;
        uint8_t first = 1;

        while (offset < len) {
            uint16_t chunk = len - offset;
            uint8_t chunk_flags = 0;

            if (chunk > (PROTO_DATA_MAX_LEN - 2)) {
                chunk = PROTO_DATA_MAX_LEN - 2;
            }

            if (first) {
                chunk_flags |= CLI_FLAG_SOF;
            }
            if (offset + chunk >= len) {
                chunk_flags |= CLI_FLAG_EOF;
            }

            data_buf[0] = CMD_BT_EXT_CLI_DATA;
            data_buf[1] = chunk_flags;
            tmos_memcpy(data_buf + 2, app_data + offset, chunk);

            spi_len = Protocol_PackFrame(MODULE_ID_WIRELESS, MODULE_ID_CORE,
                                          CMD_BT_EXT_BASE, data_buf, chunk + 2,
                                          out_buf, sizeof(out_buf));
            if (spi_len > 0) {
                SPI_Slave_EnqueueTx(out_buf, spi_len);
                PrintHex("[SPI TX] CLI_DATA chunk", out_buf, spi_len);
            }

            offset += chunk;
            first = 0;
        }
        tmos_set_event(Peripheral_TaskID, SBP_SPI_TX_READY_EVT);
    }
}

static void ProcessSpiRxData(void)
{
    uint8_t rx_byte;
    static uint32_t last_rx = 0;
    uint32_t rx_cnt = SPI_Slave_GetRxCountTotal();

    while (SPI_Slave_DequeueRx(&rx_byte, 1) == 1) {
        Protocol_ParseByte(rx_byte);
    }

    /* Only print when new data received and processed */
    if (rx_cnt != last_rx) {
        uint16_t queue_cnt = SPI_Slave_RxCount();
        if (queue_cnt > 200) {
            PRINT("[SPI] queue high: %u\n", queue_cnt);
        }
        last_rx = rx_cnt;
    }
}

/*********************************************************************
 * LOCAL FUNCTIONS - Protocol Callbacks
 *********************************************************************/

static void OnProtocolStdFrame(const proto_frame_t *frame)
{
    uint8_t rsp_buf[PROTO_FRAME_MAX_LEN];
    uint16_t rsp_len;

    PRINT("[SPI RX] STD frame: SRC=%02X DST=%02X CMD=%02X LEN=%d\n",
          frame->src, frame->dst, frame->cmd, frame->len);
    if (frame->len > 1) {
        PrintHex("[SPI RX] DATA", frame->data, frame->len - 1);
    }

    /* Only handle frames destined to Wireless module */
    if (frame->dst != MODULE_ID_WIRELESS) {
        return;
    }

    switch (frame->cmd) {
        case CMD_GET_TYPE: {
            uint8_t type_data[6] = {
                0x02,   /* Module type: Wireless */
                0x01,   /* Sub-type: BLE */
                0x01,   /* HW version */
                0x01,   /* FW major */
                0x00,   /* FW minor */
                0x00    /* Reserved */
            };
            rsp_len = Protocol_PackAck(MODULE_ID_WIRELESS, MODULE_ID_CORE,
                                        type_data, sizeof(type_data),
                                        rsp_buf, sizeof(rsp_buf));
            if (rsp_len > 0) {
                SPI_Slave_EnqueueTx(rsp_buf, rsp_len);
                PrintHex("[SPI TX] ACK(GET_TYPE)", rsp_buf, rsp_len);
            }
            break;
        }

        case CMD_BT_GET_STATUS: {
            uint8_t status_data[9] = {
                0x09,   /* bit0=BLE on, bit3=APP connected */
                0x01,   /* 1 device connected */
                0x02,   /* Peripheral mode */
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* MAC */
            };
            if (Peripheral_IsAppConnected()) {
                status_data[0] |= 0x09; /* BLE on + APP connected */
            }
            rsp_len = Protocol_PackAck(MODULE_ID_WIRELESS, MODULE_ID_CORE,
                                        status_data, sizeof(status_data),
                                        rsp_buf, sizeof(rsp_buf));
            if (rsp_len > 0) {
                SPI_Slave_EnqueueTx(rsp_buf, rsp_len);
                PrintHex("[SPI TX] ACK(GET_STATUS)", rsp_buf, rsp_len);
            }
            break;
        }

        case CMD_BT_RESET:
            rsp_len = Protocol_PackAck(MODULE_ID_WIRELESS, MODULE_ID_CORE,
                                        NULL, 0, rsp_buf, sizeof(rsp_buf));
            if (rsp_len > 0) {
                SPI_Slave_EnqueueTx(rsp_buf, rsp_len);
                PrintHex("[SPI TX] ACK(RESET)", rsp_buf, rsp_len);
            }
            break;

        case CMD_BT_EXT_BASE:
            /* Extended commands: check sub-command in data[0] */
            if (frame->len >= 2) {
                uint8_t ext_cmd = frame->data[0];
                if (ext_cmd == CMD_BT_EXT_CLI_DATA && frame->len >= 3) {
                    /* CLI data from Core -> forward to APP */
                    uint8_t flags = frame->data[1];
                    const uint8_t *cli_data = frame->data + 2;
                    uint8_t cli_len = frame->len - 3;

                    if (cli_len > 0 && Peripheral_IsAppConnected()) {
                        uint8_t app_flags = APP_PROTO_FLAG_DIR_UP;
                        if (flags & CLI_FLAG_SOF) app_flags |= APP_PROTO_FLAG_SOF;
                        if (flags & CLI_FLAG_EOF) app_flags |= APP_PROTO_FLAG_EOF;
                        AppProtoSendNotify(cli_data, cli_len, app_flags);
                    }
                }
            }
            break;

        default:
            /* Unsupported command: send NACK */
            PRINT("[SPI RX] Unsupported CMD=%02X\n", frame->cmd);
            rsp_len = Protocol_PackNack(MODULE_ID_WIRELESS, MODULE_ID_CORE,
                                         PROTO_ERR_UNSUPPORTED_CMD,
                                         rsp_buf, sizeof(rsp_buf));
            if (rsp_len > 0) {
                SPI_Slave_EnqueueTx(rsp_buf, rsp_len);
                PrintHex("[SPI TX] NACK", rsp_buf, rsp_len);
            }
            break;
    }
}

static void OnProtocolStreamChunk(const uint8_t *chunk, uint16_t chunk_len,
                                   uint8_t is_final, uint8_t flags)
{
    /* Streaming frame received from Core */
    PRINT("[SPI RX] STREAM chunk: len=%d final=%d flags=%02X\n", chunk_len, is_final, flags);
    if (chunk_len > 0) {
        PrintHex("[SPI RX] DATA", chunk, chunk_len);
    }
    /* Forward to APP via BLE Notify */
    if (chunk_len > 2 && Peripheral_IsAppConnected()) {
        uint8_t cli_flags = chunk[1];
        const uint8_t *cli_data = chunk + 2;
        uint16_t cli_len = chunk_len - 2;
        uint8_t app_flags = APP_PROTO_FLAG_DIR_UP;

        if (cli_flags & CLI_FLAG_SOF) app_flags |= APP_PROTO_FLAG_SOF;
        if (cli_flags & CLI_FLAG_EOF) app_flags |= APP_PROTO_FLAG_EOF;

        AppProtoSendNotify(cli_data, cli_len, app_flags);
    }
}

/*********************************************************************
*********************************************************************/
