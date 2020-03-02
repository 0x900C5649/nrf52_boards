
#ifndef _CONNECTIVITY_NFC_H
#define _CONNECTIVITY_NFC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "util.h"

retvalue nfc_init    (void);
retvalue nfc_process (void);

#ifdef __cplusplus
} //extern c
#endif

#endif //define _connectivity_nfc_h
