#include "hal.h"
//#include "storage.h"

#include "nrf.h"
#include "app_util_platform.h"
#include "nrf_drv_clock.h"
#include "nrf_gpio.h"
#include "nrf_drv_power.h"

#include "nrf_sdh.h"

#include "bsp_cli.h"
#include "nrf_cli.h"
#include "nrf_cli_uart.h"
#include "nrf_peripherals.h"
#include "app_gpiote.h"
/*  LOG INIT  */
#define NRF_LOG_MODULE_NAME hal

#include "nrf_log.h"

NRF_LOG_MODULE_REGISTER();
/*            */


/**
 * @brief CLI interface over UART
 */
//NRF_CLI_UART_DEF(m_cli_uart_transport, 0, 64, 16);
/**NRF_CLI_DEF(m_cli_uart,*/
            /**"ctap_cli:~$ ",*/
            /**&m_cli_uart_transport.transport,*/
            /**'\r',*/
            /**8);*/

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

static void bsp_event_callback(bsp_event_t ev)
{
    NRF_LOG_DEBUG("bsp_event_callback");
    switch ((unsigned int)ev)
    {
        case CONCAT_2(BSP_EVENT_KEY_, BTN_U2F_USER):
            NRF_LOG_INFO("BTN_U2F_USER pressed!");
            break;
        case CONCAT_2(BSP_USER_EVENT_RELEASE_, BTN_U2F_USER):
            m_user_button_pressed = true;
            //NRF_LOG_INFO("BTN_U2F_USER released!");
            break;
        default:
            return; // no implementation needed
    }
}


ret_code_t init_bsp(void)
{
    APP_GPIOTE_INIT(4); 
    NRF_LOG_DEBUG("init_bsp start");
    NRF_LOG_DEBUG("timers: %d", TIMER_COUNT)
    ret_code_t ret;
    ret = bsp_init(BSP_INIT_BUTTONS, bsp_event_callback);
    NRF_LOG_DEBUG("bsp_init returned: %d", ret)
    //APP_ERROR_CHECK(ret);
    //if VERIFY_MODULE_INITIALIZED_BOOL(BSP)
    NRF_LOG_DEBUG("bsp_init");
    
    INIT_BSP_ASSIGN_RELEASE_ACTION(BTN_U2F_USER);
    
    ret = bsp_buttons_enable();
    NRF_LOG_DEBUG("bsp_button returned: %d", ret)


    NRF_LOG_DEBUG("init assign release action");
    /* Configure LEDs */
    bsp_board_init(BSP_INIT_LEDS);
    NRF_LOG_DEBUG("board init");
    bsp_board_led_invert(LED_U2F_WINK);
    /* Enable SysTick interrupt for non busy wait delay. */
    if (SysTick_Config(SystemCoreClock / 1000)) {
        NRF_LOG_ERROR("init_bsp: SysTick configuration error!");
        while(1);
    }
    return ret;
}

void init_cli(void)
{
    /**ret_code_t ret;*/
    /**ret = bsp_cli_init(bsp_event_callback);*/
    /**APP_ERROR_CHECK(ret);*/
    /**nrf_drv_uart_config_t uart_config = NRF_DRV_UART_DEFAULT_CONFIG;*/
    /**uart_config.pseltxd = TX_PIN_NUMBER;*/
    /**uart_config.pselrxd = RX_PIN_NUMBER;*/
    /**uart_config.hwfc    = NRF_UART_HWFC_DISABLED;*/
    /**ret = nrf_cli_init(&m_cli_uart, &uart_config, true, true, */
                       /**NRF_LOG_SEVERITY_INFO);*/
    /**APP_ERROR_CHECK(ret);*/
    /**ret = nrf_cli_start(&m_cli_uart);*/
    /**APP_ERROR_CHECK(ret);*/
}

ret_code_t init_device(void)
{
    NRF_LOG_DEBUG("init_device");
    ret_code_t ret;
    
    NRF_LOG_DEBUG("init bsp");
    NRF_LOG_FLUSH();
    ret = init_bsp();
    APP_ERROR_CHECK(ret);
    
/*    NRF_LOG_DEBUG("init sd");
    NRF_LOG_FLUSH();
    ret = init_softdevice();
    APP_ERROR_CHECK(ret); */
    
    NRF_LOG_DEBUG("init device done");
    NRF_LOG_FLUSH();

    //init_cli();
    return ret;
}

void board_reboot(void)
{

}


void power_manage(void)
{
#ifdef SOFTDEVICE_PRESENT
    (void) sd_app_evt_wait();
#else
    __WFE();
#endif
}

ret_code_t init_softdevice(void)
{
    ret_code_t err_code;
    // Attempt to enable the SoftDevice
    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);
    
    return err_code;
}

