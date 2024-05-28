#ifndef EXT4_THREAD_H
#define EXT4_THREAD_H

#include "osmutex.h"
#include "syscalls.h"
#include "thread.h"
#include "ext4.h"
#include "_sys_dirent.h"

static inline void check_buffer(const void *addr)
{
    auto paddr = (uint32_t)(uintptr_t)addr;
    if((paddr >= 0x20000000) && (paddr < 0x20020000))
    {
        __asm__ volatile("bkpt \n" ::: "memory");
    }
}

void init_ext4();

struct ext4_message
{
    enum msg_type
    {
        Open,
        Close,
        Read,
        Write,
        Lseek,
        Fstat,
        Mkdir,
        ReadDir,
        Unlink
    };

    msg_type type;
    PProcess p;

    union params_t
    {
        struct open_params_t
        {
            const char *pathname;
            int flags;
            int mode;
            int f;      // Pre-prepared file descriptor to place into
        } open_params;
        struct rw_params_t
        {
            ext4_file *e4f;
            char *buf;
            int nbytes;
        } rw_params;
        struct lseek_params_t
        {
            ext4_file *e4f;
            off_t offset;
            int whence;
        } lseek_params;
        struct fstat_params_t
        {
            ext4_file *e4f;
            ext4_dir *e4d;
            struct stat *st;
            const char *pathname;
        } fstat_params;
        struct close_params_t
        {
            ext4_file e4f;      // copy here because LwextFile object is already deleted
        } close_params;
        struct readdir_params_t
        {
            ext4_dir *e4d;
            struct dirent *de;
        } readdir_params;
        struct unlink_params_t
        {
            const char *pathname;
        } unlink_params;
    } params;

    SimpleSignal *ss;
    WaitSimpleSignal_params *ss_p;
};

bool ext4_send_message(ext4_message &msg);

[[maybe_unused]] static ext4_message ext4_mkdir_message(const char *pathname, int mode,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message::params_t::open_params_t _p {
        .pathname = pathname,
        .mode = mode
    };

    ext4_message::params_t __p {
        .open_params = _p
    };

    ext4_message ret {
        .type = ext4_message::msg_type::Mkdir,
        .params = __p,
        .ss = &ss,
        .ss_p = &ss_p
    };
    return ret;
}

[[maybe_unused]] static ext4_message ext4_open_message(const char *pathname, int flags, int mode,
    PProcess p, int f, SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message::params_t::open_params_t _p {
        .pathname = pathname,
        .flags = flags,
        .mode = mode,
        .f = f
    };

    ext4_message::params_t __p {
        .open_params = _p
    };

    ext4_message ret {
        .type = ext4_message::msg_type::Open,
        .p = p,
        .params = __p,
        .ss = &ss,
        .ss_p = &ss_p,
    };
    return ret;
}

[[maybe_unused]] static ext4_message ext4_read_message(ext4_file &e4f, char *buf, int nbytes,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message::params_t::rw_params_t _p {
        .e4f = &e4f,
        .buf = buf,
        .nbytes = nbytes
    };

    ext4_message::params_t __p {
        .rw_params = _p
    };

    ext4_message ret {
        .type = ext4_message::msg_type::Read,
        .params = __p,
        .ss = &ss,
        .ss_p = &ss_p };
    return ret;
}

[[maybe_unused]] static ext4_message ext4_write_message(ext4_file &e4f, char *buf, int nbytes,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message::params_t::rw_params_t _p {
        .e4f = &e4f,
        .buf = buf,
        .nbytes = nbytes
    };

    ext4_message::params_t __p {
        .rw_params = _p
    };

    ext4_message ret {
        .type = ext4_message::msg_type::Write,
        .params = __p,
        .ss = &ss,
        .ss_p = &ss_p };
    return ret;
}

[[maybe_unused]] static ext4_message ext4_lseek_message(ext4_file &e4f, off_t offset, int whence,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message::params_t::lseek_params_t _p {
        .e4f = &e4f,
        .offset = offset,
        .whence = whence
    };

    ext4_message::params_t __p {
        .lseek_params = _p
    };

    ext4_message ret {
        .type = ext4_message::msg_type::Lseek,
        .params = __p,
        .ss = &ss,
        .ss_p = &ss_p };
    return ret;
}

[[maybe_unused]] static ext4_message ext4_fstat_message(ext4_file &e4f, ext4_dir &e4d, bool is_dir,
    struct stat *st,
    const char *pathname,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message::params_t::fstat_params_t _p {
        .e4f = is_dir ? nullptr : &e4f,
        .e4d = is_dir ? &e4d : nullptr,
        .st = st,
        .pathname = pathname
    };

    ext4_message::params_t __p {
        .fstat_params = _p
    };

    ext4_message ret {
        .type = ext4_message::msg_type::Fstat,
        .params = __p,
        .ss = &ss,
        .ss_p = &ss_p };
    return ret;
}

[[maybe_unused]] static ext4_message ext4_close_message(ext4_file &e4f,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message::params_t::close_params_t _p {
        .e4f = e4f
    };

    ext4_message::params_t __p {
        .close_params = _p
    };

    ext4_message ret {
        .type = ext4_message::msg_type::Close,
        .params = __p,
        .ss = &ss,
        .ss_p = &ss_p };
    return ret;
}

[[maybe_unused]] static ext4_message ext4_readdir_message(ext4_dir &e4d,
    struct dirent *de,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message::params_t::readdir_params_t _p {
        .e4d = &e4d,
        .de = de
    };

    ext4_message::params_t __p {
        .readdir_params = _p
    };

    ext4_message ret {
        .type = ext4_message::msg_type::ReadDir,
        .params = __p,
        .ss = &ss,
        .ss_p = &ss_p
    };
    return ret;
}

[[maybe_unused]] static ext4_message ext4_unlink_message(const char *pathname,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message::params_t::unlink_params_t _p {
        .pathname = pathname
    };

    ext4_message::params_t __p {
        .unlink_params = _p
    };

    ext4_message ret {
        .type = ext4_message::msg_type::Unlink,
        .params = __p,
        .ss = &ss,
        .ss_p = &ss_p
    };
    return ret;
}

#endif
