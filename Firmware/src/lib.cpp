/* The following are to avoid linker warnings */

extern "C" {
int _close(int file)
{
    return -1;
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
    return -1;
}

int _read(int file, char *ptr, int len)
{
    return -1;
}

int _write(int file, char *buf, int nbytes)
{
    return -1;
}
}
