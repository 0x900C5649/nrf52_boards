
#include "util.h"
#include "fido_interfaces.h"
#include "app_config.h"

#if APP_MODULE_ENABLED(BLE)
    #include "fido_ble.h"
#endif

#if APP_MODULE_ENABLED(NFC)
    #include "fido_nfc.h"
#endif

#if APP_MODULE_ENABLED(HID)
    #include "fido_hid.h"
#endif

/*  LOG INIT  */
#define NRF_LOG_MODULE_NAME interfaces

#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();
/*            */


/***************************************************************************** 
*							GLOBALS
*****************************************************************************/

volatile uint32_t     device_status;
volatile req_origin_t origin;

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


/***************************************************************************** 
*							FUNCTIONS
*****************************************************************************/

void initIfaces(void)
{
    //iterate all interfaces
    for (uint8_t i = 0; i < sizeof(initHooks) / sizeof(initHooks[0]); i++)
    {
        NRF_LOG_INFO("inited %d interfaces", i);
        //call every hook in array
        initHooks[i]();
    }
}

void processIfaces(void)
{
    //iterate all interfaces
    for (uint8_t i = 0; i < sizeof(processHooks) / sizeof(processHooks[0]); i++)
    {
        //call every hook in array
        processHooks[i]();
    }
}

void device_set_status(uint32_t status) { device_status = status; }

req_origin_t get_req_origin() { return origin; }

void set_req_origin(req_origin_t toset) { origin = toset; }
