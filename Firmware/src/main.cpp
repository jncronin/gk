#include <stm32h7xx.h>
#include "memblk.h"
#include "thread.h"
#include "scheduler.h"

void system_init_cm7();
void system_init_cm4();

static void idle_thread(void *p);

__attribute__((section(".sram4")))Scheduler s;

int main()
{
    system_init_cm7();
    init_memblk();

    s.Schedule(Thread::Create("idle_cm7", idle_thread, (void*)0, true, 0, CPUAffinity::M7Only, 512));
    s.Schedule(Thread::Create("idle_cm4", idle_thread, (void*)1, true, 0, CPUAffinity::M4Only, 512));

    // Set MCO2 (PC9 AF0 = SDMMC1_D1), MCO1 = PA8
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOCEN;
    (void)RCC->AHB4ENR;
    {
        auto gc = GPIOC->MODER;
        gc &= ~(3UL << 18);
        gc |= 2UL << 18;
        GPIOC->MODER = gc;

        auto af = GPIOC->AFR[1];
        af &= ~(0xFUL << 4);
        GPIOC->AFR[1] = af;

        GPIOC->OSPEEDR |= (3UL << 18);

        auto ga = GPIOA->MODER;
        ga &= ~(3UL << 16);
        ga |= 2UL << 16;
        GPIOA->MODER = ga;

        auto afa = GPIOA->AFR[1];
        afa &= ~(0xFUL << 0);
        GPIOA->AFR[1] = afa;

        GPIOA->OSPEEDR |= (3UL << 16);
    }
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_MCO2_Msk) | (3UL << RCC_CFGR_MCO2_Pos);
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_MCO1_Msk) | (2UL << RCC_CFGR_MCO1_Pos);
    RCC->CFGR |= (10UL << RCC_CFGR_MCO2PRE_Pos) | (10UL << RCC_CFGR_MCO1PRE_Pos);

    // Get systick to flash an LED for now (BTNLED_R = PC6)
    {
        auto gc = GPIOC->MODER;
        gc &= ~(3UL << 12);
        gc |= 1UL << 12;
        GPIOC->MODER = gc;
    }
    SysTick->CTRL = 0;
    SysTick->VAL = 0;
    SysTick->LOAD = 47999;      // assume 48 MHz => 1 kHz tick
    __enable_irq();
    SysTick->CTRL = 7UL;

    while(true);
    s.StartForCurrentCore();

    return 0;
}

extern "C" int main_cm4()
{
    system_init_cm4();
    
    // TODO: wait upon startup - should signal from idle_cm7
    while(true)
    {
        __WFI();
    }

    s.StartForCurrentCore();

    return 0;
}

void idle_thread(void *p)
{
    (void)p;
    while(true)
    {
        __WFI();
    }
}

/* The following just to get it to compile */
static constexpr uint32_t max_sbrk = 8192;
__attribute__((section(".sram4"))) static uint8_t malloc_buf[max_sbrk];
static uint32_t cur_sbrk = 0;

extern "C" void * _sbrk(int n)
{
    if(n < 0) n = 0;
    auto nn = static_cast<uint32_t>(n);

    if((n + cur_sbrk) > max_sbrk)
    {
        return (void *)-1;
    }
    auto old_brk = cur_sbrk;

    cur_sbrk += nn;

    return &malloc_buf[old_brk];
}

volatile uint32_t cfsr, hfsr;
extern "C" void HardFault_Handler()
{
    cfsr = SCB->CFSR;
    hfsr = SCB->HFSR;
    
    while(true);
}

extern "C" void MemManage_Handler()
{
    while(true);
}

extern "C" void BusFault_Handler()
{
    while(true);
}

extern "C" void UsageFault_Handler()
{
    while(true);
}

extern "C" void SysTick_Handler()
{
    if(GPIOC->ODR & (1UL << 6))
    {
        GPIOC->BSRR = 1UL << 22;
    }
    else
    {
        GPIOC->BSRR = 1UL << 6;
    }
}
