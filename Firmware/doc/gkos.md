# gkos #

This is the main documentation file for the gk operating system, gkos.

Whilst being developed for a MCU, gkos is not a real-time operating system but rather a general purpose multiprocessor multitasking operating system.  It has support for synchronization primitives, processes, threads, memory block allocation and inter-process memory protection, a virtual file system and networking.  It provides serializing drivers for the USB, SD, graphics and audio capabilities built into the STM32H747 chip, as well as WiFi networking through an externally connected ATWINC1500 module.

It has an extensive collection of userspace libraries, including a custom GCC toolchain, available at https://github.com/jncronin/gk-userland.  Notable userspace libraries include pthreads, SDL(1+2), Mesa, TCL, LVGL and several audio libraries for both waveform and MIDI music.

The kernel is configured by the gk_conf.h configuration file.  In particular, the main scheduler can be configured in unicore (Cortex-M7 only), AMP (both cores but no thread migration) and SMP (both cores with thread migration) modes.  Of these, the SMP configuration is the least well tested and, surprisingly, the least performant due to the need to ensure cache coherence between the M7 and M4 cores which is not performed in hardware.  Unicore/AMP modes are suggested.

Further documentation is provided within this directory regarding the relevant subsystems.
