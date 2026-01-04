/*

The ssbl runs from QSPI NOR flash at 60100000

Sysram is still in use for the stack, the bootrom_api structure and
 the AP executing code in an infinite loop.

Therefore, use RETRAM for our RAM needs
The AP does a WFI loop in SRAM2 (where it also has its stack)
 - need to set the first page of SRAM2 for secure access therefore

We use the second 128 kiB of flash.
*/

ENTRY(Reset_Handler)

MEMORY
{
    VDERAM          (xrw) : ORIGIN = 0x0e0a0000, LENGTH = 0x14000   /* stack space at end */
    VDERAM_SHARED   (rw)  : ORIGIN = 0x0e0bfe00, LENGTH = 512
    FLASH           (xrw) : ORIGIN = 0x60020000, LENGTH = 0x80000
}

SECTIONS
{
  .text :
  {
    KEEP(*(.text.Reset_Handler)) /* Startup code */
    /* KEEP(*(.vtors)) */
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(16);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  .rodata : ALIGN(16)
  {
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
  } >FLASH

  /* Store a fixed vtor table in QSPI for SSBL */
  .vtors : ALIGN(2048)
  {
    KEEP(*(.vtors))
  } >FLASH

  .ARM.extab (READONLY) : ALIGN(16)/* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH

  .ARM (READONLY) : ALIGN(16) /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
   {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array  (READONLY) : ALIGN(16) /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array (READONLY) :  ALIGN(16) /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array (READONLY) : ALIGN(16) /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* AP runs from RAM to allow easier flash updating whilst it is running.
      Store the function here. */
  .ap_text : ALIGN(16)
  {
    _sap_text = .;
    *(.ap_text)

    . = ALIGN(16);
    _eap_text = .;
  } >VDERAM AT> FLASH
  _sap_text_flash = LOADADDR(.ap_text);

  .data : ALIGN(16)
  {
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */
    . = ALIGN(16);
    _edata = .;        /* define a global symbol at data end */

  } >VDERAM AT> FLASH
  _sdata_flash = LOADADDR(.data);

  .bss : ALIGN(16)
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(16);
    _heap_start = .;
    . = . + 16*1024;
    _heap_end = .;

    . = ALIGN(16);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >VDERAM

  /* Provide stack addresses */
  _mp_sstack = 0x0e0b4000;
  _mp_estack = 0x0e0b6000;
  _ap_sstack = 0x0e0b6000;
  _ap_estack = 0x0e0b8000;

  /* Just provide addresses for s_ns_shared to stop it exploding .bin file */
  _s_ns_shared_start = 0x0e0bfe00;
  _cur_s = _s_ns_shared_start + 0;
  _tim_precision_ns = _s_ns_shared_start + 8;
}
