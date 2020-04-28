// Hardware abstraction layer

#ifndef _HAL_H
#define _HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sdk_errors.h"

#include "util.h"
#include "bsp.h"

/**
 * @brief Auxiliary internal macro
 *
 * Macro used only in @ref init_bsp to simplify the configuration
 */
#define INIT_BSP_ASSIGN_RELEASE_ACTION(btn)                      \
    APP_ERROR_CHECK(                                             \
        bsp_event_to_button_action_assign(                       \
            btn,                                                 \
            BSP_BUTTON_ACTION_RELEASE,                           \
            (bsp_event_t)CONCAT_2(BSP_USER_EVENT_RELEASE_, btn)) \
    )


/**
 * @brief Additional key release events
 *
 * This example needs to process release events of used buttons
 */
enum {
    BSP_USER_EVENT_RELEASE_0 = BSP_EVENT_KEY_LAST + 1, /**< Button 0 released */
    BSP_USER_EVENT_RELEASE_1,                          /**< Button 1 released */
    BSP_USER_EVENT_RELEASE_2,                          /**< Button 2 released */
    BSP_USER_EVENT_RELEASE_3,                          /**< Button 3 released */
    BSP_USER_EVENT_RELEASE_4,                          /**< Button 4 released */
    BSP_USER_EVENT_RELEASE_5,                          /**< Button 5 released */
    BSP_USER_EVENT_RELEASE_6,                          /**< Button 6 released */
    BSP_USER_EVENT_RELEASE_7,                          /**< Button 7 released */
};

//GLOBALS
volatile uint32_t ms_ticks;


//FUNCTIONS
ret_code_t init_bsp(void);
ret_code_t init_softdevice(void);
ret_code_t init_storage(void);
void init_cli(void);
ret_code_t init_device(void);
//static void bsp_event_callback(bsp_event_t ev);
bool is_user_button_pressed(void);
void SysTick_Handler(void);
void board_wink(void);
void board_reboot(void);
void power_manage(void);
void init_app_timer(void);


// Return 2 for disabled, 1 for user is present, 0 user not present, -1 if cancel is requested.
/** Test for user presence.
 *  Perform test that user is present.  Returns status on user presence.  This is used by FIDO and U2F layer
 *  to check if an operation should continue, or if the UP flag should be set.
 * 
 * @param delay number of milliseconds to delay waiting for user before timeout.
 * 
 * @return 2 - User presence is disabled.  Operation should continue, but UP flag not set.
 *         1 - User presence confirmed.  Operation should continue, and UP flag is set.
 *         0 - User presence is not confirmed.  Operation should be denied.
 *        -1 - Operation was canceled.  Do not continue, reset transaction state.
 * 
 * *Optional*, the default implementation will return 1, unless a FIDO2 operation calls for no UP, where this will then return 2.
*/
int ctap_user_presence_test(uint64_t delay);

/** Disable the next user presence test.  This is called by FIDO2 layer when a transaction
 *  requests UP to be disabled.  The next call to ctap_user_presence_test should return 2,
 *  and then UP should be enabled again.
 * 
 * @param request_active indicate to activate (true) or disable (false) UP.
 * 
 * *Optional*, the default implementation will provide expected behaviour with the default ctap_user_presence_test(...).
*/
void device_disable_up(bool disable);


#ifdef __cplusplus
}
#endif 

#endif // _HAL_H
