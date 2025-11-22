#include "ramdisk.h"
#include "fcntl.h"
#include <cstring>

int ramdisk_open(const std::string &fname, PFile *f,
    bool is_read, bool is_write)
{
    // for testing, support a single file
    if(fname == "test.bin")
    {
        extern int _binary_test_strip_elf_start;
        extern int _binary_test_strip_elf_end;
        void *test_addr = (void *)&_binary_test_strip_elf_start;
        auto test_size = (size_t)((char *)&_binary_test_strip_elf_end - (char *)&_binary_test_strip_elf_start);

        *f = std::make_shared<RamdiskFile>(test_addr, test_size, is_read, is_write);
        return 0;
    }

    return -1;
}

RamdiskFile::RamdiskFile(void *_buf, size_t len, bool for_read, bool for_write)
{
    buf = _buf;
    size = len;
    can_read = for_read;
    can_write = for_write;
    offset = 0;
    type = FileType::FT_Ram;
}

ssize_t RamdiskFile::Write(const char *_buf, size_t count, int *_errno)
{
    if(!can_write)
    {
        *_errno = EBADF;
        return -1;
    }

    if((size_t)offset == size)
    {
        *_errno = EFBIG;
        return -1;
    }

    if(offset + count > size)
    {
        count = size - offset;
    }

    memcpy(&((char *)buf)[offset], _buf, count);
    offset += count;
    return count;
}

ssize_t RamdiskFile::Read(char *_buf, size_t count, int *_errno)
{
    if(!can_read)
    {
        *_errno = EBADF;
        return -1;
    }
    if((size_t)offset >= size)
    {
        return 0;
    }
    if(offset + count > size)
    {
        count = size - offset;
    }

    memcpy(_buf, &((char *)buf)[offset], count);
    offset += count;
    return count;
}

int RamdiskFile::Fstat(struct stat *_buf, int *_errno)
{
    memset(_buf, 0, sizeof(struct stat));
    _buf->st_size = size;
    return 0;
}

off_t RamdiskFile::Lseek(off_t _offset, int whence, int *_errno)
{
    off_t new_offset = 0;

    switch(whence)
    {
        case SEEK_SET:
            new_offset = _offset;
            break;
        case SEEK_CUR:
            new_offset = offset + _offset;
            break;
        case SEEK_END:
            new_offset = size + _offset;
            break;
        default:
            *_errno = EINVAL;
            return -1;
    }

    if(new_offset < 0 || (size_t)new_offset > size)
    {
        *_errno = EINVAL;
        return -1;
    }
    offset = new_offset;
    return offset;
}

int Isattty(int *_errno)
{
    return 0;
}
