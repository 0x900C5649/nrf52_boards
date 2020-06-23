/******************************************************************************
* File:             fido_nfc.c
*
* Author:           Joshua Steffensky <joshua.steffensky@cispa.saarland>  
* Created:          06/18/20 
* Description:      nfc ctap implementation
*****************************************************************************/

#ifndef _CONNECTIVITY_NFC_H
#define _CONNECTIVITY_NFC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "util.h"

/***************************************************************************** 
*							CONSTS
*****************************************************************************/

/** original from SOLO:fido2/apdu.h **/
#define APDU_FIDO_U2F_REGISTER        0x01
#define APDU_FIDO_U2F_AUTHENTICATE    0x02
#define APDU_FIDO_U2F_VERSION         0x03
#define APDU_FIDO_NFCCTAP_MSG         0x10
#define APDU_FIDO_U2F_VENDOR_FIRST    0xc0    // First vendor defined command
#define APDU_FIDO_U2F_VENDOR_LAST     0xff    // Last vendor defined command
//#define APDU_SOLO_RESET               0xee

#define APDU_INS_SELECT               0xA4
#define APDU_INS_READ_BINARY          0xB0
#define APDU_GET_RESPONSE             0xC0

#define SW_SUCCESS                    0x9000
#define SW_GET_RESPONSE               0x6100  // Command successfully executed; 'XX' bytes of data are available and can be requested using GET RESPONSE.
#define SW_WRONG_LENGTH               0x6700
#define SW_COND_USE_NOT_SATISFIED     0x6985
#define SW_FILE_NOT_FOUND             0x6a82
#define SW_INCORRECT_P1P2             0x6a86
#define SW_INS_INVALID                0x6d00  // Instruction code not supported or invalid
#define SW_CLA_INVALID                0x6e00  
#define SW_INTERNAL_EXCEPTION         0x6f00
/**         SOLO:fido2/apdu.h       **/

/** original from SOLO:targets/stm32l432/src/nfc.h **/
#define AID_NDEF_TYPE_4               "\xD2\x76\x00\x00\x85\x01\x01"
#define AID_NDEF_MIFARE_TYPE_4        "\xD2\x76\x00\x00\x85\x01\x00"
#define AID_CAPABILITY_CONTAINER      "\xE1\x03"
#define AID_NDEF_TAG                  "\xE1\x04"
#define AID_FIDO                      "\xa0\x00\x00\x06\x47\x2f\x00\x01"
/**         SOLO:targets/stm32l432/src/nfc.h       **/

/** original from SOLO:fido2/apdu.h **/
typedef struct
{
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t lc;
} __attribute__((packed)) APDU_HEADER;

typedef struct
{
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t lc[3];
} __attribute__((packed)) EXT_APDU_HEADER;

typedef struct
{
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint16_t lc;
    uint8_t *data;
    uint32_t le;
    bool extended_apdu;
    uint8_t case_type;
} __attribute__((packed)) APDU_STRUCT;

uint8_t apdu_decode(uint8_t *data, size_t len, APDU_STRUCT *apdu);
/**         SOLO:fido2/apdu.h       **/


/***************************************************************************** 
*							STRUCTS
*****************************************************************************/

/** original from SOLO:targets/stm32l432/src/nfc.h **/
typedef enum
{
    APP_NOTHING = 0,
    APP_NDEF_TYPE_4 = 1,
    APP_MIFARE_TYPE_4,
    APP_CAPABILITY_CONTAINER,
    APP_NDEF_TAG,
	APP_FIDO,
} NFC_APPLETS;
/**         SOLO:targets/stm32l432/src/nfc.h       **/

typedef struct
{
    NRF_APPLETS selected_applet;
    uint8_t     dispatch_ready:1;
    APDU_HEADER last_dispatch_header;
    uint8_t     dispatch_package[CTAP_MAX_MESSAGE_SIZE];
    uint8_t     response_buffer[CTAP_MAX_RESPONSE_SIZE+1];
} NFC_CTAP;


/***************************************************************************** 
*							public interface
*****************************************************************************/

void nfc_init    (void);
void nfc_process (void);

#ifdef __cplusplus
} //extern c
#endif

#endif //define _connectivity_nfc_h
