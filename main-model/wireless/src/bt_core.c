/********************************** (C) COPYRIGHT *******************************
 * File Name          : bt_core.c
 *******************************************************************************/

#include "bt_core.h"
#include "hardware.h"
#include "spi_slave.h"
#include "protocol.h"
#include "bt_peripheral.h"
#include "bt_app_profile.h"

bt_status_t g_bt_status;

static uint8_t  g_spi_tx_tmp[PROTO_FRAME_MAX_LEN];
static uint8_t  g_spi_rx_tmp[PROTO_FRAME_MAX_LEN];
static uint16_t g_spi_rx_idx = 0;
static uint8_t  g_app_mac[6] = {0};
static uint8_t  g_app_name[16] = {0};

/*********************************************************************
 * Forward declarations
 */
static void BT_Core_HandleGetType(const proto_frame_t *req);
static void BT_Core_HandleGetStatus(const proto_frame_t *req);
static void BT_Core_HandleSetDiscoverable(const proto_frame_t *req);
static void BT_Core_HandleReset(const proto_frame_t *req);
static void BT_Core_HandleExtCliData(const proto_frame_t *req);
static void BT_Core_HandleExtSetMode(const proto_frame_t *req);
static void BT_Core_HandleUnsupported(const proto_frame_t *req);
static void BT_Core_SendAck(uint8_t dst, const uint8_t *data, uint8_t len);
static void BT_Core_SendNack(uint8_t dst, uint8_t err);

/*********************************************************************
 * @fn      BT_Core_Init
 */
void BT_Core_Init(void)
{
    tmos_memset(&g_bt_status, 0, sizeof(g_bt_status));
    g_bt_status.mode = BT_MODE_PERIPHERAL;

    /* BLE stack init */
    CH58x_BLEInit();
    HAL_Init();

    /* GAP roles */
    GAPRole_PeripheralInit();
    GAPRole_CentralInit();   /* reserved for future HID Host */

    /* Peripheral + Profile */
    BT_Peripheral_Init(g_bt_task_id);
    AppProfile_RegisterRxCb(BT_Core_OnAppData);
}

/*********************************************************************
 * @fn      BT_Core_SetMode
 */
void BT_Core_SetMode(bt_mode_t mode)
{
    g_bt_status.mode = mode;
    if(mode == BT_MODE_PERIPHERAL || mode == BT_MODE_DUAL)
    {
        BT_Peripheral_SetAdvertising(TRUE);
    }
    else
    {
        BT_Peripheral_SetAdvertising(FALSE);
    }
}

/*********************************************************************
 * @fn      BT_Core_GetStatus
 */
void BT_Core_GetStatus(uint8_t *out_buf, uint8_t *out_len)
{
    out_buf[0] = 0;
    if(g_bt_status.mode != BT_MODE_OFF) out_buf[0] |= 0x01;
    if(g_bt_status.is_scanning)         out_buf[0] |= 0x02;
    if(g_bt_status.hid_connected)       out_buf[0] |= 0x04;
    if(g_bt_status.app_connected)       out_buf[0] |= 0x08;
    out_buf[1] = g_bt_status.conn_dev_num;
    out_buf[2] = g_bt_status.mode;
    tmos_memcpy(&out_buf[3], g_bt_status.main_conn_mac, 6);
    *out_len = 9;
}

/*********************************************************************
 * @fn      BT_Core_ProcessCmd
 *
 * @brief   Process a complete protocol frame received from SPI.
 */
void BT_Core_ProcessCmd(const proto_frame_t *frame)
{
    if(frame->dst != MODULE_ID_WIRELESS)
    {
        /* Not for us */
        return;
    }

    switch(frame->cmd)
    {
        case CMD_GET_TYPE:
            BT_Core_HandleGetType(frame);
            break;

        case CMD_BT_GET_STATUS:
            BT_Core_HandleGetStatus(frame);
            break;

        case CMD_BT_SET_DISCOVERABLE:
            BT_Core_HandleSetDiscoverable(frame);
            break;

        case CMD_BT_RESET:
            BT_Core_HandleReset(frame);
            break;

        case CMD_BT_EXT_BASE:
            if(frame->len > 2 && frame->data[0] == CMD_BT_EXT_BASE)
            {
                uint8_t subcmd = frame->data[1];
                if(subcmd == CMD_BT_EXT_CLI_DATA)
                {
                    BT_Core_HandleExtCliData(frame);
                }
                else if(subcmd == CMD_BT_EXT_SET_MODE)
                {
                    BT_Core_HandleExtSetMode(frame);
                }
                else
                {
                    BT_Core_HandleUnsupported(frame);
                }
            }
            else
            {
                BT_Core_HandleUnsupported(frame);
            }
            break;

        default:
            BT_Core_HandleUnsupported(frame);
            break;
    }
}

/*********************************************************************
 * @fn      BT_Core_NotifyEvent
 */
void BT_Core_NotifyEvent(uint8_t evt_cmd, const uint8_t *data, uint8_t len)
{
    uint16_t packed = Protocol_PackFrame(MODULE_ID_WIRELESS, MODULE_ID_CORE,
                                          evt_cmd, data, len,
                                          g_spi_tx_tmp, PROTO_FRAME_MAX_LEN);
    if(packed > 0)
    {
        SPI_Slave_EnqueueTx(g_spi_tx_tmp, packed);
    }
}

/*********************************************************************
 * @fn      BT_Core_OnAppConnected
 */
void BT_Core_OnAppConnected(uint16_t conn_handle)
{
    (void)conn_handle;
    g_bt_status.app_connected = 1;
    g_bt_status.conn_dev_num = 1;

    /* Build connection event: [evt_type=0x00, dev_type=0x04, mac[6], name[16]] */
    uint8_t evt[24];
    evt[0] = 0x00; /* connected */
    evt[1] = 0x04; /* phone/PC APP */
    tmos_memcpy(&evt[2], g_app_mac, 6);
    tmos_memcpy(&evt[8], g_app_name, 16);
    BT_Core_NotifyEvent(CMD_BT_CONN_EVT, evt, 24);

    PRINT("BT_Core: APP connected event -> Core\n");
}

/*********************************************************************
 * @fn      BT_Core_OnAppDisconnected
 */
void BT_Core_OnAppDisconnected(void)
{
    g_bt_status.app_connected = 0;
    g_bt_status.conn_dev_num = 0;

    uint8_t evt[24];
    evt[0] = 0x01; /* disconnected */
    evt[1] = 0x04;
    tmos_memcpy(&evt[2], g_app_mac, 6);
    tmos_memcpy(&evt[8], g_app_name, 16);
    BT_Core_NotifyEvent(CMD_BT_CONN_EVT, evt, 24);

    PRINT("BT_Core: APP disconnected event -> Core\n");
}

/*********************************************************************
 * @fn      BT_Core_OnAppData
 *
 * @brief   Callback from bt_app_profile: APP wrote data to RX char.
 *          Forward to Core via CMD_BT_EXT_CLI_DATA.
 */
void BT_Core_OnAppData(const uint8_t *data, uint16_t len)
{
    /* Pack as CMD_BT_EXT_CLI_DATA: [0x50, 0x07, data...] */
    if(len > (PROTO_DATA_MAX_LEN - 2))
    {
        /* Too long, truncate or fragment. For now, truncate. */
        len = PROTO_DATA_MAX_LEN - 2;
    }

    uint8_t payload[PROTO_DATA_MAX_LEN];
    payload[0] = CMD_BT_EXT_BASE;
    payload[1] = CMD_BT_EXT_CLI_DATA;
    tmos_memcpy(&payload[2], data, len);

    BT_Core_NotifyEvent(CMD_BT_EXT_BASE, payload, 2 + len);
    PRINT("BT_Core: APP data %d bytes -> Core\n", len);
}

/*********************************************************************
 * @fn      BT_Core_SendToApp
 */
bStatus_t BT_Core_SendToApp(const uint8_t *data, uint16_t len)
{
    uint16_t conn_handle = BT_Peripheral_GetConnHandle();
    if(conn_handle == GAP_CONNHANDLE_INIT)
        return bleNotConnected;
    return AppProfile_SendData(conn_handle, data, len);
}

/*********************************************************************
 * @fn      BT_Core_Poll
 */
void BT_Core_Poll(void)
{
    static proto_frame_t rx_frame;
    uint8_t byte;

    /* Drain SPI RX bytes and run protocol parser */
    while(SPI_Slave_DequeueRx(&byte))
    {
        if(Protocol_ParseByte(byte, &rx_frame))
        {
            BT_Core_ProcessCmd(&rx_frame);
        }
    }
}

/*********************************************************************
 * Internal handlers
 *********************************************************************/

static void BT_Core_HandleGetType(const proto_frame_t *req)
{
    uint8_t rsp[8];
    rsp[0] = 0x02; /* module type = wireless */
    rsp[1] = 0x01; /* sub-type = BLE */
    rsp[2] = 0x01; /* hw ver */
    rsp[3] = 0x00; /* fw major */
    rsp[4] = 0x01; /* fw minor */
    BT_Core_SendAck(req->src, rsp, 5);
}

static void BT_Core_HandleGetStatus(const proto_frame_t *req)
{
    uint8_t status[16];
    uint8_t len = 0;
    BT_Core_GetStatus(status, &len);
    BT_Core_SendAck(req->src, status, len);
}

static void BT_Core_HandleSetDiscoverable(const proto_frame_t *req)
{
    (void)req;
    if(req->len >= 2)
    {
        uint8_t enable = req->data[0];
        BT_Peripheral_SetAdvertising(enable ? TRUE : FALSE);
    }
    /* Set command: no ACK required per protocol */
}

static void BT_Core_HandleReset(const proto_frame_t *req)
{
    (void)req;
    BT_Core_SendAck(req->src, NULL, 0);
    /* Delay then reset */
    DelayMs(50);
    PFIC_SystemReset();
}

static void BT_Core_HandleExtCliData(const proto_frame_t *req)
{
    /* Frame format: CMD=0x50, DATA=[0x50, 0x07, <cli_raw_data>...] */
    if(req->len < 3)
    {
        BT_Core_SendNack(req->src, PROTO_ERR_LEN_MISMATCH);
        return;
    }

    uint8_t subcmd = req->data[1];
    if(subcmd != CMD_BT_EXT_CLI_DATA)
    {
        BT_Core_SendNack(req->src, PROTO_ERR_UNSUPPORTED_CMD);
        return;
    }

    uint8_t cli_len = req->len - 3;
    if(cli_len > 0)
    {
        bStatus_t status = BT_Core_SendToApp(&req->data[2], cli_len);
        if(status != SUCCESS)
        {
            BT_Core_SendNack(req->src, PROTO_ERR_BUSY);
            return;
        }
    }
    BT_Core_SendAck(req->src, NULL, 0);
}

static void BT_Core_HandleExtSetMode(const proto_frame_t *req)
{
    if(req->len < 4)
    {
        BT_Core_SendNack(req->src, PROTO_ERR_LEN_MISMATCH);
        return;
    }
    bt_mode_t mode = (bt_mode_t)req->data[2];
    BT_Core_SetMode(mode);
    BT_Core_SendAck(req->src, NULL, 0);
}

static void BT_Core_HandleUnsupported(const proto_frame_t *req)
{
    BT_Core_SendNack(req->src, PROTO_ERR_UNSUPPORTED_CMD);
}

static void BT_Core_SendAck(uint8_t dst, const uint8_t *data, uint8_t len)
{
    uint16_t packed = Protocol_PackAck(MODULE_ID_WIRELESS, dst, (uint8_t *)data, len,
                                       g_spi_tx_tmp, PROTO_FRAME_MAX_LEN);
    if(packed > 0)
    {
        SPI_Slave_EnqueueTx(g_spi_tx_tmp, packed);
    }
}

static void BT_Core_SendNack(uint8_t dst, uint8_t err)
{
    uint16_t packed = Protocol_PackNack(MODULE_ID_WIRELESS, dst, err,
                                        g_spi_tx_tmp, PROTO_FRAME_MAX_LEN);
    if(packed > 0)
    {
        SPI_Slave_EnqueueTx(g_spi_tx_tmp, packed);
    }
}
