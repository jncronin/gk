/**
 * Copyright (c) 2023 John Cronin
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#ifndef CC_H
#define CC_H

#include <sys/time.h>
#include <stdlib.h>
#define LWIP_TIMEVAL_PRIVATE    0

typedef uint32_t                sys_prot_t;

#define LWIP_RAND() ((u32_t)rand())

#endif
