# TODOs

* initialize nrf crypto module (https://infocenter.nordicsemi.com/index.jsp?topic=%2Fsdk_nrf5_v16.0.0%2Flib_crypto_hash.html)
    ```
    Example main-function 
    ret_code_t      err_code;
    // Initialize nrf_crypto
    err_code = nrf_crypto_init();
    VERIFY_SUCCESS(err_code);
    // The nrf_crypto_hash API is now ready to be used.
    ```
    Call crypto_init in main
* empty out hash contexts after finalize in crypto.c
* use nrf_crypto hmac (crypto.c)
* enable crypto backend support in sdk_config.h
  * sha 256/512
  * ecc (curve(s)?)
  * hmac?
