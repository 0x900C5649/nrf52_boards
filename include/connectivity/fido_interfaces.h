#ifndef _CONNECTIVITY_IFS_H
#define _CONNECTIVITY_IFS_H

#include "util.h"

#define TYPE_MASK 0x80  // Frame type mask
#define TYPE_INIT 0x80  // Initial frame identifier
#define TYPE_CONT 0x00  // Continuation frame identifier

#define FRAME_TYPE(f) ((f).type & TYPE_MASK)
#define FRAME_CMD(f)  ((f).init.cmd & ~TYPE_MASK)
#define MSG_LEN(f)    ((f).init.bcnth * 256 + (f).init.bcntl)
#define FRAME_SEQ(f)  ((f).cont.seq & ~TYPE_MASK)

enum
{
    CTAP_STATUS_IDLE = 0,
    CTAP_STATUS_PROCESSING,
    CTAP_STATUS_UPNEEDED
};

typedef enum
{
    REQ_ORIGIN_NONE = 0,
    REQ_ORIGIN_HID,
    REQ_ORIGIN_BLE,
    REQ_ORIGIN_NFC
} req_origin_t;

void initIfaces();
void processIfaces();
void shutdownIfaces();

//status for interfaces (e.g. hid keepalive)
void device_set_status(uint32_t status);
//void ctaphid_update_status(int8_t status)

// Request origin
req_origin_t get_req_origin();
void         set_req_origin(req_origin_t toset);

#endif  //define _connectivity_IFS_h
