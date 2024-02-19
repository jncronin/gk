#include "osfile.h"

int File::Isatty(int *_errno)
{
    *_errno = ENOTTY;
    return 0;
}

int File::Close(int *_errno)
{
    (void)_errno;
    return 0;
}
