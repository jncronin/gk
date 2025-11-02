/*

The ssbl runs from QSPI NOR flash at 60100000

Sysram is still in use for the stack, the bootrom_api structure and
 the AP executing code in an infinite loop.

Therefore, use RETRAM for our RAM needs
The AP does a WFI loop in SRAM2 (where it also has its stack)
 - need to set the first page of SRAM2 for secure access therefore

We use the 2nd 1 MiB of flash for our needs.  Leave the last 2 MiB for CM33 code.
*/

ENTRY(Reset_Handler)

MEMORY
{
    RETRAM          (xrw) : ORIGIN = 0x0e080000, LENGTH = 64*1024
    FLASH           (xrw) : ORIGIN = 0x60100000, LENGTH = 1*1024*1024
    SRAM2           (xrw) : ORIGIN = 0x0e060000, LENGTH = 128*1024
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

    . = ALIGN(8);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  .rodata :
  {
    . = ALIGN(8);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(8);
  } >FLASH

  .ARM.extab (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(8);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(8);
  } >FLASH

  .ARM (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
   {
    . = ALIGN(8);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(8);
  } >FLASH

  .preinit_array  (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(8);
  } >FLASH

  .init_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(8);
  } >FLASH

  .fini_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(8);
  } >FLASH

  /* AP runs from RAM to allow easier flash updating whilst it is running.
      Store the function here. */
  . = ALIGN(8);
  .ap_text :
  {
    _sap_text = .;
    *(.ap_text)

    . = ALIGN(8);
    _eap_text = .;
  } >SRAM2 AT> FLASH
  _sap_text_flash = LOADADDR(.ap_text);

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

  } >RETRAM AT> FLASH
  _sdata_flash = LOADADDR(.data);

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
  } >RETRAM
}
