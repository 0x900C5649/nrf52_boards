/******************************************************************************
* File:             fido_nfc.c
*
* Author:           Joshua Steffensky <joshua.steffensky@cispa.saarland>  
* Created:          06/18/20 
* Description:      nfc ctap implementation
*****************************************************************************/

#ifndef CONNECTIVITY_NFC_H
#define CONNECTIVITY_NFC_H

#include "util.h"
#include "apdu.h"


/***************************************************************************** 
*							Settings
*****************************************************************************/

#define MAX_NFC_LENGTH 512


/***************************************************************************** 
*							CONSTS
*****************************************************************************/

/** original from SOLO:targets/stm32l432/src/nfc.h **/
#define AID_NDEF_TYPE_4          "\xD2\x76\x00\x00\x85\x01\x01"
#define AID_NDEF_MIFARE_TYPE_4   "\xD2\x76\x00\x00\x85\x01\x00"
#define AID_CAPABILITY_CONTAINER "\xE1\x03"
#define AID_NDEF_TAG             "\xE1\x04"
#define AID_FIDO                 "\xa0\x00\x00\x06\x47\x2f\x00\x01"
/**         SOLO:targets/stm32l432/src/nfc.h       **/


/***************************************************************************** 
*							STRUCTS
*****************************************************************************/

/** original from SOLO:targets/stm32l432/src/nfc.h **/
typedef enum
{
    APP_NOTHING     = 0,
    APP_NDEF_TYPE_4 = 1,
    APP_MIFARE_TYPE_4,
    APP_CAPABILITY_CONTAINER,
    APP_NDEF_TAG,
    APP_FIDO,
} NFC_APPLETS;
/**         SOLO:targets/stm32l432/src/nfc.h       **/

typedef struct
{
    NFC_APPLETS selected_applet;
    uint8_t     dispatch_ready : 1;
    //    uint8_t         response_ready:1;
    APDU_STRUCT *dispatch_head;
    uint8_t *    dispatch_package;
    size_t       dispatch_length;
    uint8_t *    response_buffer;
    size_t       response_length;
    size_t       response_offset;
} NFC_STATE;


/***************************************************************************** 
*							public interface
*****************************************************************************/

void nfc_init(void);
void nfc_process(void);

#endif  //define _connectivity_nfc_h
