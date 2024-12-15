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
#include <sys/types.h>
#include <unordered_set>

struct audio_conf
{
    unsigned int nbuffers = 0;
    unsigned int buf_size_bytes;
    unsigned int buf_ndtr;
    unsigned int wr_ptr;
    unsigned int rd_ptr;
    unsigned int rd_ready_ptr;
    uint32_t *silence;
    Thread *waiting_thread = nullptr;
    bool enabled = false;

    MemRegion mr_sound = InvalidMemregion();
    Spinlock sl_sound;
};

class Process
{
    public:
        Process();
        ~Process();
        
        std::string name;
        std::string window_title;
        std::vector<Thread *> threads;

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

        Spinlock sl;

        /* pthread TLS data */
        pthread_key_t next_key = 0;
        std::map<pthread_key_t, void (*)(void *)> tls_data;

        /* mmap regions */
        struct mmap_region { MemRegion mr; int fd; int is_read; int is_write; int is_exec; bool is_sync;
            bool is_stack = false; bool is_priv = false;
            mpu_saved_state to_mpu(unsigned int mpu_id) const; };
        std::map<uint32_t, mmap_region> mmap_regions;
        std::map<uint32_t, mmap_region>::iterator get_mmap_region(uint32_t addr, uint32_t len);

        /* The MPU regions defined for this process */
        mpu_saved_state p_mpu[16];
        unsigned int p_mpu_tls_id = 0;
        void UpdateMPURegionsForThreads();
        void UpdateMPURegionForThread(Thread *t);
        int AddMPURegion(const mmap_region &r);
        int AddMPURegion(const mpu_saved_state &r);
        int DeleteMPURegion(const mpu_saved_state &r);
        int DeleteMPURegion(const mmap_region &r);
        int DeleteMPURegion(const MemRegion &r);

        /* display modes */
        uint16_t screen_w = 640;
        uint16_t screen_h = 480;
        uint8_t screen_pf = 0;
        bool screen_ignore_vsync = false;
        bool screen_software_scale = false;

        /* current working directory */
        std::string cwd = "/";

        /* parameters passed to program */
        int argc;
        char **argv;

        /* gamepad mapping */
        bool gamepad_is_joystick = true;
        bool gamepad_is_keyboard = true;
        bool gamepad_is_mouse = false;

        bool tilt_is_joystick = true;
        bool tilt_is_keyboard = true;
        char tilt_keyboard_state = 0;

        bool joystick_is_joystick = false;
        unsigned int joy_last_x = 32768;
        unsigned int joy_last_y = 32768;

        bool touch_is_mouse = false;

        enum GamepadKey {
            Left = GK_KEYLEFT,
            Right = GK_KEYRIGHT,
            Up = GK_KEYUP,
            Down = GK_KEYDOWN,
            A = GK_KEYA,
            B = GK_KEYB,
            X = GK_KEYX,
            Y = GK_KEYY,
            VolUp = GK_KEYVOLUP,
            VolDown = GK_KEYVOLDOWN,
            Menu = GK_KEYMENU,
            Power = GK_KEYPOWER,
            Joy = GK_KEYJOY
        };

        unsigned int gamepad_buttons = 0;
        unsigned char mouse_buttons = 0;
        void HandleGamepadEvent(GamepadKey key, bool pressed, bool ongoing_press = false);
        void HandleTiltEvent(int x, int y);
        void HandleJoystickEvent(unsigned int x, unsigned int y);
        enum TouchEventType { Press, Release, Drag };
        void HandleTouchEvent(unsigned int x, unsigned int y, TouchEventType type);

        unsigned short int gamepad_to_scancode[GK_NUMKEYS] = { 
            80, 79, 82, 81,         /* L, R, U, D */
            224, 226, 225, 'z',     /* A, B, X, Y */
            0, 0,                   /* Vol Down, Vol Up */
            0, 0, 0, 0,             /* Tilt L, R, U, D */
            0, 0,                   /* Menu, Power */
            224,                    /* Joystick click */
            80, 79, 82, 81,         /* Joystick (as digital input) L, R, U, D */
        };

        /* Events */
        FixedQueue<Event, GK_NUM_EVENTS_PER_PROCESS> events;

        /* TLS segment, if any */
        bool has_tls = false;
        size_t tls_base = 0;
        size_t tls_filsz = 0;
        size_t tls_memsz = 0;

        /* PID support */
        pid_t pid;
        pid_t ppid;     // parent pid
        std::unordered_set<pid_t> child_processes;

        /* primitives owned by this process */
        std::unordered_set<Mutex *> owned_mutexes;
        std::unordered_set<Condition *> owned_conditions;
        std::unordered_set<RwLock *> owned_rwlocks;
        std::unordered_set<UserspaceSemaphore *> owned_semaphores;

        /* default stack size for new threads */
        size_t default_stack_size = 0;
        unsigned int stack_preference = STACK_PREFERENCE_TCM_RAM_SDRAM;

        /* hot code region */
        MemRegion mr_hot = InvalidMemregion();
        bool use_hot_region = true;

        /* audio buffers */
        audio_conf audio;

        /* graphics textures - mapped as WT */
        MemRegion mr_gtext = InvalidMemregion();
        size_t gtext_ptr = 0;
};

extern Process *focus_process;
extern Process kernel_proc;

/* support for atomically providing a new pid and associating with a process */
class ProcessList
{
    public:
        pid_t RegisterProcess(Process *p);
        void DeleteProcess(pid_t pid, int retval);
        int GetReturnValue(pid_t pid, int *retval, bool wait = false);
        Process *GetProcess(pid_t pid);
        pid_t GetParentProcess(pid_t pid);
        bool IsChildOf(pid_t child, pid_t parent);

    private:
        struct pval
        {
            Process *p;
            bool is_alive;
            int retval;
            std::unordered_set<Thread *> waiting_threads;
        };

        std::vector<pval> pvals;
};

extern ProcessList proc_list;

bool SetFocusProcess(Process *proc);

#endif
