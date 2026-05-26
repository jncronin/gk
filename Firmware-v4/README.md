This directory contains the firmware for the V4 (STM32MP2) edition of gk

# Boot Sequence #
GKOSv4 comprises 5 separate binaries that are all stored on a 4 MiB QSPI flash chip on the board (W25Q32JV).  The boot sequence comprises the following:

## FSBL-A ##
The boot pins on the STM32MP2 are configured as 0100b i.e. Cortex-A35 master, Serial NOR boot.  A pushbutton is provided on BOOT3 to toggle this to 1100b i.e. Development boot if required.  This should only be required for the initial flashing of FSBL-A because this binary immediately opens debug access to the device.

The FSBL-A binary is loaded directly by the STM32MP2 BOOTROM.  It is located at the start of FLASH (0x60000000) and executes in SYSRAM at location 0x0e002600.  It is signed using the STM32 Signing Tool which codes this load address into its header.

The main roles of FSBL-A are to set up the UART6 as a debug log and then initialise the OCTOSPI1 peripheral to interface with the FLASH at 50 MHz QSPI.  It includes a short start-up delay to allow debugger attach, then it jumps to SSBL-A which executes directly from FLASH.

## SSBL-A ##
This contains the bulk of the pre-kernel init code.  It executes on the Cortex-A35 without the MMU enabled directly from the FLASH at address 0x60020000.  It's main roles are:
- Set CPU clock to use PLL at reasonable speed
- Set up a kernel timebase using TIM3 at 10 ns resolution
- Set up RIFSC to allow access to the various memories
- Determine the reason the gk turned on by polling the STPMIC25 chip - if it was an auto turn on due, e.g. to USB charging attach, then don't proceed to start the rest of the system, just switch off again.
- Enable DDR (load firmware, perform training etc)
- Load the EL3 secure monitor (secure_monitor) to DDR and set up a EL3 lower half address
- Enable MMU and jump to the secure monitor

## secure_monitor ##
This is a small stub designed to ease the transition from EL3 non-paged (SSBL-A) through EL3 paged through to EL1 paged (gkos proper).  It is linked to run at virtual offset 0x20000000 in the EL3 address space, and is loaded from FLASH at 0x60300000.  It is separated from SSBL-A (which is non-paged) simply to make linking the various binaries easier.  It's main role is to load gkos from FLASH into DDR, set up its pagine structures and execute it.  It creates a small data structure (gkos_boot_interface) that tells gkos how much DDR memory is left for it to use after the various uses for page tables by SSBL-A and secure_monitor, as well as handoff addresses for the TIM3 clock to ensure it remains monotonic through the boot process.  Finally, it provides a syscall for gkos to use to start the AP (throughout the boot process the AP is held in wait loops within the various loaders and passed on to the next whenever the MP does).

## gkos ##
This is the main kernel binary and runs at 0xffffffff80000000 in the upper half of the EL1 virtual address space.  It is loaded from FLASH at offset 0x60060000.

The gkos architecture is documented elsewhere but it also has a driver for the CM33 subsystem which it runs with the cm33_firmware.  gkos sets up the CM33 to run this directly from FLASH.

## cm33_firmware ##
This is firmware to run on the Cortex-M33 core within the STM32MP2.  It runs directly from FLASH at address 0x60380000.  It comprises FreeRTOS for threaded execution and is mainly concerned with input devices for the gk including all buttons with the exception of the power button (this is handled via direct interface to the STPMIC25 instead), the ADCs for the various thumbsticks etc, the touch screen and the tilt sensor.  It exposes a command buffer and register interface to gkos running on the CA35 and is able to offload the processing for these various input devices and provide real-time filtering, especially of the tilt sensor inputs.

## Summary ##

| firmware       | FLASH location | FLASH max size | Execute location                       |
| -------------- | -------------- | -------------- | -------------------------------------- |
| FSBL-A         | 0x6000 0000    | 128 kiB        | SYSRAM, 0x0e002600 physical            |
| SSBL-A         | 0x6002 0000    | 256 kiB        | FLASH,  0x60020000 physical            |
| secure_monitor | 0x6030 0000    | 512 kiB        | DDR,    0x20000000 EL3 virtual         |
| gkos           | 0x6006 0000    | 2688 kiB       | DDR,    0xffffffff80000000 EL1 virtual |
| cm33_firmware  | 0x6038 0000    | 512 kiB        | FLASH,  0x60380000 physical (on CM33)  |

# Directory layout #
The 5 above mentioned binaries exist in their own directories with the relevant CMakeLists.txt files within each.  Other relevant directories include:

- assets: contains artwork used during early boot by gkos and scripts to convert these to binaries
- cmake: toolchain files for building
- common-a: various files shared by the different binaries for example logging, spinlock management etc
- Infineon_AIROC...: driver for the Wifi/BT chip on gkos
- ospi_flasher: open flash programmer to allow the FLASH chip to be programmed
- stm32-ddr-phy-binary: the firmware for the DDR.  Used by SSBL-A.  Submodule from STMicroelectronics repository.
- STM32CubeMP2: CMSIS definitions for the chip.  Submodule from STMicroelectronics repository.
- unit_tests: host-side tests of various components of gkos.
