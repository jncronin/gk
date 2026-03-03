#include "syscalls_int.h"
#include "process.h"

bool is_parent_of(id_t child, id_t parent);

int syscall_peekevent(Event *ev, int *_errno)
{
    ADDR_CHECK_STRUCT_W(ev);
    auto p = GetCurrentProcessForCore();
    if(p->events.TryPop(ev))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int syscall_eventsend(pid_t pid, const Event *ev, int *_errno)
{
    // ensure we are a parent of pid
    auto pp = GetCurrentProcessForCore();
    if(pp == nullptr)
    {
        *_errno = EINVAL;
        return -1;
    }
    if(pid < 0)
    {
        *_errno = EINVAL;
        return -1;
    }
    if((id_t)pid != pp->id && !is_parent_of(pid, pp->id))
    {
        *_errno = EPERM;
        klog("syscall: invalid request for pid %d which is not a child of %d\n", pid, pp->id);
        return -1;
    }

    auto p = ProcessList.Get(pid);
    if(!p.v)
    {
        *_errno = EINVAL;
        return -1;
    }

    ADDR_CHECK_STRUCT_R(ev);

    auto ret = p.v->events.Push(*ev);
    if(!ret)
    {
        *_errno = EBUSY;
        return -1;
    }
    return 0;
}
