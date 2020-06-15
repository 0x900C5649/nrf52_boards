/******************************************************************************
* File:             ble_ctap.h
*
* Author:           Joshua Steffensky   <joshua.steffensky@cispa.saarland>  
* Created:          06/05/20 
* Description:      ctap ble gatt service
*****************************************************************************/

/***************************************************************************** 
*							BLE_CONTS
*****************************************************************************/

/* COMMANDS */
#define BLE_CTAP_CMD_PING        0x81
#define BLE_CTAP_CMD_KEEPALIVE   0x82
#define BLE_CTAP_CMD_MSG         0x83
#define BLE_CTAP_CMD_CANCEL      0xbe
#define BLE_CTAP_CMD_ERROR       0xbf

/* KEEPALIVE STATUS */
#define BLE_CTAP_STATUS_PROCESSING   0x01
#define BLE_CTAP_STATUS_UP_NEEDED    0x02

/* ERROR CODES */
#define BLE_CTAP_ERR_INVALID_CMD     0x01    // The command in the request is unknown/invalid
#define BLE_CTAP_ERR_INVALID_PAR     0x02    // The parameter(s) of the command is/are invalid or missing
#define BLE_CTAP_ERR_INVALID_LEN     0x03    // The length of the request is invalid
#define BLE_CTAP_ERR_INVALID_SEQ     0x04    // The sequence number is invalid
#define BLE_CTAP_ERR_REQ_TIMEOUT     0x05    // The request timed out
#define BLE_CTAP_ERR_BUSY            0x06    // The device is busy and can’t accept commands at this time.
#define BLE_CTAP_ERR_OTHER           0x7f    // Other, unspecified error


/* UUIDs */
// Service
#define BLE_CTAP_GATT_SERVICE_UUID  0xFFFD


// Characteristics
#define BLE_CTAP_CHARACTERISTICS_UUID_BASE          {                                                   \
                                                    0xBB, 0x23, 0xD6, 0x7E, 0xBA, 0xC9, 0x2F, 0xB4, \
                                                    0xEE, 0xEC, 0xAA, 0xDE, 0x00, 0x00, 0xD0, 0xF1  \
                                                    }

// F1D0FFF1-DEAA-ECEE-B42F-C9BA7ED623BB
#define BLE_CTAP_CONTROL_POINT_UUID                 0xFFF1
// F1D0FFF2-DEAA-ECEE-B42F-C9BA7ED623BB
#define BLE_CTAP_STATUS_UUID                        0xFFF2
// F1D0FFF3-DEAA-ECEE-B42F-C9BA7ED623BB
#define BLE_CTAP_CONTROL_POINT_LENGTH_UUID          0xFFF3
// F1D0FFF4-DEAA-ECEE-B42F-C9BA7ED623BB
#define BLE_CTAP_SERVICE_REVISION_BITFIELD_UUID     0xFFF4

#define BLE_CTAP_SERVICE_REVISION_UUID              0x2A28


#define CTAP_SERVICE_REVISION_BITFIELD_U2F_1_1  (1 << 7)
#define CTAP_SERVICE_REVISION_BITFIELD_U2F_1_2  (1 << 6)
#define CTAP_SERVICE_REVISION_BITFIELD_FIDO2    (1 << 5)


/***************************************************************************** 
*							BLE FIDO SETTING
*****************************************************************************/

#define BLE_CTAP_MTU                (NRF_SDH_BLE_GATT_MAX_MTU_SIZE -3)
#define BLE_CTAP_CTRL_PT_LENGTH     BLE_CTAP_MTU
#define BLE_CTAP_STATUS_LENGTH      BLE_CTAP_MTU
#define BLE_CTAP_BLE_OBSERVER_PRIO  2


/***************************************************************************** 
*							 USEFUL MACROS
*****************************************************************************/

/**@brief   Macro for defining a ble_fido instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_FIDO_DEF(_name)                          \
    static ble_fido_t _name;                         \
    NRF_SDH_BLE_OBSERVER(_name ## _obs,             \
                         BLE_FIDO_BLE_OBSERVER_PRIO, \
                         ble_fido_on_ble_evt, &_name)


/***************************************************************************** 
*							DATA STRUCTURES
*****************************************************************************/

typedef struct {
  union {
    uint8_t type;                       // Frame type - b7 defines type
    struct {
      uint8_t cmd;                      // Command - b7 set
      uint8_t bcnth;                    // Message byte count - high part
      uint8_t bcntl;                    // Message byte count - low part
      uint8_t data[BLE_MTU - 3];        // Data payload
    } init;
    struct {
      uint8_t seq;                      // Sequence number - b7 cleared
      uint8_t data[BLE_MTU - 1];        // Data payload
    } cont;
  };
} BLE_CTAP_FRAME; // Cmp. HID_FRAME

typedef struct {
    uint8_t cmd;
    uint16_t length;
    uint8_t data[CTAP_MAX_MESSAGE_SIZE];
} BLE_CTAP_REQ;

typedef struct {
    uint8_t stat;
    CTAP_RESPONSE data;
} BLE_CTAP_RESP;

typedef struct {
    uint16_t                    service_handle;
    uint8_t                     chr_uuid_type;
    ble_gatts_char_handles_t    ctrl_pt_handle;
    ble_gatts_char_handles_t    status_handle;
    ble_gatts_char_handles_t    ctrl_length_handle;
    ble_gatts_char_handles_t    revision_bitfield_handle;
    volatile uint16_t           conn_handle;
    nrf_ble_gq_t *              p_gatt_queue;
    volatile uint8_t            waitingforContinuation:1;
    volatile uint8_t            readyfordispatch:1;
    volatile uint16_t           recvoffset;
    volatile uint8_t            recvnextseq;
    volatile BLE_REQ *          dispatchPackage;
} ble_fido_t;

typedef void (*ble_ctap_msg_handler_t)(ble_fido_t * p_fido, BLE_REQ* req);

typedef struct {
    ble_ctap_msg_handler_t  evt_handler; //TODO
//    ble_srv_error_handler_t error_handler;
} ble_fido_init_t;


/***************************************************************************** 
*							 USEFUL MACROS
*****************************************************************************/

/**@brief   Macro for defining a ble_fido instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_FIDO_DEF(_name)                          \
    static ble_fido_t _name;                         \
    NRF_SDH_BLE_OBSERVER(_name ## _obs,             \
                         BLE_FIDO_BLE_OBSERVER_PRIO, \
                         ble_fido_on_ble_evt, &_name)


/***************************************************************************** 
*							 USEFUL MACROS
*****************************************************************************/

/**@brief   Macro for defining a ble_fido instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_FIDO_DEF(_name)                          \
    static ble_fido_t _name;                         \
    NRF_SDH_BLE_OBSERVER(_name ## _obs,             \
                         BLE_FIDO_BLE_OBSERVER_PRIO, \
                         ble_fido_on_ble_evt, &_name)

/***************************************************************************** 
*							FUNCTIONS
*****************************************************************************/

/**@brief Function for handling writes to fido control point.
 *
 * @param[in]   p_fido                  fido service structure
 * @param[in]   ble_gatts_evt_write_t   write event
 */
static void ble_fido_on_ctrl_pt_write(ble_fido_t * p_fido, ble_gatts_evt_write_t const * p_evt_write);

/**@brief Function for handling writes to fido revision bitfield.
 *
 * @param[in]   p_fido                  fido service structure
 * @param[in]   ble_gatts_evt_write_t   write event
 */
static void ble_fido_on_revision_bitfield_write(ble_fido_t * p_fido, ble_gatts_evt_write_t const * p_evt_write);

/**@brief Function for cleaning the recving datastructures in the fido service structure
 *
 * @param[in]   p_fido                  fido service structure
 */
static void ble_fido_clear_recv(ble_fido_t * p_fido);


/**@brief ble fido write event handler
 *
 * @param[in]   p_fido                  fido service structure
 * @param[in]   p_ble_evt               ble event
 */
static void ble_fido_on_write(ble_fido_t * p_fido, ble_evt_t const * p_ble_evt);

/**@brief ble fido connect event handler
 *
 * @param[in]   p_fido                  fido service structure
 * @param[in]   p_ble_evt               ble event
 */
static void ble_fido_on_connect(ble_fido_t * p_fido, ble_evt_t const * p_ble_evt);

/**@brief ble fido disconnect event handler
 *
 * @param[in]   p_fido                  fido service structure
 * @param[in]   p_ble_evt               ble event
 */
static void ble_fido_on_disconnect(ble_fido_t * p_fido, ble_evt_t const * p_ble_evt);

/**@brief ble power manage until at least 1 transmission is completed
 */
static void ble_waitforsendcmpl();

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
static ret_code_t ble_frame_send(uint16_t conn_handle,  ble_gatts_char_handles_t char_handle, BLE_FRAME * p_frame, uint16_t * p_length);

/**
 * @brief		Process received MSG request
 * 
 * @param[in] 	req request to be processed
 *
 * @return 		void
 */
static void ble_fido_msg_response(BLE_REQ* req);

/** 
 * @brief		Process received PING request
 *
 * @param[in] 	req	request to be processed
 *
 * @return 		void
 */
static void ble_fido_ping_response(BLE_REQ* req);

