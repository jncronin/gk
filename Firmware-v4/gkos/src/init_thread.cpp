#include "process.h"

#include "logger.h"
#include "syscalls_int.h"
#include "elf.h"
#include <fcntl.h>

PProcess p_test;

void *init_thread(void *)
{
    // load a test userspace thread from memory

    auto proc_fd = syscall_open("/gkmenu-0.1.1-gkv4/bin/gkmenu", O_RDONLY, 0, &errno);
    if(proc_fd < 0)
    {
        klog("init: failed to open test process\n");
    }

    klog("init: opened test process id %d\n", proc_fd);
    p_test = std::make_shared<Process>("test", false);

    // give test access to uart
    {
        CriticalGuard cg(p_test->open_files.sl);
        auto fd2 = p_test->open_files.get_fixed_fildes(2);
        p_test->open_files.f[fd2] = std::make_shared<UARTFile>(false, true);
    }

    Thread::threadstart_t test_ep;
    auto ret = elf_load_fildes(proc_fd, p_test, &test_ep);
    klog("init: elf_load_fildes: ret: %d, ep: %llx\n", ret, test_ep);

    if(ret == 0)
    {
        auto t_test = Thread::Create("gkmenu", test_ep, nullptr, false, GK_PRIORITY_NORMAL, p_test);
        if(t_test)
        {
            klog("init: thread created, scheduling it\n");
            sched.Schedule(t_test);
        }    
    }

    while(true);
}
