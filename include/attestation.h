#ifndef ATTESTATION_H
#define ATTESTATION_H

#include <stdint.h>
/***************************************************************************** 
*							GLOBALS
*****************************************************************************/
extern uint8_t       attestation_private_key[];
extern uint8_t       attestation_private_key_size;
extern const uint8_t attestation_cert[];
extern uint16_t      attestation_cert_size;


/***************************************************************************** 
*							FUNCITONS
*****************************************************************************/

/** Read the device's 16 byte AAGUID into a buffer.
 * @param dst buffer to write 16 byte AAGUID into.
 * */
void get_aaguid(uint8_t* dst);

/** Return pointer to attestation key.
 * @return pointer to attestation private key, raw encoded.  For P256, this is 32 bytes.
*/
uint8_t* get_attestation_key();

/** Read the device's attestation certificate into buffer @dst.
 * @param dst the destination to write the certificate.
 * 
 * The size of the certificate can be retrieved using `device_attestation_cert_der_get_size()`.
*/
void attestation_read_cert_der(uint8_t* dst);

/** Returns the size in bytes of attestation_cert_der.
 * @return number of bytes in attestation_cert_der, not including any C string null byte.
*/
uint16_t attestation_cert_der_get_size();

#endif /* end of include guard: ATTESTATION_H */
