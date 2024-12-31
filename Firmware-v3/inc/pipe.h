#ifndef PIPE_H
#define PIPE_H

#include "gk_conf.h"
#include "osmutex.h"

class Pipe
{
    protected:
        char b[GK_PIPESIZE];
        unsigned int read_ptr = 0;
        unsigned int write_ptr = 0;
        Spinlock sl;        

    public:
        int write(const char *buf, unsigned int nbytes);
        int read(char *buf, unsigned int nbytes);

        unsigned int readable_size() const;
        unsigned int writeable_size() const;
};

#include "osfile.h"
#include <memory>

class PipeFile : public File
{
    public:
        ssize_t Write(const char *buf, size_t count, int *_errno);
        ssize_t Read(char *buf, size_t count, int *_errno);

        bool is_write_end = false;

        std::shared_ptr<Pipe> p;

        PipeFile();

        int Close();
};

std::pair<std::shared_ptr<PipeFile>, std::shared_ptr<PipeFile>> make_pipe();

#endif
