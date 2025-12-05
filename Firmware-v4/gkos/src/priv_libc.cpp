/* Ideally use syscall_ functions wherever possible as it provides thread-safe errno handling

    These functions are provided to help external libraries (e.g. zlib) which rely on the
    default libc functions.
*/

#include "syscalls_int.h"
#include <unistd.h>

extern "C" {
int _open(const char *pathname, int flags, mode_t mode)
{
    return -1;
}

int _close(int file)
{
    int ret = syscall_close1(file, &errno);
    if(ret != 0)
        return ret;
    return syscall_close2(file, &errno);
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
    return syscall_lseek(file, offset, whence, &errno);
}

int _read(int file, char *ptr, int len)
{
    return syscall_read(file, ptr, len, &errno);
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
