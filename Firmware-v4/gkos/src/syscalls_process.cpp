#include "syscalls_int.h"
#include "process.h"
#include "elf.h"
#include "threadproclist.h"
#include <fcntl.h>
#include <sys/wait.h>
#include "gk_conf.h"

int syscall_proccreate(const char *fname, const proccreate_t *proc_info, pid_t *pid, int *_errno)
{
    ADDR_CHECK_BUFFER_R(fname, 1);
    ADDR_CHECK_STRUCT_R(proc_info);
    if(pid)
    {
        ADDR_CHECK_STRUCT_W(pid);
    }

    // get last part of path to use as process name
    const char *pname = fname;
    auto lastslash = strrchr(fname, '/');
    if(lastslash && strlen(lastslash + 1))
        pname = lastslash + 1;

    // open the file
    auto fd = syscall_open(fname, O_RDONLY, 0, _errno);
    if(fd < 0)
    {
        klog("process_create: open(%s) failed %d\n", fname, _errno);
        return fd;
    }

    // create process object
    auto proc = Process::Create(pname, proc_info->is_priv != 0, GetCurrentProcessForCore());
    if(!proc)
    {
        klog("process_create: Process::Create failed\n");
        *_errno = EFAULT;
        syscall_close1(fd, _errno);
        syscall_close2(fd, _errno);
        return -1;
    }

    // parse elf file
    Thread::threadstart_t proc_ep;
    auto ret = elf_load_fildes(fd, proc, &proc_ep);
    syscall_close1(fd, _errno);
    syscall_close2(fd, _errno);
    if(ret != 0)
    {
        klog("process_create: elf_load_fildes failed: ret: %d\n", ret);
        *_errno = EFAULT;
        Process::Kill(proc->id, -1);
        return -1;
    }

    // set arguments
    {
        CriticalGuard cg(proc->screen.sl);

        proc->screen.screen_pf = proc_info->pixel_format;
        proc->screen.screen_w = proc_info->screen_w;
        proc->screen.screen_h = proc_info->screen_h;
        proc->screen.screen_refresh = proc_info->screen_refresh;
        proc->screen.updates_each_frame = proc_info->screen_overwritten_each_frame;

        if(proc->screen.screen_w == 0)
            proc->screen.screen_w = GK_SCREEN_WIDTH;
        if(proc->screen.screen_w > GK_MAX_SCREEN_WIDTH)
            proc->screen.screen_w = GK_MAX_SCREEN_WIDTH;
        proc->screen.screen_w = (proc->screen.screen_w + 3) & ~3;

        if(proc->screen.screen_h == 0)
            proc->screen.screen_h = GK_SCREEN_HEIGHT;
        if(proc->screen.screen_h > GK_MAX_SCREEN_HEIGHT)
            proc->screen.screen_h = GK_MAX_SCREEN_HEIGHT;
        proc->screen.screen_h = (proc->screen.screen_h + 3) & ~3;

        if(proc->screen.screen_refresh < GK_MIN_SCREEN_REFRESH
            || proc->screen.screen_refresh > GK_MAX_SCREEN_REFRESH)
        {
            proc->screen.screen_refresh = GK_SCREEN_REFRESH;
        }

        if(proc->screen.screen_pf > GK_PIXELFORMAT_MAX)
            proc->screen.screen_pf = 0;

        if(proc->screen.updates_each_frame < 0 ||
            proc->screen.updates_each_frame >= 3)
        {
            proc->screen.updates_each_frame = 0;
        }
    }

    {
        CriticalGuard cg(proc->env.sl);
        proc->env.cwd = (proc_info->cwd != nullptr) ? std::string(proc_info->cwd) : "";
        for(auto i = 0; i < proc_info->argc; i++)
        {
            std::string carg(proc_info->argv[i]);
            proc->env.args.push_back(carg);
        }
    }

    // keymap
    proc->keymap = proc_info->keymap;

#if GK_OVERCLOCK_MHZ
    const unsigned int max_cpu_freq = GK_OVERCLOCK_MHZ * 1000000U;
#else
    const unsigned int max_cpu_freq = 1500000000U;
#endif
    if(proc_info->cpu_freq >= 400000000U && proc_info->cpu_freq <= max_cpu_freq)
    {
        proc->cpu_freq = proc_info->cpu_freq;
    }
    else
    {
        proc->cpu_freq = 1200000000U;
    }

    // Create startup thread
    auto t_t0 = Thread::Create(proc->name, proc_ep, nullptr, false, GK_PRIORITY_NORMAL, proc);
    if(!t_t0)
    {
        klog("process_create: Thread::Create failed\n");
        Process::Kill(proc->id, -1);
        return -1;
    }

    // Return pid, if requested
    if(pid)
        *pid = proc->id;

    // Set with focus
    if(proc_info->with_focus)
    {
        SetFocusProcess(proc);
    }

    // Schedule thread
    sched.Schedule(t_t0);

    return 0;
}

int syscall_waitpid(pid_t pid, int *retval, int options, int *_errno)
{
    if(retval)
        ADDR_CHECK_STRUCT_W(retval);

    while(true)
    {
        // ensure pid is a child of ours
        auto pproc = ProcessList.Get(pid);
        auto cp = GetCurrentProcessForCore();
        if(!cp)
        {
            *_errno = EFAULT;
            return -1;
        }
        if(pproc.ppid != cp->id)
        {
            klog("waitpid: request for a process (%d) which is not a child of the calling process (%d: %s)\n",
                pid, cp->id, cp->name.c_str());
            *_errno = ECHILD;
            return -1;
        }

        if(pproc.has_ended)
        {
            if(retval)
                *retval = pproc.retval;
            return pid;
        }
        {
            if(options & WNOHANG)
            {
                return 0;
            }
            else
            {
                {
                    CriticalGuard cg(pproc.v->sl);
                    pproc.v->waiting_threads.insert(GetCurrentThreadForCore()->id);
                }
                Block();
            }
        }
    }
}

pid_t syscall_get_proc_ppid(pid_t pid, int *_errno)
{
    auto p = ProcessList.Get(pid).v;
    if(p)
    {
        return p->id;
    }
    else
    {
        *_errno = ESRCH;
        return (pid_t)-1;
    }
}

int syscall_kill(pid_t pid, int sig, int *_errno)
{
    auto p = ProcessList.Get(pid).v;
    if(!p)
    {
        *_errno = ESRCH;
        return -1;
    }

    switch(sig)
    {
        case SIGKILL:
        case SIGABRT:
            Process::Kill(pid, 128 + sig);
            break;

        default:
            klog("kill: send %d to %s\n", sig, p->name.c_str());
            break;
    }

    return 0;
}

int syscall_setwindowtitle(const char *title, int *_errno)
{
    klog("windowtitle: %s\n", title);

    auto p = GetCurrentProcessForCore();
    if(p)
    {
        CriticalGuard cg(p->sl);
        p->window_title = std::string(title);
        cg.unlock();

        extern PProcess p_supervisor;
        Event ev;
        ev.type = Event::event_type_t::CaptionChange;
        p_supervisor->events.Push(ev);
    }

    return 0;
}

bool is_parent_of(id_t child, id_t parent)
{
    auto p = ProcessList.Get(child);
    if(!p.v)
        return false;
    if(parent == p.v->id)
    {
        return true;
    }
    return is_parent_of(p.v->ppid, parent);
}

int syscall_getscreenmodeforprocess(pid_t pid, size_t *w, size_t *h, unsigned int *pf, int *refresh, int *_errno)
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

    CriticalGuard cg(p.v->screen.sl);
    if(w)
    {
        ADDR_CHECK_STRUCT_W(w);
        *w = p.v->screen.screen_w;
    }
    if(h)
    {
        ADDR_CHECK_STRUCT_W(h);
        *h = p.v->screen.screen_h;
    }
    if(pf)
    {
        ADDR_CHECK_STRUCT_W(pf);
        *pf = p.v->screen.screen_pf;
    }
    if(refresh)
    {
        ADDR_CHECK_STRUCT_W(refresh);
        *refresh = p.v->screen.screen_refresh;
    }

    return 0;    
}

int syscall_getprocessname(pid_t pid, char *name, size_t len, int *_errno)
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

    ADDR_CHECK_BUFFER_W(name, len);
    strncpy(name, p.v->name.c_str(), len);

    return 0;
}
