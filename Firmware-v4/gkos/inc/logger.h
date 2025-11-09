#ifndef LOGGER_H
#define LOGGER_H

// Avoid including unistd.h here because it defines unlink() which dlmalloc tries to redefine
typedef signed long int ssize_t;
typedef unsigned long int size_t;

#ifdef __cplusplus
extern "C" {
#endif

int klog(const char *format, ...);
ssize_t log_fwrite(const void *buf, size_t count);

#ifdef __cplusplus
}
#endif

#endif
