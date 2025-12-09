#include <stdlib.h>
#include "logger.h"

void unimpl()
{
    klog("unimplemented klibc function\n");
    __asm__ volatile("svc #253\n" ::: "memory");
    while(true);
}

extern "C" unsigned long int strtoul(const char *nptr, char **endptr, int base)
{
    unimpl();
    return 0;
}

extern "C" unsigned long long int strtoull(const char *nptr, char **endptr,
                                int base)
{
    unimpl();
    return 0;
}

extern "C" double strtod(const char *nptr, char **endptr)
{
    unimpl();
    return 0;
}

extern "C" float strtof(const char *nptr, char **endptr)
{
    unimpl();
    return 0;
}

extern "C" long double strtold(const char *nptr, char **endptr)
{
    unimpl();
    return 0;
}

extern "C" void qsort(void *base, size_t n, size_t size,
    int (*compar)(const void *, const void *))
{
    unimpl();
}

extern "C" size_t strftime(char *s, size_t max, const char *format, const struct tm *tm)
{
    unimpl();
    return 0;
}

extern "C" size_t wcsftime(wchar_t *s, size_t max, const wchar_t *format, const struct tm *tm)
{
    unimpl();
    return 0;
}
