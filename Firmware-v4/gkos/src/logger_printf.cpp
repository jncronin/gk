#include <cstdarg>
#include "logger.h"
#include <ctime>
#include <cstdint>
#include <cstring>

/* Simple printf implementation that does not use floating point registers or malloc (newlib's does) */


static int logger_string(const char *str, size_t len = ~0ULL);
static int logger_int(int64_t ival, uint64_t uval, bool is_signed, unsigned int base, int width, int precision, int length, bool zeropad,
    bool capitals);

#define INVALID() do { ret += logger_string("<INVALID FORMAT>"); return ret; } while(0)

int logger_printf(const timespec &now, const char *format, va_list va)
{
    int ret = 0;

    ret += logger_string("[", 1);
    ret += logger_int(now.tv_sec, 0, true, 10, -1, -1, 8, false, false);
    ret += logger_string(".", 1);
    ret += logger_int(now.tv_nsec, 0, true, 10, 9, -1, 8, true, false);
    ret += logger_string("]: ", 3);

    while(*format)
    {
        if(*format != '%')
        {
            ret += logger_string(format, 1);
            format++;
            continue;
        }

        format++;
        if(!*format)
            INVALID();
        
        if(*format == '%')
        {
            ret += logger_string("%", 1);
            format++;
            continue;
        }
        
        bool has_width = false;
        bool has_precision = false;
        bool zeropad = false;

        if(*format == '0')
        {
            zeropad = true;

            format++;
            if(!*format)
                INVALID();
        }

        // width?
        int width = 0;
        int precision = 0;
        while(*format >= '0' && *format <= '9')
        {
            int cur_digit = *format - '0';
            width *= 10;
            width += cur_digit;
            has_width = true;

            format++;
            if(!*format)
                INVALID();
        }

        if(*format == '.')
        {
            format++;
            if(!*format)
                INVALID();
            
            while(*format >= '0' && *format <= '9')
            {
                int cur_digit = *format - '0';
                precision *= 10;
                precision += cur_digit;
                has_precision = true;

                format++;
                if(!*format)
                    INVALID();
            }
        }

        // length
        int length = 8;
        if(*format == 'l')
        {
            length = 4;
            format++;
            if(!*format)
                INVALID();
            
            if(*format == 'l')
            {
                length = 8;
                format++;
            }
        }
        else if(*format == 'h')
        {
            length = 2;
            format++;
        }
        else if(*format == 'j' || *format == 't' || *format == 'z')
        {
            length = 8;
            format++;
        }

        if(!*format)
            INVALID();
        
        if(*format == 's')
        {
            ret += logger_string(va_arg(va, const char *));
        }
        else if(*format == 'd')
        {
            ret += logger_int(va_arg(va, int64_t), 0, true, 10,
                has_width ? width : -1,
                has_precision ? precision : -1,
                length, zeropad, false);
        }
        else if(*format == 'x' || *format == 'X')
        {
            ret += logger_int(0, va_arg(va, uint64_t), false, 16,
                has_width ? width : -1,
                has_precision ? precision : -1,
                length, zeropad, *format == 'X');
        }
        else if(*format == 'u')
        {
            ret += logger_int(0, va_arg(va, uint64_t), false, 10,
                has_width ? width : -1,
                has_precision ? precision : -1,
                length, zeropad, false);
        }
        else
        {
            INVALID();
        }

        format++;
    }

    return ret;
}

int logger_string(const char *str, size_t len)
{
    if(len == ~0ULL)
        len = strlen(str);
    return log_fwrite(str, len);
}

int logger_int(int64_t ival, uint64_t uval, bool is_signed, unsigned int base, int width, int precision, int length, bool zeropad, bool capitals)
{
    // is it negative? in which case flip into a positive value in uval
    bool is_negative = false;
    if(is_signed)
    {
        if(length == 8)
        {
            if(ival < 0)
            {
                is_negative = true;
                uval = (uint64_t)-ival;
            }
            else
            {
                uval = (uint64_t)ival;
            }
        }
        else if(length == 4)
        {
            int32_t ival4 = (int32_t)ival;
            if(ival4 < 0)
            {
                is_negative = true;
                uval = (uint64_t)-ival4;
            }
            else
            {
                uval = (uint64_t)ival4;
            }
        }
        else if(length == 2)
        {
            int16_t ival2 = (int16_t)ival;
            if(ival2 < 0)
            {
                is_negative = true;
                uval = (uint64_t)-ival2;
            }
            else
            {
                uval = (uint64_t)ival2;
            }
        }
        else if(length == 1)
        {
            int8_t ival1 = (int8_t)ival;
            if(ival1 < 0)
            {
                is_negative = true;
                uval = (uint64_t)-ival1;
            }
            else
            {
                uval = (uint64_t)ival1;
            }
        }
    }
    else
    {
        if(length < 8)
        {
            // mask out upper bits
            uval &= ~(~0ULL << (8 * length));
        }
    }

    char buf[32] = { 0 };

    char *ptr = &buf[sizeof(buf) - 1];

    while(uval != 0 || width > 0)
    {
        if(uval == 0)
        {
            if(zeropad)
            {
                *--ptr = '0';
            }
            else
            {
                *--ptr = ' ';
            }
        }
        else
        {
            auto cur_digit = uval % (uint64_t)base;
            uval /= (uint64_t)base;

            if(cur_digit < 10)
            {
                *--ptr = '0' + cur_digit;
            }
            else if(capitals)
            {
                *--ptr = 'A' + (cur_digit - 10);
            }
            else
            {
                *--ptr = 'a' + (cur_digit - 10);
            }
        }
        width--;

        // don't handle precision
    }

    if(is_negative)
        *--ptr = '-';
    
    return logger_string(ptr);
}
