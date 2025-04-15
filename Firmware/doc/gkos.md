# gkos #

This is the main documentation file for the gk operating system, gkos.

Whilst being developed for a MCU, gkos is not a real-time operating system but rather a general purpose multiprocessor multitasking operating system.  It has support for synchronization primitives, processes, threads, memory block allocation and inter-process memory protection, a virtual file system and networking.  It provides serializing drivers for the USB, SD, graphics and audio capabilities built into the STM32H7S7 chip, as well as WiFi networking through an externally connected ATWILC3000 module.

It has an extensive collection of userspace libraries, including a custom GCC toolchain, available at https://github.com/jncronin/gk-userland.  Notable userspace libraries include pthreads, SDL(1+2), Mesa, TCL, LVGL, the Squirrel scripting language and several audio libraries for both waveform and MIDI music.

The kernel is configured by the gk_conf.h configuration file.

Further documentation is provided within this directory regarding the relevant subsystems.
