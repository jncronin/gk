#ifndef LOGGER_H
#define LOGGER_H

// Avoid including unistd.h here because it defines unlink() which dlmalloc tries to redefine
typedef signed int ssize_t;
typedef unsigned int size_t;


#ifdef __cplusplus
extern "C" {
#endif

int GKOS_FUNC(klog)(const char *format, ...);
int GKOS_FUNC(init_log)();
ssize_t GKOS_FUNC(log_fwrite)(const void *buf, size_t count);
void GKOS_FUNC(log_freeze_persistent_log)();
void GKOS_FUNC(log_unfreeze_persistent_log)();

#ifdef __cplusplus
#include "memblk_types.h"
MemRegion GKOS_FUNC(log_get_persistent)();
#endif

#ifdef __cplusplus
}
#endif

#endif
