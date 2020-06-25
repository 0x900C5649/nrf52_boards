/******************************************************************************
 * File:             fido_nfc.c
 *
 * Author:           Joshua Steffensky <joshua.steffensky@cispa.saarland>  
 * Created:          06/18/20 
 * Description:      nfc ctap implementation
 *****************************************************************************/

#include "fido_nfc.h"


/***************************************************************************** 
 *                            local prototypes
 *****************************************************************************/

/**
 * @brief Callback function for handling NFC events.
 */
static void nfc_callback(
    void *          context,
    nfc_t4t_event_t event,
    const uint8_t * data,
    size_t          dataLength,
    uint32_t        flags);

/***************************************************************************** 
 *                            GLOBALS
 *****************************************************************************/
NFC_STATE nfc_mod;

uint8_t txbuffer[MAX_NFC_LENGTH];

/***************************************************************************** 
 *                            APDU decoding
 *****************************************************************************/

/** original from SOLO:fido2/apdu.c **/
int apdu_decode(uint8_t *data, size_t len, APDU_STRUCT *apdu)
{
    EXT_APDU_HEADER *hapdu = (EXT_APDU_HEADER *) data;

    apdu->cla = hapdu->cla & 0xef;  // mask chaining bit if any
    apdu->ins = hapdu->ins;
    apdu->p1  = hapdu->p1;
    apdu->p2  = hapdu->p2;

    apdu->lc            = 0;
    apdu->data          = NULL;
    apdu->le            = 0;
    apdu->extended_apdu = false;
    apdu->case_type     = 0x00;

    uint8_t b0 = hapdu->lc[0];

    // case 1
    if (len == 4)
    {
        apdu->case_type = 0x01;
    }

    // case 2S (Le)
    if (len == 5)
    {
        apdu->case_type = 0x02;
        apdu->le        = b0;
        if (!apdu->le)
            apdu->le = 0x100;
    }

    // case 3S (Lc + data)
    if (len == 5U + b0 && b0 != 0)
    {
        apdu->case_type = 0x03;
        apdu->lc        = b0;
    }

    // case 4S (Lc + data + Le)
    if (len == 5U + b0 + 1U && b0 != 0)
    {
        apdu->case_type = 0x04;
        apdu->lc        = b0;
        apdu->le        = data[len - 1];
        if (!apdu->le)
            apdu->le = 0x100;
    }

    // extended length apdu
    if (len >= 7 && b0 == 0)
    {
        uint16_t extlen = (hapdu->lc[1] << 8) + hapdu->lc[2];

        // case 2E (Le) - extended
        if (len == 7)
        {
            apdu->case_type     = 0x12;
            apdu->extended_apdu = true;
            apdu->le            = extlen;
            if (!apdu->le)
                apdu->le = 0x10000;
        }

        // case 3E (Lc + data) - extended
        if (len == 7U + extlen)
        {
            apdu->case_type     = 0x13;
            apdu->extended_apdu = true;
            apdu->lc            = extlen;
        }

        // case 4E (Lc + data + Le) - extended 2-byte Le
        if (len == 7U + extlen + 2U)
        {
            apdu->case_type     = 0x14;
            apdu->extended_apdu = true;
            apdu->lc            = extlen;
            apdu->le            = (data[len - 2] << 8) + data[len - 1];
            if (!apdu->le)
                apdu->le = 0x10000;
        }

        // case 4E (Lc + data + Le) - extended 3-byte Le
        if (len == 7U + extlen + 3U && data[len - 3] == 0)
        {
            apdu->case_type     = 0x24;
            apdu->extended_apdu = true;
            apdu->lc            = extlen;
            apdu->le            = (data[len - 2] << 8) + data[len - 1];
            if (!apdu->le)
                apdu->le = 0x10000;
        }
    }

    if (!apdu->case_type)
        return 1;

    if (apdu->lc)
    {
        if (apdu->extended_apdu)
        {
            apdu->data = data + 7;
        }
        else
        {
            apdu->data = data + 5;
        }
    }

    return 0;
}
/**         SOLO:fido2/apdu.c       **/


/***************************************************************************** 
 *                            NFC CONTROL
 *****************************************************************************/

void nfc_send_status(uint16_t status) { nfc_send_response(NULL, 0, status); }

void nfc_send_response_plain(uint8_t *data, size_t len)
{
    memcpy(txbuffer, data, len);
    nfc_send_response_raw(txbuffer, len);
}

void nfc_send_response(uint8_t *data, size_t len, uint16_t status)
{
    //TODO check length
    memcpy(txbuffer, data, len);

    txbuffer[len + 0] = status >> 8;
    txbuffer[len + 1] = status & 0xff;

    nfc_send_response_raw(txbuffer, len + 2);
}

void nfc_send_response_raw(uint8_t *data, size_t len)
{
    // Send the response PDU over NFC.
    err_code = nfc_t4t_response_pdu_send(data, len);
    APP_ERROR_CHECK(err_code);
}


/***************************************************************************** 
 *                            NFC_UTILS
 *****************************************************************************/

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


void nfc_clean(NFC_STATE *nfc_mod)
{
    nfc_mod->selected_applet = APP_NOTHING;
    nfc_clean_dispatch(nfc_mod);
    nfc_clean_response(nfc_mod);
}

void nfc_clean_dispatch(NFC_STATE *nfc_mod)
{
    FREEANDNULL(nrf_mod->dispatch_package)
    FREEANDNULL(nrf_mod->dispatch_head)
    nfc_mod->dispatch_len    = 0;
    nfc_mod->dispatch_offset = 0;
}

void nfc_clean_response(NFC_STATE *nfc_mod)
{
    FREEANDNULL(nrf_mod->response_buffer)
    nfc_mod->response_len    = 0;
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
    ret_code_t err_code;
    uint32_t   resp_len;

    (void) context;

    switch (event)
    {
        case NFC_T4T_EVENT_FIELD_ON:
            NRF_LOG_INFO("NFC Tag has been selected");

            nrf_clean((NFC_STATE *) context);
            break;

        case NFC_T4T_EVENT_FIELD_OFF: NRF_LOG_INFO("NFC field lost"); break;

        case NFC_T4T_EVENT_DATA_IND:
            process_apdu((NFC_STATE *) context, data, dataLength, flags);
            break;

        case NFC_T4T_EVENT_DATA_TRANSMITTED:
        default: NRF_LOG_WARNUNG("something else happend"); break;
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
    if (apdu_decode(data, dataLength, apdu))
    {
        NRF_LOG_WARNING("apdu decode error");
        nfc_send_status(SW_INS_INVALID);
        nfc_clean(nfc_mod);
        nrf_free(apdu);
        return;
    };

    NRF_LOG_INFO(
        "apdu ok. %scase=%02x cla=%02x ins=%02x p1=%02x p2=%02x lc=%d "
        "le=%d\r\n",
        apdu->extended_apdu ? "[e]" : "",
        apdu->case_type,
        apdu->cla,
        apdu->ins,
        apdu->p1,
        apdu->p2,
        apdu->lc,
        apdu->le);

    if (!nfc_mod->dispatch_package)
    {
        nfc_mod->dispatch_package = nrf_malloc(MAX_CTAP_MESSAGE);
        nfc_mod->dispatch_len     = 0;
    }
    if (nfc_mod->dispatch_len + apdu->lc > MAX_CTAP_MESSAGE)
    {
        nfc_send_status(SW_WRONG_LENGTH);
        nfc_clean(nfc_mod);
        nrf_free(apdu);
        return;
    }
    memcpy(
        nfc_mod->dispatch_package + nfc_mod->dispatch_len,
        apdu->data,
        apdu->lc);
    nfc_mod->dispatch_len += apdu->lc;

    if (apdu->cla == 0x90 && apdu->ins == 0x10)  //chaining
    {
        nfc_send_status(SW_SUCCESS);
        nrf_free(apdu);
        return;
    }
    else
    {
        nfc_mod->dipatch_head   = apdu;
        nfc_mod->dispatch_ready = true;
    }
}


/***************************************************************************** 
 *                            APDU MSG HANDLING
 *****************************************************************************/

void process_dispatach(NFC_STATE *nfc_mod)
{
    //check cla
    if (nfc_mod->dispatch_head->cla != 0x00
        && nfc_mod->dispatch_head->cla != 0x80)
    {
        //illegal command
        NRF_LOG_WARNING("illegal cla: 0x%x", nfc_mod->dispatch_head->cla);
        nfc_send_status(SW_CLA_INVALID);
        nfc_clean(nfc_mod);
        return
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
            nfc_send_status(SW_INS_INVALID);
            nfc_clean(nfc_mod);
            break;
    }

    /** endref original code from solo:targets/stm32l432/nfc.c **/
}

static void apdu_get_response(NFC_STATE *nfc_mod)
{
    APDU_STRUCT apdu      = nfc_mod->dispatch_head;
    size_t      data_left = nfc_mod->response_len - nfc_mod->response_offset;

    if (apdu->p1 != 0x00 || apdu->p2 != 0x00)
    {
        nfc_send_response(SW_INCORRECT_P1P2);
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

    if (nfc_mod->response_offset == nfc_mod->response_len)
        nfc_clean_response(nfc_mod);
}

static void apdu_select_applet(NFC_STATE *nfc_mod)
{
    APDU_STRUCT *apdu     = nfc_mod->dispatch_head;
    int          selected = select_applet(
        nfc_mod,
        nfc_mod->dispatch_package,
        nfc_mod->dispatch_len);
    if (selected == APP_FIDO)
    {
        nfc_send_response((uint8_t *) "FIDO_2_0", 8, SW_SUCCESS)
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
        nfc_mod->dispatch_len,
        &ctap_resp);

    set_req_origin(REQ_ORIGIN_NONE);
    //    request_from_nfc(false);
    // if (!WTX_off())
    //     return;

    NRF_LOG_INFO("CTAP resp: 0x%02x  len: %d\r\n", status, ctap_resp.length);

    if (status == CTAP1_ERR_SUCCESS)
    {
        nfc_mod->dispatch_buffer = nrf_malloc(ctap_resp.length + 3);
        memmove(
            &nfc_mod->dispatch_buffer[1],
            &ctap_resp.data[0],
            ctap_resp.length);
        ctap_resp.length += 3;
    }
    else
    {
        nfc_mod->dispatch_buffer = nrf_malloc(3);
        ctap_resp.length         = 3;
    }

    nfc_mod->dispatch_buffer[0]     = status;
    nfc_mod->[ctap_resp.length - 2] = SW_SUCCESS >> 8;
    nfc_mod->[ctap_resp.length - 1] = SW_SUCCESS & 0xff;

    nfc_mod->response_offset = 0;
    nfc_mod->response_len    = ctap_resp.length;
    /**ctap_resp.data[0]                    = status;*/
    /**ctap_resp.data[ctap_resp.length - 2] = SW_SUCCESS >> 8;*/
    /**ctap_resp.data[ctap_resp.length - 1] = SW_SUCCESS & 0xff;*/

    //    nfc_write_response_chaining(buf0, ctap_resp.data, ctap_resp.length, apdu->extended_apdu);
    nfc_reponse_init(nfc_mod);
    nfc_clean_dispatch(nfc_mod);
    NRF_LOG_INFO("CTAP answered");
}

static void apdu_read_binary(
    NFC_STATE *    nfc_mod,
    APDU_STRUCT *  apdu,
    const uint8_t *data,
    size_t         dataLength,
    uint32_t       flags)
{
    // response length
    reslen = apdu->le & 0xffff;
    switch (NFC_STATE.selected_applet)
    {
        case APP_CAPABILITY_CONTAINER:
            printf1(TAG_NFC, "APP_CAPABILITY_CONTAINER\r\n");
            if (reslen == 0 || reslen > sizeof(NFC_CC))
                reslen = sizeof(NFC_CC);
            nfc_write_response_ex(
                buf0,
                (uint8_t *) &NFC_CC,
                reslen,
                SW_SUCCESS);
            ams_wait_for_tx(10);
            break;
        case APP_NDEF_TAG:
            printf1(TAG_NFC, "APP_NDEF_TAG\r\n");
            if (reslen == 0 || reslen > sizeof(NDEF_SAMPLE) - 1)
                reslen = sizeof(NDEF_SAMPLE) - 1;
            nfc_write_response_ex(buf0, NDEF_SAMPLE, reslen, SW_SUCCESS);
            ams_wait_for_tx(10);
            break;
        default:
            nfc_write_response(buf0, SW_FILE_NOT_FOUND);
            printf1(TAG_ERR, "No binary applet selected!\r\n");
            return;
            break;
    }
}


/***************************************************************************** 
 *                            public interface
 *****************************************************************************/

void nfc_init(void)
{
    ret_code_t err_code;
    err_code = nfc_t4t_setup(nfc_callback, NULL);
    APP_ERROR_CHECK(err_code);


    //TODO INIT NFC STATE
}

void nfc_process(void)
{
    if (m_nfc.dispatch_ready)
    {
        process_dispatch(&m_nfc);
        nfc_clean_dispatch(&m_nfc);
    }
}
