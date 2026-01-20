#ifndef CPU_H
#define CPU_H

#include "buddy.h"

class Cpu
{
    public:
        
};

void cpu_setup_vmem();
void cpu_start_local_timer();
void cpu_setup_userspace_permissions();

#endif
