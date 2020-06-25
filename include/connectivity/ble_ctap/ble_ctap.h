/******************************************************************************
* File:             ble_ctap.h
*
* Author:           Joshua Steffensky   <joshua.steffensky@cispa.saarland>  
* Created:          06/05/20 
* Description:      ctap ble gatt service
*****************************************************************************/

#include "string.h"
#include "stdint.h"
#include "sdk_config.h"

#include "ctap.h"

#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "ble_conn_state.h"
#include "nrf_ble_gatt.h"

/***************************************************************************** 
*							BLE_CONTS
*****************************************************************************/

/* COMMANDS */
#define BLE_CTAP_CMD_PING      0x81
#define BLE_CTAP_CMD_KEEPALIVE 0x82
#define BLE_CTAP_CMD_MSG       0x83
#define BLE_CTAP_CMD_CANCEL    0xbe
#define BLE_CTAP_CMD_ERROR     0xbf

/* KEEPALIVE STATUS */
#define BLE_CTAP_STATUS_PROCESSING 0x01
#define BLE_CTAP_STATUS_UP_NEEDED  0x02

/* ERROR CODES */
#define BLE_CTAP_ERR_INVALID_CMD \
    0x01  // The command in the request is unknown/invalid
#define BLE_CTAP_ERR_INVALID_PAR \
    0x02  // The parameter(s) of the command is/are invalid or missing
#define BLE_CTAP_ERR_INVALID_LEN 0x03  // The length of the request is invalid
#define BLE_CTAP_ERR_INVALID_SEQ 0x04  // The sequence number is invalid
#define BLE_CTAP_ERR_REQ_TIMEOUT 0x05  // The request timed out
#define BLE_CTAP_ERR_BUSY \
    0x06  // The device is busy and can’t accept commands at this time.
#define BLE_CTAP_ERR_OTHER 0x7f  // Other, unspecified error


/* UUIDs */
// Service
#define BLE_CTAP_GATT_SERVICE_UUID 0xFFFD


// Characteristics
#define BLE_CTAP_CHARACTERISTICS_UUID_BASE                                \
    {                                                                     \
        0xBB, 0x23, 0xD6, 0x7E, 0xBA, 0xC9, 0x2F, 0xB4, 0xEE, 0xEC, 0xAA, \
            0xDE, 0x00, 0x00, 0xD0, 0xF1                                  \
    }

// F1D0FFF1-DEAA-ECEE-B42F-C9BA7ED623BB
#define BLE_CTAP_CONTROL_POINT_UUID 0xFFF1
// F1D0FFF2-DEAA-ECEE-B42F-C9BA7ED623BB
#define BLE_CTAP_STATUS_UUID 0xFFF2
// F1D0FFF3-DEAA-ECEE-B42F-C9BA7ED623BB
#define BLE_CTAP_CONTROL_POINT_LENGTH_UUID 0xFFF3
// F1D0FFF4-DEAA-ECEE-B42F-C9BA7ED623BB
#define BLE_CTAP_SERVICE_REVISION_BITFIELD_UUID 0xFFF4

#define BLE_CTAP_SERVICE_REVISION_UUID 0x2A28


#define CTAP_SERVICE_REVISION_BITFIELD_U2F_1_1 (1 << 7)
#define CTAP_SERVICE_REVISION_BITFIELD_U2F_1_2 (1 << 6)
#define CTAP_SERVICE_REVISION_BITFIELD_CTAP2   (1 << 5)


/***************************************************************************** 
*							BLE CTAP SETTING
*****************************************************************************/

//#define BLE_CTAP_MTU                (NRF_SDH_BLE_GATT_MAX_MTU_SIZE -3)
#define BLE_CTAP_MTU               NRF_SDH_BLE_GATT_MAX_MTU_SIZE - 3
#define BLE_CTAP_CTRL_PT_LENGTH    BLE_CTAP_MTU
#define BLE_CTAP_STATUS_LENGTH     BLE_CTAP_MTU
#define BLE_CTAP_BLE_OBSERVER_PRIO 2


/***************************************************************************** 
*							 USEFUL MACROS
*****************************************************************************/

/**@brief   Macro for defining a ble_ctap instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_CTAP_DEF(_name)         \
    static ble_ctap_t _name;        \
    NRF_SDH_BLE_OBSERVER(           \
        _name##_obs,                \
        BLE_CTAP_BLE_OBSERVER_PRIO, \
        ble_ctap_on_ble_evt,        \
        &_name)


/***************************************************************************** 
*							DATA STRUCTURES
*****************************************************************************/

typedef struct
{
    union
    {
        uint8_t type;  // Frame type - b7 defines type
        struct
        {
            uint8_t cmd;                     // Command - b7 set
            uint8_t bcnth;                   // Message byte count - high part
            uint8_t bcntl;                   // Message byte count - low part
            uint8_t data[BLE_CTAP_MTU - 3];  // Data payload
        } init;
        struct
        {
            uint8_t seq;                     // Sequence number - b7 cleared
            uint8_t data[BLE_CTAP_MTU - 1];  // Data payload
        } cont;
    };
} BLE_CTAP_FRAME;  // Cmp. HID_FRAME

typedef struct
{
    uint8_t  cmd;
    uint16_t length;
    uint8_t  data[CTAP_MAX_MESSAGE_SIZE];
} BLE_CTAP_REQ;

typedef struct
{
    uint8_t       stat;
    CTAP_RESPONSE data;
} BLE_CTAP_RESP;


typedef struct ble_ctap_t ble_ctap_t;

typedef void (
    *ble_ctap_request_handler_t)(ble_ctap_t* p_ctap, BLE_CTAP_REQ* req);

typedef struct
{
    ble_ctap_request_handler_t request_handler;  //TODO
    nrf_ble_gatt_t*            m_gatt;
} ble_ctap_init_t;

struct ble_ctap_t
{
    uint16_t                 service_handle;
    uint8_t                  chr_uuid_type;
    ble_gatts_char_handles_t ctrl_pt_handle;
    ble_gatts_char_handles_t status_handle;
    ble_gatts_char_handles_t ctrl_length_handle;
    ble_gatts_char_handles_t revision_bitfield_handle;
    volatile uint16_t        conn_handle;
    //    nrf_ble_gq_t *              p_gatt_queue;
    ble_ctap_request_handler_t request_handler;
    volatile uint8_t           waitingforContinuation : 1;
    volatile uint8_t           readyfordispatch : 1;
    volatile uint16_t          recvoffset;
    volatile uint8_t           recvnextseq;
    volatile BLE_CTAP_REQ* volatile dispatchPackage;
    nrf_ble_gatt_t* m_gatt;
};


/***************************************************************************** 
*							 USEFUL MACROS
*****************************************************************************/

/**@brief   Macro for defining a ble_ctap instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_CTAP_DEF(_name)         \
    static ble_ctap_t _name;        \
    NRF_SDH_BLE_OBSERVER(           \
        _name##_obs,                \
        BLE_CTAP_BLE_OBSERVER_PRIO, \
        ble_ctap_on_ble_evt,        \
        &_name)


/***************************************************************************** 
*	       					 BLE CTAP INTERFACE
*****************************************************************************/

uint32_t ble_ctap_init(ble_ctap_t* p_ctap, ble_ctap_init_t const* p_ctap_init);

void ble_ctap_on_ble_evt(ble_evt_t const* p_ble_evt, void* p_context);

void ble_ctap_send_resp(
    ble_ctap_t* p_ctap,
    uint8_t     cmd,
    uint8_t*    p_data,
    size_t      size,
    bool        reqcomplete);

void ble_ctap_send_err(ble_ctap_t* p_ctap, uint8_t err_code);

bool ble_ctap_process(ble_ctap_t* p_ctap);
