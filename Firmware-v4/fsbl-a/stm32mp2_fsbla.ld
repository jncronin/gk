/*

Executes from max 247 kiB of 256 kiB kiB SYSRAM @ 0x20000000
Save 4 kiB at end for stack

Add flash load address so J-Link can program properly

*/

ENTRY(Reset_Handler)

MEMORY
{
    SYSRAM          (xrw) : ORIGIN = 0x0e002600, LENGTH = 0x0e040000 - 0x0e002600
    FLASH                 : ORIGIN = 0x60000000, LENGTH = 0x20000
}

SECTIONS
{
  .text :
  {
    /* KEEP(*(.vtors)) */
    *(.text)           /* .text sections (code) */
    KEEP(*(.text.Reset_Handler)) /* Startup code */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(8);
    _etext = .;        /* define a global symbols at end of code */
  } >SYSRAM /* AT> FLASH */

  .rodata :
  {
    . = ALIGN(8);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(8);
  } >SYSRAM /* AT> FLASH */

  .ARM.extab (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(8);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(8);
  } >SYSRAM /* AT> FLASH */

  .ARM (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
   {
    . = ALIGN(8);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(8);
  } >SYSRAM /* AT> FLASH */

  .preinit_array  (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(8);
  } >SYSRAM /* AT> FLASH */

  .init_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(8);
  } >SYSRAM /* AT> FLASH */

  .fini_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(8);
  } >SYSRAM /* AT> FLASH */

  .data :
  {
    . = ALIGN(8);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */
    . = ALIGN(8);
    _edata = .;        /* define a global symbol at data end */

  } >SYSRAM /* AT> FLASH */

  . = ALIGN(8);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(8);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >SYSRAM /* AT> FLASH */

  _estack = 0x0e040000;
}
