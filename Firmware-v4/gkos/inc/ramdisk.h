#ifndef RAMDISK_H
#define RAMDISK_H

#include "osfile.h"
#include <cstddef>

class RamdiskFile : public File
{
    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);

        int Fstat(struct stat *buf, int *_errno);
        off_t Lseek(off_t offset, int whence, int *_errno);

        int Isattty(int *_errno);
        
        RamdiskFile(void *buf, size_t len, bool for_read, bool for_write);
        
        virtual ~RamdiskFile() = default;

    protected:
        void *buf;
        size_t size;
        off_t offset;
        bool can_read, can_write;
};

int ramdisk_open(const std::string &fname, PFile *f, bool for_read, bool for_write);

#endif
