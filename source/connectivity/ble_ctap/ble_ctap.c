/******************************************************************************
* File:             ble_ctap.c
*
* Author:           Joshua Steffensky <joshua.steffensky@cispa.saarland>
* Created:          06/10/20 
* Description:      implemting ble gatt ctap service
*****************************************************************************/

#include "ble_ctap.h"
#include "fido_interfaces.h"
#include "hal.h"

#include "string.h"

#include "mem_manager.h"
#include "delay/nrf_delay.h"

/*  LOG INIT  */
#define NRF_LOG_MODULE_NAME ble_ctap

#include "nrf_log.h"
#include "nrf_log_ctrl.h"

NRF_LOG_MODULE_REGISTER();
/*            */

/***************************************************************************** 
*							INTERNAL PROTOTYPES
*****************************************************************************/

/**@brief Function for handling writes to ctap control point.
 *
 * @param[in]   p_ctap                  ctap service structure
 * @param[in]   ble_gatts_evt_write_t   write event
 */
static void ble_ctap_on_ctrl_pt_write(
    ble_ctap_t *                 p_ctap,
    ble_gatts_evt_write_t const *p_evt_write);

/**@brief Function for handling writes to ctap revision bitfield.
 *
 * @param[in]   p_ctap                  ctap service structure
 * @param[in]   ble_gatts_evt_write_t   write event
 */
static void ble_ctap_on_revision_bitfield_write(
    ble_ctap_t *                 p_ctap,
    ble_gatts_evt_write_t const *p_evt_write);

/**@brief Function for cleaning the recving datastructures in the ctap service structure
 *
 * @param[in]   p_ctap                  ctap service structure
 */
static void ble_ctap_clear_recv(ble_ctap_t *p_ctap);


/**@brief ble ctap write event handler
 *
 * @param[in]   p_ctap                  ctap service structure
 * @param[in]   p_ble_evt               ble event
 */
static void ble_ctap_on_write(ble_ctap_t *p_ctap, ble_evt_t const *p_ble_evt);

/**@brief ble ctap connect event handler
 *
 * @param[in]   p_ctap                  ctap service structure
 * @param[in]   p_ble_evt               ble event
 */
static void ble_ctap_on_connect(ble_ctap_t *p_ctap, ble_evt_t const *p_ble_evt);

/**@brief ble ctap disconnect event handler
 *
 * @param[in]   p_ctap                  ctap service structure
 * @param[in]   p_ble_evt               ble event
 */
static void
    ble_ctap_on_disconnect(ble_ctap_t *p_ctap, ble_evt_t const *p_ble_evt);

/**@brief ble power manage until at least 1 transmission is completed
 */
static void ble_ctap_waitforsendcmpl();

/** 
 * @brief		internal function for sending a single ble frame
 *
 * @param[in] 	conn_handle	    ble connection handle
 * @param[in] 	char_handle	    characteristics handle to send notification from
 * @param[in] 	p_frame 	    frame to be send
 * @param[in] 	p_length	    length of the to be send frame (len(p_frame))
 * 
 * @return 		return nrf-sdk ret_code_t
 */
static ret_code_t ble_ctap_frame_send(
    uint16_t                 conn_handle,
    ble_gatts_char_handles_t char_handle,
    BLE_CTAP_FRAME *         p_frame,
    uint16_t *               p_length,
    bool                     reqcmpl);


/***************************************************************************** 
*							GLOBALS
*****************************************************************************/

volatile bool m_hvx_pending = false;


/***************************************************************************** 
*							BLE_CTAP IMPL
*****************************************************************************/

uint32_t ble_ctap_init(ble_ctap_t *p_ctap, ble_ctap_init_t const *p_ctap_init)
{
    VERIFY_PARAM_NOT_NULL(p_ctap);
    VERIFY_PARAM_NOT_NULL(p_ctap_init);
    /**VERIFY_PARAM_NOT_NULL(p_ctap_init->p_gatt_queue);*/

    uint32_t              err_code;
    ble_uuid_t            ble_srv_uuid;
    ble_add_char_params_t add_char_params;
    uint8_t               ctrl_point_length_init[2] = {0};
    uint16_big_encode(BLE_CTAP_CTRL_PT_LENGTH, ctrl_point_length_init);
    uint8_t revision_bitfield = 0;

    /**p_ctap->evt_handler = p_ctap_init->evt_handler;*/
    /**p_ctap->error_handler = p_ctap_init->error_handler;*/
    /**p_ctap->p_gatt_queue = p_ctap_init->p_gatt_queue;*/
    p_ctap->conn_handle     = BLE_CONN_HANDLE_INVALID;
    p_ctap->request_handler = p_ctap_init->request_handler;
    p_ctap->m_gatt          = p_ctap_init->m_gatt;

    // Add service
    BLE_UUID_BLE_ASSIGN(ble_srv_uuid, BLE_CTAP_GATT_SERVICE_UUID);

    err_code = sd_ble_gatts_service_add(
        BLE_GATTS_SRVC_TYPE_PRIMARY,
        &ble_srv_uuid,
        &p_ctap->service_handle);

    if (err_code != NRF_SUCCESS)
    {
        APP_ERROR_CHECK(err_code);
        return err_code;
    }

    //add characteristics
    //set base uuid
    ble_uuid128_t base_uuid = {BLE_CTAP_CHARACTERISTICS_UUID_BASE};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_ctap->chr_uuid_type);
    APP_ERROR_CHECK(err_code);


    //ctrl point
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = BLE_CTAP_CONTROL_POINT_UUID;
    add_char_params.uuid_type         = p_ctap->chr_uuid_type;
    add_char_params.is_var_len        = true;
    add_char_params.init_len          = 1;
    add_char_params.max_len           = BLE_CTAP_CTRL_PT_LENGTH;
    add_char_params.char_props.read   = false;
    add_char_params.char_props.notify = false;
    add_char_params.char_props.write  = true;
    //    add_char_params.char_props.write_wo_resp = true;

    /**add_char_params.read_access         = SEC_JUST_WORKS;//TODO*/
    add_char_params.write_access = SEC_JUST_WORKS;  //TODO

    err_code = characteristic_add(
        p_ctap->service_handle,
        &add_char_params,
        &p_ctap->ctrl_pt_handle);
    if (err_code != NRF_SUCCESS)
    {
        APP_ERROR_CHECK(err_code);
        return err_code;
    }

    //ctrl point length
    NRF_LOG_DEBUG(
        "ctrl point length: %x, 0x%x %x",
        BLE_CTAP_CTRL_PT_LENGTH,
        ctrl_point_length_init[0],
        ctrl_point_length_init[1]);
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = BLE_CTAP_CONTROL_POINT_LENGTH_UUID;
    add_char_params.uuid_type         = p_ctap->chr_uuid_type;
    add_char_params.max_len           = 2;
    add_char_params.init_len          = 2;
    add_char_params.p_init_value      = ctrl_point_length_init;
    add_char_params.char_props.read   = true;
    add_char_params.char_props.notify = false;
    add_char_params.char_props.write  = false;
    // add_char_params.is_defered_read     = true;


    add_char_params.read_access = SEC_JUST_WORKS;

    err_code = characteristic_add(
        p_ctap->service_handle,
        &add_char_params,
        &p_ctap->ctrl_length_handle);
    if (err_code != NRF_SUCCESS)
    {
        APP_ERROR_CHECK(err_code);
        return err_code;
    }

    //Status
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = BLE_CTAP_STATUS_UUID;
    add_char_params.uuid_type         = p_ctap->chr_uuid_type;
    add_char_params.is_var_len        = true;
    add_char_params.init_len          = 1;
    add_char_params.max_len           = BLE_CTAP_STATUS_LENGTH;
    add_char_params.char_props.notify = true;

    add_char_params.read_access       = SEC_JUST_WORKS;  //TODO
    add_char_params.write_access      = SEC_JUST_WORKS;  //TODO
    add_char_params.cccd_write_access = SEC_JUST_WORKS;

    err_code = characteristic_add(
        p_ctap->service_handle,
        &add_char_params,
        &p_ctap->status_handle);
    if (err_code != NRF_SUCCESS)
    {
        APP_ERROR_CHECK(err_code);
        return err_code;
    }

    //Service Revision Bitfield
    revision_bitfield |= CTAP_SERVICE_REVISION_BITFIELD_CTAP2;
    //TODO enable U2F 1.1/1.2
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid      = BLE_CTAP_SERVICE_REVISION_BITFIELD_UUID;
    add_char_params.uuid_type = p_ctap->chr_uuid_type;
    /**add_char_params.is_var_len          = false;*/
    add_char_params.init_len          = 1;
    add_char_params.max_len           = 1;
    add_char_params.p_init_value      = &revision_bitfield;
    add_char_params.char_props.read   = true;
    add_char_params.char_props.notify = false;
    add_char_params.char_props.write  = true;
    //    add_char_params.is_defered_read     = true;

    add_char_params.read_access  = SEC_JUST_WORKS;  //TODO
    add_char_params.write_access = SEC_JUST_WORKS;  //TODO

    err_code = characteristic_add(
        p_ctap->service_handle,
        &add_char_params,
        &p_ctap->revision_bitfield_handle);
    if (err_code != NRF_SUCCESS)
    {
        APP_ERROR_CHECK(err_code);
        return err_code;
    }

    /* Service Revision Characteristics omitted as U2F 1.0 is not supported */

    return NRF_SUCCESS;
}


static void ble_ctap_on_ctrl_pt_write(
    ble_ctap_t *                 p_ctap,
    ble_gatts_evt_write_t const *p_evt_write)
{
    //    CRITICAL_REGION_ENTER();
    NRF_LOG_DEBUG("on_ctrl_pt_write");
    NRF_LOG_DEBUG("received %d bytes", p_evt_write->len);
    //    NRF_LOG_HEXDUMP_DEBUG(p_evt_write->data, p_evt_write->len);
    /**NRF_LOG_FLUSH();*/

    BLE_CTAP_FRAME *f;
    size_t          tocpy;
    //    memset(&f, 0, sizeof(BLE_CTAP_FRAME));
    //    NRF_LOG_DEBUG("checkpoint %d", __LINE__);

    //check length
    //    assert(false);
    //    assert(p_evt_write->len <= sizeof(BLE_CTAP_FRAME) && p_evt_write->len > 1);
    //    memcpy(&f, p_evt_write->data, p_evt_write->len);
    f = (BLE_CTAP_FRAME *) p_evt_write->data;
    NRF_LOG_DEBUG("checkpoint %d", __LINE__);
    //cast to BLE_CTAP_FRAME
    //if initialization fragment
    if (FRAME_TYPE(*f) == TYPE_INIT)
    {
        //check for ongoing fragment
        if (p_ctap->waitingforContinuation)  // && !p_ctap->readyfordispatch){
        {
            NRF_LOG_DEBUG("[%d] were not waitting to cont pkg", __LINE__);
            ble_ctap_send_err(p_ctap, BLE_CTAP_ERR_INVALID_SEQ);
            ble_ctap_clear_recv(p_ctap);  // clean state
            return;
        };
        NRF_LOG_DEBUG("checkpoint %d", __LINE__);
        /**NRF_LOG_FLUSH();*/

        //alloc BLE_CTAP_REQ
        p_ctap->dispatchPackage =
            (BLE_CTAP_REQ *) nrf_malloc(sizeof(BLE_CTAP_REQ));

        //set cmd
        p_ctap->dispatchPackage->cmd = f->init.cmd;

        //set length
        p_ctap->dispatchPackage->length = MSG_LEN(*f);
        NRF_LOG_DEBUG("msg len %d", p_ctap->dispatchPackage->length);
        //move data
        tocpy = MIN(p_evt_write->len - 3, p_ctap->dispatchPackage->length);
        memcpy((void *) p_ctap->dispatchPackage->data, f->init.data, tocpy);

        NRF_LOG_DEBUG("checkpoint %d", __LINE__);
        //set offset
        p_ctap->recvoffset = tocpy;

        //set next seq number
        p_ctap->recvnextseq = 0x00;

        //set waiting for cont
        p_ctap->waitingforContinuation = true;
    }
    //if continuation fragment
    else
    {
        NRF_LOG_DEBUG("checkpoint %d", __LINE__);
        // check for ongoing fragment
        if (!p_ctap->waitingforContinuation
            || p_ctap->readyfordispatch)  // && !p_ctap->readyfordispatch) {
        {
            NRF_LOG_DEBUG(
                "[%d] waiting: %d, ready: %d",
                __LINE__,
                p_ctap->waitingforContinuation,
                p_ctap->readyfordispatch);
            /**NRF_LOG_FLUSH();*/
            ble_ctap_send_err(p_ctap, BLE_CTAP_ERR_INVALID_SEQ);
            ble_ctap_clear_recv(p_ctap);  // clean state
            return;
        }

        // check sequence number
        if (p_ctap->recvnextseq != FRAME_SEQ(*f))
        {
            NRF_LOG_DEBUG(
                "[%d] wrong seq %d, expected %d",
                __LINE__,
                FRAME_SEQ(*f),
                p_ctap->recvnextseq);
            ble_ctap_send_err(p_ctap, BLE_CTAP_ERR_INVALID_SEQ);
            ble_ctap_clear_recv(p_ctap);  // clean state
            return;
        }

        // move data
        tocpy =
            MIN(p_evt_write->len - 1,
                p_ctap->dispatchPackage->length - p_ctap->recvoffset);
        memcpy(
            ((void *) p_ctap->dispatchPackage->data) + p_ctap->recvoffset,
            f->cont.data,
            tocpy);
        NRF_LOG_DEBUG("checkpoint %d", __LINE__);

        // update offset
        p_ctap->recvoffset += tocpy;

        // update next seq
        p_ctap->recvnextseq++;
    }
    // check for readyfordispatch
    p_ctap->readyfordispatch =
        (p_ctap->recvoffset == p_ctap->dispatchPackage->length);
    NRF_LOG_DEBUG(
        "checkpoint %d, readyfordispatch: %d",
        __LINE__,
        p_ctap->readyfordispatch);

    //exit:
    //    CRITICAL_REGION_EXIT();
    return;
}

static void ble_ctap_clear_recv(ble_ctap_t *p_ctap)
{
    if (p_ctap->dispatchPackage != NULL)
        nrf_free((void *) p_ctap->dispatchPackage);
    p_ctap->dispatchPackage        = NULL;
    p_ctap->waitingforContinuation = false;
    p_ctap->readyfordispatch       = false;
    p_ctap->recvoffset             = 0;
    p_ctap->recvnextseq            = 0;
}

static void ble_ctap_on_revision_bitfield_write(
    ble_ctap_t *                 p_ctap,
    ble_gatts_evt_write_t const *p_evt_write)
{
    //TODO
    //if the unsupported bit is written disconnect ???
}

static void ble_ctap_on_connect(ble_ctap_t *p_ctap, ble_evt_t const *p_ble_evt)
{
    /**assert(p_ctap->conn_handle == BLE_CONN_HANDLE_INVALID);*/
    p_ctap->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    //TODO
}

static void
    ble_ctap_on_disconnect(ble_ctap_t *p_ctap, ble_evt_t const *p_ble_evt)
{
    /**assert(p_ctap->conn_handle != BLE_CONN_HANDLE_INVALID);*/
    p_ctap->conn_handle = BLE_CONN_HANDLE_INVALID;
    //TODO
}

static void ble_ctap_on_write(ble_ctap_t *p_ctap, ble_evt_t const *p_ble_evt)
{
    ble_gatts_evt_write_t const *p_evt_write =
        &p_ble_evt->evt.gatts_evt.params.write;

    if (p_evt_write->handle == p_ctap->revision_bitfield_handle.value_handle)
    {
        ble_ctap_on_revision_bitfield_write(p_ctap, p_evt_write);
    }
    if (p_evt_write->handle == p_ctap->ctrl_pt_handle.value_handle)
    {
        ble_ctap_on_ctrl_pt_write(p_ctap, p_evt_write);
    }
}

void ble_ctap_on_ble_evt(ble_evt_t const *p_ble_evt, void *p_context)
{
    //    NRF_LOG_DEBUG("ble_ctap_on_ble_evt: 0x%x", p_ble_evt->header.evt_id);
    ble_ctap_t *p_ctap = (ble_ctap_t *) p_context;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_DEBUG("ble_ctap connected");
            ble_ctap_on_connect(p_ctap, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            ble_ctap_on_disconnect(p_ctap, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE: ble_ctap_on_write(p_ctap, p_ble_evt); break;

        case BLE_GATTS_EVT_HVN_TX_COMPLETE: m_hvx_pending = false; break;

        default:
            // No implementation needed.
            break;
    }
}

void ble_ctap_send_err(ble_ctap_t *p_ctap, uint8_t err_code)
{
    ble_ctap_send_resp(p_ctap, BLE_CTAP_CMD_ERROR, &err_code, 1, false);
}


void ble_ctap_send_resp(
    ble_ctap_t *p_ctap,
    uint8_t     cmd,
    uint8_t *   p_data,
    size_t      size,
    bool        reqcomplete)
{
    ret_code_t     err_code;
    uint8_t        nextseq;
    size_t         frameLen;
    uint16_t       payloadLen;
    BLE_CTAP_FRAME frame;
    size_t         eff_mtu =
        (size_t) nrf_ble_gatt_eff_mtu_get(p_ctap->m_gatt, p_ctap->conn_handle);
    NRF_LOG_DEBUG("eff mtu: %d", eff_mtu);

    //INIT variables
    nextseq = 0x00;

    // Construct init frame
    memset(&frame, 0, sizeof(BLE_CTAP_FRAME));

    frame.init.cmd   = TYPE_INIT | cmd;
    frame.init.bcnth = (size >> 8) & 0xFF;
    frame.init.bcntl = (size & 0xFF);

    frameLen = MIN(size, eff_mtu - (3 + 3));
    /**frameLen = MIN(size, sizeof(frame.init.data));*/
    payloadLen = (size_t) frameLen + 3;
    memset(frame.init.data, 0, sizeof(frame.init.data));
    memcpy(frame.init.data, p_data, frameLen);

    do
    {
        NRF_LOG_DEBUG("send frame len: %d", frameLen)
        // send frame
        err_code = ble_ctap_frame_send(
            p_ctap->conn_handle,
            p_ctap->status_handle,
            &frame,
            &payloadLen,
            reqcomplete);
        APP_ERROR_CHECK(err_code);
        m_hvx_pending = true;

        // update counters
        size -= frameLen;
        p_data += frameLen;

        // construct cont frame
        frame.cont.seq = nextseq++;
        if (nextseq & TYPE_MASK)
            nextseq = 0x00;  //overflow seq
        frameLen = MIN(size, eff_mtu - (3 + 1));
        /**frameLen = MIN(size, sizeof(frame.cont.data));*/
        payloadLen = frameLen + 1;
        memset(frame.cont.data, 0, sizeof(frame.cont.data));
        memcpy(frame.cont.data, p_data, frameLen);
    } while (size);

    //wait for send complete
    if (reqcomplete)
    {
        ble_ctap_waitforsendcmpl();
    }
}

static void ble_ctap_waitforsendcmpl()
{
    while (m_hvx_pending) { power_manage(); }
}

static ret_code_t ble_ctap_frame_send(
    uint16_t                 conn_handle,
    ble_gatts_char_handles_t char_handle,
    BLE_CTAP_FRAME *         p_frame,
    uint16_t *               p_length,
    bool                     reqcmpl)
{
    ret_code_t             err_code;
    ble_gatts_hvx_params_t hvx_params;


    memset(&hvx_params, 0, sizeof(hvx_params));
    hvx_params.handle = char_handle.value_handle;
    hvx_params.p_data = (uint8_t *) p_frame;
    hvx_params.p_len  = p_length;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;

    err_code = sd_ble_gatts_hvx(conn_handle, &hvx_params);
    while (err_code == NRF_ERROR_RESOURCES)
    {  // just busy ... wait
        /**assert(m_hvx_pending == true);*/
        if (reqcmpl)
        {
            ble_ctap_waitforsendcmpl();
        }
        else
        {
            nrf_delay_ms(10);
        }
        err_code = sd_ble_gatts_hvx(conn_handle, &hvx_params);
    }

    return err_code;
}


bool ble_ctap_process(ble_ctap_t *p_ctap)
{
    VERIFY_PARAM_NOT_NULL(p_ctap->request_handler);
    if (p_ctap->readyfordispatch)  // message to handle
    {
        NRF_LOG_INFO("request received, need to handle");

        p_ctap->request_handler(
            p_ctap,
            (BLE_CTAP_REQ *) p_ctap->dispatchPackage);
        ble_ctap_clear_recv(p_ctap);
        return true;
    }
    return false;
}
