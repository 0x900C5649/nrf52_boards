#ifndef _CONNECTIVITY_IFS_H
#define _CONNECTIVITY_IFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "util.h"


enum
{
    CTAP_STATUS_IDLE = 0,
    CTAP_STATUS_PROCESSING,
    CTAP_STATUS_UPNEEDED
};

void initIfaces();
void processIfaces();
void shutdownIfaces();

//status for interfaces (e.g. hid keepalive)
void device_set_status(uint32_t status);
//void ctaphid_update_status(int8_t status)

#ifdef __cplusplus
} //extern c
#endif

#endif //define _connectivity_IFS_h
