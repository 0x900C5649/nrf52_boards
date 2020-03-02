#include "hal.h"

#include "nrf.h"
#include "app_util_platform.h"
#include "nrf_drv_clock.h"
#include "nrf_gpio.h"
#include "nrf_drv_power.h"

#include "bsp_cli.h"
#include "nrf_cli.h"
#include "nrf_cli_uart.h"

/*  LOG INIT  */
#define NRF_LOG_MODULE_NAME hal

#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();
/*            */


static bool m_user_button_pressed = false;
volatile uint32_t ms_ticks = 0;


/**
 * \brief SysTick handler used to measure precise delay. 
 */
void SysTick_Handler(void)
{
    ms_ticks++;
}


/**
 * \brief Check user button state. 
 */
bool is_user_button_pressed(void)
{
    if(m_user_button_pressed)
    {
        m_user_button_pressed = false;
        return true;
    }
    return false;
}

void bsp_event_callback(bsp_event_t ev)
{
    switch ((unsigned int)ev)
    {
        case CONCAT_2(BSP_EVENT_KEY_, BTN_U2F_USER):
            //NRF_LOG_INFO("BTN_U2F_USER pressed!");
            break;
        case CONCAT_2(BSP_USER_EVENT_RELEASE_, BTN_U2F_USER):
            m_user_button_pressed = true;
            //NRF_LOG_INFO("BTN_U2F_USER released!");
            break;
        default:
            return; // no implementation needed
    }
}


void init_bsp(void)
{
    
    ret_code_t ret;
    ret = bsp_init(BSP_INIT_BUTTONS, bsp_event_callback);
    APP_ERROR_CHECK(ret);
    
    INIT_BSP_ASSIGN_RELEASE_ACTION(BTN_U2F_USER);
    
    /* Configure LEDs */
    bsp_board_init(BSP_INIT_LEDS);

    /* Enable SysTick interrupt for non busy wait delay. */
    if (SysTick_Config(SystemCoreClock / 1000)) {
        NRF_LOG_ERROR("init_bsp: SysTick configuration error!");
        while(1);
    }
}

void init_cli(void)
{
    ret_code_t ret;
    ret = bsp_cli_init(bsp_event_callback);
    APP_ERROR_CHECK(ret);
    nrf_drv_uart_config_t uart_config = NRF_DRV_UART_DEFAULT_CONFIG;
    uart_config.pseltxd = TX_PIN_NUMBER;
    uart_config.pselrxd = RX_PIN_NUMBER;
    uart_config.hwfc    = NRF_UART_HWFC_DISABLED;
    ret = nrf_cli_init(&m_cli_uart, &uart_config, true, true, 
                       NRF_LOG_SEVERITY_INFO);
    APP_ERROR_CHECK(ret);
    ret = nrf_cli_start(&m_cli_uart);
    APP_ERROR_CHECK(ret);
}

