/*
GKOS runs in -2 GiB memory space
*/

ENTRY(_kstart)

PHDRS
{
  text PT_LOAD ;
  rodata PT_LOAD ;
  data PT_LOAD ;
}

SECTIONS
{
  .text 0xffffffff80000000 :
  {
    KEEP(*(.vtors))
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(16);
    _etext = .;        /* define a global symbols at end of code */
  } :text

  . = ALIGN(CONSTANT (COMMONPAGESIZE));
  .rodata : ALIGN(16)
  {
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
  } :rodata

  .ARM.extab (READONLY) : ALIGN(16)/* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } :rodata

  .ARM (READONLY) : ALIGN(16) /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
   {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } :rodata

  .preinit_array  (READONLY) : ALIGN(16) /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } :rodata

  .init_array (READONLY) :  ALIGN(16) /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } :rodata

  .fini_array (READONLY) : ALIGN(16) /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } :rodata

  . = ALIGN(CONSTANT(COMMONPAGESIZE));
  .data : ALIGN(16)
  {
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */
    . = ALIGN(16);
    _edata = .;        /* define a global symbol at data end */

  } :data

  . = ALIGN(CONSTANT(COMMONPAGESIZE));
  .bss : ALIGN(16)
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(16);
    . = . + 65536;
    _estack = .;

    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } :data
}
