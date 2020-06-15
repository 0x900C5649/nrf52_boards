#ifndef _CONNECTIVITY_HID_H
#define _CONNECTIVITY_HID_H

#ifdef __cplusplus
extern "C" {
#endif

#include "util.h"
#include "timer_interface.h"
#include "nrf_drv_usbd.h"
#include "app_usbd_hid_generic.h"
#include "ctap.h"

// Size of HID reports 

#define HID_RPT_SIZE            64      // Default size of raw HID report
    
// Frame layout - command- and continuation frames

#define CID_BROADCAST           0xffffffff // Broadcast channel id


typedef struct {
  uint32_t cid;                        // Channel identifier
  union {
    uint8_t type;                      // Frame type - b7 defines type
    struct {
      uint8_t cmd;                     // Command - b7 set
      uint8_t bcnth;                   // Message byte count - high part
      uint8_t bcntl;                   // Message byte count - low part
      uint8_t data[HID_RPT_SIZE - 7];  // Data payload
    } init;
    struct {
      uint8_t seq;                     // Sequence number - b7 cleared
      uint8_t data[HID_RPT_SIZE - 5];  // Data payload
    } cont;
  };
} HID_FRAME;


// HID usage- and usage-page definitions

#define FIDO_USAGE_PAGE         0xf1d0  // FIDO alliance HID usage page
#define FIDO_USAGE_CTAPHID      0x01    // HID usage for top-level collection
#define FIDO_USAGE_DATA_IN      0x20    // Raw IN data report
#define FIDO_USAGE_DATA_OUT     0x21    // Raw OUT data report

// General constants    

#define HID_IF_VERSION       2       // Current interface implementation version
#define HID_TRANS_TIMEOUT    30000    // Default message timeout in ms

#define HID_FW_VERSION_MAJOR 1       // Major version number
#define HID_FW_VERSION_MINOR 0       // Minor version number
#define HID_FW_VERSION_BUILD 2       // Build version number

// HID native commands

#define HID_PING         (TYPE_INIT | 0x01)  // Echo data through local processor only
#define HID_MSG          (TYPE_INIT | 0x03)  // Send U2F message frame
#define HID_CBOR         (TYPE_INIT | 0x10)  // FIDO2 CBOR encoded command
#define HID_LOCK         (TYPE_INIT | 0x04)  // Send lock channel command
#define HID_INIT         (TYPE_INIT | 0x06)  // Channel initialization
#define HID_WINK         (TYPE_INIT | 0x08)  // Send device identification wink
#define HID_SYNC         (TYPE_INIT | 0x3c)  // Protocol resync command
#define HID_ERROR        (TYPE_INIT | 0x3f)  // Error response
#define HID_KEEPALIVE    (TYPE_INIT | 0x3b  // Error response

#define HID_VENDOR_FIRST (TYPE_INIT | 0x40)  // First vendor defined command
#define HID_VENDOR_LAST  (TYPE_INIT | 0x7f)  // Last vendor defined command
    
// HID_INIT command defines

#define INIT_NONCE_SIZE         8       // Size of channel initialization challenge
#define CAPABILITY_WINK         0x01    // If set to 1, authenticator implements CTAPHID_WINK function 
#define CAPABILITY_CBOR         0x04    // If set to 1, authenticator implements CTAPHID_CBOR function 
#define CAPABILITY_NMSG         0x08    // If set to 1, authenticator DOES NOT implement CTAPHID_MSG function 

typedef struct __attribute__ ((__packed__)) {
  uint8_t nonce[INIT_NONCE_SIZE];       // Client application nonce
} HID_INIT_REQ;

typedef struct __attribute__ ((__packed__)) {
  uint8_t nonce[INIT_NONCE_SIZE];       // Client application nonce
  uint32_t cid;                         // Channel identifier
  uint8_t versionInterface;             // Interface version // CTAPHID protocol version
  uint8_t versionMajor;                 // Major version number
  uint8_t versionMinor;                 // Minor version number
  uint8_t versionBuild;                 // Build version number
  uint8_t capFlags;                     // Capabilities flags  
} HID_INIT_RESP;

// HID_SYNC command defines

typedef struct __attribute__ ((__packed__)) {
  uint8_t nonce;                        // Client application nonce
} HID_SYNC_REQ;

typedef struct __attribute__ ((__packed__)) {
  uint8_t nonce;                        // Client application nonce
} HID_SYNC_RESP;

// Low-level error codes. Return as negatives.

#define ERR_NONE                0x00    // No error
#define ERR_INVALID_CMD         0x01    // Invalid command
#define ERR_INVALID_PAR         0x02    // Invalid parameter
#define ERR_INVALID_LEN         0x03    // Invalid message length
#define ERR_INVALID_SEQ         0x04    // Invalid message sequencing
#define ERR_MSG_TIMEOUT         0x05    // Message has timed out
#define ERR_CHANNEL_BUSY        0x06    // Channel busy
#define ERR_LOCK_REQUIRED       0x0a    // Command requires channel lock
#define ERR_SYNC_FAIL           0x0b    // SYNC command failed
#define ERR_OTHER               0x7f    // Other unspecified error



/**
 * @brief HID generic class interface number.
 * */
#define FIDO_HID_INTERFACE  0

/**
 * @brief HID generic class endpoint number.
 * */
#define HID_EPIN       NRF_DRV_USBD_EPIN1
#define HID_EPOUT      NRF_DRV_USBD_EPOUT1

/**
 * @brief Number of reports defined in report descriptor.
 */
#define REPORT_IN_QUEUE_SIZE    1

/**
 * @brief Size of maximum output report. HID generic class will reserve
 *        this buffer size + 1 memory space.
 *
 * Maximum value of this define is 63 bytes. Library automatically adds
 * one byte for report ID. This means that output report size is limited
 * to 64 bytes.
 */
#define REPORT_OUT_MAXSIZE  63



#define REPORT_FEATURE_MAXSIZE  255

/**
 * @brief HID generic class endpoints count.
 * */
#define HID_GENERIC_EP_COUNT  2

/**
 * @brief List of HID generic class endpoints. TODO
 * */
#define ENDPOINT_LIST()                                   \
(                                                         \
        HID_EPIN,                                         \
        HID_EPOUT                                         \
)

/**
 * @brief FIDO CTAP HID report descriptor.
 *
 */
#define APP_USBD_CTAP_HID_REPORT_DSC {                                     \
    0x06, 0xd0, 0xf1, /*     Usage Page (FIDO Alliance),           */     \
    0x09, 0x01,       /*     Usage (FIDO HID Authenticator Device), */     \
    0xa1, 0x01,       /*     Collection (Application),             */     \
    0x09, 0x20,       /*     Usage (Input Report Data),            */     \
    0x15, 0x00,       /*     Logical Minimum (0),                  */     \
    0x26, 0xff, 0x00, /*     Logical Maximum (255),                */     \
    0x75, 0x08,       /*     Report Size (8),                      */     \
    0x95, 0x40,       /*     Report Count (64),                    */     \
    0x81, 0x02,       /*     Input (Data, Variable, Absolute)      */     \
    0x09, 0x21,       /*     Usage (Output Report Data),           */     \
    0x15, 0x00,       /*     Logical Minimum (0),                  */     \
    0x26, 0xff, 0x00, /*     Logical Maximum (255),                */     \
    0x75, 0x08,       /*     Report Size (8),                      */     \
    0x95, 0x40,       /*     Report Count (64),                    */     \
    0x91, 0x02,       /*     Output (Data, Variable, Absolute)     */     \
    0xc0,             /*     End Collection,                       */     \
}

#ifndef USBD_POWER_DETECTION
    #define USBD_POWER_DETECTION true
#endif

#define MAX_HID_CHANNELS    5

#define CID_STATE_IDLE      1
#define CID_STATE_READY     2


typedef struct { struct channel *pFirst, *pLast; } channel_list_t;

typedef struct channel {
    struct channel * pPrev;
    struct channel * pNext;
    uint32_t cid;
    uint8_t cmd;
    uint8_t state;
    Timer timer;
    uint16_t bcnt;
    uint8_t req[CTAP_MAX_MESSAGE_SIZE]; // TODO
    uint8_t resp[CTAP_RESPONSE_BUFFER_SIZE+1]; //TODO
} hid_channel_t;

typedef struct __attribute__ ((__packed__)) //TODO REMOVE
{
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    uint8_t lc1;
    uint8_t lc2;
    uint8_t lc3;
} hid_req_apdu_header_t; 


void hid_init    (void);
void hid_process (void);


#ifdef __cplusplus
} //extern c
#endif

#endif //define _connectivity_hid_h

