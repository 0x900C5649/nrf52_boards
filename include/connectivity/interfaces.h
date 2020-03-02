#ifndef _CONNECTIVITY_IFS_H
#define _CONNECTIVITY_IFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "util.h"

#if MODULE_ENABLED(APP_BLE)
#include "ble.h"
#endif

#if MODULE_ENABLED(APP_NFC)
#include "nfc.h"
#endif

#if MODULE_ENABLED(APP_USB)
#include "usb.h"
#endif


retvalue initIfaces();
retvalue processIfaces();
retvalue shutdownIfaces();


#ifdef __cplusplus
} //extern c
#endif

#endif //define _connectivity_IFS_h
