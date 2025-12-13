#include "syscalls_int.h"
#include "process.h"

int syscall_get_env_count(int *_errno)
{
    auto &penv = GetCurrentProcessForCore()->env;
    CriticalGuard cg(penv.sl);

    return penv.envs.size();
}

int syscall_get_ienv_size(unsigned int idx, int *_errno)
{
    auto &penv = GetCurrentProcessForCore()->env;
    CriticalGuard cg(penv.sl);

    if(idx >= penv.envs.size())
    {
        *_errno = EINVAL;
        return -1;
    }

    return penv.envs[idx].size();
}

int syscall_get_ienv(char *outbuf, size_t outbuf_len, unsigned int idx, int *_errno)
{
    ADDR_CHECK_BUFFER_W(outbuf, 1);

    auto &penv = GetCurrentProcessForCore()->env;
    CriticalGuard cg(penv.sl);

    if(idx >= penv.envs.size())
    {
        *_errno = EINVAL;
        return -1;
    }

    if(penv.envs[idx].size() > outbuf_len)
    {
        *_errno = E2BIG;
        return -1;
    }

    strncpy(outbuf, penv.envs[idx].c_str(), outbuf_len);
    return 0;
}

int syscall_get_arg_count(int *_errno)
{
    auto &penv = GetCurrentProcessForCore()->env;
    CriticalGuard cg(penv.sl);

    return penv.args.size() + 1;
}

int syscall_get_iarg_size(unsigned int idx, int *_errno)
{
    auto &penv = GetCurrentProcessForCore()->env;
    CriticalGuard cg(penv.sl);

    if(idx >= penv.args.size() + 1)
    {
        *_errno = EINVAL;
        return -1;
    }

    if(idx == 0)
    {
        return GetCurrentProcessForCore()->name.size();
    }

    return penv.args[idx - 1].size();
}

int syscall_get_iarg(char *outbuf, size_t outbuf_len, unsigned int idx, int *_errno)
{
    ADDR_CHECK_BUFFER_W(outbuf, 1);

    auto &penv = GetCurrentProcessForCore()->env;
    CriticalGuard cg(penv.sl);

    if(idx >= penv.args.size() + 1)
    {
        *_errno = EINVAL;
        return -1;
    }

    const auto &v = (idx == 0) ? GetCurrentProcessForCore()->name :
        penv.args[idx - 1];

    if(v.size() > outbuf_len)
    {
        *_errno = E2BIG;
        return -1;
    }

    strncpy(outbuf, v.c_str(), outbuf_len);
    return 0;
}
