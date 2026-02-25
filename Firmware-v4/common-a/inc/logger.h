#ifndef LOGGER_H
#define LOGGER_H

// Avoid including unistd.h here because it defines unlink() which dlmalloc tries to redefine
#if __SIZEOF_SIZE_T__ == 4
typedef signed int ssize_t;
typedef unsigned int size_t;
#else
typedef signed long int ssize_t;
typedef unsigned long int size_t;
#endif

#ifdef __cplusplus
class logger_printf_outputter
{
    public:
        virtual ssize_t output(const void *buf, size_t count) = 0;
};

#include <stdarg.h>
int logger_vprintf(logger_printf_outputter &oput, const char *format, va_list va);
extern "C" {
#endif

int klog(const char *format, ...);
int klogv(const char *format, va_list va);
ssize_t log_fwrite(const void *buf, size_t count);

#ifdef __cplusplus
}
#endif

#endif
