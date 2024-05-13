#include "clocks.h"
#include <sys/time.h>
#include "syscalls.h"
#include "syscalls_int.h"
#include "memblk.h"
#include <errno.h>
#include "gpu.h"
#include <sys/times.h>
#include "process.h"
#include "scheduler.h"
#include "cache.h"
#include "SEGGER_RTT.h"

extern Spinlock s_rtt;
extern Process kernel_proc;

int syscall_gettimeofday(timeval *tv, timezone *tz, int *_errno)
{
    if(!tv)
    {
        *_errno = EINVAL;
        return -1;
    }

    timespec ts;
    clock_get_now(&ts);
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / 1000;

    if(tz)
    {
        // just return UTC for now
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = DST_NONE;
    }

    return 0;
}

int syscall_memalloc(size_t len, void **retaddr, int is_sync, int *_errno)
{
    if(!retaddr)
    {
        *_errno = EINVAL;
        return -1;
    }

    // get free mpu slot
    auto t = GetCurrentThreadForCore();
    int mpu_slot = -1;
    for(int i = 0; i < 8; i++)
    {
        if(!(t->tss.mpuss[i].rasr & 0x1U))
        {
            mpu_slot = i;
            break;
        }
    }

    if(mpu_slot == -1)
    {
        *_errno = ENOMEM;
        return -1;
    }

    auto mr = memblk_allocate(len, MemRegionType::AXISRAM);
    if(!mr.valid)
    {
        mr = memblk_allocate(len, MemRegionType::SDRAM);
        if(!mr.valid)
        {
            *_errno = ENOMEM;
            return -1;
        }
    }

    {
        auto &p = t->p;

        CriticalGuard cg_p(p.sl);
        p.mmap_regions[mr.address] = Process::mmap_region { mr, -1, 1, 1, 0 };

        // set mpu region for this thread and all others
        auto mpur = MPUGenerate(mr.address, mr.length, mpu_slot, false,
            RW, RW, is_sync ? WT_NS : WBWA_NS);
        for(auto curt : p.threads)
        {
            CriticalGuard cg_t(curt->sl);
            curt->tss.mpuss[mpu_slot] = mpur;

            // Invalidate here on the off-chance the M7 cache has entries for the 0x38000000 range
            // When MPU is disabled in task switch, cache may be re-enabled for reads from this
            //  region
            InvalidateM7Cache((uint32_t)(uintptr_t)&curt->tss.mpuss[0],
                8 * sizeof(mpu_saved_state), CacheType_t::Data);
        }

        // and for this thread
        {
            CriticalGuard cg_t(t->sl);
            auto ctrl = MPU->CTRL;
            MPU->CTRL = 0;
            MPU->RBAR = mpur.rbar;
            MPU->RASR = mpur.rasr;
            MPU->CTRL = ctrl;
            __DSB();
            __ISB();
        }
    }

    *retaddr = (void *)mr.address;
    return 0;
}

int syscall_memdealloc(size_t len, const void *addr, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;

    MemRegion mreg;
    {
        CriticalGuard cg(p.sl);
        auto mr = p.get_mmap_region((uint32_t)(uintptr_t)addr, len);
        if(mr == p.mmap_regions.end())
        {
            *_errno = EINVAL;
            return -1;
        }
        mreg = mr->second.mr;
        p.mmap_regions.erase(mr);
    }

    memblk_deallocate(mreg);
    return 0;
}

int syscall_setprot(const void *addr, int is_read, int is_write, int is_exec, int *_errno)
{
    // We have run out of MPU slots, so this does nothing at the moment
    return 0;
}

int syscall_gpuenqueue(const gpu_message *msgs, size_t nmsg, size_t *nsent, int *_errno)
{
    if(!msgs || !nsent)
    {
        *_errno = EINVAL;
        return -1;
    }

    *nsent = GPUEnqueueMessages(msgs, nmsg);
    if(*nsent == nmsg && nmsg && msgs[nmsg - 1].type == gpu_message_type::SignalThread)
        return -2;  // deferred return
    
    return 0;
}

int syscall_peekevent(Event *ev, int *_errno)
{
    auto &p = GetCurrentThreadForCore()->p;
    CriticalGuard cg(p.sl);
    if(p.events.TryPop(ev))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

clock_t syscall_times(tms *buf, int *_errno)
{
    if(!buf)
    {
        *_errno = EFAULT;
        return (clock_t)-1;
    }

    uint64_t tot_us = 0ULL;
    uint64_t tot_s = 0ULL;
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;
    {
        CriticalGuard cg_p(p.sl);
        for(auto pt : p.threads)
        {
            CriticalGuard cg_t(pt->sl);
            tot_s += pt->total_s_time;
            tot_us += pt->total_us_time;
        }
    }
    auto tot_u = tot_us - tot_s;

    buf->tms_utime = tot_u;
    buf->tms_stime = tot_s;
    buf->tms_cutime = tot_u;
    buf->tms_cstime = tot_s;

    return (clock_t)clock_cur_ms();
}

int syscall_kill(pid_t pid, int sig, int *_errno)
{
    if(pid < 0 || pid >= 0x10000)
    {
        *_errno = ESRCH;
        return -1;
    }
    auto p = reinterpret_cast<Process *>((unsigned int)pid + 0x38000000U);
    if(sig == SIGKILL)
    {
        if(p != &kernel_proc)
        {
            CriticalGuard cg_p(p->sl);
            for(auto thr : p->threads)
            {
                CriticalGuard cg_t(thr->sl);
                thr->for_deletion = true;
                if(thr == GetCurrentThreadForCore())
                {
                    Yield();
                }
            }
            p->for_deletion = true;
        }
        else
        {
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "error: SIGKILL sent to kernel process\n");
            }
            while(true)
            {
                __asm__ volatile("bkpt \n" ::: "memory");
            }
        }
    }
    return 0;
}