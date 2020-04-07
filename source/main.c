/**
* Copyright (c) 2018 makerdiary
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
* * Redistributions of source code must retain the above copyright
*   notice, this list of conditions and the following disclaimer.
*
* * Redistributions in binary form must reproduce the above
*   copyright notice, this list of conditions and the following
*   disclaimer in the documentation and/or other materials provided
*   with the distribution.

* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "nrf.h"
#include "app_util_platform.h"
#include "nrf_drv_clock.h"
#include "nrf_gpio.h"
#include "nrf_drv_power.h"

#include "app_timer.h"
#include "app_error.h"
#include "bsp.h"

#include "app_config.h"
#include "hal.h"
#include "storage.h"

#include "bsp_cli.h"
#include "nrf_cli.h"
#include "nrf_cli_uart.h"

#define NRF_LOG_MODULE_NAME main

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

NRF_LOG_MODULE_REGISTER();

#include "log.h"

/**
 * @brief CLI interface over UART
 */
//NRF_CLI_UART_DEF(m_cli_uart_transport, 0, 64, 16);
//NRF_CLI_DEF(m_cli_uart,
//            "ctap_cli:~$ ",
//            &m_cli_uart_transport.transport,
//           '\r',
//            8);



int main(void)
{
    ret_code_t ret;

    ret = init_log();
    APP_ERROR_CHECK(ret);
    
    NRF_LOG_DEBUG("log initialised");
/*
    ret = nrf_drv_clock_init();
    APP_ERROR_CHECK(ret);
    
    NRF_LOG_DEBUG("drv_clock_init");
    NRF_LOG_FLUSH();

    nrf_drv_clock_lfclk_request(NULL);

    while(!nrf_drv_clock_lfclk_is_running())
    {
        // Just waiting 
    }
    NRF_LOG_DEBUG("lfclk_is_running");
    NRF_LOG_FLUSH();
*/ 
    ret = init_softdevice();
    APP_ERROR_CHECK(ret);
    NRF_LOG_DEBUG("sd inited");

    ret = app_timer_init();
    APP_ERROR_CHECK(ret);
    
    NRF_LOG_DEBUG("timer_init");
    NRF_LOG_FLUSH();

    ret = init_device();
    APP_ERROR_CHECK(ret);
    
    NRF_LOG_DEBUG("initing storage");
    NRF_LOG_FLUSH();

    ret = init_storage();
    APP_ERROR_CHECK(ret);

    NRF_LOG_DEBUG("init bsp + cli");
    NRF_LOG_FLUSH();
    
    NRF_LOG_INFO("Hello FIDO U2F Security Key!");

//    ret = u2f_hid_init();
//    if(ret != NRF_SUCCESS)
//    {
//        NRF_LOG_ERROR("Fail to initialize U2F HID![%d]", ret);
//    }
//    APP_ERROR_CHECK(ret);
    
    NRF_LOG_DEBUG("testing storage now");
    NRF_LOG_FLUSH();
    
    // delete it
    reset_ctap_rk();
    NRF_LOG_DEBUG("delete rks")
    NRF_LOG_FLUSH();
    
    // initialize test data
    uint8_t testdata1[] = {0x81, 0x18, 0xF1, 0xD0};
    uint8_t testdata2[] = {0x18, 0x81, 0xF1, 0xD0};
    CTAP_residentKey input;
    CTAP_residentKey output;
    NRF_LOG_DEBUG("memcpy");
    NRF_LOG_FLUSH();
    memcpy(&input, &testdata1, 4);
    
    // write it to flash
    store_ctap_rk(1, &input);
    NRF_LOG_DEBUG("write test rk");
    NRF_LOG_FLUSH();

    // read it again
    load_ctap_rk(1, &output);
    NRF_LOG_DEBUG("load test rk");
    NRF_LOG_FLUSH();

    // print read data
    NRF_LOG_HEXDUMP_DEBUG(&output,4)
    NRF_LOG_FLUSH();

    // update entry
    memcpy(&input, &testdata2, 4);
    overwrite_ctap_rk(1, &input);

    // read it
    load_ctap_rk(1, &output);
    NRF_LOG_DEBUG("load test rk");
    NRF_LOG_FLUSH();

    // print it
    NRF_LOG_HEXDUMP_DEBUG(&output,4)

    // delete it
    void reset_ctap_rk();
    NRF_LOG_DEBUG("delete rks")
    NRF_LOG_FLUSH();

    while (true)
    {

//        u2f_hid_process();

//        nrf_cli_process(&m_cli_uart);
//        NRF_LOG_INFO("THIS IS A TEST")
        if (is_user_button_pressed()){
            NRF_LOG_INFO("button pressed");
        }

        UNUSED_RETURN_VALUE(NRF_LOG_PROCESS());
        
    }
}

