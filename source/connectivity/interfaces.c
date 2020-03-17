
#include "util.h"
#include "connectivity/interfaces.h"

/*  LOG INIT  */
#define NRF_LOG_MODULE_NAME interfaces

#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();
/*            */

//shoud return returncode (0 -> ok)
static retvalue (*initHooks[])() = {
    #if APP_MODULE_ENABLED(HID)
        &hid_init,
    #endif
    #if APP_MODULE_ENABLED(BLE)
        &ble_init,
    #endif
    #if APP_MODULE_ENABLED(NFC)
        &nfc_init
    #endif
};

//shoud return amount of data processed ?
static retvalue (*processHooks[])() = {
    #if APP_MODULE_ENABLED(HID)
        &hid_process,
    #endif
    #if APP_MODULE_ENABLED(BLE)
        &ble_process,
    #endif
    #if APP_MODULE_ENABLED(NFC)
        &nfc_process
    #endif
};


retvalue initIfaces() {
    retvalue ret = 0;

    //iterate all interfaces
    for(uint8_t i = 0; i < sizeof(initHooks) / sizeof(initHooks[0] ) ; i++ )
    {
        //call every hook in array
        ret = initHooks[i]();
        
        //check return value
        //TODO
    }
    return ret;
}

retvalue processIfaces() {
    retvalue ret = 0;

    //iterate all interfaces
    for(uint8_t i = 0; i < sizeof(processHooks) / sizeof(processHooks[0] ) ; i++ )
    {
        //call every hook in array
        ret = processHooks[i]();
        
        //check return value
        //TODO
    }
    return ret;
}

