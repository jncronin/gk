#include "syscalls.h"
#include "syscalls_int.h"
#include "scheduler.h"
#include "_gk_proccreate.h"
#include <fcntl.h>
#include <cstring>
#include "SEGGER_RTT.h"
#include "process.h"
#include "elf.h"
#include "tilt.h"
#include "screen.h"
#include <sys/wait.h>
#include <unistd.h>

extern Process p_supervisor;

struct pct_params
{
    Thread *calling_thread;
    SimpleSignal *ss;
    int *ss_p;
    const char *fname;
    const proccreate_t *pcinfo;
    pid_t *pid_out;
};

static void *proccreate_thread(void *ptr);

int syscall_proccreate(const char *fname, const proccreate_t *pcinfo, pid_t *pid, int *_errno)
{
    if(!fname)
    {
        *_errno = EINVAL;
        return -1;
    }
    if(!pcinfo)
    {
        *_errno = EINVAL;
        return -1;
    }
    ADDR_CHECK_BUFFER_R(fname, 1);
    ADDR_CHECK_STRUCT_R(pcinfo);
    if(pid)
    {
        ADDR_CHECK_STRUCT_W(pid);
    }

    /* Create a separate thread that will do the heavy lifting here,
        needed because we cannot do complex work in a syscall */
    auto t = GetCurrentThreadForCore();

    auto param = new pct_params();
    if(!param)
    {
        klog("proccreate: coulnt'd create new pct_params\n");
        *_errno = ENOMEM;
        return -1;
    }
    param->calling_thread = t;
    param->ss = &t->ss;
    param->ss_p = (int *)&t->ss_p.ival1;
    param->fname = fname;
    param->pcinfo = pcinfo;
    param->pid_out = pid;

    // ensure pct stack is not in DTCM as this isn't handled by ext4_read yet
    auto pct_stack = memblk_allocate(4096, MemRegionType::SDRAM, "pct_stack");
    if(!pct_stack.valid) pct_stack = memblk_allocate(4096, MemRegionType::SRAM, "pct_stack");
    if(!pct_stack.valid) pct_stack = memblk_allocate(4096, MemRegionType::AXISRAM, "pct_stack");

    auto pct = Thread::Create(t->name + "_proccreate", proccreate_thread,
        param, true, t->base_priority, t->p, t->tss.affinity,
        pct_stack, t->tss.mpuss);
    
    if(pct == nullptr)
    {
        *_errno = ENOMEM;
        return -1;
    }

    Schedule(pct);

    // deferred return - performed by proccreate_thread
    return -2;
}

void *proccreate_thread(void *ptr)
{
    /* extract parameters */
    auto pct = reinterpret_cast<pct_params *>(ptr);
    auto fname = pct->fname;
    auto pcinfo = pct->pcinfo;
    auto t = pct->calling_thread;
    auto ss = pct->ss;
    auto ss_p = pct->ss_p;
    auto pid_out = pct->pid_out;
    delete pct;

    // get last part of path to use as process name
    const char *pname = fname;
    auto lastslash = strrchr(fname, '/');
    if(lastslash && strlen(lastslash + 1))
        pname = lastslash + 1;
    
    // open the file
    auto fd = deferred_call(syscall_open, fname, O_RDONLY, 0);
    if(fd < 0)
    {
        {
            klog("process_create: open(%s) failed %d\n", fname, errno);
        }
        *ss_p = EFAULT;
        ss->Signal();
        return nullptr;
    }

    // set defaults
    std::string cpname(pname);

    auto heap_size = pcinfo->heap_size;
    if(!heap_size) heap_size = 8192;

    auto stack_size = pcinfo->stack_size;
    if(!stack_size) stack_size = 4096;

    auto core_affinity = pcinfo->core_mask & 3;
    if(core_affinity == 0) core_affinity = Either;
    core_affinity |= (pcinfo->prefer_core_mask & 0x3) << 2;

    bool is_priv = pcinfo->is_priv != 0;
    if(!t->is_privileged)
        is_priv = false;    // unprivileged task cannot create a privileged one

    // create argc/argv vector
    std::vector<std::string> params;
    for(int i = 0; i < pcinfo->argc; i++)
    {
        params.push_back(std::string(pcinfo->argv[i]));
    }

    // create stack and heap
    auto stack = memblk_allocate_for_stack(stack_size, (CPUAffinity)core_affinity,
        std::string(pname) + " main thread stack",
        pcinfo->stack_preference);
    if(!stack.valid)
    {
        {
            klog("process_create: could not allocate stack of %d\n", stack_size);
        }
        close(fd);
        *ss_p = ENOMEM;
        ss->Signal();
        return nullptr;
    }
    auto heap = memblk_allocate(heap_size, MemRegionType::AXISRAM, "process heap");
    if(!heap.valid) heap = memblk_allocate(heap_size, MemRegionType::SRAM, "process heap");
    if(!heap.valid) heap = memblk_allocate(heap_size, MemRegionType::SDRAM, "process heap");
    if(!heap.valid)
    {
        {
            klog("process_create: could not allocate heap of %d\n", heap_size);
        }
        close(fd);
        memblk_deallocate(stack);
        *ss_p = ENOMEM;
        ss->Signal();
        return nullptr;
    }

    auto proc = new Process();
    if(!proc)
    {
        klog("proccreate: couldn't create new Process structure\n");
        memblk_deallocate(heap);
        memblk_deallocate(stack);
        close(fd);
        *ss_p = ENOMEM;
        ss->Signal();
        return nullptr;
    }
    proc->need_to_free = true;
    proc->name = cpname;
    proc->heap = heap;
    proc->default_affinity = (CPUAffinity)core_affinity;
    proc->heap_is_exec = pcinfo->heap_is_exec ? true : false;
    proc->is_priv = is_priv;
    proc->default_stack_size = stack_size;
    proc->stack_preference = pcinfo->stack_preference;
    if(pcinfo->osd)
    {
        std::string sosd(pcinfo->osd);
        proc->set_osd(sosd);
    }
    
    // load the elf file
    uint32_t epoint;
    auto eret = elf_load_fildes(fd, *proc, &epoint, cpname,
        stack.address + stack.length, params);
    close(fd);
    if(eret != 0)
    {
        {
            klog("process_create: elf_load_fildes() failed %d\n", eret);
        }
        memblk_deallocate(heap);
        memblk_deallocate(stack);
        delete proc;
        *ss_p = eret;
        ss->Signal();
        return nullptr;
    }

    // Initialize process MPU regions
    memcpy(proc->p_mpu, mpu_default, sizeof(mpu_default));
    proc->AddMPURegion({ .mr = heap, .fd = -1, .is_read = true, .is_write = true, .is_exec = proc->heap_is_exec, .is_sync = false });
    proc->AddMPURegion({ .mr = proc->code_data, .fd = -1, .is_read = true, .is_write = true, .is_exec = true, .is_sync = false });
    if(proc->mr_hot.valid)
    {
        proc->AddMPURegion({ .mr = proc->mr_hot, .fd = -1, .is_read = true, .is_write = false, .is_exec = true, .is_sync = false });
    }
    if(proc->has_tls)
    {
        proc->p_mpu_tls_id = proc->AddMPURegion({ .mr = InvalidMemregion() });
    }
    auto gtext_size = pcinfo->graphics_texture_size;
    if(pcinfo->graphics_texture_size)
    {
        proc->mr_gtext = memblk_allocate(gtext_size, MemRegionType::AXISRAM, std::string(pname) + " gtext");
        if(!proc->mr_gtext.valid)
            proc->mr_gtext = memblk_allocate(gtext_size, MemRegionType::SRAM, std::string(pname) + " gtext");
        if(!proc->mr_gtext.valid)
            proc->mr_gtext = memblk_allocate(gtext_size, MemRegionType::SDRAM, std::string(pname) + " gtext");

        if(proc->mr_gtext.valid)
        {
            if(proc->AddMPURegion({ .mr = proc->mr_gtext, .fd = -1, .is_read = true, .is_write = true, .is_exec = false, .is_sync = true }) < 0)
            {
                memblk_deallocate(proc->mr_gtext);
            }
        }
    }
    
    // create startup thread
    auto start_t = Thread::Create(cpname + "_0",
        (Thread::threadstart_t)(uintptr_t)epoint,
        (void *)proc->argc, is_priv, GK_PRIORITY_NORMAL,
        *proc, (CPUAffinity)core_affinity,
        stack,
        (void *)proc->argv);
    if(start_t == nullptr)
    {
        {
            klog("process_create: Thread::Create() failed\n");
        }
        memblk_deallocate(heap);
        memblk_deallocate(stack);
        delete proc;
        *ss_p = ENOMEM;
        ss->Signal();
        return nullptr;
    }

    memset(&proc->open_files[0], 0, sizeof(File *) * GK_MAX_OPEN_FILES);
    if(!t || &t->p == &kernel_proc)
    {
        // if started by kernel, just get stdin/out
        proc->open_files[STDIN_FILENO] = std::make_shared<SeggerRTTFile>(0, true, false);
        proc->open_files[STDOUT_FILENO] = std::make_shared<SeggerRTTFile>(0, false, true);
        proc->open_files[STDERR_FILENO] = std::make_shared<SeggerRTTFile>(0, false, true);
    }
    else
    {
        for(unsigned int i = 0; i < GK_MAX_OPEN_FILES; i++)
        {
            // acquire_fds numbered from 1 to allow 0 to be used as no inherit
            if(pcinfo->acquire_fds[i] > 0 && pcinfo->acquire_fds[i] <= GK_MAX_OPEN_FILES)
                proc->open_files[i] = t->p.open_files[pcinfo->acquire_fds[i]-1];
        }
    }

    // Set default pixel mode
    switch(pcinfo->pixel_format)
    {
        case GK_PIXELFORMAT_ARGB8888:
        case GK_PIXELFORMAT_RGB888:
        case GK_PIXELFORMAT_RGB565:
            proc->screen_pf = pcinfo->pixel_format;
            break;
        case GK_PIXELFORMAT_L8:
            // DMA2D cannot write to L8 buffers (but can read from them)
            proc->screen_pf = GK_PIXELFORMAT_RGB565;
            break;
        default:
            proc->screen_pf = GK_PIXELFORMAT_RGB565;  // something sensible
            break;
    }
    if(pcinfo->screen_w == 0 && pcinfo->screen_h == 0)
    {
        proc->screen_w = 640;
        proc->screen_h = 480;
    }
    else if(pcinfo->screen_w <= 160 && pcinfo->screen_h <= 120)
    {
        proc->screen_w = 160;
        proc->screen_h = 120;
    }
    else if(pcinfo->screen_w <= 320 && pcinfo->screen_h <= 240)
    {
        proc->screen_w = 320;
        proc->screen_h = 240;
    }
    else
    {
        proc->screen_w = 640;
        proc->screen_h = 480;
    }
    
    proc->screen_ignore_vsync = pcinfo->screen_ignore_vsync != 0;
    proc->screen_overwritten_each_frame = pcinfo->screen_overwritten_each_frame != 0;
    proc->audio_max_buffer_size = pcinfo->audio_max_buffer_size;

    // Set cwd
    if(pcinfo->cwd)
    {
        proc->cwd = pcinfo->cwd;
    }
    else if(lastslash)
    {
        proc->cwd = std::string(fname, lastslash - fname);
    }
    else
    {
        proc->cwd = "/";
    }

    // Set key mappings
    proc->gamepad_is_joystick = pcinfo->keymap.gamepad_is_joystick != 0;
    proc->gamepad_is_keyboard = pcinfo->keymap.gamepad_is_keyboard != 0;
    proc->gamepad_is_mouse = pcinfo->keymap.gamepad_is_mouse != 0;
    proc->tilt_is_keyboard = pcinfo->keymap.tilt_is_keyboard != 0;
    proc->tilt_is_joystick = pcinfo->keymap.tilt_is_joystick != 0;
    proc->joystick_is_joystick = pcinfo->keymap.joystick_is_joystick != 0;
    proc->touch_is_mouse = pcinfo->keymap.touch_is_mouse != 0;
    memcpy(proc->gamepad_to_scancode, pcinfo->keymap.gamepad_to_scancode,
        GK_NUMKEYS * sizeof(unsigned short int));

    // Set as focus if possible
    if(pcinfo->with_focus)
    {
        SetFocusProcess(proc);
    }

#if GK_MEMBLK_STATS
    memblk_stats();
#endif

    // bind to parent and report back if requested
    if(t)
    {
        t->p.child_processes.insert(proc->pid);
        proc->ppid = t->p.pid;
    }
    if(pid_out)
        *pid_out = proc->pid;

    *ss_p = 0;
    ss->Signal();

    // schedule startup thread
    Schedule(start_t);

    return nullptr;
}

int syscall_waitpid(pid_t pid, int *retval, int options, int *_errno)
{
    if(retval)
        ADDR_CHECK_STRUCT_W(retval);
    
    auto wret = proc_list.GetReturnValue(pid, retval,
        (options & WNOHANG) == 0);
    if(wret == -1)
    {
        *_errno = ECHILD;
        return -1;
    }
    else if(wret < 0)
    {
        return -3;  // we should block in GetReturnValue, therefore if awakened ask for retry
    }
    return pid;
}

int syscall_getheap(void **ptr, size_t *sz, int *_errno)
{
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;

    if(ptr)
    {
        ADDR_CHECK_STRUCT_W(ptr);
        *ptr = (void *)p.heap.address;
    }
    if(sz)
    {
        ADDR_CHECK_STRUCT_W(sz);
        *sz = (size_t)p.heap.length;
    }
    return 0;
}

pid_t syscall_get_focus_pid(int *_errno)
{
    if(!focus_process)
    {
        *_errno = EFAULT;
        return (pid_t)-1;
    }
    else
    {
        return focus_process->pid;
    }
}

pid_t syscall_get_proc_ppid(pid_t pid, int *_errno)
{
    return proc_list.GetParentProcess(pid);
}

int syscall_pushevents(pid_t pid, const Event *e, size_t nevents, int *_errno)
{
    auto p = proc_list.GetProcess(pid);
    if(!p)
    {
        *_errno = EINVAL;
        return (pid_t)-1;
    }
    ADDR_CHECK_BUFFER_R(e, sizeof(Event) * nevents);
    return p->events.Push(e, nevents);
}

int syscall_get_pid_valid(pid_t pid, int *_errno)
{
    return proc_list.GetProcess(pid) ? 1 : 0;
}
