#include <stm32h7xx.h>
#include "memblk.h"

// The following are the ends of all the input sections in the
//  relevant memory region.  We choose the highest to init the
//  memory manager.

// axisram
extern int _edata;
extern int _ebss;

// dtcm
extern int _edtcm_bss;
extern int _edtcm2;
extern int _ecm7_stack;

// sram
extern int _esram_bss;
extern int _ecm4_stack;

// sdram
extern int _esdram;

static void add_memory_region(uintptr_t base, uintptr_t size)
{

}

template<typename T> constexpr static T align_up(T val, T align)
{
    if(val % align)
    {
        val -= val % align;
        val += align;
    }
    return val;
}

void init_memblk()
{
    uintptr_t eaxisram = 0;
    uintptr_t edtcm = 0;
    uintptr_t esram = 0;
    uintptr_t esdram = 0;

    if((uintptr_t)&_edata > eaxisram)
        eaxisram = (uintptr_t)&_edata;
    if((uintptr_t)&_ebss > eaxisram)
        eaxisram = (uintptr_t)&_ebss;

    if((uintptr_t)&_edtcm_bss > edtcm)
        edtcm = (uintptr_t)&_edtcm_bss;
    if((uintptr_t)&_edtcm2 > edtcm)
        edtcm = (uintptr_t)&_edtcm2;
    if((uintptr_t)&_ecm7_stack > edtcm)
        edtcm = (uintptr_t)&_ecm7_stack;

    if((uintptr_t)&_esram_bss > esram)
        esram = (uintptr_t)&_esram_bss;
    if((uintptr_t)&_ecm4_stack > esram)
        esram = (uintptr_t)&_ecm4_stack;

    if((uintptr_t)&_esdram > esdram)
        esdram = (uintptr_t)&_esdram;

    eaxisram = align_up(eaxisram, 1024U);
    edtcm = align_up(edtcm, 1024U);
    esram = align_up(esram, 1024U);
    esdram = align_up(esdram, 1024U);

    // TODO: specify type of memory here
    add_memory_region(eaxisram, 0x24080000UL - eaxisram);
    add_memory_region(edtcm, 0x20020000 - edtcm);
    add_memory_region(esram, 0x30048000 - esram);
    add_memory_region(esdram, 0x60000000UL + 64 * 1024 * 1024 - esdram);
    add_memory_region(0x38000000UL, 0x10000);   // SRAM4
}