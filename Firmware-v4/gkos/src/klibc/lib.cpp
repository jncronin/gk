#include "logger.h"

void *__dso_handle = (void *)&__dso_handle;

extern "C" int __cxa_atexit(void (*) (void *), void *, void *)
{
    return 0;
}

extern "C" void abort()
{
    klog("abort called\n");
    __asm__ volatile ("svc #255\n" ::: "memory");
    while(true);
}

extern "C" void __assert_func (const char *file, int line, const char *func, const char *str)
{
    klog("assertation failed %s: %s line %d: %s\n",
        file, func, line, str);
    __asm__ volatile ("svc #254\n" ::: "memory");
    while(true);
}

extern "C" char *getenv(const char *name)
{
    return nullptr;
}
