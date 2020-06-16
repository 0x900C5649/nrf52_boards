#ifndef _CONNECTIVITY_BLE_H
#define _CONNECTIVITY_BLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ctap.h"
#include "util.h"
#include "sdk_config.h"

#include "ble.h"
#include "nrf_ble_gq.h"


/***************************************************************************** 
*                            BLE SETTINGS
*****************************************************************************/

#define APP_BLE_OBSERVER_PRIO           3                                       /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_BLE_CONN_CFG_TAG            1                                       /**< A tag identifying the SoftDevice BLE configuration. */

#define APP_ADV_INTERVAL                64                                      /**< The advertising interval (in units of 0.625 ms; this value corresponds to 40 ms). */
#define APP_ADV_DURATION                BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED   /**< The advertising time-out (in units of seconds). When set to 0, we will never time out. */

#define BLE_DEVICE_APPEARANCE           BLE_APPEARANCE_GENERIC_TAG

#define MIN_CONN_INTERVAL              MSEC_TO_UNITS(200, UNIT_1_25_MS)       /**< Minimum acceptable connection interval (0.5 seconds). */
#define MAX_CONN_INTERVAL              MSEC_TO_UNITS(400, UNIT_1_25_MS)      /**< Maximum acceptable connection interval (1 second). */
#define SLAVE_LATENCY                  0                                      /**< Slave latency. */
#define CONN_SUP_TIMEOUT               MSEC_TO_UNITS(5000, UNIT_10_MS)        /**< Connection supervisory time-out (4 seconds). */


#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(20000)                  /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (15 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(5000)                   /**< Time between each call to sd_ble_gap_conn_param_update after the first call (5 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                       /**< Number of attempts before giving up the connection parameter negotiation. */


#define SEC_PARAM_BOND                  1                                       /**< Perform bonding. */
#define SEC_PARAM_MITM                  0                                       /**< Man In The Middle protection */
#define SEC_PARAM_LESC                  0                                       /**< LE Secure Connections */
#define SEC_PARAM_KEYPRESS              0                                       /**< Keypress notifications */
#define SEC_PARAM_IO_CAPABILITIES       BLE_GAP_IO_CAPS_NONE                    /**< I/O capabilities. */
#define SEC_PARAM_OOB                   0                                       /**< Out Of Band data */
#define SEC_PARAM_MIN_KEY_SIZE          7                                       /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE          16                                      /**< Maximum encryption key size. */


/***************************************************************************** 
*                            PUBLIC INTERFACE
*****************************************************************************/

void ble_init    (void);

void ble_process (void);

#ifdef __cplusplus
} //extern c
#endif

#endif //define _connectivity_ble_h
