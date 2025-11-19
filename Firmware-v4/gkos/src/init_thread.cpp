#include "process.h"

#include "logger.h"
#include "syscalls_int.h"
#include <fcntl.h>

void *init_thread(void *)
{
    // load a test userspace thread from memory

    auto proc_fd = syscall_open("/dev/ramdisk/test.bin", O_RDONLY, 0, &errno);
    if(proc_fd < 0)
    {
        klog("init: failed to open test process\n");
    }

    klog("init: opened test process id %d\n", proc_fd);

    while(true);
}
