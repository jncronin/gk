#include "process.h"
#include <sys/types.h>
#include <vector>
#include "osmutex.h"

SRAM4_DATA ProcessList proc_list;


Process::Process()
{
    proc_list.RegisterProcess(this);
}

Process::~Process()
{
    proc_list.DeleteProcess(pid, this->rc);
}


pid_t ProcessList::RegisterProcess(Process *p)
{
    CriticalGuard cg(sl);
    auto ret = (pid_t)pvals.size();

    pval pv { .p = p, .is_alive = true, .retval = 0 };
    pvals.push_back(pv);

    p->pid = ret;
    return ret;
}

void ProcessList::DeleteProcess(pid_t pid, int retval)
{
    CriticalGuard cg(sl);
    if(pid < 0 || pid >= (pid_t)pvals.size())
        return;

    auto &cpval = pvals[pid];
    auto p = cpval.p;
    if(p == focus_process && p->ppid && pvals[p->ppid].is_alive)
        focus_process = pvals[p->ppid].p;

    for(const auto &wait_t : cpval.waiting_threads)
    {
        wait_t->is_blocking = false;
        wait_t->block_until = 0;
        wait_t->blocking_on = nullptr;
        signal_thread_woken(wait_t);
    }
    pvals[pid] = { .p = nullptr, .is_alive = false, .retval = retval };
}

int ProcessList::GetReturnValue(pid_t pid, int *retval, bool wait)
{
    CriticalGuard cg(sl);
    if(pid < 0 || pid >= (pid_t)pvals.size())
        return -1;
    if(pvals[pid].is_alive)
    {
        if (wait)
        {
            pvals[pid].waiting_threads.insert(GetCurrentThreadForCore());
            Block();
        }
        
        return -2;
    }
    if(retval)
        *retval = pvals[pid].retval;
    return 0;
}