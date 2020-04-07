#ifndef _UTIL_H
#define _UTIL_H

//default includes
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// see nrf_sdk ./components/libraries/util/nordic_common.h (TODO licence)
#define APP_MODULE_ENABLED( module ) \
    ((defined(APP_ ## module ## _ENABLED) && (module ## _ENABLED)) ? 1 : 0)

#endif // _util_h
