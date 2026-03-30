#include "osfile.h"

DRIFile::DRIFile()
{
    type = FT_DRI;
    klog("DRI: opened\n");
}

ssize_t DRIFile::Write(const char *, size_t, int *_errno)
{
    *_errno = EINVAL;
    return -1;
}

ssize_t DRIFile::Read(char *, size_t, int *_errno)
{
    *_errno = EINVAL;
    return -1;
}

int DRIFile::Ioctl(unsigned int nr, void *ptr, size_t len, int *_errno)
{
    klog("DRI: ioctl(%u) not supported\n", nr);
    *_errno = EINVAL;
    return -1;
}
