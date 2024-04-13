#ifndef EXT4_THREAD_H
#define EXT4_THREAD_H

#include "osmutex.h"
#include "syscalls.h"
#include "thread.h"
#include "ext4.h"

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
    };

    msg_type type;

    union params_t
    {
        struct open_params_t
        {
            Process *p;
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
            struct stat *st;
            const char *pathname;
        } fstat_params;
        struct close_params_t
        {
            ext4_file e4f;      // copy here because LwextFile object is already deleted
        } close_params;
    } params;

    SimpleSignal *ss;
    WaitSimpleSignal_params *ss_p;
};

bool ext4_send_message(ext4_message &msg);

static constexpr ext4_message ext4_open_message(const char *pathname, int flags, int mode,
    Process &p, int f, SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message::params_t::open_params_t _p {
        .p = &p,
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
        .params = __p,
        .ss = &ss,
        .ss_p = &ss_p };
    return ret;
}

static constexpr ext4_message ext4_read_message(ext4_file &e4f, char *buf, int nbytes,
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

static constexpr ext4_message ext4_write_message(ext4_file &e4f, char *buf, int nbytes,
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

static constexpr ext4_message ext4_lseek_message(ext4_file &e4f, off_t offset, int whence,
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

static constexpr ext4_message ext4_fstat_message(ext4_file &e4f, struct stat *st,
    const char *pathname,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message::params_t::fstat_params_t _p {
        .e4f = &e4f,
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

static constexpr ext4_message ext4_close_message(ext4_file &e4f,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message::params_t::close_params_t _p {
        .e4f = e4f,
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

#endif
