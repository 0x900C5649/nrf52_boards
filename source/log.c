
#include "log.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "nrf.h"

int init_log()
{
    ret_code_t ret;

    ret = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(ret);
    
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    return ret;
}

