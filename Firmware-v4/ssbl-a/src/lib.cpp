#include <cstdint>
#include <cstddef>
#include <sys/types.h>
#include "logger.h"

extern int _heap_start;
extern int _heap_end;

extern "C" void *_sbrk(intptr_t increment)
{
    static uintptr_t cur_brk = 0;

    if(cur_brk == 0)
    {
        cur_brk = (uintptr_t)&_heap_start;
    }

    if(increment == 0)
    {
        return (void *)cur_brk;
    }
    else if(increment > 0)
    {
        uintptr_t to_increment = (uintptr_t)increment;
        if(to_increment > ((uintptr_t)_heap_end - cur_brk))
        {
            return (void *)-1;
        }
        auto ret = cur_brk;
        cur_brk += increment;
        return (void *)ret;
    }
    else
    {
        uintptr_t to_decrement = (uintptr_t)-increment;
        if(to_decrement > (cur_brk - (uintptr_t)_heap_end))
        {
            return (void *)-1;
        }
        auto ret = cur_brk;
        cur_brk -= to_decrement;
        return (void *)ret;
    }
}

extern "C" ssize_t _write(int fd, const void *buf, size_t count)
{
    return -1;
}

extern "C"
int _open(const char *pathname, int flags, mode_t mode)
{
    return -1;
}

extern "C"
int _close(int file)
{
    return -1;
}

extern "C"
int _fstat(int file, void *st)
{
    return -1;
}

extern "C"
int _getpid()
{
    return -1;
}

extern "C"
int _isatty(int file)
{
    return -1;
}

extern "C"
int _kill(int pid, int sig)
{
    return -1;
}

extern "C"
int _lseek(int file, int offset, int whence)
{
    return -1;
}

extern "C"
int _read(int file, char *ptr, int len)
{
    return -1;
}

/* Provide versions of memcpy/memmove that do not result in unaligned accesses - without
    the MMU enabled all memory is of type device and therefore cannot be unaligned */
extern "C" void *__wrap_memcpy(void *dest, const void *src, size_t n)
{
    // forwards copy
    char *d = (char *)dest;
    const char *s = (const char *)src;

    while(n--)
    {
        *d++ = *s++;
    }

    return dest;
}

extern "C" void *__wrap_memmove(void *dest, const void *src, size_t n)
{
    if(dest > src)
        return __wrap_memcpy(dest, src, n);
    
    // backwards copy
    char *d = (char *)dest;
    const char *s = (const char *)src;
    while(n--)
    {
        d[n] = s[n];
    }

    return dest;
}

extern "C" void *__wrap_memset(void *dest, int c, size_t n)
{
    char *d = (char *)dest;
    while(n--)
        *d++ = (char)c;
    return dest;
}

extern "C" size_t __wrap_strlen(const char *s)
{
    size_t ret = 0;
    while(*s++)
        ret++;
    return ret;
}
