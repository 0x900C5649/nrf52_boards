/******************************************************************************
* File:             tests.c
*
* Author:           Joshua Steffensky  
* Created:          04/09/20 
* Description:      Testing components of nrf52-fido2
*****************************************************************************/

#include "storage.h"
#include "crypto.h"
#include "tests.h"

#include "nrf_crypto_types.h"

#define NRF_LOG_MODULE_NAME tests

#include "nrf_log.h"
#include "nrf_log_ctrl.h"

NRF_LOG_MODULE_REGISTER();

#include "log.h"

/***************************************************************************** 
*							GLOBAL TEST DATA
*****************************************************************************/

uint8_t testdata1[] = {0x81, 0x18, 0xF1, 0xD0};
uint8_t testdata2[] = {0x18, 0x81, 0xF1, 0xD0};


void test_flash()
{
    NRF_LOG_DEBUG("testing storage now");
    NRF_LOG_FLUSH();

    // delete it
    reset_ctap_rk();
    NRF_LOG_DEBUG("delete rks")
    NRF_LOG_FLUSH();

    // initialize test data
    CTAP_residentKey input;
    CTAP_residentKey output;
    NRF_LOG_DEBUG("memcpy");
    NRF_LOG_FLUSH();
    memcpy(&input, &testdata1, 4);

    // write it to flash
    store_ctap_rk(1, &input);
    NRF_LOG_DEBUG("write test rk");
    NRF_LOG_FLUSH();

    // read it again
    load_ctap_rk(1, &output);
    NRF_LOG_DEBUG("load test rk");
    NRF_LOG_FLUSH();

    // print read data
    NRF_LOG_HEXDUMP_DEBUG(&output, 4)
    NRF_LOG_FLUSH();

    // update entry
    memcpy(&input, &testdata2, 4);
    overwrite_ctap_rk(1, &input);

    // read it
    load_ctap_rk(1, &output);
    NRF_LOG_DEBUG("load test rk");
    NRF_LOG_FLUSH();

    // print it
    NRF_LOG_HEXDUMP_DEBUG(&output, 4)

    // delete it
    void reset_ctap_rk();
    NRF_LOG_DEBUG("delete rks")
    NRF_LOG_FLUSH();
}

void test_hash()
{
    uint8_t resulthash[64];
    memset(&resulthash, 0, 64);

    NRF_LOG_INFO("starting hash tests");

    NRF_LOG_INFO("sha256 test");
    crypto_sha256_init();
    NRF_LOG_DEBUG("sha256 init");
    crypto_sha256_update(testdata1, 4);
    NRF_LOG_DEBUG("sha256 update 1");
    crypto_sha256_update(testdata2, 4);
    NRF_LOG_DEBUG("sha256 update 2");
    crypto_sha256_final(resulthash);
    NRF_LOG_DEBUG("sha256 final");
    NRF_LOG_HEXDUMP_DEBUG(&resulthash, 32);

    memset(&resulthash, 0, 64);
    NRF_LOG_INFO("sha256 test2");
    crypto_sha256_init();
    NRF_LOG_DEBUG("sha256 init");
    crypto_sha256_update(testdata1, 4);
    NRF_LOG_DEBUG("sha256 update 1");
    crypto_sha256_update(testdata2, 4);
    NRF_LOG_DEBUG("sha256 update 2");
    crypto_sha256_final(resulthash);
    NRF_LOG_DEBUG("sha256 final");
    NRF_LOG_HEXDUMP_DEBUG(&resulthash, 32);

    memset(&resulthash, 0, 64);

    NRF_LOG_INFO("sha512 test");
    crypto_sha512_init();
    NRF_LOG_DEBUG("sha512 init");
    crypto_sha512_update(testdata1, 4);
    NRF_LOG_DEBUG("sha512 update 1");
    crypto_sha512_update(testdata2, 4);
    NRF_LOG_DEBUG("sha512 update 2");
    crypto_sha512_final(resulthash);
    NRF_LOG_DEBUG("sha512 final");
    NRF_LOG_HEXDUMP_DEBUG(&resulthash, 64);
}

void test_hmac()
{
    NRF_LOG_INFO("starting HMAC-sha256 testing");

    uint8_t resulthash[64];
    size_t  len = 64;
    memset(&resulthash, 0, 64);

    crypto_sha256_hmac_init(testdata1, 4);
    NRF_LOG_DEBUG("hmac init");

    crypto_sha256_hmac_update(testdata2, 4);
    NRF_LOG_DEBUG("hmac update");

    crypto_sha256_hmac_final(resulthash, &len);
    NRF_LOG_DEBUG("hmac final");

    NRF_LOG_HEXDUMP_DEBUG(&resulthash, 32);
}

void test_random()
{
    NRF_LOG_INFO("starting RNG test");
    uint8_t result[64];
    memset(&result, 0, 64);

    crypto_generate_rng(result, 64);
    NRF_LOG_INFO("random 1");
    NRF_LOG_HEXDUMP_DEBUG(&result, 64);

    memset(&result, 0, 64);

    crypto_generate_rng(result, 64);
    NRF_LOG_INFO("random 2");
    NRF_LOG_HEXDUMP_DEBUG(&result, 64);
}

void test_aes(void)
{
    uint8_t nonce[16];
    size_t  datalen  = 32;
    uint8_t data[32] = "THIS IS A TEST STING, A BAD ONE.";
    uint8_t key[32];

    NRF_LOG_INFO("Testing AES");

    NRF_LOG_INFO("generating nonce");
    crypto_generate_rng(nonce, 16);
    NRF_LOG_DEBUG("nonce: ");
    NRF_LOG_HEXDUMP_DEBUG(nonce, 16);

    NRF_LOG_INFO("input data:");
    NRF_LOG_HEXDUMP_DEBUG(data, 32);

    memcpy(key, testdata1, 4);
    memcpy(key + 4, testdata2, 4);
    memcpy(key + 8, testdata1, 4);
    memcpy(key + 12, testdata2, 4);
    memcpy(key + 16, testdata1, 4);
    memcpy(key + 20, testdata2, 4);
    memcpy(key + 24, testdata1, 4);
    memcpy(key + 28, testdata2, 4);
    NRF_LOG_DEBUG("key:");
    NRF_LOG_HEXDUMP_DEBUG(key, 32);

    NRF_LOG_INFO("Testing encryption");
    NRF_LOG_DEBUG("init");
    crypto_aes256_init(key, nonce, NRF_CRYPTO_ENCRYPT);
    NRF_LOG_DEBUG("op");
    crypto_aes256_op(data, &datalen);
    NRF_LOG_DEBUG("datalen:%d", datalen);
    NRF_LOG_DEBUG("encrypted data:");
    NRF_LOG_HEXDUMP_DEBUG(data, datalen);
    NRF_LOG_DEBUG("uninit");
    crypto_aes256_uninit();

    NRF_LOG_INFO("Testing decryption");
    NRF_LOG_DEBUG("nonce: ");
    NRF_LOG_HEXDUMP_DEBUG(nonce, 16);
    NRF_LOG_DEBUG("key:");
    NRF_LOG_HEXDUMP_DEBUG(key, 32);
    NRF_LOG_DEBUG("init");
    crypto_aes256_init(key, nonce, NRF_CRYPTO_DECRYPT);
    NRF_LOG_DEBUG("op");
    crypto_aes256_op(data, &datalen);
    NRF_LOG_DEBUG("datalen:%d", datalen);
    NRF_LOG_DEBUG("decrypted data:");
    NRF_LOG_HEXDUMP_DEBUG(data, datalen);
    NRF_LOG_DEBUG("uninit");
    crypto_aes256_uninit();

    NRF_LOG_INFO("AES testing done");
}

void test_ecc(void)
{
    uint8_t private_key[NRF_CRYPTO_ECC_SECP256R1_RAW_PRIVATE_KEY_SIZE];
    size_t  priv_size = sizeof(private_key);
    uint8_t public_key[NRF_CRYPTO_ECC_SECP256R1_RAW_PUBLIC_KEY_SIZE];
    size_t  pub_size = sizeof(public_key);
    uint8_t data[32] = "THIS IS A TEST STING, A BAD ONE.";
    uint8_t signature[NRF_CRYPTO_ECDSA_SECP256R1_SIGNATURE_SIZE];
    size_t  sigsize = sizeof(signature);
    uint8_t hash[32];
    size_t  hashsize = 32;

    NRF_LOG_INFO("testing uecc");

    NRF_LOG_INFO("make key pair");
    crypto_ecc256_make_key_pair(public_key, &pub_size, private_key, &priv_size);
    NRF_LOG_DEBUG("public key");
    NRF_LOG_HEXDUMP_DEBUG(
        public_key,
        NRF_CRYPTO_ECC_SECP256R1_RAW_PUBLIC_KEY_SIZE);

    NRF_LOG_DEBUG("private key");
    NRF_LOG_HEXDUMP_DEBUG(
        private_key,
        NRF_CRYPTO_ECC_SECP256R1_RAW_PRIVATE_KEY_SIZE);

    NRF_LOG_INFO("load pair");
    crypto_ecc256_load_raw_key(private_key, priv_size);

    NRF_LOG_INFO("sign");
    crypto_sha256_init();
    crypto_sha256_update(data, 32);
    crypto_sha256_final(hash);

    crypto_ecc256_sign(hash, hashsize, signature, &sigsize);

    NRF_LOG_DEBUG("signature:");
    NRF_LOG_HEXDUMP_DEBUG(signature, sigsize);

    NRF_LOG_INFO("finished testing uecc");
}


void test_crypto(void)
{
    test_hash();
    test_hmac();
    test_random();
    test_aes();
    test_ecc();
}
