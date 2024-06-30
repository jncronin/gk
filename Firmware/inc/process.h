#ifndef PROCESS_H
#define PROCESS_H

class Thread;

#include "memblk.h"
#include "region_allocator.h"
#include <string>
#include <map>
#include "osmutex.h"
#include "osqueue.h"
#include "osfile.h"
#include "_gk_event.h"
#include "_gk_proccreate.h"
#include "gk_conf.h"

class Process
{
    public:
        Spinlock sl;
        
        std::string name;
        std::string window_title;
        SRAM4Vector<Thread *> threads;

        MemRegion heap;
        MemRegion code_data;
        bool heap_is_exec = false;
        bool is_priv = true;
        uint32_t thread_finalizer = 0;

        uint32_t brk = 0;

        int rc;
        bool for_deletion = false;

        File *open_files[GK_MAX_OPEN_FILES];

        CPUAffinity default_affinity;

        /* pthread TLS data */
        pthread_key_t next_key = 0;
        std::map<pthread_key_t, void (*)(void *)> tls_data;

        /* mmap regions */
        struct mmap_region { MemRegion mr; int fd; int is_read; int is_write; int is_exec; };
        std::map<uint32_t, mmap_region> mmap_regions;
        std::map<uint32_t, mmap_region>::iterator get_mmap_region(uint32_t addr, uint32_t len);

        /* display modes */
        uint16_t screen_w = 640;
        uint16_t screen_h = 480;
        uint8_t screen_pf = 0;
        bool screen_ignore_vsync = false;

        /* current working directory */
        std::string cwd = "/";

        /* parameters passed to program */
        int argc;
        char **argv;

        /* gamepad mapping */
        bool gamepad_is_joystick = true;
        bool gamepad_is_keyboard = true;
        bool gamepad_is_mouse = false;

        enum GamepadKey { Left, Right, Up, Down, A, B, X, Y };
        unsigned int gamepad_buttons = 0;
        unsigned char mouse_buttons = 0;
        void HandleGamepadEvent(GamepadKey key, bool pressed, bool ongoing_press = false);

        unsigned short int gamepad_to_scancode[GK_NUMKEYS] = { 80, 79, 82, 81, 224, 226, 225, 'z' };

        /* Events */
        FixedQueue<Event, GK_NUM_EVENTS_PER_PROCESS> events;

        /* TLS segment, if any */
        bool has_tls = false;
        size_t tls_base = 0;
        size_t tls_len = 0;
};

extern Process *focus_process;
extern Process kernel_proc;

#endif
