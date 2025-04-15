# gkos memory block allocation #

gkos uses the STM32H7S7 DTCM for its own internal data structures including the kernel malloc functions.  All synchronization primitives and process control blocks live in this region.

The rest of RAM (the internal AXISRAM, SRAM, ITCM, and external XSPI SDRAM) is divided up for processes using a buddy system (inc/buddy.h).  This means that returned memory blocks sizes are aligned up to a power of 2, and the returned block is aligned upon its size.  Whilst potentially wasteful, this is useful in that these alignments are required by the Cortex-M7 MPU.
