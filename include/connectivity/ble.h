#ifndef _CONNECTIVITY_BLE_H
#define _CONNECTIVITY_BLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "util.h"

retvalue ble_init    (void);
retvalue ble_process (void);

#ifdef __cplusplus
} //extern c
#endif

#endif //define _connectivity_ble_h
