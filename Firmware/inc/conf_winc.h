/**
 * Copyright (c) 2023 John Cronin
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef CONF_WINC_H
#define CONF_WINC_H

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
#endif
int rtt_printf_wrapper(const char *format, ...);

#define CONF_WINC_DEBUG         1
#define CONF_WINC_PRINTF        rtt_printf_wrapper
#define CONF_WINC_USE_SPI       1
#define CONF_WINC_SPI_CLOCK     48000000
#define NM_EDGE_INTERRUPT       1
#define ETH_MODE                1

#ifndef __cplusplus
#define max(a,b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b;       \
})

#define min(a,b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b;       \
})
#endif

#endif
