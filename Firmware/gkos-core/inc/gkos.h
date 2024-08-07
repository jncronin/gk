#ifndef GKOS_H
#define GKOS_H

/* Define GKOS_CODE_CORE_MASK to be a bitmask of cores that may run this code.
    For STM32H747:
        GKOS_CODE_CORE_MASK=1        M7
        GKOS_CODE_CORE_MASK=2        M4
        GKOS_CODE_CORE_MASK=3        either
 */
#ifndef GKOS_CODE_CORE_MASK
#define GKOS_CODE_CORE_MASK 3
#endif

/* Define GKOS_CODE_CORE_NAME to be a symbolic name to append to functions that may run this code. */

/* Interpret the above for STM32H747 */
#if (GKOS_CODE_CORE_MASK & 0x3) == 0x3
    #ifndef GKOS_CODE_CORE_NAME
        #define GKOS_CODE_CORE_NAME generic
    #endif
    #define CORE_CM7
    #define IS_GENERIC  1
    #define IS_CM7_ONLY 0
    #define IS_CM4_ONLY 0
#elif (GKOS_CODE_CORE_MASK == 0x1)
    #ifndef GKOS_CODE_CORE_NAME
        #define GKOS_CODE_CORE_NAME m7
    #endif
    #define CORE_CM7
    #define IS_GENERIC  0
    #define IS_CM7_ONLY 1
    #define IS_CM4_ONLY 0
#elif (GKOS_CODE_CORE_MASK == 0x2)
    #ifndef GKOS_CODE_CORE_NAME
        #define GKOS_CODE_CORE_NAME m4
    #endif
    #define CORE_M4
    #define IS_GENERIC  0
    #define IS_CM7_ONLY 0
    #define IS_CM4_ONLY 1
#else
    #error GKOS_CODE_CORE_MASK value is not supported
#endif

#define CAT_I(a,b) a##_##b
#define CAT(a,b) CAT_I(a, b)

#define GKOS_FUNC(fname) CAT(fname, GKOS_CODE_CORE_NAME)

#include "process.h"
#include "_gk_proccreate.h"
#include "osmutex.h"
#include <string>

/* The following functions need defining by the non-core OS */
int gkos_noncore_set_process_defaults(Process *proc, const proccreate_t *pct);
int gkos_noncore_handle_syscall(syscall_no sno, void *r1, void *r2, void *r3);
int gkos_noncore_handle_open(const std::string &name, int flags, int mode,
    Process &p, int fd, SimpleSignal &ss, WaitSimpleSignal_params &ss_p, int *_errno);


#endif
