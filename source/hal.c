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
#include "mem_manager.h"
#include "app_timer.h"
#include "led_softblink.h"

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
//volatile uint32_t ms_ticks = 0;
static bool up_disabled = false;
/* Counter timer. */
APP_TIMER_DEF(m_timer_0);

static void bool_to_true_timeout_handler(void * p_context);
static void init_softblink(void);
static void start_softblink(void);
static void stop_softblink(void);

/**
 * \brief SysTick handler used to measure precise delay. 
 */
//void SysTick_Handler(void)
//{
//    ms_ticks++;
//}


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
    NRF_LOG_DEBUG("timers: %d", TIMER_COUNT);
    NRF_LOG_FLUSH();
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
//    bsp_board_init(BSP_INIT_LEDS);
    NRF_LOG_DEBUG("board init");
//    bsp_board_led_invert(LED_U2F_WINK);
    bsp_board_leds_off();
    /**bsp_board_led_invert(2);*/
    /**bsp_board_led_invert(1);*/
    /**bsp_board_led_invert(0);*/
    /* Enable SysTick interrupt for non busy wait delay. */
    /**if (SysTick_Config(SystemCoreClock / 1000)) {*/
        /**NRF_LOG_ERROR("init_bsp: SysTick configuration error!");*/
        /**while(1);*/
    /**}*/
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
   
    
    ret = nrf_drv_power_init(NULL);
    APP_ERROR_CHECK(ret);

    NRF_LOG_INFO("sd init...");
    ret = init_softdevice();
    APP_ERROR_CHECK(ret);

    NRF_LOG_DEBUG("mem init ...");
    ret = nrf_mem_init();
    APP_ERROR_CHECK(ret);

    NRF_LOG_INFO("timer init ...");
    init_app_timer();
    
    NRF_LOG_INFO("storage init ...");
    ret = init_storage();
    APP_ERROR_CHECK(ret);

    NRF_LOG_INFO("bsp init...");
    ret = init_bsp();
    APP_ERROR_CHECK(ret);
   
    NRF_LOG_INFO("softblink init...");
    init_softblink();

    NRF_LOG_INFO("init device done");
    NRF_LOG_FLUSH();
    //init_cli();
    return ret;
}

void board_reboot(void)
{
    NRF_LOG_ERROR("reboot not implemted");
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


static void bool_to_true_timeout_handler(void * p_context)
{
    *(bool *)p_context = true;
}

int ctap_user_presence_test(uint64_t delay)
{
    int ret;    
    ret_code_t err_code;

    if (up_disabled)
    {
        return 2;
    }
    NRF_LOG_INFO("waiting for user presence");
    APP_TIMER_DEF(up_test_timer_id);
    bool up_test_timed_out = false;

    err_code = app_timer_create(
                                &up_test_timer_id,
                                APP_TIMER_MODE_SINGLE_SHOT, 
                                bool_to_true_timeout_handler
                                );
    APP_ERROR_CHECK(err_code);
    
    //reset button pressed state
    UNUSED_RETURN_VALUE(is_user_button_pressed());
    
    start_softblink();
    err_code = app_timer_start(up_test_timer_id, APP_TIMER_TICKS(delay) , &up_test_timed_out);
    APP_ERROR_CHECK(err_code);

    //while button not pressed and timer not timed out power manage
    while(true) 
    {
        if(up_test_timed_out)
        {
            ret = 0;
            break;
        }
        else if(is_user_button_pressed())
        {
            ret = 1;
            break;
        }
        else
        {
            power_manage();
        }
    }
    
    err_code = app_timer_stop(up_test_timer_id);
    APP_ERROR_CHECK(err_code);
    stop_softblink();
    return ret;
}

void device_disable_up(bool disable)
{
    up_disabled = disable;
}

void board_wink(void)
{
    bsp_board_led_invert(LED_U2F_WINK);
}


static void timer_handle(void * p_context)
{
    //nothing to do
}

static void init_softblink(void)
{
    ret_code_t err_code;
    const led_sb_init_params_t led_sb_init_param = LED_SB_INIT_DEFAULT_PARAMS(SOFTBLINKMASK(BSP_GREEN));
    err_code = led_softblink_init(&led_sb_init_param);
    APP_ERROR_CHECK(err_code);
    led_softblink_off_time_set(400);
    led_softblink_on_time_set(200);
}

static void start_softblink(void)
{
    ret_code_t err_code;
    err_code = led_softblink_start(SOFTBLINKMASK(BSP_GREEN));
    APP_ERROR_CHECK(err_code);
}

static void stop_softblink(void)
{
    ret_code_t err_code;
    err_code = led_softblink_stop();
    APP_ERROR_CHECK(err_code);
}

void init_app_timer(void)
{
    ret_code_t ret;
    ret = app_timer_init();
    APP_ERROR_CHECK(ret);

    ret = app_timer_create(&m_timer_0, APP_TIMER_MODE_REPEATED, timer_handle);
    APP_ERROR_CHECK(ret);

    ret = app_timer_start(m_timer_0, APP_TIMER_TICKS(5000), NULL);
    APP_ERROR_CHECK(ret);
}  
