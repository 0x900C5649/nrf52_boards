#ifndef _UTIL_H
#define _UTIL_H

//default includes
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// see nrf_sdk ./components/libraries/util/nordic_common.h (TODO licence)
#define MODULE_ENABLED( module ) \
    ((defined(module ## _ENABLED) && (module ## _ENABLED)) ? 1 : 0)


typedef uint8_t retvalue;

#endif // _util_h
