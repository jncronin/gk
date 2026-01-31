#include "process.h"
#include "pmem.h"
#include "vmem.h"
#include "thread.h"
#include "screen.h"
#include "ipi.h"
#include "cleanup.h"
#include "process_interface.h"
#include "_gk_memaddrs.h"
#include "_gk_scancodes.h"
#include "syscalls_int.h"
#include "cm33_interface.h"
#include <atomic>

static std::atomic<pid_t> focus_process = 0;

PProcess Process::Create(const std::string &_name, bool _is_privileged, PProcess parent)
{
    auto ret = ProcessList.Create();

    ret->name = _name;
    ret->is_privileged = _is_privileged;

    // don't allow unprivileged processes to create privileged ones
    if(parent && parent->is_privileged == false)
        ret->is_privileged = false;

    if(!ret->is_privileged)
    {
        // generate a user space paging setup
        auto ttbr0_reg = Pmem.acquire(VBLOCK_64k);
        if(ttbr0_reg.valid == false)
        {
            klog("Process: could not allocate ttbr0\n");
            while(true);
        }
        quick_clear_64((void *)PMEM_TO_VMEM(ttbr0_reg.base));
        ((volatile uint64_t *)PMEM_TO_VMEM(ttbr0_reg.base))[8191] = process_highest_pt.base |
            PAGE_ACCESS | DT_PT;

        {
            CriticalGuard cg(ret->owned_pages.sl);
            ret->owned_pages.add(ttbr0_reg);
        }

        ret->user_mem = std::make_unique<userspace_mem_t>();
        {
            MutexGuard cg(ret->user_mem->m);
            ret->user_mem->ttbr0 = ttbr0_reg.base | ((uint64_t)ret->id << 48);
        }
    }

    // inherit fds + environ
    if(parent)
    {
        {
            CriticalGuard cg(ret->open_files.sl, parent->open_files.sl);
            ret->open_files.f = parent->open_files.f;
        }

        {
            CriticalGuard cg(ret->env.sl, parent->env.sl);
            ret->env.envs = parent->env.envs;
        }

        ret->ppid = parent->id;
        ProcessList.SetPPID(ret->id, parent->id);
    }

    ret->window_title = _name;

    return ret;
}

void Process::owned_pages_t::add(const PMemBlock &b)
{
    auto start = b.base & ~(VBLOCK_64k - 1ULL);
    auto end = (b.base + b.length + (VBLOCK_64k - 1ULL)) & ~(VBLOCK_64k - 1ULL);

    while(start < end)
    {
        auto val = (uint32_t)(start >> 16);
        if(b.is_shared)
            val |= 0x80000000UL;
        p.insert(val);
        start += VBLOCK_64k;
    }
}

void Process::owned_pages_t::release_all()
{
    for(auto curp : p)
    {
        if(curp & 0x80000000UL)
            continue;
        auto addr = ((uint64_t)curp) << 16;
        PMemBlock pb;
        pb.base = addr;
        pb.is_shared = false;
        pb.length = VBLOCK_64k;
        pb.valid = true;
        Pmem.release(pb);
    }
}

void Process::Kill(id_t pid, int rc)
{
    CriticalGuard cg(ProcessList.sl);
    auto p = ProcessList._get(pid);
    if(!p.v)
    {
        klog("process: request to kill a process (%u) that doesn't exist", pid);
        return;
    }

    // restore focus process, if applicable
    if(GetFocusPid() == pid)
    {
        auto pparent = ProcessList._get(p.v->ppid);
        if(pparent.v)
        {
            SetFocusProcess(pparent.v);
        }
    }

    ProcessList._setexitcode(pid, rc);
    cg.unlock();

    for(auto t : p.v->threads)
    {
        Thread::Kill(t, (void *)0);
    }

    // Wake up any waiting threads
    for(auto t_wait : p.v->waiting_threads)
    {
        auto pt_wait = ThreadList.Get(t_wait);
        if(pt_wait.v)
        {
            pt_wait.v->blocking.unblock();
        }
    }

    CleanupQueue.Push(cleanup_message { .is_thread = false, .id = pid });
}

Process::~Process()
{
    // This should only be called by the cleanup thread finally deleting the entry in ProcessList
    klog("process: %u:%s destructor called\n", id, name.c_str());

    // Release resources
    owned_pages.release_all();

    owned_conditions.clear();
    owned_mutexes.clear();
    owned_rwlocks.clear();
    owned_semaphores.clear();
}

extern PMemBlock process_kernel_info_page;

static void set_joystick_mapping(char stick_map, int16_t *x, int16_t *y)
{
    auto kinfo = (gk_kernel_info *)PMEM_TO_VMEM(process_kernel_info_page.base);

    if((stick_map >= GK_STICK_JOY0) && (stick_map <= GK_STICK_JOY2))
    {
        auto stick_id = (unsigned int)(stick_map - GK_STICK_JOY0);
        auto x_axis_id = stick_id * 2;
        auto y_axis_id = x_axis_id + 1;

        kinfo->joystick_axes[x_axis_id] = x;
        kinfo->joystick_axes[y_axis_id] = y;

        if((y_axis_id + 1) > kinfo->joystick_naxes)
        {
            kinfo->joystick_naxes = y_axis_id + 1;
        }
    }
}

int SetFocusProcess(PProcess p)
{
    focus_process = p->id;

    // clear screen on process switch
    screen_clear_all_userspace();

    // update userspace input mapping
    auto kinfo = (gk_kernel_info *)PMEM_TO_VMEM(process_kernel_info_page.base);

    kinfo->joystick_naxes = 0;
    memset(kinfo->joystick_axes, 0, sizeof(kinfo->joystick_axes));

    set_joystick_mapping(p->keymap.left_stick,
        (int16_t *)(GK_JOYSTICK_ADDRESS),
        (int16_t *)(GK_JOYSTICK_ADDRESS + 4));
    set_joystick_mapping(p->keymap.right_stick,
        (int16_t *)(GK_JOYSTICKB_ADDRESS),
        (int16_t *)(GK_JOYSTICKB_ADDRESS + 4));
    set_joystick_mapping(p->keymap.tilt_stick,
        (int16_t *)(GK_TILT_ADDRESS),
        (int16_t *)(GK_TILT_ADDRESS + 4));

    unsigned int nbuttons = 0;
    for(auto i = 0U; i < GK_NUMKEYS; i++)
    {
        auto scancode = p->keymap.gamepad_to_scancode[i];
        if(scancode >= GK_GAMEPAD_BUTTON && scancode <= GK_GAMEPAD_END)
        {
            auto btn_id = scancode - GK_GAMEPAD_BUTTON;
            if(btn_id < 64 && (btn_id + 1U) > nbuttons)
            {
                nbuttons = btn_id + 1U;
            }
        }
    }
    kinfo->joystick_buttons = 0;
    kinfo->joystick_nbuttons = nbuttons;

    // enable/disable tilt if appropriate
    if(p->keymap.tilt_stick == GK_STICK_DIGITAL &&
        p->keymap.gamepad_to_scancode[GK_KEYTILTLEFT] == 0 &&
        p->keymap.gamepad_to_scancode[GK_KEYTILTRIGHT] == 0 &&
        p->keymap.gamepad_to_scancode[GK_KEYTILTUP] == 0 &&
        p->keymap.gamepad_to_scancode[GK_KEYTILTDOWN] == 0)
    {
        cm33_set_tilt(false);
    }
    else
    {
        cm33_set_tilt(true);
    }
    
    // restore palette if used

    return 0;
}

PProcess GetFocusProcess()
{
    return ProcessList.Get(focus_process).v;
}

id_t GetFocusPid()
{
    return focus_process;
}

