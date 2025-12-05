#ifndef FATFS_FILE_H
#define FATFS_FILE_H

#include <string>
#include <cstddef>
#include "osfile.h"
#include "ff.h"

int fatfs_open(const std::string &fname, PFile *f, bool for_read, bool for_write);

class FatFsFile : public File
{
    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);

        int Fstat(struct stat *buf, int *_errno);
        off_t Lseek(off_t offset, int whence, int *_errno);

        int Isattty(int *_errno);
        
        FatFsFile(FIL *fil, bool for_read, bool for_write);

        int Close(int *_errno);
        
        virtual ~FatFsFile() = default;

    protected:
        FIL f;
        bool can_read, can_write;
};

#endif
