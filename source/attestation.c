
#include "attestation.h"
#include <stdint.h>
#include <string.h>

void get_aaguid(uint8_t *dst)
{
    uint8_t * aaguid = (uint8_t *)"\xeb\x77\x58\x68\x9c\x77\x33\x7e\xec\x38\xeb\x0c\x6d\xa9\x35\x55";
    memcpy(dst, aaguid, 16);
}

uint8_t * get_attestation_key()
{
    return attestation_private_key;
}

void attestation_read_cert_der(uint8_t * dst)
{
    memcpy(dst, attestation_cert, attestation_cert_size); 
}

uint16_t attestation_cert_der_get_size()
{
    return (uint16_t) attestation_cert_size;
}

