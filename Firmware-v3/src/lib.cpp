/* The following are to avoid linker warnings */

#include "syscalls_int.h"
#include "logger.h"
#include <stdio.h>
#include <unistd.h>

extern "C" {
int _open(const char *pathname, int flags, mode_t mode)
{
    return -1;
}

int _close(int file)
{
    BKPT();

#if 0
    int ret = syscall_close1(file, &errno);
    if(ret != 0)
        return ret;
    return syscall_close2(file, &errno);
#endif
    return 0;
}

int _fstat(int file, void *st)
{
    return -1;
}

int _getpid()
{
    return -1;
}

int _isatty(int file)
{
    return -1;
}

int _kill(int pid, int sig)
{
    return -1;
}

int _lseek(int file, int offset, int whence)
{
    BKPT();
#if 0
    return syscall_lseek(file, offset, whence, &errno);
#endif
    return 0;
}

int _read(int file, char *ptr, int len)
{
    BKPT();
#if 0
    return syscall_read(file, ptr, len, &errno);
#endif
    return 0;
}

int _write(int file, char *buf, int nbytes)
{
    if(file == STDERR_FILENO)
    {
        return log_fwrite(buf, nbytes);
    }
    return -1;
}
}