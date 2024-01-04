/* Complex set-up:
    M7 starts booting at 0x08000000 (option bytes FLASH_BOOT7_ADD0) - when boot pin is low
    M4 starts booting at 0x08100000 (option bytes FLASH_BOOT4_ADD0) - when boot pin is low
    Both are enabled in FLASH_OPTSR

    These addresses represent the beginning of flash banks 1 and 2 respectively
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