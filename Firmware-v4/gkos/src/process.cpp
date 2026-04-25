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
#include "supervisor.h"
#include "cm33_interface.h"
#include <atomic>

static std::atomic<pid_t> focus_process = 0;
extern PProcess p_gksupervisor;

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

void Process::owned_pages_t::add(const PMemBlock &b, bool is_gpu)
{
    std::shared_ptr<shared_page> sp = nullptr;
    auto start = b.base & ~(VBLOCK_64k - 1ULL);
    auto end = (b.base + b.length + (VBLOCK_64k - 1ULL)) & ~(VBLOCK_64k - 1ULL);
    auto length = end - start;

    if(b.is_shared)
    {
        klog("process: shared pages not yet implemented\n");
    }
    if(is_gpu)
    {
        auto ret = gpu_pages.p.AllocFixed({ (uintptr_t)start, (uintptr_t)length }, std::move(sp));
        if(ret == gpu_pages.p.end())
        {
            klog("process: failed to add pages %llx - %llx to gpu list: already present\n",
                start, start + length);
        }
        else
        {
            klog("process: ADDED %llx - %llx to gpu list\n", start, start + length);
            gpu_pages.npages += length / PAGE_SIZE;
        }
        return;
    }
    else if(b.length != PAGE_SIZE || b.is_shared)
    {
        auto ret = other_pages.p.AllocFixed({ (uintptr_t)start, (uintptr_t)length }, std::move(sp));
        if(ret == other_pages.p.end())
        {
            klog("process: failed to add pages %llx - %llx to other list: already present\n",
                start, start + length);
        }
        else
        {
            other_pages.npages += length / PAGE_SIZE;
        }
        return;
    }

    while(start < end)
    {
        auto val = (uint32_t)(start >> 16);
        p.insert(val);
        start += VBLOCK_64k;
    }
}

void Process::owned_pages_t::release_all()
{
    for(auto curp : p)
    {
        auto addr = ((uint64_t)curp) << 16;
        PMemBlock pb;
        pb.base = addr;
        pb.is_shared = false;
        pb.length = VBLOCK_64k;
        pb.valid = true;
        Pmem.release(pb);
    }
    p.clear();

    for(auto l : { &other_pages, &gpu_pages })
    {
        for(auto iter = l->p.begin(); iter != l->p.end();)
        {
            if(iter->second)
            {
                // shared pages not handled yet
                iter++;
                continue;
            }

            PMemBlock pb;
            pb.base = iter->first.start;
            pb.length = iter->first.length;
            pb.is_shared = false;
            pb.valid = true;
            Pmem.release(pb);

            l->npages -= iter->first.length / PAGE_SIZE;
            iter = l->p.erase(iter);
        }
    }
}

void Process::owned_pages_t::release(const PMemBlock &pb)
{
    if(!pb.valid)
        return;
    
    for(auto l : { &other_pages, &gpu_pages })
    {
        auto is_alloc = l->p.IsAllocated(pb.base);
        if(is_alloc == l->p.end())
            continue;

        if(l == &gpu_pages)
        {
            klog("process: REMOVED %llx - %llx from gpu list\n", pb.base, pb.base + pb.length);
        }
        
        if(is_alloc->first.start != pb.base ||
            is_alloc->first.length != pb.length)
        {
            klog("process: WARN: release only a portion off whole allocated physmem area\n");
        }
        if(is_alloc->second)
        {
            klog("process: WARN: shared pages not yet implemented\n");
        }
        l->p.erase(is_alloc);
        l->npages -= pb.length / PAGE_SIZE;
        return;
    }

    bool released_all = true;
    for(auto pstart = pb.base; pstart < (pb.base + pb.length); pstart += PAGE_SIZE)
    {
        auto is_alloc = p.find(pstart >> 16);
        if(is_alloc != p.end())
        {
            p.erase(is_alloc);
        }
        else
        {
            released_all = false;
        }
    }

    if(!released_all)
    {
        klog("process: WARN: tried to release memory %llx - %llx which we have no record of\n",
            pb.base, pb.base + pb.length);
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

    if((stick_map >= GK_STICK_JOY0) && (stick_map <= GK_STICK_JOY3))
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
    else if(stick_map == GK_STICK_MOUSE)
    {
        kinfo->mouse_axes[0] = x;
        kinfo->mouse_axes[1] = y;
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
    memset(kinfo->mouse_axes, 0, sizeof(kinfo->mouse_axes));

    set_joystick_mapping(p->keymap.left_stick,
        (int16_t *)(GK_JOYSTICK_ADDRESS),
        (int16_t *)(GK_JOYSTICK_ADDRESS + 4));
    set_joystick_mapping(p->keymap.right_stick,
        (int16_t *)(GK_JOYSTICKB_ADDRESS),
        (int16_t *)(GK_JOYSTICKB_ADDRESS + 4));
    set_joystick_mapping(p->keymap.tilt_stick,
        (int16_t *)(GK_TILT_ADDRESS),
        (int16_t *)(GK_TILT_ADDRESS + 4));
    set_joystick_mapping(p->keymap.throttle_stick,
        (int16_t *)(GK_THROTTLE_ADDRESS + 4),
        (int16_t *)(GK_THROTTLE_ADDRESS + 4));

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

    // enable/disable ctp
    if(p->keymap.touch_is_mouse || supervisor_is_active())
    {
        cm33_set_touch(true);
    }
    else
    {
        cm33_set_touch(false);
    }

    // restore palette if used


    // update cpu freq
    if(p->cpu_freq != clock_get_cpu())
    {
        clock_set_cpu_and_vddcpu(p->cpu_freq);
    }

    // tell p_supervisor about it
    if(p_gksupervisor)
    {
        p_gksupervisor->events.Push({ .type = Event::event_type_t::CaptionChange });
    }

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

