/* Complex set-up:
    M7 starts booting at 0x08000000 (option bytes FLASH_BOOT7_ADD0) - when boot pin is low
    M4 starts booting at 0x08100000 (option bytes FLASH_BOOT4_ADD0) - when boot pin is low
    Both are enabled in FLASH_OPTSR

    These addresses represent the beginning of flash banks 1 and 2 respectively

    The kernel predominantly runs on CM7, we set up the CM4 with a minimal startup code, busy spinning
        until told to run.
*/

ENTRY(Reset_Handler)

MEMORY
{
    /* CM7 memory banks */
    ITCM (rwx)          : ORIGIN = 0x00000000, LENGTH = 64K
    DTCM (rw)           : ORIGIN = 0x20000000, LENGTH = 128K
    AXISRAM (rwx)       : ORIGIN = 0x24000000, LENGTH = 512K
    FLASH1 (rx)         : ORIGIN = 0x08000000, LENGTH = 1024K

    /* CM4 memory banks */
    SRAM (rwx)          : ORIGIN = 0x30000000, LENGTH = 288K
    FLASH2 (rx)         : ORIGIN = 0x08100000, LENGTH = 1024K

    /* Shared banks */
    SRAM4 (rwx)         : ORIGIN = 0x38000000, LENGTH = 64K
    BACKUPSRAM (rw)     : ORIGIN = 0x38800000, LENGTH = 4K
    SDRAM (rwx)         : ORIGIN = 0x60000000, LENGTH = 65536K  /* SDRAM is remapped */
}

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        KEEP(*(.isr_vector*))
        . = ALIGN(4);
    } >FLASH1

    .isr_vec_cm4 :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vec_cm4))
        KEEP(*(.isr_vec_cm4*))
        . = ALIGN(4);
    } >FLASH2

  .itcm (NOLOAD) :
  {
    . = ALIGN(4);
    _sitcm = .;       /* create a global symbol at ccmram start */
    *(.itcm)
    *(.itcm*)
    
    . = ALIGN(4);
    _eitcm = .;       /* create a global symbol at ccmram end */
  } >ITCM AT> FLASH1

  _sitcm_flash = LOADADDR(.itcm);

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.text.*)
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH1

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH1

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH1
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH1

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH1
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH1
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH1

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data : 
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >AXISRAM AT> FLASH1

  . = ALIGN(4);
  .dtcm_bss (NOLOAD) :
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _sdtcm_bss = .;         /* define a global symbol at bss start */
    *(.dtcm_bss)
    *(.dtcm_bss*)
    *(COMMON)

    . = ALIGN(4);
    _edtcm_bss = .;         /* define a global symbol at bss end */
  } >DTCM

  .dtcm (NOLOAD) :
  {
    . = ALIGN(4);
    _sdtcm = .;       /* create a global symbol at ccmram start */
    *(.dtcm)
    *(.dtcm*)
    
    . = ALIGN(4);
    _edtcm = .;       /* create a global symbol at ccmram end */
    *(.noinit_dtcm*)  /* for those bits we do not need to initialize */
  } >DTCM AT> FLASH1
  _sdtcm_flash = LOADADDR(.dtcm);


  .sdram (NOLOAD) :
  {
    . = ALIGN(4);
    _ssdram = .;
    *(.sdram)
    *(.sdram*)
    . = ALIGN(4);
    _esdram = .;
  } >SDRAM

  
  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >AXISRAM

  /* Initial stack for CM7 */
  . = ALIGN(4);
  .cm7_stack (NOLOAD) :
  {
    _scm7_stack = .;
    . = . + 4K;
    _ecm7_stack = .;
  } >AXISRAM

  /* Initial stack for CM4 */
  . = ALIGN(4);
  .sram_bss (NOLOAD) :
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _ssram_bss = .;         /* define a global symbol at bss start */
    *(.sram_bss)
    *(.sram_bss*)
    *(COMMON)

    . = ALIGN(4);
    _esram_bss = .;         /* define a global symbol at bss end */
  } >DTCM
  

  . = ALIGN(4);
  .cm4_stack (NOLOAD) :
  {
    _scm4_stack = .;
    . = . + 4K;
    _ecm4_stack = .;
  } >SRAM
}