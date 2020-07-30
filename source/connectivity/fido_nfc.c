/******************************************************************************
 * File:             fido_nfc.c
 *
 * Author:           Joshua Steffensky <joshua.steffensky@cispa.saarland>  
 * Created:          06/18/20 
 * Description:      nfc ctap implementation
 *****************************************************************************/

#include "fido_nfc.h"
#include "fido_interfaces.h"
#include "apdu.h"
#include "ctap.h"
#include "ctap_errors.h"

#include "nfc_t4t_lib.h"
#include "app_util.h"
#include "mem_manager.h"
//#include "app_error.h"
//#include "nordic_common.h"

/*  LOG INIT  */
#define NRF_LOG_MODULE_NAME nfc

#include "nrf_log.h"
#include "nrf_log_ctrl.h"

NRF_LOG_MODULE_REGISTER();
/*            */


/***************************************************************************** 
 *                            local prototypes
 *****************************************************************************/

/* send response */

/** 
 * @brief		respond to nfc request with just a status
 * 
 * @param[in] 	status	status to send
 *
 * @return 		void
 */
static void nfc_send_status(uint16_t status);

/** 
 * @brief		response to nfc request with plain data. data copied to internal buffer.
 * 
 * @param[in] 	data	data to send
 * @param[in] 	len	    length of data to send
 *
 * @return 		void
 */
static void nfc_send_response_plain(uint8_t *data, size_t len);

/** 
 * @brief		respond to nfc request with data and status code. data copied to internat buffer.
 *
 * @param[in] 	data	data to send
 * @param[in] 	len	    length of data to send
 * @param[in] 	status	status to send
 * 
 * @return 		Return parameter description
 */
static void nfc_send_response(uint8_t *data, size_t len, uint16_t status);

/** 
 * @brief		respond to nfc request with raw data **DATA NOT COPIED**
 *
 * @param[in] 	data	raw data to send (**NOT COPIED OVER**)
 * @param[in] 	len	    length of data to send
 * 
 * @return 		Return parameter description
 */
static void nfc_send_response_raw(uint8_t *data, size_t len);

/** 
 * @brief		initialise response buffer if necessary and send first message
 * 
 * @param[in,out]  nfc_mod  nfc state
 * @param[in]      data     data to be send (copied to internat buffer)
 * @param[in]      len      length of data to be send
 * @param[in]      ext      whether received request was received are nfc extended buffer
 *
 * @return 		void
 */
static void nfc_response_init_chain(
    NFC_STATE *nfc_mod,
    uint8_t *  data,
    size_t     len,
    bool       ext);

/* nfc utils */

/** 
 * @brief		compare raw nfc aid (rid:pix)
 *
 * @param[in] 	aid	incoming aid
 * @param[in] 	len	lenght of aid
 * @param[in] 	const_aid	aid to compare to
 * @param[in] 	const_len	length of const_aid
 *
 * @return
 */
static int applet_cmp(uint8_t *aid, int len, uint8_t *const_aid, int const_len);

/** 
 * @brief		select applet in state
 * 
 * @param[out] 	nfc_mod	nfc state which might be changed
 * @param[in] 	aid	    aid to change to
 * @param[in] 	len	    length of aid
 *
 * @return 		1 is success, 0 otherwise
 */
static int select_applet(NFC_STATE *nfc_mod, uint8_t *aid, int len);

/** 
 * @brief		append nfc get append to a response
 * 
 * @param[out] 	data	    data to append to (at postition of end (i.e. bufferend -2))
 * @param[in] 	rest_len	length of remaining data to be send
 *
 * @return 		void
 */
static void append_get_response(uint8_t *data, size_t rest_len);

/** 
 * @brief		clean nfc_state 
 * 
 * @param[out] 	nfc_mod	    state to clean
 *
 * @return 		void
 */
static void nfc_clean(NFC_STATE *nfc_mod);

/** 
 * @brief		clean dispatch from nfc state
 * 
 * @param[out] 	nfc_mod     state to clean
 *
 * @return 		void
 */
static void nfc_clean_dispatch(NFC_STATE *nfc_mod);

/** 
 * @brief		clean response from nfc state
 * 
 * @param[out] 	nfc_mod	    state to clean
 * 
 * @return 		void
 */
static void nfc_clean_response(NFC_STATE *nfc_mod);

/* nfc msg handling */

/**
 * @brief Callback function for handling NFC events.
 */
static void nfc_callback(
    void *          context,
    nfc_t4t_event_t event,
    const uint8_t * data,
    size_t          dataLength,
    uint32_t        flags);

/** 
 * @brief		process incoming single apdu package on event
 * 
 * @param[out]  nfc_mod     nfc state that might be altered
 * @param[in]   data        raw incoming apdu command
 * @param[in]   dataLength  length of incomming data
 * @param[in]   flags       nrf sdk flags
 *
 * @return 		void
 */
static void process_apdu(
    NFC_STATE *    nfc_mod,
    const uint8_t *data,
    size_t         dataLength,
    uint32_t       flags);

/* APDU message handling (once command complete) */

/** 
 * @brief		process received and completed apdu command (main loop)
 * 
 * @param[in,out] 	nfc_mod	state whos dispatch package to process
 *
 * @return 		void
 */
static void process_dispatch(NFC_STATE *nfc_mod);

/** 
 * @brief		process get response command
 * 
 * @param[in,out] 	nfc_mod	corresponding state
 *
 * @return 		void
 */
static void apdu_get_response(NFC_STATE *nfc_mod);

/** 
 * @brief		process select applet command
 * 
 * @param[in,out] 	nfc_mod	corresponding state
 *
 * @return 		void
 */
static void apdu_select_applet(NFC_STATE *nfc_mod);

/** 
 * @brief		handle nfcctap msg
 * 
 * @param[in,out] 	nfc_mod	corresponding state
 *
 * @return 		void
 */
static void apdu_nfcctap_msg(NFC_STATE *nfc_mod);

/** 
 * @brief		handle read binary request
 * 
 * @param[in,out] 	nfc_mod	corresponding state
 *
 * @return 		void
 */
static void apdu_read_binary(NFC_STATE *nfc_mod);


/***************************************************************************** 
*							CONSTS
*****************************************************************************/

// Capability container
//const CAPABILITY_CONTAINER NFC_CC = {
//.cclen_hi = 0x00, .cclen_lo = 0x0f,
//.version = 0x20,
//.MLe_hi = 0x00, .MLe_lo = 0x7f,
//.MLc_hi = 0x00, .MLc_lo = 0x7f,
//.tlv = { 0x04,0x06,
//0xe1,0x04,
//0x00,0x7f,
//0x00,0x00 }
//};

const uint8_t NFC_CC[] = {
    0x00,  //CCLEN (hi, low)
    0x0f,
    0x20,  //version
    0x00,  //MLe (hi,low)
    0x7f,
    0x00,  //MLc (hi,low)
    0x7f,
    0x04,
    0x06,  //TLV
    0xe1,
    0x04,
    0x00,
    0x7f,
    0x00,
    0x00};

// 13 chars
const uint8_t NDEF_SAMPLE[] = "\x00\x14\xd1\x01\x0eU\x04cispa.saarland/";


/***************************************************************************** 
 *                            GLOBALS
 *****************************************************************************/
NFC_STATE m_nfc;

uint8_t txbuffer[MAX_NFC_LENGTH];


/***************************************************************************** 
 *                            NFC CONTROL
 *****************************************************************************/

static void nfc_send_status(uint16_t status)
{
    NRF_LOG_DEBUG("send status: %d", status);
    nfc_send_response(NULL, 0, status);
}

static void nfc_send_response_plain(uint8_t *data, size_t len)
{
    memcpy(txbuffer, data, len);
    nfc_send_response_raw(txbuffer, len);
}

static void nfc_send_response(uint8_t *data, size_t len, uint16_t status)
{
    NRF_LOG_DEBUG("sending response: %d", status);
    NRF_LOG_HEXDUMP_DEBUG(data, len);
    //TODO check length
    memcpy(txbuffer, data, len);

    txbuffer[len + 0] = status >> 8;
    txbuffer[len + 1] = status & 0xff;

    nfc_send_response_raw(txbuffer, len + 2);
}

static void nfc_send_response_raw(uint8_t *data, size_t len)
{
    ret_code_t err_code;
    // Send the response PDU over NFC.
    err_code = nfc_t4t_response_pdu_send(data, len);
    APP_ERROR_CHECK(err_code);
}

static void nfc_response_init_chain(
    NFC_STATE *nfc_mod,
    uint8_t *  data,
    size_t     len,
    bool       ext)
{
    if (len <= 255 || ext)
    {
        nfc_send_response_plain(
            nfc_mod->response_buffer,
            nfc_mod->response_length);
    }
    else
    {
        size_t pcklen    = MIN(253, len);
        size_t bytesleft = len - pcklen;
        NRF_LOG_INFO("61XX chaining %d/%d.", pcklen, bytesleft);

        // put the rest data into response buffer
        nfc_mod->response_buffer = nrf_malloc(bytesleft);
        nfc_mod->response_length = bytesleft;
        nfc_mod->response_offset = 0;
        memcpy(nfc_mod->response_buffer, &data[pcklen], bytesleft);

        {
            //init package
            uint8_t pck[255] = {0};
            memmove(pck, data, pcklen);
            append_get_response(&pck[pcklen], bytesleft);

            //send package
            nfc_send_response_plain(pck, pcklen + 2);  // 2 for 61XX
        }
    }
}


/***************************************************************************** 
 *                            NFC_UTILS
 *****************************************************************************/

/**  ref  solo:targets/stm32l432/nfc.c  **/
// international AID = RID:PIX
// RID length == 5 bytes
// usually aid length must be between 5 and 16 bytes
int applet_cmp(uint8_t *aid, int len, uint8_t *const_aid, int const_len)
{
    if (len > const_len)
        return 10;

    // if international AID
    if ((const_aid[0] & 0xf0) == 0xa0)
    {
        if (len < 5)
            return 11;
        return memcmp(aid, const_aid, MIN(len, const_len));
    }
    else
    {
        if (len != const_len)
            return 11;

        return memcmp(aid, const_aid, const_len);
    }
}

// Selects application.  Returns 1 if success, 0 otherwise
int select_applet(NFC_STATE *nfc_mod, uint8_t *aid, int len)
{
    if (applet_cmp(aid, len, (uint8_t *) AID_FIDO, sizeof(AID_FIDO) - 1) == 0)
    {
        nfc_mod->selected_applet = APP_FIDO;
        return APP_FIDO;
    }
    else if (
        applet_cmp(
            aid,
            len,
            (uint8_t *) AID_NDEF_TYPE_4,
            sizeof(AID_NDEF_TYPE_4) - 1)
        == 0)
    {
        nfc_mod->selected_applet = APP_NDEF_TYPE_4;
        return APP_NDEF_TYPE_4;
    }
    else if (
        applet_cmp(
            aid,
            len,
            (uint8_t *) AID_CAPABILITY_CONTAINER,
            sizeof(AID_CAPABILITY_CONTAINER) - 1)
        == 0)
    {
        nfc_mod->selected_applet = APP_CAPABILITY_CONTAINER;
        return APP_CAPABILITY_CONTAINER;
    }
    else if (
        applet_cmp(aid, len, (uint8_t *) AID_NDEF_TAG, sizeof(AID_NDEF_TAG) - 1)
        == 0)
    {
        nfc_mod->selected_applet = APP_NDEF_TAG;
        return APP_NDEF_TAG;
    }
    return APP_NOTHING;
}

static void append_get_response(uint8_t *data, size_t rest_len)
{
    data[0] = 0x61;
    data[1] = 0x00;
    if (rest_len <= 0xff)
        data[1] = rest_len & 0xff;
}
/** endref solo:targets/stm32l432/nfc. **/

static void nfc_clean(NFC_STATE *nfc_mod)
{
    nfc_mod->selected_applet = APP_NOTHING;
    nfc_clean_dispatch(nfc_mod);
    nfc_clean_response(nfc_mod);
}

static void nfc_clean_dispatch(NFC_STATE *nfc_mod)
{
    NRF_LOG_DEBUG("nfc_clean_dispatch");
    FREEANDNULL(nfc_mod->dispatch_package)
    FREEANDNULL(nfc_mod->dispatch_head)
    nfc_mod->dispatch_length = 0;
    nfc_mod->dispatch_ready = 0;
}

static void nfc_clean_response(NFC_STATE *nfc_mod)
{
    FREEANDNULL(nfc_mod->response_buffer)
    nfc_mod->response_length = 0;
    nfc_mod->response_offset = 0;
}


/***************************************************************************** 
 *                            NFC MSG HANDLING
 *****************************************************************************/

static void nfc_callback(
    void *          context,
    nfc_t4t_event_t event,
    const uint8_t * data,
    size_t          dataLength,
    uint32_t        flags)
{
    /**ret_code_t err_code;*/
    /**uint32_t   resp_len;*/

    //    (void) context;
    NRF_LOG_INFO("NFC callback");

    switch (event)
    {
        case NFC_T4T_EVENT_FIELD_ON:
            NRF_LOG_INFO("NFC Tag has been selected");

            nfc_clean((NFC_STATE *) context);
            break;

        case NFC_T4T_EVENT_FIELD_OFF: NRF_LOG_INFO("NFC field lost"); break;

        case NFC_T4T_EVENT_DATA_IND:
            process_apdu((NFC_STATE *) context, data, dataLength, flags);
            break;

        case NFC_T4T_EVENT_DATA_TRANSMITTED:
        default: NRF_LOG_WARNING("something else happend"); break;
    }
}

static void process_apdu(
    NFC_STATE *    nfc_mod,
    const uint8_t *data,
    size_t         dataLength,
    uint32_t       flags)
{
    APDU_STRUCT *apdu = nrf_malloc(sizeof(APDU_STRUCT));
    //parse header

    NRF_LOG_DEBUG("received apdu");
    NRF_LOG_HEXDUMP_DEBUG(data, dataLength);

    memset(apdu, 0, sizeof(APDU_STRUCT));
    if (apdu_decode((uint8_t *) data, dataLength, apdu))
    {
        NRF_LOG_WARNING("apdu decode error");
        nfc_send_status(SW_INS_INVALID);
        nfc_clean(nfc_mod);
        nrf_free(apdu);
        return;
    };

    NRF_LOG_INFO(
        "apdu ok. %scase=%02x cla=%02x ins=%02x",
        apdu->extended_apdu ? "[e]" : "",
        apdu->case_type,
        apdu->cla,
        apdu->ins);
    NRF_LOG_INFO(
        "p1=%02x p2=%02x lc=%d le=%d\r\n",
        apdu->p1,
        apdu->p2,
        apdu->lc,
        apdu->le);

    if (!nfc_mod->dispatch_package)
    {
        nfc_mod->dispatch_package = nrf_malloc(CTAP_MAX_MESSAGE_SIZE);
        nfc_mod->dispatch_length  = 0;
    }
    if (nfc_mod->dispatch_length + apdu->lc > CTAP_MAX_MESSAGE_SIZE)
    {
        nfc_send_status(SW_WRONG_LENGTH);
        nfc_clean(nfc_mod);
        nrf_free(apdu);
        return;
    }
    memcpy(
        nfc_mod->dispatch_package + nfc_mod->dispatch_length,
        apdu->data,
        apdu->lc);
    nfc_mod->dispatch_length += apdu->lc;

    if (apdu->cla == 0x90 && apdu->ins == 0x10)  //chaining
    {
        nfc_send_status(SW_SUCCESS);
        nrf_free(apdu);
        return;
    }
    else
    {
        nfc_mod->dispatch_head  = apdu;
        nfc_mod->dispatch_ready = true;
    }
}


/***************************************************************************** 
 *                            APDU MSG HANDLING
 *****************************************************************************/

static void process_dispatch(NFC_STATE *nfc_mod)
{
    //check cla
    if (nfc_mod->dispatch_head->cla != 0x00
        && nfc_mod->dispatch_head->cla != 0x80)
    {
        //illegal command
        NRF_LOG_WARNING("illegal cla: 0x%x", nfc_mod->dispatch_head->cla);
        nfc_send_status(SW_CLA_INVALID);
        nfc_clean(nfc_mod);
        return;
    }

    /**  ref  original code from solo:targets/stm32l432/nfc.c  **/
    switch (nfc_mod->dispatch_head->ins)
    {
        // ISO 7816. 7.1 GET RESPONSE command
        case APDU_GET_RESPONSE: apdu_get_response(nfc_mod); break;

        case APDU_INS_SELECT: apdu_select_applet(nfc_mod); break;

        case APDU_FIDO_NFCCTAP_MSG: apdu_nfcctap_msg(nfc_mod); break;

        case APDU_INS_READ_BINARY: apdu_read_binary(nfc_mod); break;

        default:
            NRF_LOG_WARNING("Unknown INS %02x", nfc_mod->dispatch_head->ins);
            NRF_LOG_INFO(
                "apdu: %scase=%02x cla=%02x ins=%02x",
                nfc_mod->dispatch_head->extended_apdu ? "[e]" : "",
                nfc_mod->dispatch_head->case_type,
                nfc_mod->dispatch_head->cla,
                nfc_mod->dispatch_head->ins);
            NRF_LOG_INFO(
                "p1=%02x p2=%02x lc=%d le=%d\r\n",
                nfc_mod->dispatch_head->p1,
                nfc_mod->dispatch_head->p2,
                nfc_mod->dispatch_head->lc,
                nfc_mod->dispatch_head->le);

            nfc_send_status(SW_INS_INVALID);
            nfc_clean(nfc_mod);
            break;
    }

    /** endref original code from solo:targets/stm32l432/nfc.c **/
}

static void apdu_get_response(NFC_STATE *nfc_mod)
{
    APDU_STRUCT *apdu = nfc_mod->dispatch_head;
    size_t data_left  = nfc_mod->response_length - nfc_mod->response_offset;

    if (apdu->p1 != 0x00 || apdu->p2 != 0x00)
    {
        nfc_send_status(SW_INCORRECT_P1P2);
        NRF_LOG_WARNING("P1 or P2 error");
        nfc_clean(nfc_mod);
        return;
    }

    // too many bytes needs. 0x00 and 0x100 - any length
    if (apdu->le != 0 && apdu->le != 0x100 && apdu->le > data_left)
    {
        uint16_t wlresp =
            SW_WRONG_LENGTH;  // here can be 6700, 6C00, 6FXX. but the most standard way - 67XX or 6700
        if (data_left <= 0xff)
            wlresp += data_left & 0xff;
        nfc_send_status(wlresp);
        NRF_LOG_WARNING("buffer length less than requesteds");
        return;
    }

    // create temporary packet
    uint8_t pck[255] = {0};
    size_t  pcklen   = 253;
    if (apdu->le)
        pcklen = apdu->le;
    if (pcklen > data_left)
        pcklen = data_left;

    NRF_LOG_INFO("GET RESPONSE. pck len: %d buffer len: %d", pcklen, data_left);

    // create packet and add 61XX there if we have another portion(s) of data
    memmove(pck, nfc_mod->response_buffer, pcklen);
    size_t dlen = 0;
    if (data_left - pcklen)
    {
        append_get_response(&pck[pcklen], data_left - pcklen);
        dlen = 2;
    }

    // send
    nfc_send_response_plain(pck, pcklen + dlen);  //dlen for 61XX

    // shift the buffer
    nfc_mod->response_offset += pcklen;

    if (nfc_mod->response_offset == nfc_mod->response_length)
        nfc_clean_response(nfc_mod);
}

static void apdu_select_applet(NFC_STATE *nfc_mod)
{
    APDU_STRUCT *apdu     = nfc_mod->dispatch_head;
    int          selected = select_applet(
        nfc_mod,
        nfc_mod->dispatch_package,
        nfc_mod->dispatch_length);
    if (selected == APP_FIDO)
    {
        nfc_send_response((uint8_t *) "FIDO_2_0", 8, SW_SUCCESS);
        NRF_LOG_INFO("FIDO applet selected");
    }
    else if (selected != APP_NOTHING)
    {
        nfc_send_status(SW_SUCCESS);
        NRF_LOG_INFO("SELECTED %d", selected);
    }
    else
    {
        nfc_send_status(SW_FILE_NOT_FOUND);
        NRF_LOG_INFO("NOT selected");
        NRF_LOG_HEXDUMP_DEBUG(apdu->data, apdu->lc);
    }
}

static void apdu_nfcctap_msg(NFC_STATE *nfc_mod)
{
    CTAP_RESPONSE ctap_resp;
    APDU_STRUCT * apdu = nfc_mod->dispatch_head;
    uint8_t       status;

    if (nfc_mod->selected_applet != APP_FIDO)
    {
        nfc_send_status(SW_INS_INVALID);
        nfc_clean(nfc_mod);
        return;
    }

    NRF_LOG_INFO("FIDO2 CTAP message");

    // WTX_on(WTX_TIME_DEFAULT);
    //    request_from_nfc(true);
    set_req_origin(REQ_ORIGIN_NFC);

    ctap_response_init(&ctap_resp);
    status = ctap_request(
        nfc_mod->dispatch_package,
        nfc_mod->dispatch_length,
        &ctap_resp);

    set_req_origin(REQ_ORIGIN_NONE);
    //    request_from_nfc(false);
    // if (!WTX_off())
    //     return;

    NRF_LOG_INFO("CTAP resp: 0x%02x  len: %d\r\n", status, ctap_resp.length);

    if (status == CTAP1_ERR_SUCCESS)
    {
        memmove(&ctap_resp.data[1], &ctap_resp.data[0], ctap_resp.length);
        ctap_resp.length += 3;
    }
    else
    {
        ctap_resp.length = 3;
    }
    ctap_resp.data[0]                    = status;
    ctap_resp.data[ctap_resp.length - 2] = SW_SUCCESS >> 8;
    ctap_resp.data[ctap_resp.length - 1] = SW_SUCCESS & 0xff;

    /**if (status == CTAP1_ERR_SUCCESS)*/
    /**{*/
    /**nfc_mod->dispatch_buffer = nrf_malloc(ctap_resp.length + 3);*/
    /**memmove(*/
    /**&nfc_mod->dispatch_buffer[1],*/
    /**&ctap_resp.data[0],*/
    /**ctap_resp.length);*/
    /**ctap_resp.length += 3;*/
    /**}*/
    /**else*/
    /**{*/
    /**nfc_mod->dispatch_buffer = nrf_malloc(3);*/
    /**ctap_resp.length         = 3;*/
    /**}*/

    /**nfc_mod->dispatch_buffer[0]     = status;*/
    /**nfc_mod->[ctap_resp.length - 2] = SW_SUCCESS >> 8;*/
    /**nfc_mod->[ctap_resp.length - 1] = SW_SUCCESS & 0xff;*/

    /**nfc_mod->response_offset = 0;*/
    /**nfc_mod->response_len    = ctap_resp.length;*/
    /**ctap_resp.data[0]                    = status;*/
    /**ctap_resp.data[ctap_resp.length - 2] = SW_SUCCESS >> 8;*/
    /**ctap_resp.data[ctap_resp.length - 1] = SW_SUCCESS & 0xff;*/

    //    nfc_write_response_chaining(buf0, ctap_resp.data, ctap_resp.length, apdu->extended_apdu);
    nfc_response_init_chain(
        nfc_mod,
        ctap_resp.data,
        ctap_resp.length,
        apdu->extended_apdu);
    nfc_clean_dispatch(nfc_mod);
    NRF_LOG_INFO("CTAP answered");
}

static void apdu_read_binary(NFC_STATE *nfc_mod)
{
    APDU_STRUCT *apdu = nfc_mod->dispatch_head;
    // response length
    size_t reslen = apdu->le & 0xffff;
    switch (nfc_mod->selected_applet)
    {
        case APP_CAPABILITY_CONTAINER:
            NRF_LOG_INFO("APP_CAPABILITY_CONTAINER");
            if (reslen == 0 || reslen > sizeof(NFC_CC))
                reslen = sizeof(NFC_CC);

            nfc_send_response((uint8_t *) &NFC_CC, reslen, SW_SUCCESS);
            //            ams_wait_for_tx(10);
            break;
        case APP_NDEF_TAG:
            NRF_LOG_INFO("APP_NDEF_TAG");
            if (reslen == 0 || reslen > sizeof(NDEF_SAMPLE) - 1)
                reslen = sizeof(NDEF_SAMPLE) - 1;
            nfc_send_response((uint8_t *) NDEF_SAMPLE, reslen, SW_SUCCESS);
            //            ams_wait_for_tx(10);
            break;
        default:
            nfc_send_status(SW_FILE_NOT_FOUND);
            NRF_LOG_WARNING("No binary applet selected!");
            return;
            break;
    }
}


/***************************************************************************** 
 *                            public interface
 *****************************************************************************/

void nfc_init(void)
{
    NRF_LOG_INFO("initializing nfc");
    ret_code_t err_code;
    //INIT NFC STATE
    nfc_clean(&m_nfc);

    //INIT LIBRARY
    err_code = nfc_t4t_setup(nfc_callback, &m_nfc);
    APP_ERROR_CHECK(err_code);

    //start emulation
    err_code = nfc_t4t_emulation_start();
    APP_ERROR_CHECK(err_code);
}

void nfc_process(void)
{
    if (m_nfc.dispatch_ready)
    {
        process_dispatch(&m_nfc);
        nfc_clean_dispatch(&m_nfc);
    }
}
