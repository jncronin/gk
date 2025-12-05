#include "ext4_thread.h"

ext4_message ext4_mkdir_message(const char *pathname, int mode,
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

ext4_message ext4_open_message(const char *pathname, int flags, int mode,
    int f, SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
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
        .params = __p,
        .ss = &ss,
        .ss_p = &ss_p };
    return ret;
}

ext4_message ext4_read_message(ext4_file &e4f, char *buf, int nbytes,
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

ext4_message ext4_write_message(ext4_file &e4f, char *buf, int nbytes,
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

ext4_message ext4_lseek_message(ext4_file &e4f, off_t offset, int whence,
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

ext4_message ext4_ftruncate_message(ext4_file &e4f, off_t length,
    SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message::params_t::ftruncate_params_t _p {
        .e4f = &e4f,
        .length = length
    };

    ext4_message::params_t __p {
        .ftruncate_params = _p
    };

    ext4_message ret {
        .type = ext4_message::msg_type::Ftruncate,
        .params = __p,
        .ss = &ss,
        .ss_p = &ss_p };
    return ret;
}

ext4_message ext4_fstat_message(ext4_file &e4f, ext4_dir &e4d, bool is_dir,
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

ext4_message ext4_close_message(ext4_file &e4f,
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

ext4_message ext4_readdir_message(ext4_dir &e4d,
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

ext4_message ext4_unlink_message(const char *pathname,
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

ext4_message ext4_unmount_message(SimpleSignal &ss, WaitSimpleSignal_params &ss_p)
{
    ext4_message ret {
        .type = ext4_message::msg_type::Unmount,
        .ss = &ss,
        .ss_p = &ss_p
    };
    return ret;
}
