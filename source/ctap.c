// Copyright 2019 SoloKeys Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cbor.h"

/*  LOG INIT  */
#define NRF_LOG_MODULE_NAME ctap

#include "nrf_log.h"
#include "nrf_log_ctrl.h"

NRF_LOG_MODULE_REGISTER();
/*            */

#include "ctap.h"
#include "u2f.h"
//#include "fido_hid.h"
#include "storage.h"
#include "ctap_parse.h"
#include "ctap_errors.h"
#include "cose_key.h"
#include "crypto.h"
#include "util.h"
#include "log.h"
#include "app_config.h"
#include "fido_interfaces.h"
//#include "extensions.h"
//#include "version.h"

#include "attestation.h"
#include "hal.h"


#define COUNTERID   0

/***************************************************************************** 
*							globals
*****************************************************************************/

uint8_t PIN_TOKEN[PIN_TOKEN_SIZE];
uint8_t KEY_AGREEMENT_PUB[ECC_PUBLIC_KEY_SIZE];
static uint8_t KEY_AGREEMENT_PRIV[ECC_PRIVATE_KEY_SIZE];
static int8_t PIN_BOOT_ATTEMPTS_LEFT = PIN_BOOT_ATTEMPTS;

AuthenticatorState STATE;

struct _getAssertionState getAssertionState;


/***************************************************************************** 
*						prototypes
*****************************************************************************/

static void ctap_state_init();
static void ctap_increment_rk_store();
static int is_matching_rk(CTAP_residentKey * rk, CTAP_residentKey * rk2);
static unsigned int get_credential_id_size(CTAP_credentialDescriptor * cred);
static uint8_t ctap_add_credential_descriptor(CborEncoder * map, CTAP_credentialDescriptor * cred);
static void save_credential_list(CTAP_authDataHeader * head, uint8_t * clientDataHash, CTAP_credentialDescriptor * creds, uint32_t count);
static CTAP_credentialDescriptor * pop_credential();
static int cred_cmp_func(const void * _a, const void * _b);
static int ctap_add_cose_key(CborEncoder * cose_key, uint8_t * x, uint8_t * y, uint8_t credtype, int32_t algtype);
static int ctap_generate_cose_key(CborEncoder * cose_key, uint8_t * hmac_input, int len, uint8_t credtype, int32_t algtype);
static uint16_t key_addr_offset(int index);
static void add_existing_user_info(CTAP_credentialDescriptor * cred);
static uint32_t auth_data_update_count(CTAP_authDataHeader * authData);
static int ctap_make_auth_data(struct rpId * rp, CborEncoder * map, uint8_t * auth_data_buf, uint32_t * len, CTAP_credInfo * credInfo);
static int ctap2_user_presence_test();
static int ctap_make_extensions(CTAP_extensions * ext, uint8_t * ext_encoder_buf, unsigned int * ext_encoder_buf_size);
static int trailing_zeros(uint8_t * buf, int indx);
static void ctap_reset_key_agreement();
uint8_t verify_pin_auth(uint8_t * pinAuth, uint8_t * clientDataHash);
int ctap_authenticate_credential(struct rpId * rp, CTAP_credentialDescriptor * desc);
int ctap_calculate_signature(uint8_t * data, int datalen, uint8_t * clientDataHash, uint8_t * hashbuf, uint8_t * sigbuf, uint8_t * sigder);
uint8_t ctap_add_attest_statement(CborEncoder * map, uint8_t * sigder, int len);
int ctap_filter_invalid_credentials(CTAP_getAssertion * GA);
uint8_t ctap_end_get_assertion(CborEncoder * map, CTAP_credentialDescriptor * cred, uint8_t * auth_data_buf, unsigned int auth_data_buf_sz, uint8_t * clientDataHash);
uint8_t ctap_add_user_entity(CborEncoder * map, CTAP_userEntity * user);
void make_auth_tag(uint8_t * rpIdHash, uint8_t * nonce, uint32_t count, uint8_t * tag);


/***************************************************************************** 
*							INIT
*****************************************************************************/

void ctap_init()
{
    NRF_LOG_INFO("INITING ctap");
/*    printf1(TAG_ERR,"Current firmware version address: %p\r\n", &firmware_version);
    printf1(TAG_ERR,"Current firmware version: %d.%d.%d.%d (%02x.%02x.%02x.%02x)\r\n",
            firmware_version.major, firmware_version.minor, firmware_version.patch, firmware_version.reserved,
            firmware_version.major, firmware_version.minor, firmware_version.patch, firmware_version.reserved
            );*/
//    crypto_ecc256_init();

    int is_init = read_ctap_state(&STATE);

    device_set_status(CTAP_STATUS_IDLE);

    if (is_init)
    {
        printf1(TAG_STOR,"Auth state is initialized\n");
    }
    else
    {   
        printf1(TAG_STOR, "initing ctap state");
        ctap_state_init();
        write_ctap_state(&STATE);
        //init counter only if need to init state
        counterInit(COUNTERID);
    }

    //do_migration_if_required(&STATE);

    crypto_load_master_secret(STATE.key_space);

    if (ctap_is_pin_set())
    {
        printf1(TAG_STOR, "attempts_left: %d\n", STATE.remaining_tries);
    }
    else
    {
        printf1(TAG_STOR,"pin not set.\n");
    }
    if (ctap_device_locked())
    {
        printf1(TAG_ERR, "DEVICE LOCKED!\n");
    }

    if (crypto_generate_rng(PIN_TOKEN, PIN_TOKEN_SIZE) != 1)
    {
        printf2(TAG_ERR,"Error, rng failed\n");
        exit(1);
    }

    ctap_reset_key_agreement();

#ifdef BRIDGE_TO_WALLET
    wallet_init();
#endif

}


/***************************************************************************** 
*							CTAP COMMANDS
*****************************************************************************/

uint8_t ctap_make_credential(CborEncoder * encoder, uint8_t * request, int length)
{
    CTAP_makeCredential MC;
    int ret;
    unsigned int i;
    uint8_t auth_data_buf[310];
    CTAP_credentialDescriptor * excl_cred = (CTAP_credentialDescriptor *) auth_data_buf;
    uint8_t * sigbuf = auth_data_buf + 32;
    uint8_t * sigder = auth_data_buf + 32 + 64;

    ret = ctap_parse_make_credential(&MC,encoder,request,length);

    if (ret != 0)
    {
        printf2(TAG_ERR,"error, parse_make_credential failed\n");
        return ret;
    }
    if (MC.pinAuthEmpty)
    {
        check_retr( ctap2_user_presence_test(CTAP2_UP_DELAY_MS) );
        return ctap_is_pin_set() == 1 ? CTAP2_ERR_PIN_AUTH_INVALID : CTAP2_ERR_PIN_NOT_SET;
    }
    if ((MC.paramsParsed & MC_requiredMask) != MC_requiredMask)
    {
        printf2(TAG_ERR,"error, required parameter(s) for makeCredential are missing\n");
        return CTAP2_ERR_MISSING_PARAMETER;
    }

    if (ctap_is_pin_set() == 1 && MC.pinAuthPresent == 0)
    {
        printf2(TAG_ERR,"pinAuth is required\n");
        return CTAP2_ERR_PIN_REQUIRED;
    }
    else
    {
        if (ctap_is_pin_set() || (MC.pinAuthPresent))
        {
            ret = verify_pin_auth(MC.pinAuth, MC.clientDataHash);
            check_retr(ret);
        }
    }

    if (MC.up == 1 || MC.up == 0) ///user presence?
    {
        printf1(TAG_GREEN, "up problem");
        return CTAP2_ERR_INVALID_OPTION;
    }

    // crypto_aes256_init(CRYPTO_TRANSPORT_KEY, NULL);

    /// handle exclude list
    for (i = 0; i < MC.excludeListSize; i++)
    {
        ret = parse_credential_descriptor(&MC.excludeList, excl_cred);
        if (ret == CTAP2_ERR_CBOR_UNEXPECTED_TYPE)
        {
            continue;
        }
        check_retr(ret);

        printf1(TAG_GREEN, "checking credId: "); dump_hex1(TAG_GREEN, (uint8_t*) &excl_cred->credential.id, sizeof(CredentialId));

        if (ctap_authenticate_credential(&MC.rp, excl_cred)) ///if credential authenticates -> key "exists" on this token
        {
            printf1(TAG_MC, "Cred %d failed!\r\n",i);
            return CTAP2_ERR_CREDENTIAL_EXCLUDED;
        }

        ret = cbor_value_advance(&MC.excludeList);
        check_ret(ret);
    }

    /// create new key

    CborEncoder map;
    ret = cbor_encoder_create_map(encoder, &map, 3);
    check_ret(ret);

    {
        ret = cbor_encode_int(&map,RESP_fmt);
        check_ret(ret);
        ret = cbor_encode_text_stringz(&map, "packed");
        check_ret(ret);
    }

    uint32_t auth_data_sz = sizeof(auth_data_buf);

    ret = ctap_make_auth_data(&MC.rp, &map, auth_data_buf, &auth_data_sz,
            &MC.credInfo);
    check_retr(ret);

    {
        unsigned int ext_encoder_buf_size = sizeof(auth_data_buf) - auth_data_sz;
        uint8_t * ext_encoder_buf = auth_data_buf + auth_data_sz;

        ret = ctap_make_extensions(&MC.extensions, ext_encoder_buf, &ext_encoder_buf_size);
        check_retr(ret);
        if (ext_encoder_buf_size)
        {
            ((CTAP_authData *)auth_data_buf)->head.flags |= (1 << 7);
            auth_data_sz += ext_encoder_buf_size;
        }
    }

    {
        ret = cbor_encode_int(&map,RESP_authData);
        check_ret(ret);
        ret = cbor_encode_byte_string(&map, auth_data_buf, auth_data_sz);
        check_ret(ret);
    }


    /// do attastation
    crypto_ecc256_load_attestation_key();
    int sigder_sz = ctap_calculate_signature(auth_data_buf, auth_data_sz, MC.clientDataHash, auth_data_buf, sigbuf, sigder);
    printf1(TAG_MC,"der sig [%d]: ", sigder_sz); dump_hex1(TAG_MC, sigder, sigder_sz);

    ret = ctap_add_attest_statement(&map, sigder, sigder_sz);
    check_retr(ret);

    ///return
    ret = cbor_encoder_close_container(encoder, &map);
    check_ret(ret);
    return CTAP1_ERR_SUCCESS;
}

uint8_t ctap_get_assertion(CborEncoder * encoder, uint8_t * request, int length)
{
    CTAP_getAssertion GA;

    uint8_t auth_data_buf[sizeof(CTAP_authDataHeader) + 80];
    int ret = ctap_parse_get_assertion(&GA,request,length);

    if (ret != 0)
    {
        printf2(TAG_ERR,"error, parse_get_assertion failed\n");
        return ret;
    }

    if (GA.pinAuthEmpty)
    {
        check_retr( ctap2_user_presence_test(CTAP2_UP_DELAY_MS) );
        return ctap_is_pin_set() == 1 ? CTAP2_ERR_PIN_AUTH_INVALID : CTAP2_ERR_PIN_NOT_SET;
    }
    if (GA.pinAuthPresent)
    {
        ret = verify_pin_auth(GA.pinAuth, GA.clientDataHash);
        check_retr(ret);
        getAssertionState.user_verified = 1;
    }
    else
    {
        getAssertionState.user_verified = 0;
    }

    if (!GA.rp.size || !GA.clientDataHashPresent)
    {
        return CTAP2_ERR_MISSING_PARAMETER;
    }
    CborEncoder map;

    int map_size = 3;

    printf1(TAG_GA, "ALLOW_LIST has %d creds\n", GA.credLen);
    int validCredCount = ctap_filter_invalid_credentials(&GA);

    if (validCredCount == 0)
    {
        printf2(TAG_ERR,"Error, no authentic credential\n");
        return CTAP2_ERR_NO_CREDENTIALS;
    }
    else if (validCredCount > 1)
    {
       map_size += 1;
    }


    if (GA.creds[validCredCount - 1].credential.user.id_size)
    {
        map_size += 1;
    }
    if (GA.extensions.hmac_secret_present == EXT_HMAC_SECRET_PARSED)
    {
        printf1(TAG_GA, "hmac-secret is present\r\n");
    }

    ret = cbor_encoder_create_map(encoder, &map, map_size);
    check_ret(ret);

    // if only one account for this RP, null out the user details
    if (validCredCount < 2 || !getAssertionState.user_verified)
    {
        printf1(TAG_GREEN, "Only one account, nulling out user details on response\r\n");
        memset(&GA.creds[0].credential.user.name, 0, USER_NAME_LIMIT);
    }

    printf1(TAG_GA,"resulting order of creds:\n");
    int j;
    for (j = 0; j < GA.credLen; j++)
    {
        printf1(TAG_GA,"CRED ID (# %d)\n", GA.creds[j].credential.id.count);
    }

    CTAP_credentialDescriptor * cred = &GA.creds[validCredCount - 1];

    GA.extensions.hmac_secret.credential = &cred->credential;

    uint32_t auth_data_buf_sz = sizeof(auth_data_buf);

#ifdef ENABLE_U2F_EXTENSIONS
    if ( is_extension_request((uint8_t*)&GA.creds[validCredCount - 1].credential.id, sizeof(CredentialId)) )
    {
        auth_data_buf_sz = sizeof(CTAP_authDataHeader);

        crypto_sha256_init();
        crypto_sha256_update(GA.rp.id, GA.rp.size);
        crypto_sha256_final(((CTAP_authDataHeader *)auth_data_buf)->rpIdHash);

        ((CTAP_authDataHeader *)auth_data_buf)->flags = (1 << 0);
        ((CTAP_authDataHeader *)auth_data_buf)->flags |= (1 << 2);
    }
    else
#endif
    {
        device_disable_up(GA.up == 0);
        ret = ctap_make_auth_data(&GA.rp, &map, auth_data_buf, &auth_data_buf_sz, NULL);
        device_disable_up(false);
        check_retr(ret);

        ((CTAP_authDataHeader *)auth_data_buf)->flags &= ~(1 << 2);
        ((CTAP_authDataHeader *)auth_data_buf)->flags |= (getAssertionState.user_verified << 2);

        {
            unsigned int ext_encoder_buf_size = sizeof(auth_data_buf) - auth_data_buf_sz;
            uint8_t * ext_encoder_buf = auth_data_buf + auth_data_buf_sz;

            ret = ctap_make_extensions(&GA.extensions, ext_encoder_buf, &ext_encoder_buf_size);
            check_retr(ret);
            if (ext_encoder_buf_size)
            {
                ((CTAP_authDataHeader *)auth_data_buf)->flags |= (1 << 7);
                auth_data_buf_sz += ext_encoder_buf_size;
            }
        }

    }

    save_credential_list((CTAP_authDataHeader*)auth_data_buf, GA.clientDataHash, GA.creds, validCredCount-1);   // skip last one

    ret = ctap_end_get_assertion(&map, cred, auth_data_buf, auth_data_buf_sz, GA.clientDataHash);  // 1,2,3,4
    check_retr(ret);

    if (validCredCount > 1)
    {
        ret = cbor_encode_int(&map, RESP_numberOfCredentials);  // 5
        check_ret(ret);
        ret = cbor_encode_int(&map, validCredCount);
        check_ret(ret);
    }

    ret = cbor_encoder_close_container(encoder, &map);
    check_ret(ret);

    return 0;
}

// adds 2 to map, or 3 if add_user is true
uint8_t ctap_end_get_assertion(CborEncoder * map, CTAP_credentialDescriptor * cred, uint8_t * auth_data_buf, unsigned int auth_data_buf_sz, uint8_t * clientDataHash)
{
    int ret;
    uint8_t sigbuf[64];
    uint8_t sigder[72];
    int sigder_sz;

    ret = ctap_add_credential_descriptor(map, cred);  // 1
    check_retr(ret);

    {
        ret = cbor_encode_int(map,RESP_authData);  // 2
        check_ret(ret);
        ret = cbor_encode_byte_string(map, auth_data_buf, auth_data_buf_sz);
        check_ret(ret);
    }

    unsigned int cred_size = get_credential_id_size(cred);
    crypto_ecc256_load_key((uint8_t*)&cred->credential.id, cred_size, NULL, 0);

#ifdef ENABLE_U2F_EXTENSIONS
    if ( extend_fido2(&cred->credential.id, sigder) )
    {
        sigder_sz = 72;
    }
    else
#endif
    {
        sigder_sz = ctap_calculate_signature(auth_data_buf, auth_data_buf_sz, clientDataHash, auth_data_buf, sigbuf, sigder);
    }

    {
        ret = cbor_encode_int(map, RESP_signature);  // 3
        check_ret(ret);
        ret = cbor_encode_byte_string(map, sigder, sigder_sz);
        check_ret(ret);
    }

    if (cred->credential.user.id_size)
    {
        printf1(TAG_GREEN, "adding user details to output\r\n");
        ret = ctap_add_user_entity(map, &cred->credential.user);  // 4
        check_retr(ret);
    }


    return 0;
}

uint8_t ctap_get_next_assertion(CborEncoder * encoder)
{
    int ret;
    CborEncoder map;
    CTAP_authDataHeader authData;
    memmove(&authData, &getAssertionState.authData, sizeof(CTAP_authDataHeader));

    CTAP_credentialDescriptor * cred = pop_credential();

    if (cred == NULL)
    {
        return CTAP2_ERR_NOT_ALLOWED;
    }

    auth_data_update_count(&authData);

    if (cred->credential.user.id_size)
    {
        printf1(TAG_GREEN, "adding user info to assertion response\r\n");
        ret = cbor_encoder_create_map(encoder, &map, 4);
    }
    else
    {
        printf1(TAG_GREEN, "NOT adding user info to assertion response\r\n");
        ret = cbor_encoder_create_map(encoder, &map, 3);
    }

    check_ret(ret);

    // if only one account for this RP, null out the user details
    if (!getAssertionState.user_verified)
    {
        printf1(TAG_GREEN, "Not verified, nulling out user details on response\r\n");
        memset(cred->credential.user.name, 0, USER_NAME_LIMIT);
    }

    ret = ctap_end_get_assertion(&map, cred, (uint8_t *)&authData, sizeof(CTAP_authDataHeader), getAssertionState.clientDataHash);
    check_retr(ret);

    ret = cbor_encoder_close_container(encoder, &map);
    check_ret(ret);

    return 0;
}

uint8_t ctap_get_info(CborEncoder * encoder)
{
    int ret;
    CborEncoder array;
    CborEncoder map;
    CborEncoder options;
    CborEncoder pins;
    uint8_t aaguid[16];
    get_aaguid(aaguid);

    ret = cbor_encoder_create_map(encoder, &map, 6);
    check_ret(ret);
    {

        ret = cbor_encode_uint(&map, RESP_versions);     //  versions key
        check_ret(ret);
        {
            ret = cbor_encoder_create_array(&map, &array, 1); // length 1
            check_ret(ret);
            {
// UNSUPPORTED
//                ret = cbor_encode_text_stringz(&array, "U2F_V2");
//                check_ret(ret);
                ret = cbor_encode_text_stringz(&array, "FIDO_2_0");
                NRF_LOG_DEBUG("need additional %d bytes", cbor_encoder_get_extra_bytes_needed(encoder));
                check_ret(ret);
            }
            ret = cbor_encoder_close_container(&map, &array);
            check_ret(ret);
        }

        ret = cbor_encode_uint(&map, RESP_extensions);
        check_ret(ret);
        {
            ret = cbor_encoder_create_array(&map, &array, 1);
            check_ret(ret);
            {
                ret = cbor_encode_text_stringz(&array, "hmac-secret");
                check_ret(ret);
            }
            ret = cbor_encoder_close_container(&map, &array);
            check_ret(ret);
        }

        ret = cbor_encode_uint(&map, RESP_aaguid);
        check_ret(ret);
        {
            ret = cbor_encode_byte_string(&map, aaguid, 16);
            check_ret(ret);
        }

        ret = cbor_encode_uint(&map, RESP_options);
        check_ret(ret);
        {
            ret = cbor_encoder_create_map(&map, &options,4);
            check_ret(ret);
            {
                ret = cbor_encode_text_string(&options, "rk", 2);
                check_ret(ret);
                {
                    ret = cbor_encode_boolean(&options, 1);     // Capable of storing keys locally
                    check_ret(ret);
                }

                ret = cbor_encode_text_string(&options, "up", 2);
                check_ret(ret);
                {
                    ret = cbor_encode_boolean(&options, 1);     // Capable of testing user presence
                    check_ret(ret);
                }

                // NOT [yet] capable of verifying user
                // Do not add option if UV isn't supported.
                //
                // ret = cbor_encode_text_string(&options, "uv", 2);
                // check_ret(ret);
                // {
                //     ret = cbor_encode_boolean(&options, 0);
                //     check_ret(ret);
                // }

                ret = cbor_encode_text_string(&options, "plat", 4);
                check_ret(ret);
                {
                    ret = cbor_encode_boolean(&options, 0);     // Not attached to platform
                    check_ret(ret);
                }


                ret = cbor_encode_text_string(&options, "clientPin", 9);
                check_ret(ret);
                {
                    ret = cbor_encode_boolean(&options, ctap_is_pin_set());
                    check_ret(ret);
                }


            }
            ret = cbor_encoder_close_container(&map, &options);
            check_ret(ret);
        }

        ret = cbor_encode_uint(&map, RESP_maxMsgSize);
        check_ret(ret);
        {
            ret = cbor_encode_int(&map, CTAP_MAX_MESSAGE_SIZE);
            check_ret(ret);
        }

        ret = cbor_encode_uint(&map, RESP_pinProtocols);
        check_ret(ret);
        {
            ret = cbor_encoder_create_array(&map, &pins, 1);
            check_ret(ret);
            {
                ret = cbor_encode_int(&pins, 1);
                check_ret(ret);
            }
            ret = cbor_encoder_close_container(&map, &pins);
            check_ret(ret);
        }


    }
    ret = cbor_encoder_close_container(encoder, &map);
    check_ret(ret);

    return CTAP1_ERR_SUCCESS;
}

uint8_t ctap_client_pin(CborEncoder * encoder, uint8_t * request, int length)
{
    CTAP_clientPin CP;
    CborEncoder map;
    uint8_t pinTokenEnc[PIN_TOKEN_SIZE];
    int ret = ctap_parse_client_pin(&CP,request,length);


    switch(CP.subCommand)
    {
        case CP_cmdSetPin:
        case CP_cmdChangePin:
        case CP_cmdGetPinToken:
            if (ctap_device_locked())
            {
                return  CTAP2_ERR_PIN_BLOCKED;
            }
            if (ctap_device_boot_locked())
            {
                return CTAP2_ERR_PIN_AUTH_BLOCKED;
            }
    }

    if (ret != 0)
    {
        printf2(TAG_ERR,"error, parse_client_pin failed\n");
        return ret;
    }

    if (CP.pinProtocol != 1 || CP.subCommand == 0)
    {
        return CTAP1_ERR_OTHER;
    }

    int num_map = (CP.getRetries ? 1 : 0);

    switch(CP.subCommand)
    {
        case CP_cmdGetRetries:
            printf1(TAG_CP,"CP_cmdGetRetries\n");
            ret = cbor_encoder_create_map(encoder, &map, 1);
            check_ret(ret);

            CP.getRetries = 1;

            break;
        case CP_cmdGetKeyAgreement:
            printf1(TAG_CP,"CP_cmdGetKeyAgreement\n");
            num_map++;
            ret = cbor_encoder_create_map(encoder, &map, num_map);
            check_ret(ret);

            ret = cbor_encode_int(&map, RESP_keyAgreement);
            check_ret(ret);

            #if APP_MODULE_ENABLED(NFC)
                if (device_is_nfc() == NFC_IS_ACTIVE) device_set_clock_rate(DEVICE_LOW_POWER_FAST);
            #endif
    
            size_t pub_output_len = ECC_PUBLIC_KEY_SIZE;
            crypto_ecc256_compute_public_key(KEY_AGREEMENT_PRIV, ECC_PRIVATE_KEY_SIZE, KEY_AGREEMENT_PUB, &pub_output_len);
            assert(pub_output_len == ECC_PUBLIC_KEY_SIZE);

            #if APP_MODULE_ENABLED(NFC)
                if (device_is_nfc() == NFC_IS_ACTIVE) device_set_clock_rate(DEVICE_LOW_POWER_IDLE);
            #endif

            ret = ctap_add_cose_key(&map, KEY_AGREEMENT_PUB, KEY_AGREEMENT_PUB+32, PUB_KEY_CRED_PUB_KEY, COSE_ALG_ECDH_ES_HKDF_256);
            check_retr(ret);

            break;
        case CP_cmdSetPin:
            printf1(TAG_CP,"CP_cmdSetPin\n");

            if (ctap_is_pin_set())
            {
                return CTAP2_ERR_NOT_ALLOWED;
            }
            if (!CP.newPinEncSize || !CP.pinAuthPresent || !CP.keyAgreementPresent)
            {
                return CTAP2_ERR_MISSING_PARAMETER;
            }

            ret = ctap_update_pin_if_verified(CP.newPinEnc, CP.newPinEncSize, (uint8_t*)&CP.keyAgreement.pubkey, CP.pinAuth, NULL);
            check_retr(ret);
            break;
        case CP_cmdChangePin:
            printf1(TAG_CP,"CP_cmdChangePin\n");

            if (! ctap_is_pin_set())
            {
                return CTAP2_ERR_PIN_NOT_SET;
            }

            if (!CP.newPinEncSize || !CP.pinAuthPresent || !CP.keyAgreementPresent || !CP.pinHashEncPresent)
            {
                return CTAP2_ERR_MISSING_PARAMETER;
            }

            ret = ctap_update_pin_if_verified(CP.newPinEnc, CP.newPinEncSize, (uint8_t*)&CP.keyAgreement.pubkey, CP.pinAuth, CP.pinHashEnc);
            check_retr(ret);
            break;
        case CP_cmdGetPinToken:
            if (!ctap_is_pin_set())
            {
                return CTAP2_ERR_PIN_NOT_SET;
            }
            num_map++;
            ret = cbor_encoder_create_map(encoder, &map, num_map);
            check_ret(ret);

            printf1(TAG_CP,"CP_cmdGetPinToken\n");
            if (CP.keyAgreementPresent == 0 || CP.pinHashEncPresent == 0)
            {
                printf2(TAG_ERR,"Error, missing keyAgreement or pinHashEnc for cmdGetPin\n");
                return CTAP2_ERR_MISSING_PARAMETER;
            }
            ret = cbor_encode_int(&map, RESP_pinToken);
            check_ret(ret);

            /*ret = ctap_add_pin_if_verified(&map, (uint8_t*)&CP.keyAgreement.pubkey, CP.pinHashEnc);*/
            ret = ctap_add_pin_if_verified(pinTokenEnc, (uint8_t*)&CP.keyAgreement.pubkey, CP.pinHashEnc);
            check_retr(ret);

            ret = cbor_encode_byte_string(&map, pinTokenEnc, PIN_TOKEN_SIZE);
            check_ret(ret);



            break;

        default:
            printf2(TAG_ERR,"Error, invalid client pin subcommand\n");
            return CTAP1_ERR_OTHER;
    }

    if (CP.getRetries)
    {
        ret = cbor_encode_int(&map, RESP_retries);
        check_ret(ret);
        ret = cbor_encode_int(&map, ctap_leftover_pin_attempts());
        check_ret(ret);
    }

    if (num_map || CP.getRetries)
    {
        ret = cbor_encoder_close_container(encoder, &map);
        check_ret(ret);
    }

    return 0;
}

void ctap_reset()
{
    ctap_state_init();

    write_ctap_state(&STATE);

    if (crypto_generate_rng(PIN_TOKEN, PIN_TOKEN_SIZE) != 1)
    {
        printf2(TAG_ERR,"Error, rng failed\n");
        exit(1);
    }

    ctap_reset_state();
    ctap_reset_key_agreement();

    crypto_load_master_secret(STATE.key_space);
}


/***************************************************************************** 
*							I/O
*****************************************************************************/

uint8_t ctap_request(uint8_t * pkt_raw, int length, CTAP_RESPONSE * resp)
{
    NRF_LOG_INFO("ctap_request");
    CborEncoder encoder;
    memset(&encoder,0,sizeof(CborEncoder));
    uint8_t status = 0;
    uint8_t cmd = *pkt_raw;
    pkt_raw++;
    length--;

    uint8_t * buf = resp->data;

    cbor_encoder_init(&encoder, buf, resp->data_size, 0);
    NRF_LOG_DEBUG("resp buffer size: %d", resp->data_size);

    printf1(TAG_CTAP,"cbor input structure: %d bytes\n", length);
    printf1(TAG_DUMP,"cbor req: "); dump_hex1(TAG_DUMP, pkt_raw, length);

    switch(cmd)
    {
        case CTAP_MAKE_CREDENTIAL:
        case CTAP_GET_ASSERTION:
            if (ctap_device_locked())
            {
                status = CTAP2_ERR_PIN_BLOCKED;
                goto done;
            }
            if (ctap_device_boot_locked())
            {
                status = CTAP2_ERR_PIN_AUTH_BLOCKED;
                goto done;
            }
            break;
    }

    switch(cmd)
    {
        case CTAP_MAKE_CREDENTIAL:
            printf1(TAG_CTAP,"CTAP_MAKE_CREDENTIAL\n");
            status = ctap_make_credential(&encoder, pkt_raw, length);
 //           printf1(TAG_TIME,"make_credential");

            resp->length = cbor_encoder_get_buffer_size(&encoder, buf);
            dump_hex1(TAG_DUMP, buf, resp->length);

            break;
        case CTAP_GET_ASSERTION:
            printf1(TAG_CTAP,"CTAP_GET_ASSERTION\n");
            status = ctap_get_assertion(&encoder, pkt_raw, length);
//            printf1(TAG_TIME,"get_assertion time: %d ms\n", timestamp());

            resp->length = cbor_encoder_get_buffer_size(&encoder, buf);

            printf1(TAG_DUMP,"cbor [%d]: \n",  resp->length);
            dump_hex1(TAG_DUMP,buf, resp->length);
            break;
        case CTAP_CANCEL:
            printf1(TAG_CTAP,"CTAP_CANCEL\n");
            status = CTAP1_ERR_SUCCESS;
            resp->length = 0;
            break;
        case CTAP_GET_INFO:
            printf1(TAG_CTAP,"CTAP_GET_INFO\n");
            status = ctap_get_info(&encoder);
    
            resp->length = cbor_encoder_get_buffer_size(&encoder, buf);

            dump_hex1(TAG_DUMP, buf, resp->length);

            break;
        case CTAP_CLIENT_PIN:
            printf1(TAG_CTAP,"CTAP_CLIENT_PIN\n");
            status = ctap_client_pin(&encoder, pkt_raw, length);

            resp->length = cbor_encoder_get_buffer_size(&encoder, buf);
            dump_hex1(TAG_DUMP, buf, resp->length);
            break;
        case CTAP_RESET:
            printf1(TAG_CTAP,"CTAP_RESET\n");
            status = ctap2_user_presence_test(CTAP2_UP_DELAY_MS);
            if (status == CTAP1_ERR_SUCCESS)
            {
                ctap_reset();
            }
            break;
        case GET_NEXT_ASSERTION:
            printf1(TAG_CTAP,"CTAP_NEXT_ASSERTION\n");
            if (getAssertionState.lastcmd == CTAP_GET_ASSERTION)
            {
                status = ctap_get_next_assertion(&encoder);
                resp->length = cbor_encoder_get_buffer_size(&encoder, buf);
                dump_hex1(TAG_DUMP, buf, resp->length);
                if (status == 0)
                {
                    cmd = CTAP_GET_ASSERTION;       // allow for next assertion
                }
            }
            else
            {
                printf2(TAG_ERR, "unwanted GET_NEXT_ASSERTION.  lastcmd == 0x%02x\n", getAssertionState.lastcmd);
                status = CTAP2_ERR_NOT_ALLOWED;
            }
            break;
        default:
            status = CTAP1_ERR_INVALID_COMMAND;
            printf2(TAG_ERR,"error, invalid cmd: 0x%02x\n", cmd);
    }

done:
    device_set_status(CTAP_STATUS_IDLE);
    getAssertionState.lastcmd = cmd;

    if (status != CTAP1_ERR_SUCCESS)
    {
        resp->length = 0;
    }

    printf1(TAG_CTAP,"cbor output structure: %d bytes.  Return 0x%02x\n", resp->length, status);

    return status;
}

void ctap_response_init(CTAP_RESPONSE * resp)
{
    memset(resp, 0, sizeof(CTAP_RESPONSE));
    resp->data_size = CTAP_RESPONSE_BUFFER_SIZE;
}


/***************************************************************************** 
*							STATE HANDLING
*****************************************************************************/

static void ctap_state_init()
{
    // Set to 0xff instead of 0x00 to be easier on flash
    memset(&STATE, 0xff, sizeof(AuthenticatorState));
    // Fresh RNG for key
    crypto_generate_rng(STATE.key_space, KEY_SPACE_BYTES);

    STATE.is_initialized = INITIALIZED_MARKER;
    STATE.remaining_tries = PIN_LOCKOUT_ATTEMPTS;
    STATE.is_pin_set = 0;
    STATE.rk_stored = 0;
    STATE.data_version = STATE_VERSION;

    reset_ctap_rk();

    if (crypto_generate_rng(STATE.PIN_SALT, sizeof(STATE.PIN_SALT)) != 1) {
        printf2(TAG_ERR, "Error, rng failed\n");
        exit(1);
    }

    printf1(TAG_STOR, "Generated PIN SALT: ");
    dump_hex1(TAG_STOR, STATE.PIN_SALT, sizeof STATE.PIN_SALT);
}

void ctap_flush_state()
{
    write_ctap_state(&STATE);
}

void ctap_reset_state()
{
    memset(&getAssertionState, 0, sizeof(getAssertionState));
}


/***************************************************************************** 
*							CREDENTIAL HANDLING
*****************************************************************************/

static void ctap_increment_rk_store()
{
    STATE.rk_stored++;
    ctap_flush_state();
}

static int is_matching_rk(CTAP_residentKey * rk, CTAP_residentKey * rk2)
{
    return (memcmp(rk->id.rpIdHash, rk2->id.rpIdHash, 32) == 0) &&
           (memcmp(rk->user.id, rk2->user.id, rk->user.id_size) == 0) &&
           (rk->user.id_size == rk2->user.id_size);
}

static unsigned int get_credential_id_size(CTAP_credentialDescriptor * cred)
{
    if (cred->type == PUB_KEY_CRED_CTAP1)
        return U2F_KEY_HANDLE_SIZE;
    if (cred->type == PUB_KEY_CRED_CUSTOM)
        return getAssertionState.customCredIdSize;
    return sizeof(CredentialId);
}

/// Return 1 if credential belongs to this token
int ctap_authenticate_credential(struct rpId * rp, CTAP_credentialDescriptor * desc)
{
    uint8_t rpIdHash[32];
    uint8_t tag[16];

    switch(desc->type)
    {
        case PUB_KEY_CRED_PUB_KEY:
            crypto_sha256_init();
            crypto_sha256_update(rp->id, rp->size);
            crypto_sha256_final(rpIdHash);

            printf1(TAG_RED,"rpId: %s\r\n", rp->id); dump_hex1(TAG_RED,rp->id, rp->size);
            if (memcmp(desc->credential.id.rpIdHash, rpIdHash, 32) != 0)
            {
                return 0;
            }
            make_auth_tag(rpIdHash, desc->credential.id.nonce, desc->credential.id.count, tag);
            return (memcmp(desc->credential.id.tag, tag, CREDENTIAL_TAG_SIZE) == 0);
        break;
        case PUB_KEY_CRED_CTAP1:
            crypto_sha256_init();
            crypto_sha256_update(rp->id, rp->size);
            crypto_sha256_final(rpIdHash);
            return u2f_authenticate_credential((struct u2f_key_handle *)&desc->credential.id, U2F_KEY_HANDLE_SIZE,rpIdHash);
        break;
//        case PUB_KEY_CRED_CUSTOM:
//            return is_extension_request(getAssertionState.customCredId, getAssertionState.customCredIdSize);
        break;
        default:
        printf1(TAG_ERR, "PUB_KEY_CRED_UNKNOWN %x\r\n",desc->type);
        break;
    }

    return 0;
}

static uint8_t ctap_add_credential_descriptor(CborEncoder * map, CTAP_credentialDescriptor * cred)
{
    CborEncoder desc;
    int ret = cbor_encode_int(map, RESP_credential);
    check_ret(ret);

    ret = cbor_encoder_create_map(map, &desc, 2);
    check_ret(ret);

    {
        ret = cbor_encode_text_string(&desc, "id", 2);
        check_ret(ret);

        ret = cbor_encode_byte_string(&desc, (uint8_t*)&cred->credential.id,
            get_credential_id_size(cred));
        check_ret(ret);
    }

    {
        ret = cbor_encode_text_string(&desc, "type", 4);
        check_ret(ret);

        ret = cbor_encode_text_string(&desc, "public-key", 10);
        check_ret(ret);
    }


    ret = cbor_encoder_close_container(map, &desc);
    check_ret(ret);

    return 0;
}

// @return the number of valid credentials
// sorts the credentials.  Most recent creds will be first, invalid ones last.
int ctap_filter_invalid_credentials(CTAP_getAssertion * GA)
{
    int i;
    int count = 0;
    uint8_t rpIdHash[32];
    CTAP_residentKey rk;

    for (i = 0; i < GA->credLen; i++)
    {
        if (! ctap_authenticate_credential(&GA->rp, &GA->creds[i]))
        {
            printf1(TAG_GA, "CRED #%d is invalid\n", GA->creds[i].credential.id.count);
#ifdef ENABLE_U2F_EXTENSIONS
            if (is_extension_request((uint8_t*)&GA->creds[i].credential.id, sizeof(CredentialId)))
            {
                printf1(TAG_EXT, "CRED #%d is extension\n", GA->creds[i].credential.id.count);
                count++;
            }
            else
#endif
            {
                GA->creds[i].credential.id.count = 0;      // invalidate
            }

        }
        else
        {
            // add user info if it exists
            add_existing_user_info(&GA->creds[i]);
            count++;
        }
    }

    // No allowList, so use all matching RK's matching rpId
    if (!GA->credLen)
    {
        crypto_sha256_init();
        crypto_sha256_update(GA->rp.id,GA->rp.size);
        crypto_sha256_final(rpIdHash);

        printf1(TAG_GREEN, "true rpIdHash: ");  dump_hex1(TAG_GREEN, rpIdHash, 32);
        for(i = 0; i < STATE.rk_stored; i++)
        {
            load_ctap_rk(i, &rk);
            printf1(TAG_GREEN, "rpIdHash%d: ", i);  dump_hex1(TAG_GREEN, rk.id.rpIdHash, 32);
            if (memcmp(rk.id.rpIdHash, rpIdHash, 32) == 0)
            {
                printf1(TAG_GA, "RK %d is a rpId match!\r\n", i);
                if (count == ALLOW_LIST_MAX_SIZE-1)
                {
                    printf2(TAG_ERR, "not enough ram allocated for matching RK's (%d).  Skipping.\r\n", count);
                    break;
                }
                GA->creds[count].type = PUB_KEY_CRED_PUB_KEY;
                memmove(&(GA->creds[count].credential), &rk, sizeof(CTAP_residentKey));
                count++;
            }
        }
        GA->credLen = count;
    }

    printf1(TAG_GA, "qsort length: %d\n", GA->credLen);
    qsort(GA->creds, GA->credLen, sizeof(CTAP_credentialDescriptor), cred_cmp_func);
    return count;
}

static void save_credential_list(CTAP_authDataHeader * head, uint8_t * clientDataHash, CTAP_credentialDescriptor * creds, uint32_t count)
{
    if(count)
    {
        if (count > ALLOW_LIST_MAX_SIZE-1)
        {
            printf2(TAG_ERR, "ALLOW_LIST_MAX_SIZE Exceeded\n");
            exit(1);
        }
        memmove(getAssertionState.clientDataHash, clientDataHash, CLIENT_DATA_HASH_SIZE);
        memmove(&getAssertionState.authData, head, sizeof(CTAP_authDataHeader));
        memmove(getAssertionState.creds, creds, sizeof(CTAP_credentialDescriptor) * (count));

    }
    getAssertionState.count = count;
    printf1(TAG_GA,"saved %d credentials\n",count);
}

static CTAP_credentialDescriptor * pop_credential()
{
    if (getAssertionState.count > 0)
    {
        getAssertionState.count--;
        return &getAssertionState.creds[getAssertionState.count];
    }
    else
    {
        return NULL;
    }
}

static int cred_cmp_func(const void * _a, const void * _b)
{
    CTAP_credentialDescriptor * a = (CTAP_credentialDescriptor * )_a;
    CTAP_credentialDescriptor * b = (CTAP_credentialDescriptor * )_b;
    return b->credential.id.count - a->credential.id.count;
}


/***************************************************************************** 
*							PIN HANDLING
*****************************************************************************/

int8_t ctap_leftover_pin_attempts()
{
    return STATE.remaining_tries;
}

void ctap_reset_pin_attempts()
{
    STATE.remaining_tries = PIN_LOCKOUT_ATTEMPTS;
    PIN_BOOT_ATTEMPTS_LEFT = PIN_BOOT_ATTEMPTS;
    ctap_flush_state();
}

uint8_t ctap_is_pin_set()
{
    return STATE.is_pin_set == 1;
}

/**
 * Set new PIN, by updating PIN hash. Save state.
 * Globals: STATE
 * @param pin new PIN (raw)
 * @param len pin array length
 */
void ctap_update_pin(uint8_t * pin, int len)
{
    if (len >= NEW_PIN_ENC_MIN_SIZE || len < 4)
    {
        printf2(TAG_ERR, "Update pin fail length\n");
        exit(1);
    }

    crypto_sha256_init();
    crypto_sha256_update(pin, len);
    uint8_t intermediateHash[32];
    crypto_sha256_final(intermediateHash);

    crypto_sha256_init();
    crypto_sha256_update(intermediateHash, 16);
    memset(intermediateHash, 0, sizeof(intermediateHash));
    crypto_sha256_update(STATE.PIN_SALT, sizeof(STATE.PIN_SALT));
    crypto_sha256_final(STATE.PIN_CODE_HASH);

    STATE.is_pin_set = 1;

    write_ctap_state(&STATE);

    printf1(TAG_CTAP, "New pin set: %s [%d]\n", pin, len);
    dump_hex1(TAG_ERR, STATE.PIN_CODE_HASH, sizeof(STATE.PIN_CODE_HASH));
}

uint8_t ctap_decrement_pin_attempts()
{
    if (PIN_BOOT_ATTEMPTS_LEFT > 0)
    {
        PIN_BOOT_ATTEMPTS_LEFT--;
    }
    if (! ctap_device_locked())
    {
        STATE.remaining_tries--;
        ctap_flush_state();
        printf1(TAG_CP, "ATTEMPTS left: %d\n", STATE.remaining_tries);

        if (ctap_device_locked())
        {
            lock_device_permanently();
        }
    }
    else
    {
        printf1(TAG_CP, "Device locked!\n");
        return -1;
    }
    return 0;
}

uint8_t ctap_update_pin_if_verified(uint8_t * pinEnc, int len, uint8_t * platform_pubkey, uint8_t * pinAuth, uint8_t * pinHashEnc)
{
    uint8_t shared_secret[32];
    size_t shared_secret_len = sizeof(shared_secret);
    uint8_t hmac[32];
    size_t hmac_len = sizeof(hmac);
    size_t outlen;
    int ret;

//    Validate incoming data packet len
    if (len < 64)
    {
        return CTAP1_ERR_OTHER;
    }

//    Validate device's state
    if (ctap_is_pin_set())  // Check first, prevent SCA
    {
        if (ctap_device_locked())
        {
            return CTAP2_ERR_PIN_BLOCKED;
        }
        if (ctap_device_boot_locked())
        {
            return CTAP2_ERR_PIN_AUTH_BLOCKED;
        }
    }

//    calculate shared_secret
    crypto_ecc256_shared_secret(platform_pubkey, ECC_PUBLIC_KEY_SIZE, KEY_AGREEMENT_PRIV, ECC_PRIVATE_KEY_SIZE, shared_secret, &shared_secret_len);

    crypto_sha256_init();
    crypto_sha256_update(shared_secret, 32);
    crypto_sha256_final(shared_secret);

    crypto_sha256_hmac_init(shared_secret, 32);
    crypto_sha256_hmac_update(pinEnc, len);
    if (pinHashEnc != NULL)
    {
        crypto_sha256_hmac_update(pinHashEnc, 16);
    }
    crypto_sha256_hmac_final(hmac, &hmac_len);

    if (memcmp(hmac, pinAuth, 16) != 0)
    {
        printf2(TAG_ERR,"pinAuth failed for update pin\n");
        dump_hex1(TAG_ERR, hmac,16);
        dump_hex1(TAG_ERR, pinAuth,16);
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }

//     decrypt new PIN with shared secret
    crypto_aes256_init(shared_secret, NULL, NRF_CRYPTO_DECRYPT);

    while((len & 0xf) != 0) // round up to nearest  AES block size multiple
    {
        len++;
    }
    
    outlen = (size_t) len;
    crypto_aes256_op(pinEnc, &outlen);
    assert(outlen == (size_t) len);

//      validate new PIN (length)

    ret = trailing_zeros(pinEnc, NEW_PIN_ENC_MIN_SIZE - 1);
    ret = NEW_PIN_ENC_MIN_SIZE  - ret;

    if (ret < NEW_PIN_MIN_SIZE || ret >= NEW_PIN_MAX_SIZE)
    {
        printf2(TAG_ERR,"new PIN is too short or too long [%d bytes]\n", ret);
        return CTAP2_ERR_PIN_POLICY_VIOLATION;
    }
    else
    {
        printf1(TAG_CP,"new pin: %s [%d bytes]\n", pinEnc, ret);
        dump_hex1(TAG_CP, pinEnc, ret);
    }

//    validate device's state, decrypt and compare pinHashEnc (user provided current PIN hash) with stored PIN_CODE_HASH

    if (ctap_is_pin_set())
    {
        if (ctap_device_locked())
        {
            return CTAP2_ERR_PIN_BLOCKED;
        }
        if (ctap_device_boot_locked())
        {
            return CTAP2_ERR_PIN_AUTH_BLOCKED;
        }
        crypto_aes256_reset_iv(NULL);
        outlen = 16;
        crypto_aes256_op(pinHashEnc, &outlen);
        assert(outlen == 16);

        uint8_t pinHashEncSalted[32];
        crypto_sha256_init();
        crypto_sha256_update(pinHashEnc, 16);
        crypto_sha256_update(STATE.PIN_SALT, sizeof(STATE.PIN_SALT));
        crypto_sha256_final(pinHashEncSalted);

        if (memcmp(pinHashEncSalted, STATE.PIN_CODE_HASH, 16) != 0)
        {
            ctap_reset_key_agreement();
            ctap_decrement_pin_attempts();
            if (ctap_device_boot_locked())
            {
                return CTAP2_ERR_PIN_AUTH_BLOCKED;
            }
            return CTAP2_ERR_PIN_INVALID;
        }
        else
        {
            ctap_reset_pin_attempts();
        }
    }

    crypto_aes256_uninit();
//      set new PIN (update and store PIN_CODE_HASH)
    ctap_update_pin(pinEnc, ret);

    return 0;
}

uint8_t ctap_add_pin_if_verified(uint8_t * pinTokenEnc, uint8_t * platform_pubkey, uint8_t * pinHashEnc)
{
    uint8_t shared_secret[32];
    size_t shared_secret_len = sizeof(shared_secret);
    size_t outlen;

    crypto_ecc256_shared_secret(platform_pubkey, ECC_PUBLIC_KEY_SIZE, KEY_AGREEMENT_PRIV, ECC_PRIVATE_KEY_SIZE, shared_secret, &shared_secret_len);

    crypto_sha256_init();
    crypto_sha256_update(shared_secret, 32);
    crypto_sha256_final(shared_secret);

    crypto_aes256_init(shared_secret, NULL, NRF_CRYPTO_DECRYPT);
    
    outlen = 16;
    crypto_aes256_op(pinHashEnc,&outlen);
    assert(outlen == 16);

    uint8_t pinHashEncSalted[32];
    crypto_sha256_init();
    crypto_sha256_update(pinHashEnc, 16);
    crypto_sha256_update(STATE.PIN_SALT, sizeof(STATE.PIN_SALT));
    crypto_sha256_final(pinHashEncSalted);
    if (memcmp(pinHashEncSalted, STATE.PIN_CODE_HASH, 16) != 0)
    {
        printf2(TAG_ERR,"Pin does not match!\n");
        printf2(TAG_ERR,"platform-pin-hash: "); dump_hex1(TAG_ERR, pinHashEnc, 16);
        printf2(TAG_ERR,"authentic-pin-hash: "); dump_hex1(TAG_ERR, STATE.PIN_CODE_HASH, 16);
        printf2(TAG_ERR,"shared-secret: "); dump_hex1(TAG_ERR, shared_secret, 32);
        printf2(TAG_ERR,"platform-pubkey: "); dump_hex1(TAG_ERR, platform_pubkey, 64);
        printf2(TAG_ERR,"device-pubkey: "); dump_hex1(TAG_ERR, KEY_AGREEMENT_PUB, ECC_PUBLIC_KEY_SIZE);
        // Generate new keyAgreement pair
        ctap_reset_key_agreement();
        ctap_decrement_pin_attempts();
        if (ctap_device_boot_locked())
        {
            return CTAP2_ERR_PIN_AUTH_BLOCKED;
        }
        return CTAP2_ERR_PIN_INVALID;
    }

    ctap_reset_pin_attempts();
    crypto_aes256_uninit();
    crypto_aes256_init(shared_secret, NULL, NRF_CRYPTO_ENCRYPT);
    memmove(pinTokenEnc, PIN_TOKEN, PIN_TOKEN_SIZE);
    outlen = PIN_TOKEN_SIZE;
    crypto_aes256_op(pinTokenEnc, &outlen);
    assert(outlen == PIN_TOKEN_SIZE);
    crypto_aes256_uninit();

    return 0;
}

uint8_t verify_pin_auth(uint8_t * pinAuth, uint8_t * clientDataHash)
{
    size_t hmaclen = 32;
    uint8_t hmac[hmaclen];

    crypto_sha256_hmac_init(PIN_TOKEN, PIN_TOKEN_SIZE);
    crypto_sha256_hmac_update(clientDataHash, CLIENT_DATA_HASH_SIZE);
    crypto_sha256_hmac_final(hmac, &hmaclen);

    if (memcmp(pinAuth, hmac, 16) == 0)
    {
        return 0;
    }
    else
    {
        printf2(TAG_ERR,"Pin auth failed\n");
        dump_hex1(TAG_ERR,pinAuth,16);
        dump_hex1(TAG_ERR,hmac,16);
        return CTAP2_ERR_PIN_AUTH_INVALID;
    }

}


/***************************************************************************** 
*							COSE
*****************************************************************************/

static int ctap_add_cose_key(CborEncoder * cose_key, uint8_t * x, uint8_t * y, uint8_t credtype, int32_t algtype)
{
    int ret;
    CborEncoder map;

    ret = cbor_encoder_create_map(cose_key, &map, 5);
    check_ret(ret);


    {
        ret = cbor_encode_int(&map, COSE_KEY_LABEL_KTY);
        check_ret(ret);
        ret = cbor_encode_int(&map, COSE_KEY_KTY_EC2);
        check_ret(ret);
    }

    {
        ret = cbor_encode_int(&map, COSE_KEY_LABEL_ALG);
        check_ret(ret);
        ret = cbor_encode_int(&map, algtype);
        check_ret(ret);
    }

    {
        ret = cbor_encode_int(&map, COSE_KEY_LABEL_CRV);
        check_ret(ret);
        ret = cbor_encode_int(&map, COSE_KEY_CRV_P256);
        check_ret(ret);
    }


    {
        ret = cbor_encode_int(&map, COSE_KEY_LABEL_X);
        check_ret(ret);
        ret = cbor_encode_byte_string(&map, x, 32);
        check_ret(ret);
    }

    {
        ret = cbor_encode_int(&map, COSE_KEY_LABEL_Y);
        check_ret(ret);
        ret = cbor_encode_byte_string(&map, y, 32);
        check_ret(ret);
    }

    ret = cbor_encoder_close_container(cose_key, &map);
    check_ret(ret);

    return 0;
}

static int ctap_generate_cose_key(CborEncoder * cose_key, uint8_t * hmac_input, int len, uint8_t credtype, int32_t algtype)
{
    uint8_t x[32], y[32];

    if (credtype != PUB_KEY_CRED_PUB_KEY)
    {
        printf2(TAG_ERR,"Error, pubkey credential type not supported\n");
        return -1;
    }
    switch(algtype)
    {
        case COSE_ALG_ES256:
            #if APP_MODULE_ENABLED(NFC)
                if (device_is_nfc() == NFC_IS_ACTIVE) device_set_clock_rate(DEVICE_LOW_POWER_FAST);
            #endif

            crypto_ecc256_derive_public_key(hmac_input, len, x, y);
            
            #if APP_MODULE_ENABLED(NFC)
                if (device_is_nfc() == NFC_IS_ACTIVE) device_set_clock_rate(DEVICE_LOW_POWER_IDLE);
            #endif
            break;
        default:
            printf2(TAG_ERR,"Error, COSE alg %d not supported\n", algtype);
            return -1;
    }
    int ret = ctap_add_cose_key(cose_key, x, y, credtype, algtype);
    check_ret(ret);
    return 0;
}


/***************************************************************************** 
*							RK MANAGE
*****************************************************************************/

uint16_t ctap_keys_stored()
{
    int total = 0;
    int i;
    for (i = 0; i < MAX_KEYS; i++)
    {
        if (STATE.key_lens[i] != 0xffff)
        {
            total += 1;
        }
        else
        {
            break;
        }
    }
    return total;
}

static uint16_t key_addr_offset(int index)
{
    uint16_t offset = 0;
    int i;
    for (i = 0; i < index; i++)
    {
        if (STATE.key_lens[i] != 0xffff) offset += STATE.key_lens[i];
    }
    return offset;
}

uint16_t ctap_key_len(uint8_t index)
{
    int i = ctap_keys_stored();
    if (index >= i || index >= MAX_KEYS)
    {
        return 0;
    }
    if (STATE.key_lens[index] == 0xffff) return 0;
    return STATE.key_lens[index];

}

int8_t ctap_store_key(uint8_t index, uint8_t * key, uint16_t len)
{
    int i = ctap_keys_stored();
    uint16_t offset;
    if (i >= MAX_KEYS || index >= MAX_KEYS || !len)
    {
        return ERR_NO_KEY_SPACE;
    }

    if (STATE.key_lens[index] != 0xffff)
    {
        return ERR_KEY_SPACE_TAKEN;
    }

    offset = key_addr_offset(index);

    if ((offset + len) > KEY_SPACE_BYTES)
    {
        return ERR_NO_KEY_SPACE;
    }

    STATE.key_lens[index] = len;

    memmove(STATE.key_space + offset, key, len);

    ctap_flush_state();

    return 0;
}

int8_t ctap_load_key(uint8_t index, uint8_t * key)
{
    int i = ctap_keys_stored();
    uint16_t offset;
    uint16_t len;
    if (index >= i || index >= MAX_KEYS)
    {
        return ERR_NO_KEY_SPACE;
    }

    if (STATE.key_lens[index] == 0xffff)
    {
        return ERR_KEY_SPACE_EMPTY;
    }

    offset = key_addr_offset(index);
    len = ctap_key_len(index);

    if ((offset + len) > KEY_SPACE_BYTES)
    {
        return ERR_NO_KEY_SPACE;
    }

    memmove(key, STATE.key_space + offset, len);

    return 0;
}

static void add_existing_user_info(CTAP_credentialDescriptor * cred)
{
    CTAP_residentKey rk;
    int index = STATE.rk_stored;
    int i;
    for (i = 0; i < index; i++)
    {
        load_ctap_rk(i, &rk);
        if (is_matching_rk(&rk, (CTAP_residentKey *)&cred->credential))
        {
            printf1(TAG_GREEN, "found rk match for allowList item (%d)\r\n", i);
            memmove(&cred->credential.user, &rk.user, sizeof(CTAP_userEntity));
            return;
        }

    }
    printf1(TAG_GREEN, "NO rk match for allowList item \r\n");
}


/***************************************************************************** 
*							MASTER SECRET
*****************************************************************************/

/** Overwrite master secret from external source.
 * @param keybytes an array of KEY_SPACE_BYTES length.
 *
 * This function should only be called from a privilege mode.
*/
void ctap_load_external_keys(uint8_t * keybytes)
{
    memmove(STATE.key_space, keybytes, KEY_SPACE_BYTES);
    write_ctap_state(&STATE);
    crypto_load_master_secret(STATE.key_space);
}


/***************************************************************************** 
*							AUTH DATA
*****************************************************************************/

void make_auth_tag(uint8_t * rpIdHash, uint8_t * nonce, uint32_t count, uint8_t * tag)
{
    size_t hashbuflen = 32;
    uint8_t hashbuf[hashbuflen];
    memset(hashbuf,0,sizeof(hashbuf));
    crypto_sha256_hmac_init(CRYPTO_TRANSPORT_KEY, 0);
    crypto_sha256_hmac_update(rpIdHash, 32);
    crypto_sha256_hmac_update(nonce, CREDENTIAL_NONCE_SIZE);
    crypto_sha256_hmac_update((uint8_t*)&count, 4);
    crypto_sha256_hmac_final(hashbuf, &hashbuflen);
    assert(CREDENTIAL_TAG_SIZE <= hashbuflen);

    memmove(tag, hashbuf, CREDENTIAL_TAG_SIZE);
}

static uint32_t auth_data_update_count(CTAP_authDataHeader * authData)
{
    uint32_t count = ctap_atomic_count( 0 );
    if (count == 0)     // count 0 will indicate invalid token
    {
        count = ctap_atomic_count( 0 );

    }
    uint8_t * byte = (uint8_t*) &authData->signCount;

    *byte++ = (count >> 24) & 0xff;
    *byte++ = (count >> 16) & 0xff;
    *byte++ = (count >> 8) & 0xff;
    *byte++ = (count >> 0) & 0xff;

    return count;
}

static int ctap_make_auth_data(struct rpId * rp, CborEncoder * map, uint8_t * auth_data_buf, uint32_t * len, CTAP_credInfo * credInfo)
{
    CborEncoder cose_key;

    unsigned int auth_data_sz = sizeof(CTAP_authDataHeader);
    uint32_t count;
    CTAP_residentKey rk, rk2;
    CTAP_authData * authData = (CTAP_authData *)auth_data_buf;

    uint8_t * cose_key_buf = auth_data_buf + sizeof(CTAP_authData);

    // memset(&cose_key, 0, sizeof(CTAP_residentKey));
    memset(&rk, 0, sizeof(CTAP_residentKey));
    memset(&rk2, 0, sizeof(CTAP_residentKey));

    if((sizeof(CTAP_authDataHeader)) > *len)
    {
        printf1(TAG_ERR,"assertion fail, auth_data_buf must be at least %d bytes\n", sizeof(CTAP_authData) - sizeof(CTAP_attestHeader));
        exit(1);
    }

    crypto_sha256_init();
    crypto_sha256_update(rp->id, rp->size);
    crypto_sha256_final(authData->head.rpIdHash);

    count = auth_data_update_count(&authData->head);

    int but;

    but = ctap2_user_presence_test(CTAP2_UP_DELAY_MS);
    if (CTAP2_ERR_PROCESSING == but)
    {
        authData->head.flags = (0 << 0);        // User presence disabled
    }
    else
    {
        check_retr(but);
        authData->head.flags = (1 << 0);        // User presence
    }


    device_set_status(CTAP_STATUS_PROCESSING);

    authData->head.flags |= (ctap_is_pin_set() << 2);


    if (credInfo != NULL)
    {
        // add attestedCredentialData
        authData->head.flags |= (1 << 6);//include attestation data

        cbor_encoder_init(&cose_key, cose_key_buf, *len - sizeof(CTAP_authData), 0);

        get_aaguid(authData->attest.aaguid);
        authData->attest.credLenL =  sizeof(CredentialId) & 0x00FF;
        authData->attest.credLenH = (sizeof(CredentialId) & 0xFF00) >> 8;

        memset((uint8_t*)&authData->attest.id, 0, sizeof(CredentialId));

        crypto_generate_rng(authData->attest.id.nonce, CREDENTIAL_NONCE_SIZE);

        authData->attest.id.count = count;

        memmove(authData->attest.id.rpIdHash, authData->head.rpIdHash, 32);

        // Make a tag we can later check to make sure this is a token we made
        make_auth_tag(authData->head.rpIdHash, authData->attest.id.nonce, count, authData->attest.id.tag);

        // resident key
        if (credInfo->rk)
        {
            memmove(&rk.id, &authData->attest.id, sizeof(CredentialId));
            memmove(&rk.user, &credInfo->user, sizeof(CTAP_userEntity));

            unsigned int index = STATE.rk_stored;
            unsigned int i;
            for (i = 0; i < index; i++)
            {
                load_ctap_rk(i, &rk2);
                if (is_matching_rk(&rk, &rk2))
                {
                    overwrite_ctap_rk(i, &rk);
                    goto done_rk;
                }
            }
            if (index >= ctap_rk_size())
            {
                printf2(TAG_ERR, "Out of memory for resident keys\r\n");
                return CTAP2_ERR_KEY_STORE_FULL;
            }
            ctap_increment_rk_store();
            store_ctap_rk(index, &rk);
            dump_hex1(TAG_GREEN, rk.id.rpIdHash, 32);
        }
done_rk:

        printf1(TAG_GREEN, "MADE credId: "); dump_hex1(TAG_GREEN, (uint8_t*) &authData->attest.id, sizeof(CredentialId));

        ctap_generate_cose_key(&cose_key, (uint8_t*)&authData->attest.id, sizeof(CredentialId), credInfo->publicKeyCredentialType, credInfo->COSEAlgorithmIdentifier);

        auth_data_sz = sizeof(CTAP_authData) + cbor_encoder_get_buffer_size(&cose_key, cose_key_buf);

    }


    *len = auth_data_sz;
    return 0;
}


/***************************************************************************** 
*							ATTESTATION
*****************************************************************************/

/**
 *
 * @param in_sigbuf IN location to deposit signature (must be 64 bytes)
 * @param out_sigder OUT location to deposit der signature (must be 72 bytes)
 * @return length of der signature
 * // FIXME add tests for maximum and minimum length of the input and output
 */
int ctap_encode_der_sig(const uint8_t * const in_sigbuf, uint8_t * const out_sigder)
{
    // Need to caress into dumb der format ..
    uint8_t i;
    uint8_t lead_s = 0;  // leading zeros
    uint8_t lead_r = 0;
    for (i=0; i < 32; i++)
        if (in_sigbuf[i] == 0) lead_r++;
        else break;

    for (i=0; i < 32; i++)
        if (in_sigbuf[i+32] == 0) lead_s++;
        else break;

    int8_t pad_s = ((in_sigbuf[32 + lead_s] & 0x80) == 0x80);
    int8_t pad_r = ((in_sigbuf[0 + lead_r] & 0x80) == 0x80);

    memset(out_sigder, 0, 72);
    out_sigder[0] = 0x30;
    out_sigder[1] = 0x44 + pad_s + pad_r - lead_s - lead_r;

    // R ingredient
    out_sigder[2] = 0x02;
    out_sigder[3 + pad_r] = 0;
    out_sigder[3] = 0x20 + pad_r - lead_r;
    memmove(out_sigder + 4 + pad_r, in_sigbuf + lead_r, 32u - lead_r);

    // S ingredient
    out_sigder[4 + 32 + pad_r - lead_r] = 0x02;
    out_sigder[5 + 32 + pad_r + pad_s - lead_r] = 0;
    out_sigder[5 + 32 + pad_r - lead_r] = 0x20 + pad_s - lead_s;
    memmove(out_sigder + 6 + 32 + pad_r + pad_s - lead_r, in_sigbuf + 32u + lead_s, 32u - lead_s);

    return 0x46 + pad_s + pad_r - lead_r - lead_s;
}

// require load_key prior to this
// @data data to hash before signature
// @clientDataHash for signature
// @tmp buffer for hash.  (can be same as data if data >= 32 bytes)
// @sigbuf OUT location to deposit signature (must be 64 bytes)
// @sigder OUT location to deposit der signature (must be 72 bytes)
// @return length of der signature
int ctap_calculate_signature(uint8_t * data, int datalen, uint8_t * clientDataHash, uint8_t * hashbuf, uint8_t * sigbuf, uint8_t * sigder)
{
    size_t sigbuf_len = 64;
    // calculate attestation sig
    crypto_sha256_init();
    crypto_sha256_update(data, datalen);
    crypto_sha256_update(clientDataHash, CLIENT_DATA_HASH_SIZE);
    crypto_sha256_final(hashbuf);
    

    crypto_ecc256_sign(hashbuf, 32, sigbuf, &sigbuf_len);
    return ctap_encode_der_sig(sigbuf,sigder);
}

uint8_t ctap_add_attest_statement(CborEncoder * map, uint8_t * sigder, int len)
{
    int ret;
    uint8_t cert[1024];
    uint16_t cert_size = attestation_cert_der_get_size();
    if (cert_size > sizeof(cert)){
        printf2(TAG_ERR,"Certificate is too large for CTAP2 buffer\r\n");
        return CTAP2_ERR_PROCESSING;
    }
    attestation_read_cert_der(cert);

    CborEncoder stmtmap;
    CborEncoder x5carr;

    ret = cbor_encode_int(map,RESP_attStmt);
    check_ret(ret);
    ret = cbor_encoder_create_map(map, &stmtmap, 3);
    check_ret(ret);
    {
        ret = cbor_encode_text_stringz(&stmtmap,"alg");
        check_ret(ret);
        ret = cbor_encode_int(&stmtmap,COSE_ALG_ES256);
        check_ret(ret);
    }
    {
        ret = cbor_encode_text_stringz(&stmtmap,"sig");
        check_ret(ret);
        ret = cbor_encode_byte_string(&stmtmap, sigder, len);
        check_ret(ret);
    }
    {
        ret = cbor_encode_text_stringz(&stmtmap,"x5c");
        check_ret(ret);
        ret = cbor_encoder_create_array(&stmtmap, &x5carr, 1);
        check_ret(ret);
        {
            ret = cbor_encode_byte_string(&x5carr, cert, attestation_cert_der_get_size());
            check_ret(ret);
            ret = cbor_encoder_close_container(&stmtmap, &x5carr);
            check_ret(ret);
        }
    }

    ret = cbor_encoder_close_container(map, &stmtmap);
    check_ret(ret);
    return 0;
}


/***************************************************************************** 
*							DEVICE HANDLING
*****************************************************************************/

int8_t ctap_device_locked()
{
    return STATE.remaining_tries <= 0;
}

int8_t ctap_device_boot_locked()
{
    return PIN_BOOT_ATTEMPTS_LEFT <= 0;
}

void lock_device_permanently()
{
    memset(PIN_TOKEN, 0, sizeof(PIN_TOKEN));
    memset(STATE.PIN_CODE_HASH, 0, sizeof(STATE.PIN_CODE_HASH));

    printf1(TAG_CP, "Device locked!\n");

    write_ctap_state(&STATE);
}

static int ctap2_user_presence_test()
{
    device_set_status(CTAP_STATUS_UPNEEDED);
    int ret = ctap_user_presence_test(CTAP2_UP_DELAY_MS);
    if ( ret > 1 )
    {
        return CTAP2_ERR_PROCESSING;
    }
    else if ( ret > 0 )
    {
        return CTAP1_ERR_SUCCESS;
    }
    else if (ret < 0)
    {
        return CTAP2_ERR_KEEPALIVE_CANCEL;
    }
    else
    {
        return CTAP2_ERR_ACTION_TIMEOUT;
    }
}


/***************************************************************************** 
*							EXTENSIONS
*****************************************************************************/
static int ctap_make_extensions(CTAP_extensions * ext, uint8_t * ext_encoder_buf, unsigned int * ext_encoder_buf_size)
{
    CborEncoder extensions;
    int ret;
    size_t outputlen = 64;
    uint8_t output[outputlen];
    uint8_t shared_secret[32];
    size_t shared_secret_len = 32;
    size_t hmaclen = 32;
    uint8_t hmac[hmaclen];
    size_t credRandomlen = 32;
    uint8_t credRandom[credRandomlen];

    if (ext->hmac_secret_present == EXT_HMAC_SECRET_PARSED)
    {
        printf1(TAG_CTAP, "Processing hmac-secret..\r\n");

        crypto_ecc256_shared_secret((uint8_t*) &ext->hmac_secret.keyAgreement.pubkey,
                                    ECC_PUBLIC_KEY_SIZE, 
                                    KEY_AGREEMENT_PRIV,
                                    ECC_PRIVATE_KEY_SIZE,
                                    shared_secret,
                                    &shared_secret_len
                                    );
        assert(shared_secret_len == 32);
        
        crypto_sha256_init();
        crypto_sha256_update(shared_secret, 32);
        crypto_sha256_final(shared_secret);

        crypto_sha256_hmac_init(shared_secret, 32);
        crypto_sha256_hmac_update(ext->hmac_secret.saltEnc, ext->hmac_secret.saltLen);
        crypto_sha256_hmac_final(hmac, &hmaclen);

        if (memcmp(ext->hmac_secret.saltAuth, hmac, 16) == 0)
        {
            printf1(TAG_CTAP, "saltAuth is valid\r\n");
        }
        else
        {
            printf1(TAG_CTAP, "saltAuth is invalid\r\n");
            return CTAP2_ERR_EXTENSION_FIRST;
        }

        // Generate credRandom
        crypto_sha256_hmac_init(CRYPTO_TRANSPORT_KEY2, 0);
        crypto_sha256_hmac_update((uint8_t*)&ext->hmac_secret.credential->id, sizeof(CredentialId));
        crypto_sha256_hmac_final(credRandom, &credRandomlen);

        // Decrypt saltEnc
        crypto_aes256_init(shared_secret, NULL, NRF_CRYPTO_DECRYPT);
        size_t aes_outlen = ext->hmac_secret.saltLen;
        crypto_aes256_op(ext->hmac_secret.saltEnc, &aes_outlen);

        // Generate outputs
        crypto_sha256_hmac_init(credRandom, 32);
        crypto_sha256_hmac_update(ext->hmac_secret.saltEnc, 32);
        crypto_sha256_hmac_final(output, &outputlen);

        assert(outputlen == 32);
        if (ext->hmac_secret.saltLen == 64)
        {
            crypto_sha256_hmac_init(credRandom, 32);
            crypto_sha256_hmac_update(ext->hmac_secret.saltEnc + 32, 32);
            crypto_sha256_hmac_final(output + 32, &outputlen);
        }

        // Encrypt for final output
        crypto_aes256_init(shared_secret, NULL, NRF_CRYPTO_ENCRYPT);
        
        assert(ext->hmac_secret.saltLen < 64);
        outputlen = ext->hmac_secret.saltLen;
        crypto_aes256_op(output, &outputlen);

        // output
        cbor_encoder_init(&extensions, ext_encoder_buf, *ext_encoder_buf_size, 0);
        {
            CborEncoder hmac_secret_map;
            ret = cbor_encoder_create_map(&extensions, &hmac_secret_map, 1);
            check_ret(ret);
            {
                ret = cbor_encode_text_stringz(&hmac_secret_map, "hmac-secret");
                check_ret(ret);

                ret = cbor_encode_byte_string(&hmac_secret_map, output, ext->hmac_secret.saltLen);
                check_ret(ret);
            }
            ret = cbor_encoder_close_container(&extensions, &hmac_secret_map);
            check_ret(ret);
        }
        *ext_encoder_buf_size = cbor_encoder_get_buffer_size(&extensions, ext_encoder_buf);
    }
    else if (ext->hmac_secret_present == EXT_HMAC_SECRET_REQUESTED)
    {
        cbor_encoder_init(&extensions, ext_encoder_buf, *ext_encoder_buf_size, 0);
        {
            CborEncoder hmac_secret_map;
            ret = cbor_encoder_create_map(&extensions, &hmac_secret_map, 1);
            check_ret(ret);
            {
                ret = cbor_encode_text_stringz(&hmac_secret_map, "hmac-secret");
                check_ret(ret);

                ret = cbor_encode_boolean(&hmac_secret_map, 1);
                check_ret(ret);
            }
            ret = cbor_encoder_close_container(&extensions, &hmac_secret_map);
            check_ret(ret);
        }
        *ext_encoder_buf_size = cbor_encoder_get_buffer_size(&extensions, ext_encoder_buf);
    }
    else
    {
        *ext_encoder_buf_size = 0;
    }
    return 0;
}


/***************************************************************************** 
*							UTIL
*****************************************************************************/

uint8_t ctap_add_user_entity(CborEncoder * map, CTAP_userEntity * user)
{
    CborEncoder entity;
    int ret = cbor_encode_int(map, RESP_publicKeyCredentialUserEntity);
    check_ret(ret);

    int dispname = (user->name[0] != 0) && getAssertionState.user_verified;

    if (dispname)
        ret = cbor_encoder_create_map(map, &entity, 4);
    else
        ret = cbor_encoder_create_map(map, &entity, 1);
    check_ret(ret);

    {
        ret = cbor_encode_text_string(&entity, "id", 2);
        check_ret(ret);

        ret = cbor_encode_byte_string(&entity, user->id, user->id_size);
        check_ret(ret);
    }

    if (dispname)
    {

        ret = cbor_encode_text_string(&entity, "icon", 4);
        check_ret(ret);

        ret = cbor_encode_text_stringz(&entity, (const char *)user->icon);
        check_ret(ret);

        ret = cbor_encode_text_string(&entity, "name", 4);
        check_ret(ret);

        ret = cbor_encode_text_stringz(&entity, (const char *)user->name);
        check_ret(ret);

        ret = cbor_encode_text_string(&entity, "displayName", 11);
        check_ret(ret);

        ret = cbor_encode_text_stringz(&entity, (const char *)user->displayName);
        check_ret(ret);

    }

    ret = cbor_encoder_close_container(map, &entity);
    check_ret(ret);

    return 0;
}

// Return how many trailing zeros in a buffer
static int trailing_zeros(uint8_t * buf, int indx)
{
    int c = 0;
    while(0==buf[indx] && indx)
    {
        indx--;
        c++;
    }
    return c;
}

static void ctap_reset_key_agreement()
{
    crypto_generate_rng(KEY_AGREEMENT_PRIV, sizeof(KEY_AGREEMENT_PRIV));
}


/***************************************************************************** 
*							LEFTOVERS
*****************************************************************************/

uint32_t ctap_atomic_count(uint32_t amount)
{
    if(amount==0)
    {
        uint8_t rng[1];
        crypto_generate_rng(rng, 1);
        amount = (rng[0] & 0x0f) + 1;
    }
    return counterIncrement(COUNTERID, amount);
}

/*static int pick_first_authentic_credential(CTAP_getAssertion * GA)*/
/*{*/
    /*int i;*/
    /*for (i = 0; i < GA->credLen; i++)*/
    /*{*/
        /*if (GA->creds[i].credential.enc.count != 0)*/
        /*{*/
            /*return i;*/
        /*}*/
    /*}*/
    /*return -1;*/
/*}*/

