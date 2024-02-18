#include "osfile.h"

int File::Isatty(int *_errno)
{
    *_errno = ENOTTY;
    return 0;
}

File::~File()
{

}
