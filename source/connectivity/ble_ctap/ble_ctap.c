/******************************************************************************
* File:             ble_ctap.c
*
* Author:           Joshua Steffensky <joshua.steffensky@cispa.saarland>
* Created:          06/10/20 
* Description:      implemting ble gatt ctap service
*****************************************************************************/

#include "ble_ctap/ble_ctap.h"

#include "string.h"


/*  LOG INIT  */
#define NRF_LOG_MODULE_NAME ble_ctap

#include "nrf_log.h"
#include "nrf_log_ctrl.h"

NRF_LOG_MODULE_REGISTER();
/*            */

/***************************************************************************** 
*							BLE_CTAP IMPL
*****************************************************************************/

uint32_t ble_ctap_init(ble_ctap_t * p_fido, ble_ctap_init_t const * p_fido_init)
{
    VERIFY_PARAM_NOT_NULL(p_fido);
    VERIFY_PARAM_NOT_NULL(p_fido_init);
    /**VERIFY_PARAM_NOT_NULL(p_fido_init->p_gatt_queue);*/

    uint32_t    err_code;
    ble_uuid_t  ble_srv_uuid;
    ble_add_char_params_t add_char_params;
    uint8_t     ctrl_point_length_init[2] = {0};
    uint16_big_encode 	(
                        BLE_CTAP_CTRL_PT_LENGTH,
        		        ctrl_point_length_init
	                    );
    uint8_t     revision_bitfield = 0 ;

    /**p_fido->evt_handler = p_fido_init->evt_handler;*/
    /**p_fido->error_handler = p_fido_init->error_handler;*/
    /**p_fido->p_gatt_queue = p_fido_init->p_gatt_queue;*/
    p_fido->conn_handle = BLE_CONN_HANDLE_INVALID;

    // Add service
    BLE_UUID_BLE_ASSIGN(ble_srv_uuid, BLE_CTAP_GATT_SERVICE_UUID);
    
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_srv_uuid,
                                        &p_fido->service_handle);
    
    if (err_code != NRF_SUCCESS)
    {
        APP_ERROR_CHECK(err_code);
        return err_code;
    }

    //add characteristics
    //set base uuid
    ble_uuid128_t base_uuid = {BLE_CTAP_CHARACTERISTICS_UUID_BASE};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_fido->chr_uuid_type);
    APP_ERROR_CHECK(err_code);

    
    //ctrl point
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid                = BLE_CTAP_CONTROL_POINT_UUID;
    add_char_params.uuid_type           = p_fido->chr_uuid_type;
    add_char_params.is_var_len          = true;
    add_char_params.init_len            = 1;
    add_char_params.max_len             = BLE_CTAP_CTRL_PT_LENGTH;
    add_char_params.char_props.read     = false;
    add_char_params.char_props.notify   = false;
    add_char_params.char_props.write    = true;
//    add_char_params.char_props.write_wo_resp = true;

    /**add_char_params.read_access         = SEC_JUST_WORKS;//TODO*/
    add_char_params.write_access        = SEC_JUST_WORKS;//TODO

    err_code = characteristic_add(p_fido->service_handle,
                                    &add_char_params,
                                    &p_fido->ctrl_pt_handle);
    if(err_code != NRF_SUCCESS)
    {
        APP_ERROR_CHECK(err_code);
        return err_code;
    }

    //ctrl point length
    NRF_LOG_DEBUG("ctrl point length: %x, 0x%x %x", BLE_CTAP_CTRL_PT_LENGTH, ctrl_point_length_init[0], ctrl_point_length_init[1]);
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid                = BLE_CTAP_CONTROL_POINT_LENGTH_UUID;
    add_char_params.uuid_type           = p_fido->chr_uuid_type;
    add_char_params.max_len             = 2;
    add_char_params.init_len            = 2;
    add_char_params.p_init_value        = ctrl_point_length_init;
    add_char_params.char_props.read     = true;
    add_char_params.char_props.notify   = false;
    add_char_params.char_props.write    = false;
   // add_char_params.is_defered_read     = true;


    add_char_params.read_access         = SEC_JUST_WORKS;

    err_code = characteristic_add(p_fido->service_handle,
                                    &add_char_params,
                                    &p_fido->ctrl_length_handle);
    if(err_code != NRF_SUCCESS)
    {
        APP_ERROR_CHECK(err_code);
        return err_code;
    }

    //Status
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid                = BLE_CTAP_STATUS_UUID;
    add_char_params.uuid_type           = p_fido->chr_uuid_type;
    add_char_params.is_var_len          = true;
    add_char_params.init_len            = 1;
    add_char_params.max_len             = BLE_CTAP_STATUS_LENGTH;
    add_char_params.char_props.notify   = true;

    add_char_params.read_access         = SEC_JUST_WORKS;//TODO
    add_char_params.write_access        = SEC_JUST_WORKS;//TODO
    add_char_params.cccd_write_access   = SEC_JUST_WORKS;

    err_code = characteristic_add(p_fido->service_handle,
                                    &add_char_params,
                                    &p_fido->status_handle );
    if(err_code != NRF_SUCCESS)
    {
        APP_ERROR_CHECK(err_code);
        return err_code;
    }

    //Service Revision Bitfield
    revision_bitfield |= FIDO_SERVICE_REVISION_BITFIELD_FIDO2;
    //TODO enable U2F 1.1/1.2
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid                = BLE_CTAP_SERVICE_REVISION_BITFIELD_UUID;
    add_char_params.uuid_type           = p_fido->chr_uuid_type;
    /**add_char_params.is_var_len          = false;*/
    add_char_params.init_len            = 1;
    add_char_params.max_len             = 1;
    add_char_params.p_init_value        = &revision_bitfield;
    add_char_params.char_props.read     = true;
    add_char_params.char_props.notify   = false;
    add_char_params.char_props.write    = true;
//    add_char_params.is_defered_read     = true;

    add_char_params.read_access         = SEC_JUST_WORKS;//TODO
    add_char_params.write_access        = SEC_JUST_WORKS;//TODO

    err_code = characteristic_add(p_fido->service_handle,
                                    &add_char_params,
                                    &p_fido->revision_bitfield_handle );
    if(err_code != NRF_SUCCESS)
    {
        APP_ERROR_CHECK(err_code);
        return err_code;
    }
    
    /* Service Revision Characteristics omitted as U2F 1.0 is not supported */
    
    return NRF_SUCCESS;
}


static void ble_ctap_on_ctrl_pt_write(ble_ctap_t * p_fido, ble_gatts_evt_write_t const * p_evt_write)
{
    BLE_FRAME f;
    size_t tocpy;
    memset(&f, 0, sizeof(BLE_FRAME));
    
    //check length
    assert(p_evt_write->len <= sizeof(BLE_FRAME) && p_evt_write->len > 1);
    memcpy(&f, p_evt_write->data, p_evt_write->len);

    //cast to BLE_FRAME
    //if initialization fragment 
    if(FRAME_TYPE(f) == TYPE_INIT)
    {
        //check for ongoing fragment
        if(p_fido->waitingforContinuation && !p_fido->readyfordispatch){
            ble_ctap_send_err(p_fido, BLE_ERR_INVALID_SEQ);
            ble_ctap_clear_recv(p_fido); // clean state
            return;
        };

        //alloc BLE_REQ
        p_fido->dispatchPackage = (BLE_REQ *) nrf_malloc(sizeof(BLE_REQ));
        
        //set cmd
        p_fido->dispatchPackage->cmd = f.init.cmd;
        
        //set length
        p_fido->dispatchPackage->length = MSG_LEN(f);

        //move data
        tocpy = MIN(p_evt_write->len - 3, p_fido->dispatchPackage->length);
        memcpy((void *)p_fido->dispatchPackage->data, f.init.data, tocpy);
        
        //set offset
        p_fido->recvoffset = tocpy;

        //set next seq number
        p_fido->recvnextseq = 0x00;
    }
    //if continuation fragment
    else
    {
        // check for ongoing fragment
        if (!p_fido->waitingforContinuation && !p_fido->readyfordispatch) {
            ble_ctap_send_err(p_fido, BLE_ERR_INVALID_SEQ);
            ble_ctap_clear_recv(p_fido); // clean state
            return;
        }
        
        // check sequence number
        if (!(p_fido->recvnextseq != FRAME_SEQ(f))){
            ble_ctap_send_err(p_fido, BLE_ERR_INVALID_SEQ);
            ble_ctap_clear_recv(p_fido); // clean state
            return;
        }

        // move data
        tocpy = MIN(p_evt_write->len - 1, p_fido->dispatchPackage->length - p_fido->recvoffset);
        memcpy( ((void *)p_fido->dispatchPackage->data) + p_fido->recvoffset, f.cont.data, tocpy);

        // update offset
        p_fido->recvoffset += tocpy;

        // update next seq
        p_fido->recvnextseq++;
    }
    // check for readyfordispatch
    p_fido->readyfordispatch = (p_fido->recvoffset == p_fido->dispatchPackage->length);
}

static void ble_ctap_clear_recv(ble_ctap_t * p_fido)
{
    nrf_free((void *)p_fido->dispatchPackage);
    p_fido->dispatchPackage = NULL;
    p_fido->waitingforContinuation = false;
    p_fido->readyfordispatch = false;
    p_fido->recvoffset = 0;
    p_fido->recvnextseq = 0;
}

static void ble_ctap_on_revision_bitfield_write(ble_ctap_t * p_fido, ble_gatts_evt_write_t const * p_evt_write)
{
    //TODO
    //if the unsupported bit is written disconnect ???
}

static void ble_ctap_on_connect(ble_ctap_t * p_fido, ble_evt_t const * p_ble_evt)
{
    assert(p_fido->conn_handle == BLE_CONN_HANDLE_INVALID);
    p_fido->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    //TODO
}

static void ble_ctap_on_disconnect(ble_ctap_t * p_fido, ble_evt_t const * p_ble_evt)
{
    assert(p_fido->conn_handle != BLE_CONN_HANDLE_INVALID);
    p_fido->conn_handle = BLE_CONN_HANDLE_INVALID;
    //TODO
}

static void ble_ctap_on_write(ble_ctap_t * p_fido, ble_evt_t const * p_ble_evt)
{
    ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if (p_evt_write->handle == p_fido->revision_bitfield_handle.value_handle)
    {
        ble_ctap_on_revision_bitfield_write(p_fido, p_evt_write);
    }
    if (p_evt_write->handle == p_fido->ctrl_pt_handle.value_handle){
        ble_ctap_on_ctrl_pt_write(p_fido, p_evt_write);
    }
}

void ble_ctap_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
    NRF_LOG_DEBUG("ble_ctap_on_ble_evt: 0x%x", p_ble_evt->header.evt_id);
    ble_ctap_t * p_fido = (ble_ctap_t *)p_context;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_DEBUG("ble_ctap connected");
            ble_ctap_on_connect(p_fido, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            ble_ctap_on_disconnect(p_fido, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            ble_ctap_on_write(p_fido, p_ble_evt);
            break;
        
        case BLE_GATTS_EVT_HVN_TX_COMPLETE:
            m_hvx_pending = false;
            break;

        default:
            // No implementation needed.
            break;
    }
}

void ble_ctap_send_err(ble_ctap_t * p_fido, uint8_t err_code){
    ble_ctap_send_resp(p_fido, BLE_CMD_ERROR, &err_code, 1);
}


void ble_ctap_send_resp(ble_ctap_t * p_fido, uint8_t cmd, uint8_t * p_data, size_t size)
{
    ret_code_t  err_code;
    uint8_t     nextseq;
    size_t      frameLen;
    uint16_t    payloadLen;
    BLE_FRAME   frame;

    
    //INIT variables
    nextseq = 0x00;
    
    // Construct init frame
    memset(&frame, 0, sizeof(BLE_FRAME));
    
    frame.init.cmd = TYPE_INIT | cmd;
    frame.init.bcnth = (size >> 8) & 0xFF;
    frame.init.bcntl = (size & 0xFF);
    
    frameLen = MIN(size, sizeof(frame.init.data));
    payloadLen = (size_t)frameLen + 3;
    memset(frame.init.data, 0, sizeof(frame.init.data));
    memcpy(frame.init.data, p_data, frameLen);
    
    do
    {
        // send frame
        err_code = ble_frame_send(p_fido->conn_handle, p_fido->status_handle, &frame, &payloadLen);
        APP_ERROR_CHECK(err_code);
        m_hvx_pending = true;
        
        // update counters
        size -= frameLen;
        p_data += frameLen;

        // construct cont frame
        frame.cont.seq = nextseq++;
        if(nextseq & TYPE_MASK) nextseq = 0x00; //overflow seq
        frameLen = MIN(size, sizeof(frame.cont.data));
        payloadLen = frameLen + 1;
        memset(frame.cont.data, 0, sizeof(frame.cont.data));
        memcpy(frame.cont.data, p_data, frameLen);
    } while (size);

    //wait for send complete
    ble_waitforsendcmpl();
}

static void ble_waitforsendcmpl()
{
    while(m_hvx_pending){
        power_manage();
    }
}

static ret_code_t ble_frame_send(uint16_t conn_handle,  ble_gatts_char_handles_t char_handle, BLE_FRAME * p_frame, uint16_t * p_length)
{
    ret_code_t                  err_code;
    ble_gatts_hvx_params_t      hvx_params;


    memset(&hvx_params, 0, sizeof(hvx_params));
    hvx_params.handle = char_handle.value_handle;
    hvx_params.p_data = (uint8_t *) p_frame;
    hvx_params.p_len  = p_length;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
    
    err_code = sd_ble_gatts_hvx(conn_handle, &hvx_params);
    while (err_code == NRF_ERROR_RESOURCES){ // just busy ... wait
        assert(m_hvx_pending == true);
        ble_waitforsendcmpl();
        err_code = sd_ble_gatts_hvx(conn_handle, &hvx_params);
    }

    return err_code;
}
