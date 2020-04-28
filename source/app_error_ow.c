/******************************************************************************
* File:             app_error_ow.c
*
* Author:           Joshua Steffensky 
* Created:          04/09/20 
* Description:      overwrite the default handler for APP_ERROR_CHECK
*****************************************************************************/

#include "nrf_log.h"
#include "app_error.h"

/**@brief Function for error handling, which is called when an error has occurred.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of error.
 *
 * @param[in] error_code  Error code supplied to the handler.
 * @param[in] line_num    Line number where the handler is called.
 * @param[in] p_file_name Pointer to the file name.
 *
 * Function is implemented as weak so that it can be overwritten by custom application error handler
 * when needed.
 */

/*lint -save -e14 */
void app_error_handler(ret_code_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    error_info_t error_info =
    {
        .line_num    = line_num,
        .p_file_name = p_file_name,
        .err_code    = error_code,
    };
    
    char const * p_desc = nrf_strerror_get(error_code);
    NRF_LOG_WARNING("%s:%d error code: %s(0x%x)", NRF_LOG_PUSH((char * const)p_file_name),line_num, NRF_LOG_PUSH((char * const)p_desc), error_code);
  
    app_error_fault_handler(NRF_FAULT_ID_SDK_ERROR, 0, (uint32_t)(&error_info));

    UNUSED_VARIABLE(error_info);
}

