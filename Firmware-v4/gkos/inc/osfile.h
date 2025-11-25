#ifndef OSFILE_H
#define OSFILE_H

#include <unistd.h>
//#include <ext4.h>
#include <string>
#include <memory>
//#include "ff.h"
#include "_sys_dirent.h"

enum FileType
{
    FT_Unknown = 0,
    FT_SeggerRTT,
    FT_Lwext,
    FT_Socket,
    FT_Pipe,
    FT_Ram
};

class File
{
    protected:
        FileType type;

    public:
        virtual ssize_t Write(const char *buf, size_t count, int *_errno) = 0;
        virtual ssize_t Read(char *buf, size_t count, int *_errno) = 0;
        virtual int ReadDir(dirent *de, int *_errno);

        virtual int Fstat(struct stat *buf, int *_errno);
        virtual off_t Lseek(off_t offset, int whence, int *_errno);
        virtual int Ftruncate(off_t length, int *_errno);

        virtual int Isatty(int *_errno);
        virtual int Close(int *_errno);
        virtual int Close2(int *_errno);

        virtual int Bind(void *addr, unsigned int addrlen, int *_errno);
        virtual int Listen(int backlog, int *_errno);
        virtual int Accept(void *addr, unsigned int *addrlen, int *_errno);

        FileType GetType() const;    // support type checking without rtti

        uint32_t opts = 0;

        virtual ~File() noexcept = default;
};

using PFile = std::shared_ptr<File>;
using WPFile = std::weak_ptr<File>;

class UARTFile : public File
{
    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);

        int Fstat(struct stat *buf, int *_errno);
        off_t Lseek(off_t offset, int whence, int *_errno);

        int Isattty(int *_errno);
        
        UARTFile(bool for_read, bool for_write);
        
        virtual ~UARTFile() = default;

    protected:
        bool can_read;
        bool can_write;
};

#if 0
class USBTTYFile : public File
{
    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);

        int Fstat(struct stat *buf, int *_errno);
        off_t Lseek(off_t offset, int whence, int *_errno);

        int Isattty(int *_errno);
};
#endif

/*
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
*/

#endif
