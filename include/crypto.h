// Copyright 2019 SoloKeys Developers
//
// Licensed under the Apache License, Version 2.0, <LICENSE-APACHE or
// http://apache.org/licenses/LICENSE-2.0> or the MIT license <LICENSE-MIT or
// http://opensource.org/licenses/MIT>, at your option. This file may not be
// copied, modified, or distributed except according to those terms.
#ifndef _CRYPTO_H
#define _CRYPTO_H

#include <stddef.h>

#include "nrf_crypto.h"


/***************************************************************************** 
*							DEFINES
*****************************************************************************/

#define CRYPTO_TRANSPORT_KEY2               ((uint8_t*)2)
#define CRYPTO_TRANSPORT_KEY                ((uint8_t*)1)
#define CRYPTO_MASTER_KEY                   ((uint8_t*)0)

#define ECC_PUBLIC_KEY_SIZE                     NRF_CRYPTO_ECC_SECP256R1_RAW_PUBLIC_KEY_SIZE
#define ECC_PRIVATE_KEY_SIZE                    NRF_CRYPTO_ECC_SECP256R1_RAW_PRIVATE_KEY_SIZE

/***************************************************************************** 
*							INIT
*****************************************************************************/

//init crypto module
void crypto_init();


/***************************************************************************** 
*							HASH
*****************************************************************************/

void crypto_sha256_init();
void crypto_sha256_update(uint8_t * data, size_t len);
void crypto_sha256_update_secret();
void crypto_sha256_final(uint8_t * hash);

void crypto_sha512_init();
void crypto_sha512_update(const uint8_t * data, size_t len);
void crypto_sha512_final(uint8_t * hash);


/***************************************************************************** 
*							HMAC
*****************************************************************************/

void crypto_sha256_hmac_init(uint8_t * key, uint32_t klen);
void crypto_sha256_hmac_update(const uint8_t * data, size_t len);
void crypto_sha256_hmac_final(uint8_t * hmac, size_t * len);


/***************************************************************************** 
*							ECC
*****************************************************************************/

//void crypto_ecc256_init();
void crypto_ecc256_derive_public_key(uint8_t * data, int len, uint8_t * x, uint8_t * y);
void crypto_ecc256_compute_public_key(uint8_t * privkey, size_t privlen, uint8_t * pubkey, size_t *  publen);

void crypto_ecc256_load_key(uint8_t * data, int len, uint8_t * data2, int len2);
void crypto_ecc256_load_attestation_key();
void crypto_ecc256_load_raw_key(uint8_t * key, size_t len);
void crypto_ecc256_sign(uint8_t * data, size_t len, uint8_t * sig, size_t * siglen);

void generate_private_key(uint8_t * data, size_t len, uint8_t * data2, int len2, uint8_t * privkey, size_t * keylen);
void crypto_ecc256_make_key_pair(uint8_t * pubkey, size_t * publen, uint8_t * privkey, size_t * privlen);
void crypto_ecc256_shared_secret(const uint8_t * pubkey, size_t publen, const uint8_t * privkey, size_t privlen , uint8_t * shared_secret, size_t * secretlen);


/***************************************************************************** 
*							AES
*****************************************************************************/

void crypto_aes256_init(uint8_t * key, uint8_t * nonce, nrf_crypto_operation_t op);
void crypto_aes256_reset_iv(uint8_t * nonce);
void crypto_aes256_op(uint8_t * buf, size_t * length);
void crypto_aes256_uninit(void);

// buf length must be multiple of 16 bytes
//void crypto_aes256_decrypt(uint8_t * buf, int lenth);
//void crypto_aes256_encrypt(uint8_t * buf, int lenth);


/***************************************************************************** 
*							RNG
*****************************************************************************/

int crypto_generate_rng(uint8_t * dst, size_t num);


/***************************************************************************** 
*						MASTER SECRET
*****************************************************************************/

void crypto_reset_master_secret();
void crypto_load_master_secret(uint8_t * key);

#endif
