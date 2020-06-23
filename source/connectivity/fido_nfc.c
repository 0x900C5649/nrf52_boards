/******************************************************************************
* File:             fido_nfc.c
*
* Author:           Joshua Steffensky <joshua.steffensky@cispa.saarland>  
* Created:          06/18/20 
* Description:      nfc ctap implementation
*****************************************************************************/

#include "fido_nfc.h"


/***************************************************************************** 
*							local prototypes
*****************************************************************************/

/**
 * @brief Callback function for handling NFC events.
 */
static void nfc_callback(void          * context,
                         nfc_t4t_event_t event,
                         const uint8_t * data,
                         size_t          dataLength,
                         uint32_t        flags);


/***************************************************************************** 
*							APDU decoding
*****************************************************************************/

/** original from SOLO:fido2/apdu.c **/
int apdu_decode(uint8_t *data, size_t len, APDU_STRUCT *apdu) 
{
    EXT_APDU_HEADER *hapdu = (EXT_APDU_HEADER *)data;
    
    apdu->cla = hapdu->cla & 0xef; // mask chaining bit if any
    apdu->ins = hapdu->ins;
    apdu->p1 = hapdu->p1;
    apdu->p2 = hapdu->p2;
    
    apdu->lc = 0;
    apdu->data = NULL;
    apdu->le = 0;
    apdu->extended_apdu = false;
    apdu->case_type = 0x00;
    
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
        apdu->le = b0;
        if (!apdu->le)
            apdu->le = 0x100;
    }
   
    // case 3S (Lc + data)
    if (len == 5U + b0 && b0 != 0)
    {
        apdu->case_type = 0x03;
        apdu->lc = b0;
    }
    
    // case 4S (Lc + data + Le)
    if (len == 5U + b0 + 1U && b0 != 0)
    {
        apdu->case_type = 0x04;
        apdu->lc = b0;
        apdu->le = data[len - 1];
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
            apdu->case_type = 0x12;
            apdu->extended_apdu = true;
            apdu->le = extlen;
            if (!apdu->le)
                apdu->le = 0x10000;
        }
        
       // case 3E (Lc + data) - extended
       if (len == 7U + extlen)
        {
            apdu->case_type = 0x13;
            apdu->extended_apdu = true;
            apdu->lc = extlen;
        }

       // case 4E (Lc + data + Le) - extended 2-byte Le
       if (len == 7U + extlen + 2U)
        {
            apdu->case_type = 0x14;
            apdu->extended_apdu = true;
            apdu->lc = extlen;
            apdu->le = (data[len - 2] << 8) + data[len - 1];
        if (!apdu->le)
            apdu->le = 0x10000;
        }

       // case 4E (Lc + data + Le) - extended 3-byte Le
       if (len == 7U + extlen + 3U && data[len - 3] == 0)
        {
            apdu->case_type = 0x24;
            apdu->extended_apdu = true;
            apdu->lc = extlen;
            apdu->le = (data[len - 2] << 8) + data[len - 1];
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
        } else {
            apdu->data = data + 5;
        }
        
    }   
    
    return 0;
}
/**         SOLO:fido2/apdu.c       **/


/***************************************************************************** 
*							NFC MSG HANDLING
*****************************************************************************/

static void nfc_callback(void          * context,
                         nfc_t4t_event_t event,
                         const uint8_t * data,
                         size_t          dataLength,
                         uint32_t        flags)
{
    ret_code_t err_code;
    uint32_t   resp_len;

    (void)context;

    switch (event)
    {
        case NFC_T4T_EVENT_FIELD_ON:
            NRF_LOG_INFO("NFC Tag has been selected");

            // Flush all FIFOs. Data that was collected from UART channel before selecting
            // the tag is discarded.
            fifos_flush();
            break;

        case NFC_T4T_EVENT_FIELD_OFF:
            NRF_LOG_INFO("NFC field lost");
            break;

        case NFC_T4T_EVENT_DATA_IND:
            

            if (apdu_len + dataLength > APDU_BUFF_SIZE)
            {
                APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
            }
            memcpy(apdu_buf + apdu_len, data, dataLength);
            apdu_len += dataLength;

            if (flags != NFC_T4T_DI_FLAG_MORE)
            {
                // Store data in NFC RX FIFO if payload is present.
                if (apdu_len > HEADER_FIELD_SIZE)
                {
                    NRF_LOG_DEBUG("NFC RX data length: %d", apdu_len);
                    uint32_t buff_size;

                    apdu_len -= HEADER_FIELD_SIZE;
                    buff_size = apdu_len;
                    err_code  = app_fifo_write(&m_nfc_rx_fifo,
                                               apdu_buf + HEADER_FIELD_SIZE,
                                               &buff_size);
                    if ((buff_size != apdu_len) || (err_code == NRF_ERROR_NO_MEM))
                    {
                        NRF_LOG_WARNING("NFC RX FIFO buffer overflow");
                    }
                }
                apdu_len = 0;

                // Check if there is any data in NFC TX FIFO that needs to be transmitted.
                resp_len = MIN(APDU_BUFF_SIZE - HEADER_FIELD_SIZE, MAX_APDU_LEN);
                if (app_fifo_read(&m_nfc_tx_fifo, apdu_buf + HEADER_FIELD_SIZE, &resp_len) ==
                    NRF_ERROR_NOT_FOUND)
                {
                    resp_len = 0;
                }

                if (resp_len > 0)
                {
                    NRF_LOG_DEBUG("NFC TX data length: %d", resp_len);
                }

                // Send the response PDU over NFC.
                err_code = nfc_t4t_response_pdu_send(apdu_buf, resp_len + HEADER_FIELD_SIZE);
                APP_ERROR_CHECK(err_code);

                bsp_board_led_off(BSP_BOARD_LED_1);
            }
            else
            {
                bsp_board_led_on(BSP_BOARD_LED_1);
            }
            break;
        
        case NFC_T4T_EVENT_DATA_TRANSMITTED:
        default:
            NRF_LOG_WARNUNG("something else happend");
            break;
    }
}

static void process_apdu(NFC_CTAP      * nfc_mod,
                         const uint8_t * data,
                         size_t          dataLength,
                         uint32_t        flags)
{
    //parse header
    APDU_STRUCT apdu;
    memset(&apdu, 0, sizeof(apdu));
    apdu_decode(data, dataLength, &apdu);


    //if select applet
        //check for already selected applets
        //set applet
        //respond success
    
    //if ctap package/chaining
    //add data to dispatch package
    //if command:
        // set dispatch header
    
    //set ready for dispatch 
    
    //if get data
}

/***************************************************************************** 
*							APDU MSG HANDLING
*****************************************************************************/

void process_package(APDU_HEADER * head, uint8_t * package, size_t packagelen, uint8_t * respbuffer, size_t * respsize)
{
    
}


/***************************************************************************** 
*							public interface
*****************************************************************************/

void nfc_init(void)
{

    ret_code_t err_code;
    err_code = nfc_t4t_setup(nfc_callback, NULL);
    APP_ERROR_CHECK(err_code);

}

void nfc_process(void)
{
    if (m_nfc.dispatch_ready)
    {
        apdu_process(m_nfc.dispatch_Package)
    }
}

