#include <errno.h>
#include "thread.h"
#include "scheduler.h"
#include <string.h>

extern "C" int *__errno()
{
    return &GetCurrentThreadForCore()->thread_errno;
}

static const char _strerror[] = "strerror not implemented";

extern "C" char *strerror(int errnum)
{
    return (char *)_strerror;
}

extern "C" char * __xpg_strerror_r(int errnum, char * buf, size_t buflen)
{
    strncpy(buf, strerror(errnum), buflen);
    return buf;
}
