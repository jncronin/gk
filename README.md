# gk and gkos #

This respository provides PCB schematics, firmware and debugging support for the gk handheld game console and its operating system, gkos.

## Overview ##

gk is a small, battery powered handheld gaming console running on an MCU (STM32H747) running at 480 MHz coupled to a 133 MHz 64 MB SDRAM.  It has a 640x480 pixel 24-bit colour screen, audio output (headphones/builtin speaker), accelerometer/gyrometer for tilt detection, USB interface for provisioning and WiFi network support.

The KiCAD schematics are available in the gk and gk-pcbv2 directories (the latter is a work-in-progress).

The firmware (gkos) is available in the Firmware directory and further documented in Firmware/doc

Despite being just a microcontroller, gk is able to run many native games at more-than-acceptable framerates including a PacMan clone (https://github.com/ebuc99/pacman) and sdl2-doom (https://github.com/AlexOberhofer/sdl2-doom) as well as playable versions of Mesa software-rendered 3D games including Tux Racer (https://tuxracer.sourceforge.net/).  It also supports emulation - Mednafen (https://mednafen.github.io/) Atari Lynx, GameBoy, NES and Sega Master System modules all run at >= 20 frames per second however more complex systems are slower.  The Hatari emulator (https://hatari.tuxfamily.org/) is also supported.
