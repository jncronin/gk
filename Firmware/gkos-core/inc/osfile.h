#ifndef OSFILE_H
#define OSFILE_H

#include <unistd.h>
#include <string>
#include "_sys_dirent.h"
#include "osmutex.h"

class GKOS_FUNC(File)
{
    protected:
        int type;

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

        int GetType() const;    // support type checking without rtti

        virtual ~GKOS_FUNC(File)() = default;

        // simple reference counting
        Spinlock sl;
        int nref = 1;
        GKOS_FUNC(File) *AddRef();
        void DelRef();
};

#endif
