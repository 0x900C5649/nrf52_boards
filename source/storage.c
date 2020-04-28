#include "storage.h"
#include "hal.h"

#include "fds.h"

/*  LOG INIT  */
#define NRF_LOG_MODULE_NAME storage

#include "nrf_log.h"
#include "nrf_log_ctrl.h"

NRF_LOG_MODULE_REGISTER();
/*            */

/* Keep track of the progress of a delete_all operation. */
static struct
{
    bool delete_next;   //!< Delete next record.
    bool pending;       //!< Waiting for an fds FDS_EVT_DEL_RECORD event, to delete the next record.
} volatile m_delete_all;

/* Flag to check fds initialization. */
static bool volatile m_fds_initialized = false;
static bool volatile m_fds_ongoing_op = false;

const char *fds_err_str(ret_code_t ret)
{
    /* Array to map FDS return values to strings. */
    static char const * err_str[] =
    {
        "FDS_ERR_OPERATION_TIMEOUT",
        "FDS_ERR_NOT_INITIALIZED",
        "FDS_ERR_UNALIGNED_ADDR",
        "FDS_ERR_INVALID_ARG",
        "FDS_ERR_NULL_ARG",
        "FDS_ERR_NO_OPEN_RECORDS",
        "FDS_ERR_NO_SPACE_IN_FLASH",
        "FDS_ERR_NO_SPACE_IN_QUEUES",
        "FDS_ERR_RECORD_TOO_LARGE",
        "FDS_ERR_NOT_FOUND",
        "FDS_ERR_NO_PAGES",
        "FDS_ERR_USER_LIMIT_REACHED",
        "FDS_ERR_CRC_CHECK_FAILED",
        "FDS_ERR_BUSY",
        "FDS_ERR_INTERNAL",
    };

    return err_str[ret - NRF_ERROR_FDS_ERR_BASE];
}

static char const * fds_evt_str[] =
{
    "FDS_EVT_INIT",
    "FDS_EVT_WRITE",
    "FDS_EVT_UPDATE",
    "FDS_EVT_DEL_RECORD",
    "FDS_EVT_DEL_FILE",
    "FDS_EVT_GC",
};

static void fds_evt_handler(fds_evt_t const * p_evt)
{
    if (p_evt->result == NRF_SUCCESS)
    {
        NRF_LOG_INFO("Event: %s (0x%X) received (NRF_SUCCESS)",
                      fds_evt_str[p_evt->id], p_evt->id);
    }
    else
    {
        NRF_LOG_INFO("Event: %s received (%s)",
                      fds_evt_str[p_evt->id],
                      fds_err_str(p_evt->result));
    }
    
    switch (p_evt->id)
    {
        case FDS_EVT_INIT:
            if (p_evt->result == NRF_SUCCESS)
            {
                m_fds_initialized = true;
            }
            break;

        case FDS_EVT_WRITE:
        {
            NRF_LOG_DEBUG("here");
//            NRF_LOG_DEBUG("evt:write");
            if (p_evt->result == NRF_SUCCESS)
            {
//                NRF_LOG_INFO("Record ID:\t0x%04x",  p_evt->write.record_id);
//                NRF_LOG_INFO("File ID:\t0x%04x",    p_evt->write.file_id);
//                NRF_LOG_INFO("Record key:\t0x%04x", p_evt->write.record_key);
                m_fds_ongoing_op = false;
            }
        } break;

        case FDS_EVT_DEL_RECORD:
        {
            if (p_evt->result == NRF_SUCCESS)
            {
//                NRF_LOG_INFO("Record ID:\t0x%04x",  p_evt->del.record_id);
//                NRF_LOG_INFO("File ID:\t0x%04x",    p_evt->del.file_id);
//                NRF_LOG_INFO("Record key:\t0x%04x", p_evt->del.record_key);
            m_fds_ongoing_op = false;
            m_delete_all.pending = false;
            }
        } break;
        case FDS_EVT_DEL_FILE:
        case FDS_EVT_GC:
        case FDS_EVT_UPDATE:
        {
            if (p_evt->result == NRF_SUCCESS)
            {
                m_fds_ongoing_op = false;
            }
        } break;
        default:
            break;
    }

}

static void wait_for_fds_ready(void)
{
    while (!m_fds_initialized)
    {
        power_manage();
    }
}

static void wait_for_fds_op(void)
{
    while(m_fds_ongoing_op)
    {
        power_manage();
    }
}


ret_code_t init_storage(void)
{
    ret_code_t rc;
    NRF_LOG_DEBUG("fds_register");
    NRF_LOG_FLUSH();
    rc = fds_register(fds_evt_handler);
    APP_ERROR_CHECK(rc);
    NRF_LOG_DEBUG("fds_init");
    NRF_LOG_FLUSH();
    rc = fds_init();
    NRF_LOG_DEBUG("fds_init returned: %d", rc);
    NRF_LOG_FLUSH();
    APP_ERROR_CHECK(rc);

    /* Wait for fds to initialize. */
    wait_for_fds_ready();

    return rc;
}

void resetStorage()
{
    reset_ctap_state();
    counterReset();
    reset_ctap_rk();
}


/***************************************************************************** 
*							CTAP STATE
*****************************************************************************/

void write_ctap_state(AuthenticatorState * s)
{
    ret_code_t rc;
    fds_record_t        record;
    fds_record_desc_t   record_desc;
    fds_find_token_t    ftok;
    
    /* It is required to zero the token before first use. */
    memset(&ftok, 0x00, sizeof(fds_find_token_t));
        
    record.file_id = STATE_FILE;
    record.key     = STATE_KEY;
    record.data.p_data = s;
    record.data.length_words = BYTES_TO_WORDS(sizeof(AuthenticatorState));
//    record.data.length_words =BYTES_TO_WORDS (sizeof(AuthenticatorState) + ((4 - (sizeof(AuthenticatorState) & 3)) & 3)) >> 2 ;

    rc = fds_record_find(STATE_FILE, STATE_KEY, &record_desc, &ftok);
    if(rc == NRF_SUCCESS)
    { // found entry -> overwrite
        rc = fds_record_update(&record_desc, &record);
        APP_ERROR_CHECK(rc);

        rc = fds_gc();
        APP_ERROR_CHECK(rc);
    }
    else if (rc == FDS_ERR_NOT_FOUND)
    { // entry not found -> init entry
        rc = fds_record_write(NULL, &record);
        APP_ERROR_CHECK(rc);
    }
    else
    { // other: unknown error
        APP_ERROR_CHECK(rc);
    }
}

uint8_t read_ctap_state(AuthenticatorState * s)
{
    ret_code_t rc;
    fds_record_desc_t   record_desc;
    fds_find_token_t    ftok;
    fds_flash_record_t  flash_record;
    
    /* It is required to zero the token before first use. */
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    
    rc = fds_record_find(STATE_FILE, STATE_KEY, &record_desc, &ftok);
    if(rc == NRF_SUCCESS)
    { // found entry -> open, read, copy to mem
        NRF_LOG_INFO("found state");
        //open record 
        rc = fds_record_open(&record_desc, &flash_record);
        APP_ERROR_CHECK(rc);

        //copy state to memory
        memcpy(s, flash_record.p_data, sizeof(AuthenticatorState));

        //close record
        rc = fds_record_close(&record_desc);
        APP_ERROR_CHECK(rc);
        
        return 1;
    }
    else if (rc == FDS_ERR_NOT_FOUND)
    {
        NRF_LOG_WARNING("state record not found");
    }
    else
    { // other: unknown error
        APP_ERROR_CHECK(rc);
    }
    return 0;
}

void reset_ctap_state()
{
    wait_for_fds_op();
    m_fds_ongoing_op = true;
    ret_code_t rc;
    rc = fds_file_delete(STATE_FILE);
    APP_ERROR_CHECK(rc);

    wait_for_fds_op();
    
    m_fds_ongoing_op = true;
    rc = fds_gc();
    APP_ERROR_CHECK(rc);

    wait_for_fds_op();
}

/***************************************************************************** 
*							RKs
*****************************************************************************/

void reset_ctap_rk()
{
    wait_for_fds_op();
    m_fds_ongoing_op = true;
    ret_code_t rc;
    rc = fds_file_delete(RK_FILE);
    APP_ERROR_CHECK(rc);

    wait_for_fds_op();
    
    m_fds_ongoing_op = true;
    rc = fds_gc();
    APP_ERROR_CHECK(rc);

    wait_for_fds_op();
}

uint8_t ctap_rk_size()
{
    return MAX_KEYS;
}

void store_ctap_rk(uint16_t index,CTAP_residentKey * rk)
{
    wait_for_fds_op();
    m_fds_ongoing_op = true;

    ret_code_t rc;
    fds_record_t        record;
    fds_record_desc_t   record_desc;

    record.file_id = RK_FILE;
    record.key     = index + 1;
    record.data.p_data = rk;
    record.data.length_words = BYTES_TO_WORDS(sizeof(CTAP_residentKey));
//    record.data.length_words = (sizeof(CTAP_residentKey) + ((4 - (sizeof(CTAP_residentKey) & 3)) & 3)) >> 2 ;
    NRF_LOG_DEBUG("CTAP_residentKey has size %d", sizeof(CTAP_residentKey));
    rc = fds_record_write(&record_desc, &record);
//    NRF_LOG_DEBUG("fds_record_write returned: %X, args where: %X %X %d", rc, RK_FILE, index, record.data.length_words);
    wait_for_fds_op();
    APP_ERROR_CHECK(rc);
    NRF_LOG_DEBUG("zzwrite done");
}

void load_ctap_rk(uint16_t index,CTAP_residentKey * rk)
{
    wait_for_fds_op();
    
    ret_code_t rc;
    fds_flash_record_t  flash_record;
    fds_record_desc_t   record_desc;
    fds_find_token_t    ftok;
    
    /* It is required to zero the token before first use. */
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    rc = fds_record_find(RK_FILE, index+1, &record_desc, &ftok);
    NRF_LOG_DEBUG("fds_record find: %x", rc);
    APP_ERROR_CHECK(rc);
    
    {
        //open record 
        rc = fds_record_open(&record_desc, &flash_record);
        NRF_LOG_DEBUG("fds_record open: %x", rc);
        APP_ERROR_CHECK(rc);

        //copy key to memory
        memcpy(rk, flash_record.p_data, sizeof(CTAP_residentKey));

        //close record
        rc = fds_record_close(&record_desc);
        NRF_LOG_DEBUG("fds_record close: %x", rc);
        APP_ERROR_CHECK(rc);
    }

}

void overwrite_ctap_rk(uint16_t index,CTAP_residentKey * rk)
{
    wait_for_fds_op();
    m_fds_ongoing_op = true;

    ret_code_t rc;
    fds_record_t        record;
    fds_record_desc_t   record_desc;
    fds_find_token_t    ftok;
    
    /* It is required to zero the token before first use. */
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    rc = fds_record_find(RK_FILE, index+1, &record_desc, &ftok);
    APP_ERROR_CHECK(rc);

    record.file_id = RK_FILE;
    record.key     = index+1;
    record.data.p_data = rk;
    record.data.length_words = BYTES_TO_WORDS(sizeof(CTAP_residentKey));
//    record.data.length_words = (sizeof(CTAP_residentKey) + ((4 - (sizeof(CTAP_residentKey) & 3)) & 3)) >> 2 ;

    rc = fds_record_update(&record_desc, &record);
    APP_ERROR_CHECK(rc);
    
    wait_for_fds_op();
}


/***************************************************************************** 
*							PERSISTENT COUNTER
*****************************************************************************/

uint32_t counterGet(uint16_t index)
{
    wait_for_fds_op();
    
    uint32_t ret=0;
    ret_code_t rc;
    fds_flash_record_t  flash_record;
    fds_record_desc_t   record_desc;
    fds_find_token_t    ftok;
    
    /* It is required to zero the token before first use. */
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    rc = fds_record_find(COUNTER_FILE, index+1, &record_desc, &ftok);
    NRF_LOG_DEBUG("fds_record find: %x", rc);
    APP_ERROR_CHECK(rc);
    
    {
        //open record 
        rc = fds_record_open(&record_desc, &flash_record);
        NRF_LOG_DEBUG("fds_record open: %x", rc);
        APP_ERROR_CHECK(rc);

        //copy key to memory
        memcpy(&ret, flash_record.p_data, 1);

        //close record
        rc = fds_record_close(&record_desc);
        NRF_LOG_DEBUG("fds_record close: %x", rc);
        APP_ERROR_CHECK(rc);
    }
    return ret;
}

uint32_t counterIncrement(uint16_t index, uint32_t amount)
{
    wait_for_fds_op();
    m_fds_ongoing_op = true;

    uint32_t curvalue;
    uint32_t newvalue;

    ret_code_t rc;
    fds_record_t        record;
    fds_flash_record_t  flash_record;
    fds_record_desc_t   record_desc;
    fds_find_token_t    ftok;
    
    /* It is required to zero the token before first use. */
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    rc = fds_record_find(COUNTER_FILE, index+1, &record_desc, &ftok);
    NRF_LOG_DEBUG("fds_record find: %x, %x, %x, %x", rc, FDS_ERR_NOT_INITIALIZED, FDS_ERR_NULL_ARG, FDS_ERR_NOT_FOUND);
    APP_ERROR_CHECK(rc);
    
    {
        //open record 
        rc = fds_record_open(&record_desc, &flash_record);
        NRF_LOG_DEBUG("fds_record open: %x", rc);
        APP_ERROR_CHECK(rc);

        //copy key to memory
        memcpy(&curvalue, flash_record.p_data, 1);

        //close record
        rc = fds_record_close(&record_desc);
        NRF_LOG_DEBUG("fds_record close: %x", rc);
        APP_ERROR_CHECK(rc);
    }
    
    newvalue = curvalue + amount;

    record.file_id = COUNTER_FILE;
    record.key     = index+1;
    record.data.p_data = &newvalue;
    record.data.length_words = 1;

    rc = fds_record_update(&record_desc, &record);
    APP_ERROR_CHECK(rc);

    wait_for_fds_op(); 

    return newvalue;
}

void counterReset(void)
{
    wait_for_fds_op();
    m_fds_ongoing_op = true;
    ret_code_t rc;
    rc = fds_file_delete(COUNTER_FILE);
    APP_ERROR_CHECK(rc);

    wait_for_fds_op();
    
    m_fds_ongoing_op = true;
    rc = fds_gc();
    APP_ERROR_CHECK(rc);

    wait_for_fds_op();
}

uint32_t counterInit(uint16_t index)
{
    wait_for_fds_op();
    m_fds_ongoing_op = true;
    
    NRF_LOG_DEBUG("initing counter %d", index);
    uint32_t initvalue = 0;

    ret_code_t rc;
    fds_record_t        record;
    fds_record_desc_t   record_desc;

    record.file_id = COUNTER_FILE;
    record.key     = index + 1;
    record.data.p_data = &initvalue;
    record.data.length_words = 1;
    rc = fds_record_write(&record_desc, &record);
    
    wait_for_fds_op();
    APP_ERROR_CHECK(rc);
    return initvalue;
}

