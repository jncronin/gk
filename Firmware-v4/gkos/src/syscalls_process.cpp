#include "syscalls_int.h"
#include "process.h"
#include "elf.h"
#include "threadproclist.h"
#include "completion_list.h"
#include <fcntl.h>
#include <sys/wait.h>

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
        ProcessList.Delete(proc->id);
        return -1;
    }

    // set arguments
    {
        CriticalGuard cg(proc->screen.sl);

        proc->screen.screen_pf = proc_info->pixel_format;
        proc->screen.screen_w = proc_info->screen_w;
        proc->screen.screen_h = proc_info->screen_h;
        proc->screen.screen_refresh = proc_info->screen_refresh;

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
    }

    {
        CriticalGuard cg(proc->env.sl);
        proc->env.cwd = std::string(proc_info->cwd);
        for(auto i = 0; i < proc_info->argc; i++)
        {
            std::string carg(proc_info->argv[i]);
            proc->env.args.push_back(carg);
        }
    }

    // keymap
    proc->keymap = proc_info->keymap;

    // Create startup thread
    auto t_t0 = Thread::Create(proc->name, proc_ep, nullptr, false, GK_PRIORITY_NORMAL, proc);
    if(!t_t0)
    {
        klog("process_create: Thread::Create failed\n");
        ProcessList.Delete(proc->id);
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
        CriticalGuard cg(ProcessList.sl, ProcessExitCodes.sl);

        // ensure pid is a child of ours
        auto pproc = ProcessList._get(pid);
        auto cp = GetCurrentProcessForCore();
        if(pproc->ppid != cp->id)
        {
            klog("waitpid: request for a process (%d: %s) which is not a child of the calling process (%d: %s)\n",
                pid, pproc->name.c_str(), cp->id, cp->name.c_str());
            *_errno = ECHILD;
            return -1;
        }

        auto [finished,pretval] = ProcessExitCodes._get(pid);

        if(finished)
        {
            if(retval)
                *retval = pretval;
            return pid;
        }
        {
            if(options & WNOHANG)
            {
                return 0;
            }
            else
            {
                pproc->waiting_threads.insert(GetCurrentThreadForCore()->id);
                Block();
            }
        }
    }
}
