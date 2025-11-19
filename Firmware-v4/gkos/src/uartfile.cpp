#include "osfile.h"
#include "logger.h"
#include "thread.h"
#include "process.h"
#include <cstring>
#include <fcntl.h>

UARTFile::UARTFile(bool for_read, bool for_write)
{
    can_read = for_read;
    can_write = for_write;
}

ssize_t UARTFile::Write(const char *buf, size_t count, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    klog("%s:%s: %.*s", t->name.c_str(), t->p->name.c_str(),
        buf, count);
    return count;
}

ssize_t UARTFile::Read(char *buf, size_t count, int *_errno)
{
    // don't read from UART
    *_errno = EBADF;
    return -1;
}

int UARTFile::Fstat(struct stat *buf, int *_errno)
{
    memset(buf, 0, sizeof(struct stat));
    return 0;    
}

off_t UARTFile::Lseek(off_t offset, int whence, int *_errno)
{
    *_errno = EINVAL;
    return -1;
}

int UARTFile::Isattty(int *_errno)
{
    return 1;
}
