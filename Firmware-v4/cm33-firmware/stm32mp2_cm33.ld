/*

Executes from FLASH
Uses VDERAM (last 32 kiB) for data/stack

Add flash load address so J-Link can program properly

*/

ENTRY(Reset_Handler)

MEMORY
{
    SRAM1          (rw)   : ORIGIN = 0x30040000, LENGTH = 0x20000
    SRAM2_HEADER   (rw)   : ORIGIN = 0x30060000, LENGTH = 256
    SRAM2          (rw)   : ORIGIN = 0x30060000 + 256, LENGTH = 0x20000 - 256
    FLASH          (xr)   : ORIGIN = 0x603f8000, LENGTH = 0x8000
}

SECTIONS
{
  .text :
  {
    KEEP(*(.vtors))
    *(.text)           /* .text sections (code) */
    KEEP(*(.text.Reset_Handler)) /* Startup code */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

  .ARM.extab (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >FLASH

  .ARM (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
   {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH

  .preinit_array  (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  . = ALIGN(4);
  .sram2_header (NOLOAD) :
  {
    *(.sram2_header)
    *(.sram2_header*)
  } >SRAM2_HEADER

  . = ALIGN(4);
  .sram2 (NOLOAD) :
  {
    *(.sram2)
    *(.sram2*)
  } >SRAM2

  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */
    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */

  } >SRAM2 AT> FLASH
  _sdata_flash = LOADADDR(.data);

  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >SRAM2

  . = ALIGN(4);
  .sram1 (NOLOAD) :
  {
    *(.sram1)
    *(.sram1*)
  } >SRAM1

  _estack = 0x30080000;
}
