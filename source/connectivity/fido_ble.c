/******************************************************************************
* File:             ble.c
*
* Author:           Joshua Steffensky  
* Created:          05/04/20 
* Description:      CTAP 2 BLE interface implementation
*****************************************************************************/

#include "fido_ble.h"
#include "ble_ctap.h"
#include "fido_interfaces.h"
#include "hal.h"

#include "string.h"

#include "app_util.h"
#include "mem_manager.h"
#include "app_timer.h"

#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "ble_conn_state.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "ble_dis.h"

/*  LOG INIT  */
#define NRF_LOG_MODULE_NAME ble

#include "nrf_log.h"
#include "nrf_log_ctrl.h"

NRF_LOG_MODULE_REGISTER();
/*            */

/***************************************************************************** 
*							INTERNAL PROTOTYPES
*****************************************************************************/
/*    INIT    */
/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void);

/**@brief Function for the GAP initialization.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name, appearance, and the preferred connection parameters.
 */
static void gap_params_init(void);

/**@brief Function for initializing the GATT module.
 */
static void gatt_init(void);

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void);

/**@brief Function for initializing the Advertising functionality.
 *
 * @details Encodes the required advertising data and passes it to the stack.
 *          Also builds a structure to be passed to the stack when starting advertising.
 */
static void advertising_init(void);

/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void);

/**@brief Function for the Peer Manager initialization.
 */
static void peer_manager_init(void);

/* STCK CNTRL */
/**@brief Function for starting advertising.
 */
static void advertising_start(bool erase_bonds);

/**@brief Clear bond information from persistent storage.
 */
static void delete_bonds(void);

/*  EVT HNDL  */
/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context);

/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *          @note All this function does is to disconnect. This could have been done by simply
 *                setting the disconnect_on_fail config parameter, but instead we use the event
 *                handler mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt);

/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error);

/**@brief Function for handling Peer Manager events.
 *
 * @param[in] p_evt  Peer Manager event.
 */
static void pm_evt_handler(pm_evt_t const * p_evt);

/** 
 * @brief		parse request command and forward requests to handler
 * 
 * @param[in] 	req	request to be handled
 *
 * @return 		void
 */
static void process_ctap_request(ble_ctap_t * p_ctap, BLE_CTAP_REQ* req);

/**
 * @brief		Process received MSG request
 * 
 * @param[in] 	req request to be processed
 *
 * @return 		void
 */
static void ble_ctap_msg_response(ble_ctap_t * p_ctap, BLE_CTAP_REQ* req);

/** 
 * @brief		Process received PING request
 *
 * @param[in] 	req	request to be processed
 *
 * @return 		void
 */
static void ble_ctap_ping_response(ble_ctap_t * p_ctap, BLE_CTAP_REQ* req);

/***************************************************************************** 
*							GLOBALS
*****************************************************************************/

NRF_BLE_GATT_DEF(m_gatt);                                                       /**< GATT module instance. */
//NRF_BLE_QWR_DEF(m_qwr);                                                         /**< Context for the Queued Write module.*/
BLE_CTAP_DEF(m_ctap);
BLE_ADVERTISING_DEF(m_advertising);                                             /**< Advertising module instance. */

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;

//volatile bool m_hvx_pending = false;

// YOUR_JOB: Use UUIDs for service(s) used in your application.
static ble_uuid_t m_adv_uuids[] =                                               /**< Universally unique service identifiers. */
{
    {BLE_UUID_DEVICE_INFORMATION_SERVICE, BLE_UUID_TYPE_BLE},
    {BLE_CTAP_GATT_SERVICE_UUID, BLE_UUID_TYPE_BLE}
};


/***************************************************************************** 
*							EVENT HANDLERS
*****************************************************************************/

static void pm_evt_handler(pm_evt_t const * p_evt)
{
//    NRF_LOG_DEBUG("pm_evt_handler");
    pm_handler_on_pm_evt(p_evt);
    pm_handler_flash_clean(p_evt);

    switch (p_evt->evt_id)
    {
        case PM_EVT_PEERS_DELETE_SUCCEEDED:
            advertising_start(false);
            break;

        default:
            break;
    }
}

static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
//    NRF_LOG_DEBUG("ble_evt_handler app");
    ret_code_t err_code = NRF_SUCCESS;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected.");
            // LED indication will be changed when advertising starts.
            break;

        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected.");
            /**err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);*/
            /**APP_ERROR_CHECK(err_code);*/
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            /**err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);*/
            /**APP_ERROR_CHECK(err_code);*/
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;
        
        case BLE_GATTS_EVT_TIMEOUT:
        {
            // Disconnect on GATT Server timeout event.
            NRF_LOG_DEBUG("GATT Server Timeout.");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
        } break;
        
        case BLE_GAP_EVT_CONN_SEC_UPDATE:
        {
            NRF_LOG_DEBUG("GAP CONN SEC UPDATE sm:%d, lv:%d", p_ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.sm, p_ble_evt->evt.gap_evt.params.conn_sec_update.conn_sec.sec_mode.lv);
        } break;

        default:
            // no implementation needed.
            break;
    }
}

static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    ret_code_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}

static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/***************************************************************************** 
*							INITIALISATION
*****************************************************************************/

static void ble_stack_init(void)
{
    NRF_LOG_DEBUG("ble stack init");
    ret_code_t err_code;
    
    if(!nrf_sdh_is_enabled())
    {
        err_code = nrf_sdh_enable_request();
        APP_ERROR_CHECK(err_code);
    }

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);
    
    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}

static void gap_params_init(void)
{
    NRF_LOG_DEBUG("gap_params_init");
    ret_code_t              err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

//    sec_mode.sm = 1; sec_mode.lv = 2; // encrypted, not authenticated (no MITM protection)
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    
    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);
    
    err_code = sd_ble_gap_appearance_set(BLE_DEVICE_APPEARANCE);
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;
    //TODO
    /**gap_conn_params.min_conn_interval = BLE_GAP_CP_MIN_CONN_INTVL_NONE;*/
    /**gap_conn_params.max_conn_interval = BLE_GAP_CP_MAX_CONN_INTVL_NONE;*/
    /**[>gap_conn_params.slave_latency     = SLAVE_LATENCY;<]*/
    /**gap_conn_params.conn_sup_timeout  = BLE_GAP_CP_CONN_SUP_TIMEOUT_NONE;*/

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

static void gatt_init(void)
{
    NRF_LOG_DEBUG("gatt_init");
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}

static void services_init(void)
{
    NRF_LOG_DEBUG("services init");
    ret_code_t         err_code;
    ble_ctap_init_t    ctap_init;
    ble_dis_init_t     dis_init;
    ble_dis_sys_id_t   sys_id;

    // Initialize Device Information Service.
    memset(&dis_init, 0, sizeof(dis_init));

    ble_srv_ascii_to_utf8(&dis_init.manufact_name_str, MANUFACTURER_NAME);
    ble_srv_ascii_to_utf8(&dis_init.model_num_str, MODEL_NUM);
    ble_srv_ascii_to_utf8(&dis_init.serial_num_str, SERIAL_NUM);
    ble_srv_ascii_to_utf8(&dis_init.fw_rev_str, FIRMWARE_VERSION);
    ble_srv_ascii_to_utf8(&dis_init.hw_rev_str, HW_VERSION);

    sys_id.manufacturer_id            = MANUFACTURER_ID;
    sys_id.organizationally_unique_id = ORG_UNIQUE_ID;
    dis_init.p_sys_id                 = &sys_id;

    dis_init.dis_char_rd_sec = SEC_OPEN;

    err_code = ble_dis_init(&dis_init);
    APP_ERROR_CHECK(err_code);


    // Initialize CTAP Service. TODO
    memset(&ctap_init, 0, sizeof(ctap_init));
    
    ctap_init.request_handler = &process_ctap_request;
    ctap_init.m_gatt          = &m_gatt;

    err_code = ble_ctap_init(&m_ctap, &ctap_init);
    APP_ERROR_CHECK(err_code);

}

static void advertising_init(void)
{
    NRF_LOG_DEBUG("advertising_init");
    ret_code_t             err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance      = true;
    init.advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    init.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.advdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;

    
    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

static void conn_params_init(void)
{
    NRF_LOG_DEBUG("conn_params_init");
    ret_code_t             err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}

static void peer_manager_init(void)
{
    NRF_LOG_DEBUG("peer_manager_init");
    ble_gap_sec_params_t sec_param;
    ret_code_t           err_code;

    err_code = pm_init();
    APP_ERROR_CHECK(err_code);

    memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

    // Security parameters to be used for all security procedures.
    sec_param.bond           = SEC_PARAM_BOND;
    sec_param.mitm           = SEC_PARAM_MITM;
    sec_param.lesc           = SEC_PARAM_LESC;
    sec_param.keypress       = SEC_PARAM_KEYPRESS;
    sec_param.io_caps        = SEC_PARAM_IO_CAPABILITIES;
    sec_param.oob            = SEC_PARAM_OOB;
    sec_param.min_key_size   = SEC_PARAM_MIN_KEY_SIZE;
    sec_param.max_key_size   = SEC_PARAM_MAX_KEY_SIZE;
    sec_param.kdist_own.enc  = 1;
    sec_param.kdist_own.id   = 1;
    sec_param.kdist_peer.enc = 1;
    sec_param.kdist_peer.id  = 1;

    err_code = pm_sec_params_set(&sec_param);
    APP_ERROR_CHECK(err_code);

    err_code = pm_register(pm_evt_handler);
    APP_ERROR_CHECK(err_code);
}


/***************************************************************************** 
*							STACK CONTROL
*****************************************************************************/

static void delete_bonds(void)
{
    ret_code_t err_code;

    NRF_LOG_INFO("Erase bonds!");

    err_code = pm_peers_delete();
    APP_ERROR_CHECK(err_code);
}

static void advertising_start(bool erase_bonds)
{
    NRF_LOG_DEBUG("advertising_start");
    if (erase_bonds == true)
    {
        delete_bonds();
        // Advertising is started by PM_EVT_PEERS_DELETED_SUCEEDED event
    }
    else
    {
        ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);

        APP_ERROR_CHECK(err_code);
    }
}

/***************************************************************************** 
*							MESSAGE HANDLING
*****************************************************************************/

static void process_ctap_request(ble_ctap_t * p_ctap, BLE_CTAP_REQ* req)
{
    //set timeout
    
    switch(req->cmd)
    {
        case BLE_CTAP_CMD_PING:
            NRF_LOG_INFO("BLE_PING");
            ble_ctap_ping_response(p_ctap, req);
            break;
        case BLE_CTAP_CMD_MSG:
            NRF_LOG_INFO("BLE_MSG");
            ble_ctap_msg_response(p_ctap, req);
            break;
        /**case BLE_CTAP_CMD_CANCEL:*/
            /**NRF_LOG_INFO("BLE_CANCEL");*/
            /**TODO */
            /**break;*/
        default:
            NRF_LOG_WARNING("Unknown Command: 0x%x", req->cmd);
            ble_ctap_send_err(p_ctap, BLE_CTAP_ERR_INVALID_CMD);
            break;
    }
}

static void ble_ctap_ping_response(ble_ctap_t * p_ctap, BLE_CTAP_REQ* req)
{
    //PONG
    ble_ctap_send_resp(p_ctap, req->cmd, req->data, (size_t) req->length, true);
}


static void ble_ctap_msg_response(ble_ctap_t * p_ctap, BLE_CTAP_REQ* req)
{
    
    CTAP_RESPONSE resp;
    ctap_response_init(&resp);
    uint8_t status;
    uint8_t * sendbuffer;

    status = ctap_request(req->data, (int) req->length, &resp);

    sendbuffer = nrf_malloc(resp.length+1);

    *sendbuffer = status; //cpy status
    memcpy(sendbuffer+1, resp.data, resp.length);

    ble_ctap_send_resp(p_ctap, req->cmd, sendbuffer, resp.length + 1, true);

    nrf_free(sendbuffer);
}


/***************************************************************************** 
*							INTERFACE IMPL
*****************************************************************************/

void ble_init()
{
    NRF_LOG_INFO("ble init");
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();
    peer_manager_init();

    advertising_start(!APP_PERSISTENT_MODE);
}

void ble_process()
{
    ble_ctap_process(&m_ctap);
}

