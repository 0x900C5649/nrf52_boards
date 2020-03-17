// Hardware abstraction layer

#ifndef _HAL_H
#define _HAL_H

#ifdef __cplusplus
extern "C" {
#endif

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
void init_bsp(void);
void init_cli(void);
//static void bsp_event_callback(bsp_event_t ev);
bool is_user_button_pressed(void);
void SysTick_Handler(void);
void board_wink(void);

#ifdef __cplusplus
}
#endif 

#endif // _HAL_H
