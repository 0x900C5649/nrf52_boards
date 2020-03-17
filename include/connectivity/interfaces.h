#ifndef _CONNECTIVITY_IFS_H
#define _CONNECTIVITY_IFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "util.h"

#if APP_MODULE_ENABLED(BLE)
#include "ble.h"
#endif

#if APP_MODULE_ENABLED(NFC)
#include "nfc.h"
#endif

#if APP_MODULE_ENABLED(USB)
#include "usb.h"
#endif


retvalue initIfaces();
retvalue processIfaces();
retvalue shutdownIfaces();


#ifdef __cplusplus
} //extern c
#endif

#endif //define _connectivity_IFS_h
