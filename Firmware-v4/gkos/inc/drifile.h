#ifndef DRIFILE_H
#define DRIFILE_H

#include <unistd.h>
//#include <ext4.h>
#include <string>
#include <memory>
//#include "ff.h"
#include "_sys_dirent.h"
#include <vector>
#include "osmutex.h"
#include "linux_types.h"


int dri_open(const std::string &fname, PFile *f, bool for_read, bool for_write);

class DRIFile : public File
{
    public:
        enum dri_type { dir, card, render };
        dri_type dt;
        std::string dev_name;
        unsigned int dir_iter = 0;  // re-use as device number for dri_type != dir
        std::shared_ptr<device> d;
        std::unique_ptr<drm_file> df;

    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);
        int ReadDir(dirent *de, int *_errno);
        int Fstat(struct stat *buf, int *_errno);

        int Ioctl(unsigned int id, void *ptr, size_t len, int *_errno);

        DRIFile();
};

int dri_mmap(size_t len, void **retaddr, int is_sync,
    int is_read, int is_write, int is_exec, DRIFile &fd, int is_fixed, size_t offset, int *_errno);


#endif
