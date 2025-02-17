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
#include "cleanup.h"
#include "pipe.h"
#include "util.h"

extern Process kernel_proc, p_supervisor;

int syscall_gettimeofday(timeval *tv, timezone *tz, int *_errno)
{
    if(!tv)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_W(tv);

    timespec ts;
    clock_get_now(&ts);
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec / 1000;

    if(tz)
    {
        ADDR_CHECK_STRUCT_W(tz);
        // just return UTC for now
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = DST_NONE;
    }

    return 0;
}

MemRegion syscall_memalloc_int(size_t len, int is_sync, int allow_sram,
    const std::string &usage, int *_errno)
{
    MemRegion mr = InvalidMemregion();
    auto t = GetCurrentThreadForCore();

    /* Are we after a graphics texture? */
    if(is_sync && t->p.mr_gtext.valid)
    {
        if((t->p.mr_gtext.length - t->p.gtext_ptr) > len)
        {
            mr.address = t->p.mr_gtext.address + t->p.gtext_ptr;
            mr.length = len;
            mr.rt = t->p.mr_gtext.rt;
            mr.valid = true;

            t->p.gtext_ptr += len;

            return mr;
        }
    }

    switch(t->p.stack_preference)
    {
        case STACK_PREFERENCE_SDRAM_RAM_TCM:
            mr = memblk_allocate(len, MemRegionType::SDRAM, usage);
            if(!mr.valid && allow_sram)
                mr = memblk_allocate(len, MemRegionType::SRAM, usage);
            if(!mr.valid)
                mr = memblk_allocate(len, MemRegionType::AXISRAM, usage);
            break;

        case STACK_PREFERENCE_TCM_RAM_SDRAM:
            if(allow_sram)
                mr = memblk_allocate(len, MemRegionType::SRAM, usage);
            if(!mr.valid)
                mr = memblk_allocate(len, MemRegionType::AXISRAM, usage);
            if(!mr.valid)
                mr = memblk_allocate(len, MemRegionType::SDRAM, usage);
            break;
    }
    
    if(!mr.valid)
    {
        *_errno = ENOMEM;
        return mr;
    }

    // get free mpu slot

    auto mpu_id = t->p.AddMPURegion({ mr, -1, 1, 1, 0, is_sync != 0 });
    if(mpu_id >= 0)
    {
        t->p.UpdateMPURegionsForThreads();
        return mr;
    }
    else
    {
        memblk_deallocate(mr);
        *_errno = ENOENT;
        return InvalidMemregion();
    }
}

int syscall_memalloc(size_t len, void **retaddr, int is_sync, int *_errno)
{
    if(!retaddr)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_STRUCT_R(retaddr);

    auto ret = syscall_memalloc_int(len, is_sync, 0, "mmap", _errno);
    if(ret.valid)
    {
        *retaddr = (void *)ret.address;
        return 0;
    }
    else
    {
        klog("memalloc: couldn't allocate memory of length %u\n", len);
        *retaddr = nullptr;
        return -1;
    }
}

int syscall_memdealloc(size_t len, const void *addr, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;

    MemRegion mreg;
    {
        CriticalGuard cg(p.sl);
        if(p.mr_gtext.valid)
        {
            auto paddr = (uint32_t)(uintptr_t)addr;
            if(paddr >= p.mr_gtext.address && paddr < (p.mr_gtext.address + p.mr_gtext.length))
            {
                // do nothing
                return 0;
            }
        }
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
    // find highest MPU slot containing addr
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;
    CriticalGuard cg_p(p.sl);

    int mpu_slot = -1;
    for(int i = 15; i >= 0; i--)
    {
        const auto &cmpu = p.p_mpu[i];
        if(cmpu.is_enabled() &&
            (uint32_t)(uintptr_t)addr >= cmpu.base_addr() &&
            (uint32_t)(uintptr_t)addr < (cmpu.base_addr() + cmpu.length()))
        {
            mpu_slot = i;
            break;
        }
    }

    if(mpu_slot == -1)
    {
        *_errno = EACCES;
        return -1;
    }
    else
    {
        const auto &cmpu = t->tss.mpuss[mpu_slot];

        MemRegionAccess unpriv_access;
        if(is_write)
            unpriv_access = MemRegionAccess::RW;
        else if(is_read)
            unpriv_access = MemRegionAccess::RO;
        else
            unpriv_access = MemRegionAccess::NoAccess;

        auto mpur = MPUGenerate(cmpu.base_addr(), cmpu.length(),
            mpu_slot, is_exec != 0, cmpu.priv_access(), unpriv_access,
            cmpu.tex_scb());

        {
            auto iter = p.mmap_regions.find(cmpu.base_addr());
            if(iter != p.mmap_regions.end())
            {
                iter->second.is_exec = is_exec;
                iter->second.is_read = is_read;
                iter->second.is_write = is_write;
            }

            p.p_mpu[mpu_slot] = mpur;
            p.UpdateMPURegionsForThreads();
        }
    }

    return 0;
}

int syscall_gpuenqueue(const gpu_message *msgs, size_t nmsg, size_t *nsent, int *_errno)
{
    if(!msgs || !nsent)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_BUFFER_R(msgs, sizeof(gpu_message) * nmsg);

    *nsent = GPUEnqueueMessages(msgs, nmsg);
    if(*nsent == nmsg && nmsg && msgs[nmsg - 1].type == gpu_message_type::SignalThread)
        return -2;  // deferred return
    
    return 0;
}

int syscall_peekevent(Event *ev, int *_errno)
{
    ADDR_CHECK_STRUCT_W(ev);
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
    ADDR_CHECK_STRUCT_W(buf);

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

    return (clock_t)clock_cur_us();
}

int syscall_kill(pid_t pid, int sig, int *_errno)
{
    if(pid < 0 || pid >= 0x10000)
    {
        *_errno = ESRCH;
        return -1;
    }

    auto p = proc_list.GetProcess(pid);
    if(!p)
    {
        *_errno = ESRCH;
        return -1;
    }

    // unless we are the kernel, we can only send signals to child processes
    bool is_valid = false;
    if(&GetCurrentThreadForCore()->p == &kernel_proc)
    {
        is_valid = true;
    }
    else if(&GetCurrentThreadForCore()->p == &p_supervisor)
    {
        is_valid = true;
    }
    else if(proc_list.IsChildOf(pid, GetCurrentThreadForCore()->p.pid))
    {
        is_valid = true;
    }

    if(!is_valid)
    {
        *_errno = EPERM;
        return -1;
    }
    
    if(sig == SIGKILL)
    {
        CriticalGuard cg(p->sl);
        for(auto pt : p->threads)
        {
            CriticalGuard cg2(pt->sl);
            pt->for_deletion = true;
            pt->set_is_blocking(true);
        }

        p->for_deletion = true;

        proc_list.DeleteProcess(pid, 0);

        extern CleanupQueue_t CleanupQueue;
        CleanupQueue.Push({ .is_thread = false, .p = p });

        return 0;
    }
    else
    {
        *_errno = EINVAL;
        return -1;
    }
}

int syscall_setwindowtitle(const char *title, int *_errno)
{
    if(!title)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_BUFFER_R(title, 1);

    auto len = std::min(std::strlen(title), (size_t)GK_MAX_WINDOW_TITLE);

    auto &p = GetCurrentThreadForCore()->p;
    {
        CriticalGuard c_p(p.sl);
        p.window_title = std::string(title, len);
        p.events.Push({ .type = Event::CaptionChange });
    }
    extern Process p_supervisor;
    p_supervisor.events.Push({ .type = Event::CaptionChange });
    return 0;
}

int syscall_cacheflush(void *addr, size_t len, int is_exec, int *_errno)
{
#if 0
    {
        klog("cacheflush: %x, %x, %d\n", (uint32_t)addr, len, is_exec);
    }
#endif
    /* Is this already in a sync'd area? */
    const auto t = GetCurrentThreadForCore();
    bool is_sync = false;
    bool is_valid = false;
    auto start = (uint32_t)(uintptr_t)addr;
    auto end = start + len;
    for(int i = 0; i < 16; i++)
    {
        const auto &cmpu = t->tss.mpuss[i];
        if(cmpu.is_enabled() == false)
            continue;
        auto up_access = cmpu.unpriv_access();
        if(up_access == NoAccess)
            continue;

        auto c_start = cmpu.base_addr();
        auto c_len = cmpu.length();

        // check SRD bits
        auto srd = (cmpu.rasr >> 8) & 0xffU;

        if(srd != 0)
        {
            if(srd != 0x81U)
            {
                BKPT_IF_DEBUGGER();     // not supported
                c_len = 0;
            }

            c_start += c_len / 8;
            c_len -= c_len / 4;
        }

        auto c_end = c_start + c_len;

        if(start >= c_start && end <= c_end)
        {
            is_valid = true;
            if(cmpu.tex_scb() == WT_NS)
            {
                is_sync = true;
            }
            else
            {
                is_sync = false;
            }
        }
    }
    if(!is_valid)
    {
        klog("cacheflush: invalid address\n");
        *_errno = EFAULT;
        return -1;
    }
    if(is_sync)
    {
#if 0
        klog("cacheflush: already sync'd\n");
#endif
    }
    else
    {
        CleanOrInvalidateM7Cache((uint32_t)addr, len, CacheType_t::Data);
    }
    if(is_exec)
    {
        InvalidateM7Cache((uint32_t)addr, len, CacheType_t::Instruction);
    }

    return 0;
}

extern std::vector<std::string> gk_env;
int syscall_get_env_count(int *_errno)
{
    const auto &penv = gk_env;

    return penv.size();
}

int syscall_get_ienv_size(unsigned int idx, int *_errno)
{
    const auto &penv = gk_env;

    if(idx >= penv.size())
    {
        *_errno = EINVAL;
        return -1;
    }

    return penv[idx].size();
}

int syscall_get_ienv(char *outbuf, size_t outbuf_len, unsigned int idx, int *_errno)
{
    ADDR_CHECK_BUFFER_W(outbuf, 1);

    const auto &penv = gk_env;

    if(idx >= penv.size())
    {
        *_errno = EINVAL;
        return -1;
    }

    if(penv[idx].size() > outbuf_len)
    {
        *_errno = E2BIG;
        return -1;
    }

    strncpy(outbuf, penv[idx].c_str(), outbuf_len);
    return 0;
}

int syscall_pipe(int pipefd[2], int *_errno)
{
    if(!pipefd)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_BUFFER_W(pipefd, sizeof(int) * 2);

    // try and get free process file handles
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;
    CriticalGuard cg(p.sl);

    auto newpipe = make_pipe();

    if(!newpipe.first || !newpipe.second)
    {
        *_errno = ENOMEM;
        return -1;
    }

    int fd1 = get_free_fildes(p);
    if(fd1 == -1)
    {
        *_errno = EMFILE;
        return -1;
    }
    p.open_files[fd1] = newpipe.first;
    int fd2 = get_free_fildes(p);
    if(fd2 == -1)
    {
        *_errno = EMFILE;
        p.open_files[fd1] = nullptr;
        return -1;
    }
    p.open_files[fd2] = newpipe.second;

    pipefd[0] = fd1;
    pipefd[1] = fd2;

    return 0;
}

int syscall_dup2(int oldfd, int newfd, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;
    CriticalGuard cg(p.sl);

    if(oldfd < 0 || oldfd >= GK_MAX_OPEN_FILES)
    {
        *_errno = EBADF;
        return -1;
    }

    if(p.open_files[oldfd] == nullptr)
    {
        *_errno = EBADF;
        return -1;
    }
    if(newfd != -1)
    {
        if(newfd < 0 || newfd >= GK_MAX_OPEN_FILES)
        {
            *_errno = EBADF;
            return -1;
        }
        if(p.open_files[newfd] == nullptr)
        {
            *_errno = EBADF;
            return -1;
        }
        else
        {
            p.open_files[newfd]->Close(_errno);
        }
    }
    else
    {
        newfd = get_free_fildes(p);
        if(newfd < 0)
        {
            *_errno = EBADF;
            return -1;
        }
    }

    p.open_files[newfd] = p.open_files[oldfd];

    if(p.open_files[newfd] == nullptr)
    {
        *_errno = ENOSYS;
        return -1;
    }

    return newfd;
}

int syscall_cmpxchg(void **ptr, void **oldval, void *newval, size_t len, int *_errno)
{
    ADDR_CHECK_BUFFER_W(ptr, len);
    ADDR_CHECK_BUFFER_W(oldval, len);

    if(len == 1)
    {
        cmpxchg((volatile uint8_t *)ptr, (uint8_t *)oldval, (uint8_t)(uint32_t)newval);
        return 0;
    }
    else if(len == 2)
    {
        cmpxchg((volatile uint16_t *)ptr, (uint16_t *)oldval, (uint16_t)(uint32_t)newval);
        return 0;
    }
    else if(len <= 4)
    {
        cmpxchg((volatile uint32_t *)ptr, (uint32_t *)oldval, (uint32_t)newval);
        return 0;
    }
    else
    {
        *_errno = EINVAL;
        return -1;
    }
}
