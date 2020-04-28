
#include "util.h"
#include "connectivity/interfaces.h"

#if APP_MODULE_ENABLED(BLE)
#include "ble.h"
#endif

#if APP_MODULE_ENABLED(NFC)
#include "nfc.h"
#endif

#include "hid.h"


/*  LOG INIT  */
#define NRF_LOG_MODULE_NAME interfaces

#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();
/*            */


/***************************************************************************** 
*							GLOBALS
*****************************************************************************/

volatile uint32_t device_status;


/***************************************************************************** 
*							FUNCTIONs
*****************************************************************************/

//shoud return returncode (0 -> ok)
static void (*initHooks[])() = {
    #if APP_MODULE_ENABLED(HID)
        hid_init,
    #endif
    #if APP_MODULE_ENABLED(BLE)
        ble_init,
    #endif
    #if APP_MODULE_ENABLED(NFC)
        nfc_init
    #endif
};

//shoud return amount of data processed ?
static void (*processHooks[])() = {
    #if APP_MODULE_ENABLED(HID)
        hid_process,
    #endif
    #if APP_MODULE_ENABLED(BLE)
        ble_process,
    #endif
    #if APP_MODULE_ENABLED(NFC)
        nfc_process
    #endif
};


void initIfaces(void)
{
    //iterate all interfaces
    for(uint8_t i = 0; i < sizeof(initHooks) / sizeof(initHooks[0] ) ; i++ )
    {
        NRF_LOG_INFO("inited %d interfaces", i);
        //call every hook in array
        initHooks[i]();
    }
}

void processIfaces(void) 
{
    //iterate all interfaces
    for(uint8_t i = 0; i < sizeof(processHooks) / sizeof(processHooks[0] ) ; i++ )
    {
        //call every hook in array
        processHooks[i]();
    }
}

void device_set_status(uint32_t status)
{
    device_status = status;
}

