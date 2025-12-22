#include <cstdarg>
#include "logger.h"
#include <ctime>
#include <cstdint>
#include <cstring>
#include <limits>

const constexpr auto size_t_max = std::numeric_limits<size_t>::max();

/* Simple printf implementation that does not use floating point registers or malloc (newlib's does) */
static int logger_string(logger_printf_outputter &oput, const char *str, size_t len = size_t_max);
static int logger_int(logger_printf_outputter &oput, int64_t ival, uint64_t uval, bool is_signed, unsigned int base, int width, int precision, int length, bool zeropad,
    bool capitals);

// log_fwrite output class
class log_fwrite_output : public logger_printf_outputter
{
    public:
        ssize_t output(const void *buf, size_t n)
        {
            return log_fwrite(buf, n);
        }
};


#define INVALID() do { ret += logger_string(oput, "<INVALID FORMAT>"); return ret; } while(0)

int logger_printf(const timespec &now, const char *format, va_list va)
{
    int ret = 0;

    unsigned int core_id;
    __asm__ volatile("mrs %[core_id], mpidr_el1\n" : [core_id] "=r" (core_id) :: "memory");
    core_id &= 0xf;

    log_fwrite_output fwo;
    ret += logger_string(fwo, "[C", 2);
    ret += logger_int(fwo, 0, core_id, false, 10, -1, -1, 4, false, false);
    ret += logger_string(fwo, ",", 1);
    ret += logger_int(fwo, now.tv_sec, 0, true, 10, -1, -1, 8, false, false);
    ret += logger_string(fwo, ".", 1);
    ret += logger_int(fwo, now.tv_nsec, 0, true, 10, 9, -1, 8, true, false);
    ret += logger_string(fwo, "]: ", 3);

    return ret + logger_vprintf(fwo, format, va);
}

int logger_vprintf(logger_printf_outputter &oput, const char *format, va_list va)
{
    int ret = 0;
    while(*format)
    {
        if(*format != '%')
        {
            ret += logger_string(oput, format, 1);
            format++;
            continue;
        }

        format++;
        if(!*format)
            INVALID();
        
        if(*format == '%')
        {
            ret += logger_string(oput, "%", 1);
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
            
            if(*format == '*')
            {
                precision = -1;
                has_precision = true;

                format++;
                if(!*format)
                    INVALID();
            }
            else
            {
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
        }

        // length
        int length = 4;
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
            auto str = va_arg(va, const char *);
            if(has_precision)
            {
                auto prec = (precision >= 0) ? precision : (int)va_arg(va, size_t);
                ret += logger_string(oput, str, prec);
            }
            else
            {
                ret += logger_string(oput, str);
            }
        }
        else if(*format == 'd')
        {
            ret += logger_int(oput, va_arg(va, int64_t), 0, true, 10,
                has_width ? width : -1,
                has_precision ? precision : -1,
                length, zeropad, false);
        }
        else if(*format == 'x' || *format == 'X')
        {
            ret += logger_int(oput, 0, va_arg(va, uint64_t), false, 16,
                has_width ? width : -1,
                has_precision ? precision : -1,
                length, zeropad, *format == 'X');
        }
        else if(*format == 'p' || *format == 'P')
        {
            ret += logger_string(oput, "0x", 2) +
                logger_int(oput, 0, va_arg(va, uintptr_t), false, 16,
                    has_width ? width : -1,
                    has_precision ? precision : -1,
                    sizeof(uintptr_t), zeropad, *format == 'P'); 
        }
        else if(*format == 'u')
        {
            ret += logger_int(oput, 0, va_arg(va, uint64_t), false, 10,
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

int logger_string(logger_printf_outputter &oput, const char *str, size_t len)
{
    if(len == ~0ULL)
        len = strlen(str);
#if GK_FLASH_LOADER
    return log_fwrite(str, len);
#else
    return oput.output(str, len);
#endif
}

int logger_int(logger_printf_outputter &oput, 
    int64_t ival, uint64_t uval, bool is_signed, unsigned int base,
    int width, int precision, int length, bool zeropad, bool capitals)
{
    // handle zero with no width
    if(width == -1 && ((is_signed && (ival == 0)) || (!is_signed && (uval == 0))))
    {
        return logger_string(oput, "0", 1);
    }
    
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

    auto start_width = width;

    while(uval != 0 || width > 0)
    {
        if(uval == 0)
        {
            if(zeropad || width == start_width)
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
    
    return logger_string(oput, ptr);
}
