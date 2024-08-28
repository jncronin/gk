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
    SDRAM (rwx)         : ORIGIN = 0x60000000, LENGTH = 65536K

    /* RTC backup registers */
    RTCREGS (rw)        : ORIGIN = 0x58004050, LENGTH = 128
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
    KEEP(*gk-test*(.data))
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

    . = ALIGN(1K);
    _stusb_data = .;
    *(.tusb_data)
    *(.tusb_data*)
    *(.tusb_bss)
    *(.tusb_bss*)
    *libtusb.a:*(.data)
    *libtusb.a:*(.data*)
    *libtusb.a:*(.bss)
    *libtusb.a:*(.bss*)
    _etusb_data = .;

    . = ALIGN(1K);
    _ssdt_data = .;
    *(.sdt_data)
    *(.sdt_data*)
    _esdt_data = .;

    . = ALIGN(1K);
    _sext4_data = .;
    *(.ext4_data)
    *(.ext4_data*)
    *liblwext4.a:*(.data)
    *liblwext4.a:*(.data*)
    *liblwext4.a:*(.bss)
    *liblwext4.a:*(.bss*)
    *libblockdev.a:*(.data)
    *libblockdev.a:*(.data*)
    *libblockdev.a:*(.bss)
    *libblockdev.a:*(.bss*)
    _eext4_data = .;

    . = ALIGN(1 << LOG2CEIL(_ewinc_data - _swinc_data));
    _swinc_data = .;
    *(.winc_data)
    *(.winc_data*)
    *libwinc.a:*(.data)
    *libwinc.a:*(.data*)
    *libwinc.a:*(.bss)
    *libwinc.a:*(.bss*)
    _ewinc_data = .;

    /*. = ALIGN(1K);
    _slwip_data = .;
    *(.lwip_data)
    *(.lwip_data*)
    *(.lwip_bss)
    *(.lwip_bss*)
    *liblwipcore.a:*(.data)
    *liblwipcore.a:*(.data*)
    *liblwipcore.a:*(.bss)
    *liblwipcore.a:*(.bss*)
    _elwip_data = .;*/


    . = ALIGN(1K);
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

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

  .dtcm :
  {
    . = ALIGN(4);
    _sdtcm = .;       /* create a global symbol at ccmram start */
    *(.dtcm)
    *(.dtcm*)
    
    . = ALIGN(4);
    _edtcm = .;       /* create a global symbol at ccmram end */
    *(.noinit_dtcm*)  /* for those bits we do not need to initialize */
    _edtcm2 = .;
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

  .net_data ALIGN(1 << LOG2CEIL(_elwip_data - _slwip_data)):
  {
    _slwip_data = .;
    *(.net_data)
    *(.net_data*)
    _elwip_init_data = .;
  } >AXISRAM AT> FLASH1
  
  .net_bss (NOLOAD):
  {
    _slwip_bss_data = .;
    *(.net_bss)
    *(.net_bss*)
    _elwip_data = .;
  } >AXISRAM
  _slwip_flash = LOADADDR(.net_data);



  /* Initial stack for CM7 */
  .cm7_stack 0x20001000 (NOLOAD)  :
  {
    _scm7_stack = .;
    . = . + 4K;
    _ecm7_stack = .;
  } >DTCM

  /* Initial stack for CM4 */
  . = ALIGN(4);
  
  /* reserve space for vtor table */
  .sram_vtors 0x30000000 (NOLOAD) :
  {
    _scm4_vtors = .;
    . = . + 4K;
  } >SRAM

  .cm4_stack 0x30001000 (NOLOAD) :
  {
    _scm4_stack = .;
    . = . + 4K;
    _ecm4_stack = .;

    /* fast access to TLS pointer(s) and timing bits */
    _tls_pointers = .;
    QUAD(0);
    QUAD(0);
    _cur_ms = .;
    QUAD(0);
    QUAD(0);
    toffset = .;
    QUAD(0);
    QUAD(0);
    QUAD(0);
    QUAD(0);

  } >SRAM

  .sram_data :
  {
    . = ALIGN(16);
    _ssramcm4 = .;
    *(.sram_data)
    *(.sram_data*)
    *(.sram_rdata)
    *(.sram_rdata*)
    _esramcm4 = .;
  } >SRAM AT> FLASH2
  _ssramcm4_flash = LOADADDR(.sram_data);

  .sram_bss (NOLOAD) :
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _ssram_bss = .;         /* define a global symbol at bss start */
    *(.sram_bss)
    *(.sram_bss*)
    *(COMMON)

    . = ALIGN(4);
    _esram_bss = .;         /* define a global symbol at bss end */
  } >SRAM

  . = ALIGN(4);
  .sram4 :
  {
    _ssram4 = .;
    *(.sram4)
    *(.sram4*)
    _esram4 = .;
  } >SRAM4 AT> FLASH1
  _ssram4_flash = LOADADDR(.sram4);

  .backupsram (NOLOAD):
  {
    _sbackupsram = .;
    *(.backupsram)
    *(.backupsram*)
    _ebackupsram = .;
  } >BACKUPSRAM

  .rtcregs (NOLOAD):
  {
    _srtcregs = .;
    *(.rtcregs)
    *(.rtcregs*)
    _ertcregs = .;
  } >RTCREGS

  /* defines for FreeRTOS MPU */
  PROVIDE(__FLASH_segment_start__ = 0x08000000);
  PROVIDE(__FLASH_segment_end__ = 0x08200000);
  PROVIDE(__SRAM_segment_start = 0x20000000);
  PROVIDE(__SRAM_segment_end = 0x40000000);
}
