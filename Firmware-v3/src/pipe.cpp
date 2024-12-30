#include "pipe.h"
#include <string.h>

std::pair<PipeFile *, PipeFile *> make_pipe()
{
    auto p1 = new PipeFile();
    if(!p1)
    {
        return std::make_pair(nullptr, nullptr);
    }
    auto p2 = new PipeFile();
    if(!p2)
    {
        delete p1;
        return std::make_pair(nullptr, nullptr);
    }
    auto p = std::make_shared<Pipe>();
    if(!p)
    {
        delete p1;
        delete p2;
        return std::make_pair(nullptr, nullptr);
    }

    p1->p = p;
    p2->p = p;
    p1->is_write_end = false;
    p2->is_write_end = true;

    return std::make_pair(p1, p2);
}

ssize_t PipeFile::Write(const char *buf, size_t count, int *_errno)
{
    if(!buf)
    {
        *_errno = EFAULT;
        return -1;
    }
    if(!is_write_end)
    {
        *_errno = EBADF;
        return -1;
    }

    return p->write(buf, count);
}

ssize_t PipeFile::Read(char *buf, size_t count, int *_errno)
{
    if(!buf)
    {
        *_errno = EFAULT;
        return -1;
    }
    if(is_write_end)
    {
        *_errno = EBADF;
        return -1;
    }

    return p->read(buf, count);
}

int Pipe::read(char *buf, unsigned int nbytes)
{
    CriticalGuard cg(sl);
    auto rb = readable_size();
    if(!rb)
        return 0;

    auto to_read = std::min(rb, nbytes);

    auto to_read1 = std::min(to_read, GK_PIPESIZE - read_ptr);
    auto to_read2 = to_read - to_read1;
    memcpy(buf, &b[read_ptr], to_read1);
    if(to_read2)
    {
        memcpy(&buf[to_read1], b, to_read2);
        read_ptr = to_read2;
    }
    else
    {
        read_ptr += to_read1;
    }

    return to_read;
}

int Pipe::write(const char *buf, unsigned int nbytes)
{
    CriticalGuard cg(sl);
    auto wb = writeable_size();
    if(!wb)
        return 0;

    auto to_write = std::min(wb, nbytes);

    auto to_write1 = std::min(to_write, GK_PIPESIZE - write_ptr);
    auto to_write2 = to_write - to_write1;
    memcpy(&b[read_ptr], buf, to_write1);
    if(to_write2)
    {
        memcpy(b, &buf[to_write1], to_write2);
        write_ptr = to_write2;
    }
    else
    {
        write_ptr += to_write1;
    }

    return to_write;
}

unsigned int Pipe::readable_size() const
{
    if(read_ptr >= write_ptr) return 0;
    if(write_ptr > read_ptr)
    {
        return write_ptr - read_ptr;
    }
    else
    {
        return GK_PIPESIZE - (read_ptr - write_ptr);
    }
}

unsigned int Pipe::writeable_size() const
{
    if((read_ptr == 0 && write_ptr == GK_PIPESIZE - 1) ||
        (write_ptr == read_ptr - 1))
    {
        return 0;
    }
    else if(read_ptr > write_ptr)
    {
        return read_ptr - write_ptr - 1;
    }
    else
    {
        return GK_PIPESIZE - (write_ptr - read_ptr) - 1;
    }
}
