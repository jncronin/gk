# gkos memory block allocation #

gkos uses the STM32H747 SRAM1/2/3 and SRAM4 ranges for its own internal data structures including the kernel malloc functions.   For all processes these ranges are uncacheable to facilitate simpler inter-processor communication.  All synchronization primitives live in these regions.

The rest of RAM (the 512 kB internal AXISRAM, the DTCM and ITCM, and external 512 Mbit SDRAM) is divided up for processes using a buddy system (inc/buddy.h).  This means that returned memory blocks sizes are aligned up to a power of 2, and the returned block is aligned upon its size.  Whilst potentially wasteful, this is useful in that these alignments are required by the Cortex-M4/7 MPU.

Memory blocks are used internally by gkos for kernel thread stacks, VFS caches and network packet caches.  For processes, they are used for the code/data sections, heap, stack, TLS sections, mmap regions (via the memalloc system call which supports setting cache attributes), as well as "hot code" sections.
