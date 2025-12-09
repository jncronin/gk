#include <stdio.h>
#include "logger.h"
#include <stdarg.h>
#include <string.h>

class string_outputter : public logger_printf_outputter
{
    public:
        char *dest;
        bool has_limit = false;
        size_t nleft = 0;

        ssize_t output(const void *buf, size_t n)
        {
            // only copy n_write, but return n (sprintf etc returns how many bytes would be needed)
            auto n_write = n;

            if(has_limit && n_write > nleft)
                n_write = nleft;

            if(!n_write)
                return n;

            memcpy(dest, buf, n_write);
            dest += n_write;

            if(has_limit)
                nleft -= n_write;

            return n;
        }
};

extern "C" int sprintf(char *str, const char *format, ...)
{
    string_outputter so;
    so.dest = str;
    so.has_limit = false;

    va_list args;
    va_start(args, format);

    auto ret = logger_vprintf(so, format, args);

    va_end(args);

    *str = '0';

    return ret;
}

extern "C" int snprintf(char *str, size_t n, const char *format, ...)
{
    string_outputter so;
    so.dest = str;
    so.has_limit = true;
    so.nleft = n;

    va_list args;
    va_start(args, format);

    auto ret = logger_vprintf(so, format, args);

    va_end(args);

    if(so.nleft)
        *str = '0';

    return ret;
}

extern "C" int vsnprintf(char *str, size_t n, const char *format, va_list va)
{
    string_outputter so;
    so.dest = str;
    so.has_limit = true;
    so.nleft = n;

    auto ret = logger_vprintf(so, format, va);

    if(so.nleft)
        *str = '0';

    return ret;
}
