#include "syscalls_int.h"
#include "process.h"

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
