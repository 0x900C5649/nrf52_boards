
#include "fido_hid.h"
#include "fido_interfaces.h"
#include "util.h"
#include "hal.h"

#include "timer_interface.h"
#include "ctap.h"
#include "u2f.h"

#include "mem_manager.h"
#include "app_timer.h"

/*  LOG INIT  */
#define NRF_LOG_MODULE_NAME hid

#include "nrf_log.h"
#include "nrf_log_ctrl.h"

NRF_LOG_MODULE_REGISTER();
/*            */


// static declaration

/**
 * @brief Initialize USB HID interface.
 *
 * @return Standard error code.
 */
static uint32_t hid_if_init(void);


/**
 * @brief Send CTAP HID Data.
 *
 *
 * @param[in] cid       HID Channel identifier.
 * @param[in] cmd       Frame command.
 * @param[in] p_data    Frame Data packet.
 * @param[in] size      Data length
 *
 * @return Standard error code.
 */
static uint8_t hid_if_send(uint32_t cid, uint8_t cmd,
                        uint8_t * p_data, size_t size);


/**
 * @brief Receive CTAP HID Data.
 *
 *
 * @param[out] p_cid       HID Channel identifier.
 * @param[out] p_cmd       Frame command.
 * @param[out] p_data      Frame Data packet.
 * @param[out] p_size      Data length
 * @param[in]  timeout     message timeout in ms
 *
 * @return Standard error code.
 */
static uint8_t hid_if_recv(uint32_t * p_cid, uint8_t * p_cmd,
                    uint8_t * p_data, size_t * p_size,
                    uint32_t timeout);

/**
 * @brief CTAP HID interface process.
 *
 *
 */
static void hid_if_process(void);

/**
 * @brief USBD library specific event handler.
 *
 * @param event     USBD library event.
 * */
static void usbd_user_ev_handler(app_usbd_event_type_t event);

/**
 * @brief User event handler.
 * */
static void hid_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                app_usbd_hid_user_event_t event);


/**
 * @brief handler for idle reports.
 *
 * @param p_inst      Class instance.
 * @param report_id   Number of report ID that needs idle transfer.
 * */
static ret_code_t idle_handle(app_usbd_class_inst_t const * p_inst,
                              uint8_t report_id);

/**
 * \brief send one HID frame.
 */
static uint8_t hid_if_frame_send(HID_FRAME * p_frame);

/**
 * @brief HID process function, which should be executed when data is ready.
 *
 */
static void internal_hid_process(void); //TODO

/**
 * @brief Function for initializing the HID.
 *
 * @return Error status.
 *
 */
static ret_code_t internal_hid_init(void); //TODO

/**@brief Process HID command of every ready channel.
 * 
 */
static void channel_process(void);

/**@brief Process HID command
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void channel_cmd_process(hid_channel_t * p_ch);


/**@brief Handle a HID LOCK response
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_lock_response(hid_channel_t *p_ch);

/**@brief Handle a HID SYNC response
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_sync_response(hid_channel_t *p_ch);

/**@brief Handle a HID PING response
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_ping_response(hid_channel_t *p_ch);

/**@brief Handle a HID MESSAGE response
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_msg_response(hid_channel_t * p_ch);

/**@brief Send a HID status code only
 *
 * @param[in]  p_ch    Pointer to Channel.
 * @param[in]  status  HID status code.
 *
 */
static void hid_status_response(hid_channel_t * p_ch, uint8_t status);


/**@brief Handle a HID WINK response
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_wink_response(hid_channel_t *p_ch);

/**@brief Handle a HID INIT response
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_init_response(hid_channel_t *p_ch);

/**@brief Handle a CTAPHID CBOR response
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_cbor_response(hid_channel_t *p_ch);

/**@brief Send a HID_ERROR response
 *
 * @param[in]  cid   Channel identifier.
 * @param[in]  code  Error code.
 * 
 */
static void hid_error_response(uint32_t cid, uint8_t error);

/**@brief Generate new Channel identifier.
 *
 *
 * @retval     New Channel identifier.
 */
static uint32_t generate_new_cid(void);

/**@brief Find the Channel by cid.
 *
 * @param[in]  cid  Channel identifier.
 *
 * @retval     Valid Channel if the procedure was successful, else, NULL.
 */
static hid_channel_t * channel_find(uint32_t cid);

/**@brief Uninitialize Channel.
 *
 * @param[in]  p_ch  Pointer to Channel.
 *
 */
static void channel_deinit(hid_channel_t * p_ch);

/**@brief Initialize Channel.
 *
 * @param[in]  p_ch  Pointer to Channel.
 * @param[in]  cid   Channel identifier.
 *
 */
static void channel_init(hid_channel_t * p_ch, uint32_t cid);

/**@brief Resets Channel. Keeps cid;
 *
 * @param[in]  p_ch  Pointer to Channel.
 *
 */
static void channel_reset(hid_channel_t * p_ch);

/**@brief Channel allocation function.
 *
 *
 * @retval    Valid memory location if the procedure was successful, else, NULL.
 */
static hid_channel_t * channel_alloc(void);



extern bool is_user_button_pressed(void);


/**
 * @brief Reuse HID mouse report descriptor for HID generic class
 */
APP_USBD_HID_GENERIC_SUBCLASS_REPORT_DESC(ctap_hid_desc,
                                          APP_USBD_CTAP_HID_REPORT_DSC);

static const app_usbd_hid_subclass_desc_t * reports[] = {&ctap_hid_desc};

/*lint -save -e26 -e64 -e123 -e505 -e651*/

/**
 * @brief Global HID generic instance
 */
APP_USBD_HID_GENERIC_GLOBAL_DEF(m_app_ctap_hid,
                                FIDO_HID_INTERFACE,
                                hid_user_ev_handler,
                                ENDPOINT_LIST(),
                                reports,
                                REPORT_IN_QUEUE_SIZE,
                                REPORT_OUT_MAXSIZE,
                                REPORT_FEATURE_MAXSIZE,
                                APP_USBD_HID_SUBCLASS_NONE,
                                APP_USBD_HID_PROTO_GENERIC);

/*lint -restore*/


/**
 * @brief Mark the ongoing transmission
 *
 * Marks that the report buffer is busy and cannot be used until
 * transmission finishes or invalidates (by USB reset or suspend event).
 */
static bool m_report_pending = false;


/**
 * @brief Mark a report received.
 *
 */
static bool m_report_received = false;



void hid_init(void)
{
    NRF_LOG_INFO("init hid");
    
    internal_hid_init();

    NRF_LOG_INFO("init hid done");
}

void hid_process(void)
{
    internal_hid_process();
}



static void usbd_user_ev_handler(app_usbd_event_type_t event)
{
    switch (event)
    {
        case APP_USBD_EVT_DRV_SOF:
            break;
        case APP_USBD_EVT_DRV_RESET:
            m_report_pending = false;
            break;
        case APP_USBD_EVT_DRV_SUSPEND:
            m_report_pending = false;
            // Allow the library to put the peripheral into sleep mode
            app_usbd_suspend_req();
            bsp_board_leds_off();
            break;
        case APP_USBD_EVT_DRV_RESUME:
            m_report_pending = false;
            bsp_board_led_on(LED_U2F_WINK); //TODO
            break;
        case APP_USBD_EVT_STARTED:
            m_report_pending = false;
            bsp_board_led_on(LED_U2F_WINK); //TODO
            break;
        case APP_USBD_EVT_STOPPED:
            app_usbd_disable();
            bsp_board_leds_off();
            break;
        case APP_USBD_EVT_POWER_DETECTED:
            NRF_LOG_INFO("USB power detected");
            if (!nrf_drv_usbd_is_enabled())
            {
                app_usbd_enable();
            }
            break;
        case APP_USBD_EVT_POWER_REMOVED:
            NRF_LOG_INFO("USB power removed");
            app_usbd_stop();
            break;
        case APP_USBD_EVT_POWER_READY:
            NRF_LOG_INFO("USB ready");
            app_usbd_start();
            break;
        default:
            break;
    }
}

/**
 * \brief send one HID frame.
 */
static uint8_t hid_if_frame_send(HID_FRAME * p_frame)
{
    ret_code_t ret;

    while(m_report_pending){
        while (app_usbd_event_queue_process())
        {
            /* Nothing to do */
        }
    }

    ret = app_usbd_hid_generic_in_report_set(
        &m_app_ctap_hid,
        (uint8_t *)p_frame,
        HID_RPT_SIZE);

    if(ret == NRF_SUCCESS)
    {
        m_report_pending = true;
        return ERR_NONE;
    }

    return ERR_OTHER;
}


static uint8_t hid_if_send(uint32_t cid, uint8_t cmd, uint8_t *p_data, size_t size)
{
    HID_FRAME frame;
    int ret;
    size_t frameLen;
    uint8_t seq = 0;

    frame.cid = cid;
    frame.init.cmd = TYPE_INIT | cmd;
    frame.init.bcnth = (size >> 8) & 0xFF;
    frame.init.bcntl = (size & 0xFF);

    frameLen = MIN(size, sizeof(frame.init.data));
    memset(frame.init.data, 0, sizeof(frame.init.data));
    memcpy(frame.init.data, p_data, frameLen);

    do
    {
        ret = hid_if_frame_send(&frame);
        if(ret != ERR_NONE) return ret;

        size -= frameLen;
        p_data += frameLen;

        frame.cont.seq = seq++;
        if(seq & TYPE_MASK) seq = 0x00;
        frameLen = MIN(size, sizeof(frame.cont.data));
        memset(frame.cont.data, 0, sizeof(frame.cont.data));
        memcpy(frame.cont.data, p_data, frameLen);
    } while(size);

    return ERR_NONE;
}



static uint8_t hid_if_recv(uint32_t * p_cid, uint8_t * p_cmd,
                    uint8_t * p_data, size_t * p_size,
                    uint32_t timeout)
{
    uint8_t * p_recv_buf;
    size_t recv_size, totalLen, frameLen;
    HID_FRAME * p_frame;
    uint8_t seq = 0x00;

    Timer timer;
    countdown_ms(&timer, timeout);

    if(!m_report_received) return ERR_OTHER+1;
    m_report_received = false;
    p_recv_buf = (uint8_t *)app_usbd_hid_generic_out_report_get(&m_app_ctap_hid,
                                                                &recv_size);

    if(recv_size != sizeof(HID_FRAME)) return ERR_OTHER;

    p_frame = (HID_FRAME *)p_recv_buf;

    if(FRAME_TYPE(*p_frame) != TYPE_INIT) return ERR_INVALID_CMD;

    *p_cid = p_frame->cid;
    *p_cmd = p_frame->init.cmd;

    totalLen = MSG_LEN(*p_frame);
    frameLen = MIN(sizeof(p_frame->init.data), totalLen);

    *p_size = totalLen;
    
    memcpy(p_data, p_frame->init.data, frameLen);
    totalLen -= frameLen;
    p_data += frameLen;

    while(totalLen)
    {
        while(!m_report_received)
        {
            while (app_usbd_event_queue_process())
            {
                /* Nothing to do */
            }
            if(has_timer_expired(&timer)) return ERR_MSG_TIMEOUT;
        }
        m_report_received = false;

        p_recv_buf = (uint8_t *)app_usbd_hid_generic_out_report_get(
                                                                &m_app_ctap_hid,
                                                                &recv_size);

        if(recv_size != sizeof(HID_FRAME)) continue;

        p_frame = (HID_FRAME *)p_recv_buf;

        if(p_frame->cid != *p_cid) continue;
        if(FRAME_TYPE(*p_frame) != TYPE_CONT) return ERR_INVALID_SEQ;
        NRF_LOG_DEBUG("seq %d, expected %d", FRAME_SEQ(*p_frame), seq);
        if(FRAME_SEQ(*p_frame) != seq++) return ERR_INVALID_SEQ;

        frameLen = MIN(sizeof(p_frame->cont.data), totalLen);

        memcpy(p_data, p_frame->cont.data, frameLen);
        totalLen -= frameLen;
        p_data += frameLen;
    }
    NRF_LOG_DEBUG("hid_if_recv done");

    return ERR_NONE;
}


/**
 * @brief Class specific event handler.
 *
 * @param p_inst    Class instance.
 * @param event     Class specific event.
 * */
static void hid_user_ev_handler(app_usbd_class_inst_t const * p_inst,
                                app_usbd_hid_user_event_t event)
{
    switch (event)
    {
        case APP_USBD_HID_USER_EVT_OUT_REPORT_READY:
        {
            /**NRF_LOG_DEBUG("APP_USBD_...OUT_REPORT_READY");*/
            m_report_received = true;
            break;
        }
        case APP_USBD_HID_USER_EVT_IN_REPORT_DONE:
        {
            /**NRF_LOG_DEBUG("APP_USBD_...IN_REPORT_DONE");*/
            //m_report_received = true;
            m_report_pending = false;
            break;
        }
        case APP_USBD_HID_USER_EVT_SET_BOOT_PROTO:
        {
            UNUSED_RETURN_VALUE(hid_generic_clear_buffer(p_inst));
            NRF_LOG_INFO("SET_BOOT_PROTO");
            break;
        }
        case APP_USBD_HID_USER_EVT_SET_REPORT_PROTO:
        {
            UNUSED_RETURN_VALUE(hid_generic_clear_buffer(p_inst));
            NRF_LOG_INFO("SET_REPORT_PROTO");
            break;
        }
        default:
            break;
    }
}


/**
 * @brief handler for idle reports.
 *
 * @param p_inst      Class instance.
 * @param report_id   Number of report ID that needs idle transfer.
 * */
static ret_code_t idle_handle(app_usbd_class_inst_t const * p_inst,
                              uint8_t report_id)
{
    NRF_LOG_INFO("idle handle");
    
    switch (report_id)
    {
        case 0:
        {
            uint8_t report[] = {0xBE, 0xEF};
            return app_usbd_hid_generic_idle_report_set(
              &m_app_ctap_hid,
              report,
              sizeof(report));
        }
        default:
            return NRF_ERROR_NOT_SUPPORTED;
    }
}


static uint32_t hid_if_init(void)
{
	ret_code_t ret;

	static const app_usbd_config_t usbd_config = {
	    .ev_state_proc = usbd_user_ev_handler
	};
    
    NRF_LOG_INFO("USBD inited");
    NRF_LOG_INFO("HID_EPIN: %x", HID_EPIN);
    NRF_LOG_INFO("HID_EPOUT: %x", HID_EPOUT);
	ret = app_usbd_init(&usbd_config);
    APP_ERROR_CHECK(ret);

    app_usbd_class_inst_t const * class_inst;
    class_inst = app_usbd_hid_generic_class_inst_get(&m_app_ctap_hid);
    
    NRF_LOG_DEBUG("set idle handler");
    ret = hid_generic_idle_handler_set(class_inst, idle_handle);
    APP_ERROR_CHECK(ret);

    NRF_LOG_DEBUG("class append");
    ret = app_usbd_class_append(class_inst);
    APP_ERROR_CHECK(ret);
    
    if (USBD_POWER_DETECTION)
    {
        NRF_LOG_DEBUG("power events enable");
        ret = app_usbd_power_events_enable();
        APP_ERROR_CHECK(ret);
    }
    else
    {
        NRF_LOG_INFO("No USB power detection enabled\r\nStarting USB now");

        app_usbd_enable();
        app_usbd_start();
    }
    
    return ret;
}

static void hid_if_process(void)
{
    while (app_usbd_event_queue_process())
    {
//        NRF_LOG_INFO("if_process something");
        /* Nothing to do */
    }
}



/**
 * @brief List of channels.
 *
 */
channel_list_t m_ctap_ch_list = {NULL, NULL};


/**
 * @brief The count of channel used.
 *
 */
static uint8_t m_channel_used_cnt = 0;


/**@brief Channel allocation function.
 *
 *
 * @retval    Valid memory location if the procedure was successful, else, NULL.
 */
static hid_channel_t * channel_alloc(void)
{
    hid_channel_t * p_ch;
    size_t size = sizeof(hid_channel_t);

    if(m_channel_used_cnt > MAX_HID_CHANNELS)
    {
        NRF_LOG_WARNING("MAX_HID_CHANNELS.");
        return NULL;
    }
    
    NRF_LOG_DEBUG("allocating %d bytes", size);
    p_ch = nrf_malloc(size);
    if(p_ch == NULL)
    {
        NRF_LOG_ERROR("nrf_malloc: Invalid memory location!");
//        nrf_mem_diagnose();
    }
    else
    {
        m_channel_used_cnt++;       
    }

    return p_ch;
}


/**@brief Initialize Channel.
 *
 * @param[in]  p_ch  Pointer to Channel.
 * @param[in]  cid   Channel identifier.
 *
 */
static void channel_init(hid_channel_t * p_ch, uint32_t cid)
{
    size_t size = sizeof(hid_channel_t);

    memset(p_ch, 0, size);

    p_ch->cid = cid;
    NRF_LOG_DEBUG("new channel %d", cid);
    p_ch->state = CID_STATE_IDLE;
    p_ch->pPrev = NULL;
    p_ch->pNext = NULL;

    if(m_ctap_ch_list.pFirst == NULL)
    {
        m_ctap_ch_list.pFirst = m_ctap_ch_list.pLast = p_ch;
    }
    else
    {
        p_ch->pPrev = m_ctap_ch_list.pLast;
        m_ctap_ch_list.pLast->pNext = p_ch;
        m_ctap_ch_list.pLast = p_ch;
    }
}


/**@brief Resets Channel. Keeps cid;
 *
 * @param[in]  p_ch  Pointer to Channel.
 *
 */
static void channel_reset(hid_channel_t * p_ch)
{
    p_ch->state = CID_STATE_IDLE;
    init_timer(&p_ch->timer);
}


/**@brief Uninitialize Channel.
 *
 * @param[in]  p_ch  Pointer to Channel.
 *
 */
static void channel_deinit(hid_channel_t * p_ch)
{
    if(p_ch->pPrev == NULL && p_ch->pNext == NULL)  //only one item in the list
    {
        m_ctap_ch_list.pFirst = m_ctap_ch_list.pLast = NULL;
    }
    else if(p_ch->pPrev == NULL)  // the first item
    {
        m_ctap_ch_list.pFirst = p_ch->pNext;
        p_ch->pNext->pPrev = NULL;
    }
    else if(p_ch->pNext == NULL) // the last item
    {
        m_ctap_ch_list.pLast = p_ch->pPrev;
        p_ch->pPrev->pNext = NULL;
    }
    else
    {
        p_ch->pPrev->pNext = p_ch->pNext;
        p_ch->pNext->pPrev = p_ch->pPrev;
    }
    nrf_free(p_ch);
    m_channel_used_cnt--;
}


/**@brief Find the Channel by cid.
 *
 * @param[in]  cid  Channel identifier.
 *
 * @retval     Valid Channel if the procedure was successful, else, NULL.
 */
static hid_channel_t * channel_find(uint32_t cid)
{
    
    hid_channel_t *p_ch;

    for(p_ch = m_ctap_ch_list.pFirst; p_ch != NULL; p_ch = p_ch->pNext)
    {
        if(p_ch->cid == cid)
        {
            return p_ch;
        }
    }

    return NULL;
}


/**@brief Generate new Channel identifier.
 *
 *
 * @retval     New Channel identifier.
 */
static uint32_t generate_new_cid(void)
{
    static uint32_t cid = 0;
    do
    {
        cid++;
    }while(cid == 0 || cid == CID_BROADCAST);
    return cid;
}


/**@brief Send a HID_ERROR response
 *
 * @param[in]  cid   Channel identifier.
 * @param[in]  code  Error code.
 * 
 */
static void hid_error_response(uint32_t cid, uint8_t error)
{
    hid_if_send(cid, HID_ERROR, &error, 1);
}


/**@brief Handle a HID INIT response
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_init_response(hid_channel_t *p_ch)
{
    HID_INIT_RESP *p_resp_init = (HID_INIT_RESP *) p_ch->resp;
    hid_channel_t * p_new_ch;
    NRF_LOG_INFO("ctap_hid_init"); 

    //if send on broadcast -> create new channel
    if (p_ch->cid == CID_BROADCAST) {

        //allocate mem for new channel
        p_new_ch = channel_alloc();
        if (p_new_ch == NULL) {
             hid_error_response(p_ch->cid, ERR_CHANNEL_BUSY);
             return;
        }
    
        //get new cid and init channel data
        channel_init(p_new_ch, generate_new_cid());
    }
    else //else reset existing channel
    {
        channel_reset(p_ch);
        p_new_ch = p_ch;
    }
        //copy init nonce
        memcpy(p_resp_init->nonce, p_ch->req, INIT_NONCE_SIZE);

        p_resp_init->cid = p_new_ch->cid;                 // Channel identifier 
        p_resp_init->versionInterface = HID_IF_VERSION;   // Interface version
        p_resp_init->versionMajor = HID_FW_VERSION_MAJOR; // Major version number
        p_resp_init->versionMinor = HID_FW_VERSION_MINOR; // Minor version number
        p_resp_init->versionBuild = HID_FW_VERSION_BUILD; // Build version number
        p_resp_init->capFlags =   CAPABILITY_WINK
                                | CAPABILITY_CBOR
                                | CAPABILITY_NMSG; // Capabilities flags
        
        //clear user button state
        UNUSED_RETURN_VALUE(is_user_button_pressed());
        
        //send response
        hid_if_send(p_ch->cid, p_ch->cmd, (uint8_t *)p_resp_init, sizeof(HID_INIT_RESP) );
}


/**@brief Handle a HID WINK response
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_wink_response(hid_channel_t *p_ch)
{
    //board specific action (see hal.h/hal.c)
    board_wink();
    hid_if_send(p_ch->cid, p_ch->cmd, NULL, 0);
}


/**@brief Send a HID status code only
 *
 * @param[in]  p_ch    Pointer to Channel.
 * @param[in]  status  HID status code.
 *
 */
static void hid_status_response(hid_channel_t * p_ch, uint8_t status)
{
    //send status code (in FIDO2 only single byte)
    hid_if_send(p_ch->cid, p_ch->cmd, &status, 1);
}


/**@brief Handle a CTAPHID CBOR response
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_cbor_response(hid_channel_t *p_ch)
{
    CTAP_RESPONSE resp;
    ctap_response_init(&resp);
    uint8_t status;
    status = ctap_request(p_ch->req, p_ch->bcnt, &resp);

    //set response status code
    memcpy(p_ch->resp, &status, 1);
    
    //copy over response
    memcpy(p_ch->resp+1, resp.data, MIN(resp.length, CTAP_RESPONSE_BUFFER_SIZE));

    hid_if_send(p_ch->cid, p_ch->cmd, p_ch->resp, resp.length + 1);
}


/**@brief Handle a HID MESSAGE response UNIMPLEMETED AND UNSUPPORTED
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_msg_response(hid_channel_t * p_ch)
{
    CTAP_RESPONSE resp;
//    uint8_t status;
    // by flag indicated to not be implemented -> invalid CMD
    //hid_status_response(p_ch, ERR_INVALID_CMD);
    //TODO check length

    ctap_response_init(&resp);
    u2f_request((struct u2f_request_apdu*)p_ch->req, &resp);

    hid_if_send(p_ch->cid, p_ch->cmd, p_ch->resp, resp.length);
}

/**@brief Handle a HID PING response
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_ping_response(hid_channel_t *p_ch)
{
    //PONG (echo)
    hid_if_send(p_ch->cid, p_ch->cmd, p_ch->req, p_ch->bcnt);
}


/**@brief Handle a HID SYNC response
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_sync_response(hid_channel_t *p_ch)
{
    return;
}


/**@brief Handle a HID LOCK response
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void hid_lock_response(hid_channel_t *p_ch)
{
    //unimplemented
    hid_status_response(p_ch, ERR_INVALID_CMD);
}


/**@brief Process HID command
 *
 * @param[in]  p_ch  Pointer to Channel.
 * 
 */
static void channel_cmd_process(hid_channel_t * p_ch)
{
    NRF_LOG_INFO("channel_cmd_process ");
    countdown_ms(&p_ch->timer, HID_TRANS_TIMEOUT);
    assert(p_ch->timer.timeout != 0);

    
    if(p_ch->state != CID_STATE_READY) return;

    switch(p_ch->cmd)
    {
        case HID_PING:
            NRF_LOG_INFO("HID_PING.");
            hid_ping_response(p_ch);
            break;

        case HID_MSG:
            NRF_LOG_INFO("HID_MSG.");
            hid_msg_response(p_ch);
            break;

        case HID_LOCK:
            NRF_LOG_INFO("HID_LOCK.");
            hid_lock_response(p_ch);
            break;

        case HID_INIT:
            NRF_LOG_INFO("HID_INIT.");
            hid_init_response(p_ch);
            break;
       
        case HID_CBOR:
            NRF_LOG_INFO("HID_CBOR.");
            hid_cbor_response(p_ch);
            break;

        case HID_WINK:
            NRF_LOG_INFO("HID_WINK.");
            hid_wink_response(p_ch);
            break;

        case HID_SYNC:
            NRF_LOG_INFO("HID_SYNC.");
            hid_sync_response(p_ch);
            break;

        case HID_VENDOR_FIRST:
            NRF_LOG_INFO("HID_VENDOR_FIRST.");
            hid_error_response(p_ch->cid, ERR_INVALID_CMD);
            break;

        case HID_VENDOR_LAST:
            NRF_LOG_INFO("HID_VENDOR_LAST.");
            hid_error_response(p_ch->cid, ERR_INVALID_CMD);
            break;

        default:
            NRF_LOG_WARNING("Unknown Command: %d", p_ch->cmd);
            hid_error_response(p_ch->cid, ERR_INVALID_CMD);
            break;
    }

    p_ch->state = CID_STATE_IDLE;
}


//extern volatile uint32_t ms_ticks;
/**@brief Process HID command of every ready channel.
 * 
 */
static void channel_process(void)
{
    hid_channel_t *p_ch;

    for(p_ch = m_ctap_ch_list.pFirst; p_ch != NULL;)
    {
//        if(p_ch->cid != CID_BROADCAST)
 //       {
           // NRF_LOG_DEBUG("channel: %d, t[s: %d, to:%d], c: %d, d: %d", p_ch->cid, p_ch->timer.start_time, p_ch->timer.timeout, app_timer_cnt_get(), app_timer_cnt_diff_compute(app_timer_cnt_get(), p_ch->timer.start_time));
/**            NRF_LOG_DEBUG("processing channel %d", p_ch->cid);*/
          /**NRF_LOG_DEBUG("timer[start:%d, timeout:%d] | current %d",p_ch->timer.start_time, p_ch->timer.timeout, app_timer_cnt_get());*/
//        }
        // Transaction timeout, free the channel
        if(has_timer_expired(&p_ch->timer))// && p_ch->state == CID_STATE_IDLE)
        {
            if(p_ch->cid != CID_BROADCAST)
            {
                NRF_LOG_DEBUG("channel %d timer has expired", p_ch->cid); 
                hid_channel_t * p_free_ch = p_ch;
                p_ch = p_ch->pNext;
                channel_deinit(p_free_ch);
                continue;
            }
        }
        p_ch = p_ch->pNext;
    }
}


/**
 * @brief Function for initializing the HID.
 *
 * @return Error status.
 *
 */
static ret_code_t internal_hid_init(void)
{
    ret_code_t ret;
    hid_channel_t *p_ch;

    /**ret = nrf_mem_init();*/
    /**if(ret != NRF_SUCCESS)*/
    /**{*/
        /**return ret;*/
    /**}*/

    ret = hid_if_init();
    if(ret != NRF_SUCCESS)
    {
    	return ret;
    }

    /**ret = u2f_impl_init(); //TODO*/
    /**if(ret != NRF_SUCCESS)*/
    /**{*/
        /**return ret;*/
    /**}*/

    p_ch = channel_alloc();
    if(p_ch == NULL)
    {
        NRF_LOG_ERROR("NRF_ERROR_NULL!");
        return NRF_ERROR_NULL;
    }

    channel_init(p_ch, CID_BROADCAST);

    return NRF_SUCCESS;
}


/**
 * @brief HID process function, which should be executed when data is ready.
 *
 */
static void internal_hid_process(void)
{
    uint8_t ret;
    uint32_t cid;
    uint8_t cmd;
    size_t size;
    uint8_t buf[CTAP_MAX_MESSAGE_SIZE]; //TODO

    hid_if_process();

    ret = hid_if_recv(&cid, &cmd, buf, &size, 1000);
    
    if(ret == ERR_NONE)
    {
        hid_channel_t * p_ch;

        p_ch = channel_find(cid);

        if(p_ch == NULL)
        {
            NRF_LOG_ERROR("No valid channel found!");
            hid_error_response(cid, ERR_CHANNEL_BUSY);
        }
        else
        {
            p_ch->cmd = cmd;
            p_ch->bcnt = size;
            p_ch->state = CID_STATE_READY;
            memcpy(p_ch->req, buf, size);
            channel_cmd_process(p_ch);
        }
    }

    channel_process();
}

