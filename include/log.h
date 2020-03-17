#ifndef _LOG_H
#define _LOG_H

#include "nrf_log.h"
#include "util.h"
#include "nordic_common.h"


#if APP_MODULE_ENABLED(LOGGING)

#define printf1(tag,fmt, ...) NRF_LOG_INFO(fmt, ##__VA_ARGS__)
#define printf2(tag,fmt, ...) NRF_LOG_INFO(fmt, ##__VA_ARGS__)
#define printf3(tag,fmt, ...) NRF_LOG_INFO(fmt, ##__VA_ARGS__)

#define dump_hex1(tag,data,len) NRF_LOG_HEXDUMP_DEBUG(data,len)

#else

#define set_logging_mask(mask)
#define printf1(tag,fmt, ...)
#define printf2(tag,fmt, ...)
#define printf3(tag,fmt, ...)
#define dump_hex1(tag,data,len)
#define timestamp()

#endif

int init_log(void);


#endif // _LOG_H
