#ifndef EXT4_THREAD_H
#define EXT4_THREAD_H

#include "osmutex.h"
#include "osfile.h"
#include "syscalls.h"
#include "thread.h"
#include "ext4.h"
#include "_sys_dirent.h"
#include "process.h"
#include "ostypes.h"

int gk_ext4_mkdir(const char *pathname, int mode, int *_errno);
int gk_ext4_open(const char *pathname, int flags, int mode, int f, int *_errno);
int gk_ext4_read(ext4_file &e4f, char *buf, int nbytes, int *_errno);
int gk_ext4_write(ext4_file &e4f, const char *buf, int nbytes, int *_errno);
int gk_ext4_lseek(ext4_file &e4f, off_t offset, int whence, int *_errno);
int gk_ext4_ftruncate(ext4_file &e4f, off_t length, int *_errno);
int gk_ext4_fstat(ext4_file &e4f, ext4_dir &e4d, bool is_dir, struct stat *st, const char *pathname, int *_errno);
int gk_ext4_close(ext4_file &e4f, int *_errno);
int gk_ext4_readdir(ext4_dir &e4d, struct dirent *de, int *_errno);
int gk_ext4_unlink(const char *pathname, int *_errno);
int gk_ext4_unmount(int *_errno);
int gk_ext4_link(const char *oldpath, const char *newpath, int *_errno);

class LwextFile : public File
{
    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);
        ssize_t AbsRead(const char *buf, size_t count, size_t offset, int *_errno);
        ssize_t AbsWrite(const char *buf, size_t count, size_t offset, int *_errno);
        int ReadDir(dirent *de, int *_errno);

        int Fstat(struct stat *buf, int *_errno);
        off_t Lseek(off_t offset, int whence, int *_errno);
        int Ftruncate(off_t length, int *_errno);

        int Close(int *_errno);

        LwextFile(ext4_file fildes, std::string fname);
        ext4_file f;
        ext4_dir d;

        bool is_dir = false;
        std::string fname;
};

#endif
