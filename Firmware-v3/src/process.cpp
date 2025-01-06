#include "process.h"
#include <sys/types.h>
#include <vector>
#include "osmutex.h"
#include "tilt.h"
#include "screen.h"

ProcessList proc_list;
extern Process p_supervisor;

Process::Process()
{
    proc_list.RegisterProcess(this);
}

Process::~Process()
{
    proc_list.DeleteProcess(pid, this->rc);
}

int Process::AddMPURegion(const mpu_saved_state &r)
{
    CriticalGuard cg(sl);

    // get free mpu slot
    int mpu_slot = -1;
    for(unsigned int i = 0; i < 16; i++)
    {
        if(!(p_mpu[i].rasr & 0x1U))
        {
            if(!has_tls || i != p_mpu_tls_id)
            {
                mpu_slot = i;
                break;
            }
        }
    }

    if(mpu_slot == -1)
    {
        klog("proc: request for mpu slot - none available\n");
        BKPT();
    }
    else
    {
        mpu_saved_state nr = r;
        nr.set_slot(mpu_slot);
        p_mpu[mpu_slot] = nr;
    }
    
    return mpu_slot;
}

int Process::AddMPURegion(const mmap_region &r)
{
    return AddMPURegion(r.to_mpu(0));
}

int Process::DeleteMPURegion(const mmap_region &r)
{
    return DeleteMPURegion(r.to_mpu(0)); 
}

int Process::DeleteMPURegion(const MemRegion &r)
{
    if(!r.valid)
        return -1;
    mmap_region mr { .mr = r };
    return DeleteMPURegion(mr.to_mpu(0));
}

int Process::DeleteMPURegion(const mpu_saved_state &r)
{
    int ret = -1;
    if(!r.is_enabled())
        return -1;

    for(unsigned int i = 0; i < 16; i++)
    {
        // start at whatever is pointed to in mpu_saved_state
        auto act_r = (i + r.slot()) % 16;

        const auto &cmr = p_mpu[act_r];

        if(cmr.is_enabled() &&
            cmr.base_addr() == r.base_addr() &&
            cmr.length() == r.length())
        {
            // this one
            p_mpu[act_r] = MPUGenerateNonValid(act_r);
            ret = (int)act_r;
        }
    }

    return ret;
}

void Process::UpdateMPURegionsForThreads()
{
    CriticalGuard cg(sl);
    for(auto t : threads)
    {
        UpdateMPURegionForThread(t);
    }
}

void Process::UpdateMPURegionForThread(Thread *t)
{
    CriticalGuard cg(t->sl);
    // copy everything but the TLS
    for(unsigned int i = 0; i < 16; i++)
    {
        if(has_tls && i == p_mpu_tls_id)
            continue;
        t->tss.mpuss[i] = p_mpu[i];
    }

    // If we are the calling process then update for us as well
    if(t == GetCurrentThreadForCore())
    {
        auto ctrl = MPU->CTRL;
        MPU->CTRL = 0;
        for(unsigned int i = 0; i < 16; i++)
        {
            MPU->RBAR = t->tss.mpuss[i].rbar;
            MPU->RASR = t->tss.mpuss[i].rasr;
        }
        MPU->CTRL = ctrl;
        __DSB();
        __ISB();
    }
}

mpu_saved_state Process::mmap_region::to_mpu(unsigned int mpu_id) const
{
    if(mr.valid == false)
        return MPUGenerateNonValid(mpu_id);

    MemRegionAccess user_acc;
    if(is_write)
        user_acc = MemRegionAccess::RW;
    else if(is_read || is_exec)
        user_acc = MemRegionAccess::RO;
    else
        user_acc = MemRegionAccess::NoAccess;

    MemRegionAccess priv_acc = RW;
    uint32_t srd = 0;

    if(is_stack && mr.length > 128)
    {
        // add stack guards
        if(is_priv)
        {
            // the default map will give access to this region, therefore we use
            //  subregions to disable access to the first and last subregion
            priv_acc = NoAccess;
            user_acc = NoAccess;    // can't have user higher access than priv
            srd = 0x7eU;            // only first and last regions arer disabled
        }
        else
        {
            // unprivileged.  We do not have default access to this region,
            //  therefore disable first and last regions
            srd = 0x81U;
        }
    }

    bool _is_exec = is_exec;

    if(priv_acc == NoAccess)
    {
        _is_exec = false;
    }
    
    return MPUGenerate(mr.address, mr.length, mpu_id, _is_exec, priv_acc, user_acc, 
        is_sync ? WT_NS : WBWA_NS, srd);
}

pid_t ProcessList::RegisterProcess(Process *p)
{
    CriticalGuard cg;
    auto ret = (pid_t)pvals.size();

    pval pv { .p = p, .is_alive = true, .retval = 0 };
    pvals.push_back(pv);

    p->pid = ret;
    return ret;
}

void ProcessList::DeleteProcess(pid_t pid, int retval)
{
    CriticalGuard cg;
    if(pid < 0 || pid >= (pid_t)pvals.size())
        return;

    auto &cpval = pvals[pid];
    auto p = cpval.p;
    if(p == focus_process && p->ppid && pvals[p->ppid].is_alive)
    {
        SetFocusProcess(pvals[p->ppid].p);
    }

    for(const auto &wait_t : cpval.waiting_threads)
    {
        wait_t->set_is_blocking(false);
        wait_t->block_until.invalidate();
        wait_t->blocking_on = nullptr;
        signal_thread_woken(wait_t);
    }
    pvals[pid] = { .p = nullptr, .is_alive = false, .retval = retval };
}

int ProcessList::GetReturnValue(pid_t pid, int *retval, bool wait)
{
    CriticalGuard cg;
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

Process *ProcessList::GetProcess(pid_t pid)
{
    CriticalGuard cg;
    if(pid < 0 || pid >= (pid_t)pvals.size())
        return nullptr;
    if(!pvals[pid].is_alive)
        return nullptr;
    return pvals[pid].p;
}

pid_t ProcessList::GetParentProcess(pid_t pid)
{
    CriticalGuard cg;
    if(pid < 0 || pid >= (pid_t)pvals.size())
        return (pid_t)-1;
    if(!pvals[pid].is_alive || !pvals[pid].p)
        return (pid_t)-1;
    return pvals[pid].p->ppid;
}

bool ProcessList::IsChildOf(pid_t child, pid_t parent)
{
    CriticalGuard cg;
    while(true)
    {
        if(child < 0 || child >= (pid_t)pvals.size())
            return false;

        if(!pvals[child].is_alive || !pvals[child].p)
            return false;

        if(child == parent)
            return true;

        if(child == pvals[child].p->ppid)
            return false;       // circular reference, typically kernel_proc
        
        child = pvals[child].p->ppid;
    }
}

bool SetFocusProcess(Process *proc)
{
    focus_process = proc;

    if(proc->tilt_is_keyboard || proc->tilt_is_joystick || proc->tilt_raw)
    {
        tilt_enable(true);
    }
    else
    {
        tilt_enable(false);
    }

    screen_hardware_scale scl_x, scl_y;

    if(proc->screen_w <= 160)
        scl_x = x4;
    else if(proc->screen_w <= 320)
        scl_x = x2;
    else
        scl_x = x1;

    if(proc->screen_h <= 120)
        scl_y = x4;
    else if(proc->screen_h <= 240)
        scl_y = x2;
    else
        scl_y = x1;

    /* set screen mode */
    screen_set_hardware_scale(scl_x, scl_y);

    p_supervisor.events.Push( { .type = Event::CaptionChange });
    proc->events.Push( { .type = Event::RefreshScreen });

    return true;
}
