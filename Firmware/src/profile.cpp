#include "profile.h"
#include "gk_conf.h"
#include "logger.h"
#include "memblk.h"
#include <cstring>
#include "stm32h7xx.h"
#include "util.h"

static SRAM4_DATA MemRegion mr_profile;

static const constexpr unsigned int n_profile_counts = 16*1024;

static SRAM4_DATA unsigned int next_profile_id = 0;

void init_profile()
{
    mr_profile = memblk_allocate(n_profile_counts * sizeof(void *), MemRegionType::SRAM, "profile buffer");
    if(!mr_profile.valid)
        mr_profile = memblk_allocate(n_profile_counts * sizeof(void *), MemRegionType::AXISRAM, "profile buffer");
    if(!mr_profile.valid)
        mr_profile = memblk_allocate(n_profile_counts * sizeof(void *), MemRegionType::SDRAM, "profile buffer");
    if(!mr_profile.valid)
        mr_profile = memblk_allocate(n_profile_counts * sizeof(void *), MemRegionType::DTCM, "profile buffer");
    
    if(!mr_profile.valid)
    {
        klog("profile: unable to allocate buffer of %u bytes\n", n_profile_counts * sizeof(void *));
        return;
    }

    klog("profile: buffer at %08x-%08x\n", mr_profile.address, mr_profile.address + mr_profile.length);
    memset((void *)mr_profile.address, 0, n_profile_counts * sizeof(void *));

    // Set up LPTIM3 to sample PC
    RCC->APB4ENR |= RCC_APB4ENR_LPTIM3EN;
    (void)RCC->APB4ENR;
    LPTIM3->CR = 0;
    LPTIM3->CR = LPTIM_CR_RSTARE;
    (void)LPTIM3->CR;
    LPTIM3->CR = 0;

    LPTIM3->CFGR = 4UL << LPTIM_CFGR_PRESC_Pos;     // /16 => 1 MHz tick
    LPTIM3->IER = LPTIM_IER_ARRMIE;
    LPTIM3->CR = LPTIM_CR_ENABLE;
    LPTIM3->ARR = 199;                              // Reload every 200us => for 16k buffer gicves 3.2s

    NVIC_EnableIRQ(LPTIM3_IRQn);
    LPTIM3->CR = LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT;
}

extern "C" void LPTIM3_IRQHandler() __attribute__((naked));

extern "C" void LPTIM3_IRQHandler()
{
    /* Naked function, get (M/P)SP then jump to C code */
    __asm__ volatile
    (
        "mov r0, lr         \n"
        "and r0, #0xf       \n"
        "subs r0, #0xd      \n"
        "cbz r0, .L0%=      \n"
        "mrs r0, msp        \n"
        "b do_profile       \n"
        ".L0%=:             \n"
        "mrs r0, psp        \n"
        "b do_profile       \n"
        ::: "memory"
    );
}

extern "C" void do_profile(uint32_t *sp)
{
    auto retaddr = sp[6];
    auto arr = (uint32_t *)mr_profile.address;

    auto cpsr = DisableInterrupts();
    arr[next_profile_id++] = retaddr;
    if(next_profile_id >= n_profile_counts)
        next_profile_id = 0;
    RestoreInterrupts(cpsr);

    LPTIM3->ICR = LPTIM_ICR_ARRMCF;
    __DMB();
}
