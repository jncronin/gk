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
