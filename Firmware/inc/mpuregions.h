#ifndef MPUREGIONS_H
#define MPUREGIONS_H

/* We have 8 MPU regions available.
    In case of overlapping, higher IDs take priority.

    0 - MSP - in AXISRAM for CM7 stacks and in SRAM for CM4 stacks.
        64 kb to allow interrupt stacking.  Privilege RW, unprivilege no access.
    1 - FLASH - Privilege RX, unprivilege no access.
    2 - Peripherals - Privilege RW, unprivilege no access.
    3 - PSP - both rw.  Somewhere in AXISRAM.
    4 - SRAM4 - 64 kb for control structures (TCB, queues, semaphores etc).  Shared uncacheable.
        Privilege rw, unprivilege ro.
    5 - SDRAM - Normal.  RWX for both privileged and non-privileged.
    6 - Allocatable to user mode program (e.g. peripheral/SRAM access)
    7 - Allocatable to user mode program (e.g. peripheral/SRAM access)
*/

#endif
