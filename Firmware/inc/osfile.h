#ifndef OSFILE_H
#define OSFILE_H

#include <unistd.h>
#include <ext4.h>
#include <string>

class File
{
    public:
        virtual ssize_t Write(const char *buf, size_t count, int *_errno) = 0;
        virtual ssize_t Read(char *buf, size_t count, int *_errno) = 0;

        virtual int Fstat(struct stat *buf, int *_errno) = 0;
        virtual off_t Lseek(off_t offset, int whence, int *_errno) = 0;

        virtual int Isatty(int *_errno);

        virtual ~File();
};

class SeggerRTTFile : public File
{
    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);

        int Fstat(struct stat *buf, int *_errno);
        off_t Lseek(off_t offset, int whence, int *_errno);

        int Isattty(int *_errno);
        
        SeggerRTTFile(unsigned int buf, bool for_read, bool for_write);
        

    protected:
        unsigned int buf_idx;
        bool can_read;
        bool can_write;
};

class LwextFile : public File
{
    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);

        int Fstat(struct stat *buf, int *_errno);
        off_t Lseek(off_t offset, int whence, int *_errno);

        ~LwextFile();
        LwextFile(ext4_file fildes, std::string fname);

    protected:
        ext4_file f;
        std::string fname;
};

// TODO: pipe

// TODO: socket


#endif
