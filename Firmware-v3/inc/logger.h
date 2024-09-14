#ifndef LOGGER_H
#define LOGGER_H

// Avoid including unistd.h here because it defines unlink() which dlmalloc tries to redefine
typedef signed int ssize_t;
typedef unsigned int size_t;


#ifdef __cplusplus
extern "C" {
#endif

int klog(const char *format, ...);
int init_log();
ssize_t log_fwrite(const void *buf, size_t count);
void log_freeze_persistent_log();
void log_unfreeze_persistent_log();

#ifdef __cplusplus
#include "memblk_types.h"
MemRegion log_get_persistent();
#endif

#ifdef __cplusplus
}
#endif

#endif
