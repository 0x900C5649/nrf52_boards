#ifndef _STORAGE_H
#define _STORAGE_H

#include "sdk_errors.h"
#include "ctap.h"

#define KEY_SPACE_BYTES     128
#define MAX_KEYS            (8)
#define PIN_SALT_LEN        (32)
#define STATE_VERSION        (1)


#define BACKUP_MARKER       0x5A
#define INITIALIZED_MARKER  0xA5

#define ERR_NO_KEY_SPACE    (-1)
#define ERR_KEY_SPACE_TAKEN (-2)
#define ERR_KEY_SPACE_EMPTY (-2)

#define STATE_FILE      0x0001  /* The ID of the file to write the records into. */
#define STATE_KEY       0x5AA5

#define RK_FILE         0x0002

typedef struct
{
  // Pin information
  uint8_t is_initialized;
  uint8_t is_pin_set;
  uint8_t pin_code[NEW_PIN_ENC_MIN_SIZE];
  int pin_code_length;
  int8_t remaining_tries;

  uint16_t rk_stored;

  uint16_t key_lens[MAX_KEYS];
  uint8_t key_space[KEY_SPACE_BYTES];
} AuthenticatorState_0xFF;


typedef struct
{
    // Pin information
    uint8_t is_initialized;
    uint8_t is_pin_set;
    uint8_t PIN_CODE_HASH[32];
    uint8_t PIN_SALT[PIN_SALT_LEN];
    int _reserved;
    int8_t remaining_tries;

    uint8_t rk_stored;

    uint16_t key_lens[MAX_KEYS];
    uint8_t key_space[KEY_SPACE_BYTES];
    uint8_t data_version;
} AuthenticatorState_0x01;

typedef AuthenticatorState_0x01 AuthenticatorState;

ret_code_t init_storage(void);


//write state
void write_ctap_state(AuthenticatorState * s);

//read state
uint8_t read_ctap_state(AuthenticatorState * s);

//reset rks
/** Delete all resident keys.
 * 
 * *Optional*, if not implemented, operates on non-persistant RK's.
*/
void reset_ctap_rk();

//size rks
/** Return the maximum amount of resident keys that can be stored.
 * @return max number of resident keys that can be stored, including already stored RK's.
 * 
 * *Optional*, if not implemented, returns 50.
*/
uint8_t ctap_rk_size();

/** Store a resident key into an index between [ 0, ctap_rk_size() ).
 *  Storage should be in non-volatile memory.
 * 
 * @param index between RK index range.
 * @param rk pointer to valid rk structure that should be written to NV memory.
 * 
 * *Optional*, if not implemented, operates on non-persistant RK's.
*/
void store_ctap_rk(uint16_t index,CTAP_residentKey * rk);//store rks

//load rks
/** Read a resident key from an index into memory
 * @param index to read resident key from.
 * @param rk pointer to resident key structure to write into with RK.
 * 
 * *Optional*, if not implemented, operates on non-persistant RK's.
*/
void load_ctap_rk(uint16_t index,CTAP_residentKey * rk);

//overwrite rk
/** Overwrite the RK located in index with a new RK.
 * @param index to write resident key to.
 * @param rk pointer to valid rk structure that should be written to NV memory, and replace existing RK there.
 * 
 * *Optional*, if not implemented, operates on non-persistant RK's.
*/
void overwrite_ctap_rk(uint16_t index,CTAP_residentKey * rk);

ret_code_t init_storage(void);



#endif // STORAGE_H
