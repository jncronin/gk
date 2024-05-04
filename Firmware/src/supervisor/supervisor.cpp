#include "process.h"
#include "thread.h"
#include "scheduler.h"
#include "screen.h"
#include "cache.h"
#include "_gk_event.h"

#include "gk_conf.h"

SRAM4_DATA Process p_supervisor;
SRAM4_DATA static bool overlay_visible = false;

static void *supervisor_thread(void *p);

void init_supervisor()
{
    p_supervisor.argc = 0;
    p_supervisor.argv = nullptr;
    p_supervisor.brk = 0;
    p_supervisor.code_data = InvalidMemregion();
    p_supervisor.cwd = "/";
    p_supervisor.default_affinity = M7Only;
    p_supervisor.for_deletion = false;
    p_supervisor.heap = InvalidMemregion();
    p_supervisor.mr_params = InvalidMemregion();
    p_supervisor.name = "supervisor";
    p_supervisor.next_key = 0;
    for(int i = 0; i < GK_MAX_OPEN_FILES; i++)
        p_supervisor.open_files[i] = nullptr;
    p_supervisor.screen_h = 480;
    p_supervisor.screen_w = 640;
    
    auto t = Thread::Create("supervisor_main", supervisor_thread, nullptr, true, 2,
        p_supervisor, M7Only);
    Schedule(t);
}

void *supervisor_thread(void *p)
{
    // Init a overlay for us to toggle via the space button
    auto overlay = (char *)screen_get_overlay_frame_buffer();
    for(int y = 0; y < 240; y++)
    {
        for(int x = 0; x < 640; x++)
        {
            overlay[y * 640 + x] = 0;
        }
    }
    for(int y = 240; y < 480; y++)
    {
        for(int x = 0; x < 640; x++)
        {
            overlay[y * 640 + x] = 0xd4;       // nearly opaque red
        }
    }
    screen_flip_overlay();

    // process messages
    while(true)
    {
        Event e;
        while(!p_supervisor.events.Pop(&e));

        switch(e.type)
        {
            case Event::KeyDown:
                if(e.key == ' ')
                {
                    overlay_visible = !overlay_visible;
                    screen_set_overlay_alpha(overlay_visible ? 0xffU : 0x0U);
                }
                break;

            default:
                // ignore
                break;
        }
    }

    return nullptr;
}

bool supervisor_is_active(unsigned int *x, unsigned int *y, unsigned int *w, unsigned int *h)
{
    if(overlay_visible)
    {
        if(x) *x = 0;
        if(y) *y = 240;
        if(w) *w = 640;
        if(h) *h = 240;
    }
    return overlay_visible; 
}
