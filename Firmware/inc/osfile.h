#ifndef OSFILE_H
#define OSFILE_H

#include <unistd.h>
#include <ext4.h>
#include <string>
#include "ff.h"

enum FileType
{
    FT_Unknown = 0,
    FT_SeggerRTT,
    FT_Lwext,
    FT_Socket
};

class File
{
    protected:
        FileType type;

    public:
        virtual ssize_t Write(const char *buf, size_t count, int *_errno) = 0;
        virtual ssize_t Read(char *buf, size_t count, int *_errno) = 0;

        virtual int Fstat(struct stat *buf, int *_errno);
        virtual off_t Lseek(off_t offset, int whence, int *_errno);

        virtual int Isatty(int *_errno);
        virtual int Close(int *_errno);
        virtual int Close2(int *_errno);

        virtual int Bind(void *addr, unsigned int addrlen, int *_errno);
        virtual int Listen(int backlog, int *_errno);
        virtual int Accept(void *addr, unsigned int *addrlen, int *_errno);

        FileType GetType() const;    // support type checking without rtti

        virtual ~File() = default;
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

class USBTTYFile : public File
{
    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);

        int Fstat(struct stat *buf, int *_errno);
        off_t Lseek(off_t offset, int whence, int *_errno);

        int Isattty(int *_errno);
};

class LwextFile : public File
{
    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);

        int Fstat(struct stat *buf, int *_errno);
        off_t Lseek(off_t offset, int whence, int *_errno);

        int Close(int *_errno);

        LwextFile(ext4_file fildes, std::string fname);
        ext4_file f;
        ext4_dir d;

        bool is_dir = false;
        std::string fname;
};

class FatfsFile : public File
{
    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);

        int Fstat(struct stat *buf, int *_errno);
        off_t Lseek(off_t offset, int whence, int *_errno);

        int Close(int *_errno);

        FatfsFile(FIL *file, std::string fname);
        FIL *f;
        std::string fname;
};

// TODO: pipe

// socket
class Socket;       // defined in osnet
class SocketFile : public File
{
    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);

        //int Fstat(struct stat *buf, int *_errno);
        //off_t Lseek(off_t offset, int whence, int *_errno);

        int Bind(void *addr, unsigned int addrlen, int *_errno);
        virtual int Listen(int backlog, int *_errno);
        virtual int Accept(void *addr, unsigned int *addrlen, int *_errno);

        int Close(int *_errno);
        int Close2(int *_errno);

        SocketFile(Socket *sck);

        Socket *sck;
};

#endif
