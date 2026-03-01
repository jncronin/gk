#include "process.h"
#include "thread.h"
#include "scheduler.h"
#include "screen.h"
#include "cache.h"
#include "_gk_event.h"
#include "btnled.h"
#include "sound.h"
#include "pwr.h"
#include "gk_conf.h"
#include "syscalls_int.h"
#include <array>
#include "supervisor.h"
#include "wifi_airoc_if.h"
#include "process_interface.h"
#include "_gk_memaddrs.h"
#include "vmem.h"
#include "cm33_interface.h"
#include "pwr.h"

static void *supervisor_update_userspace_thread(void *);

bool init_supervisor()
{
    sched.Schedule(Thread::Create("update_userspace", supervisor_update_userspace_thread, nullptr, true,
        GK_PRIORITY_HIGH, p_kernel));
    return true;
}

Spinlock sl_sup_active;
static bool sup_active = false;
static std::vector<gk_supervisor_visible_region> sup_regs;
void supervisor_set_active(bool active, const gk_supervisor_visible_region *regs, size_t nregs)
{
    CriticalGuard cg(sl_sup_active);
    sup_active = active;
    sup_regs.clear();
    if(active)
    {
        for(auto i = 0U; i < nregs; i++)
        {
            sup_regs.push_back(regs[i]);
        }
    }
    else
    {
        auto p = GetCurrentProcessForCore();
        if(p->keymap.touch_is_mouse == false)
        {
            cm33_set_touch(false);
        }
    }
}

bool supervisor_is_active()
{
    CriticalGuard cg(sl_sup_active);
    return sup_active;
}

bool supervisor_is_active_for_point(unsigned int x, unsigned int y)
{
    CriticalGuard cg(sl_sup_active);
    if(!sup_active)
        return false;
    for(const auto &p : sup_regs)
    {
        if((x >= p.x) && (x < (p.x + p.w)) && (y >= p.y) && (y < (p.y + p.h)))
        {
            return true;
        }
    }
    return false;
}

int syscall_setsupervisorvisible(int visible, int screen, int *_errno)
{
    return -1;
}

extern PMemBlock process_kernel_info_page;

int supervisor_update_userspace()
{
    auto kinfo = (gk_kernel_info *)PMEM_TO_VMEM(process_kernel_info_page.base);

    kinfo->brightness = screen_get_brightness();
    kinfo->volume = sound_get_volume();
    kinfo->pwr_vbus = pmic_vbus_ok.load() ? 1 : 0;
    kinfo->soc = soc.load();

    extern std::unique_ptr<WifiAirocNetInterface> airoc_if;
    if(airoc_if && airoc_if->GetDeviceActive())
    {
        if(airoc_if->GetLinkActive())
        {
            kinfo->wifi_state = 2;
        }
        else
        {
            kinfo->wifi_state = 1;
        }
    }
    else
    {
        kinfo->wifi_state = 0;
    }

    // TODO
    kinfo->usb_state = 0;
    kinfo->bt_state = 0;

    kinfo->fps = screen_get_fps();
    kinfo->temp = tavg.load();
    kinfo->vsys = vsys.load();
    kinfo->psys = psys.load();
    kinfo->cpu_usage = sched.CPUUsage();

    extern PProcess p_gksupervisor;
    if(p_gksupervisor)
        p_gksupervisor->events.Push({ .type = Event::event_type_t::RefreshScreen });

    return 0;
}

void *supervisor_update_userspace_thread(void *)
{
    while(true)
    {
        supervisor_update_userspace();
        Block(clock_cur() + kernel_time_from_ms(1000));
    }
}
