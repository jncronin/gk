#include "stm32h7xx.h"
#include "gk_conf.h"
#include "osmutex.h"

static constexpr const size_t n_mdma_channels = 16;

SRAM4_DATA static Spinlock s_mdma;
SRAM4_DATA static void (*mdma_handlers[n_mdma_channels])();

void init_mdma()
{
    CriticalGuard cg(s_mdma);

    RCC->AHB3ENR |= RCC_AHB3ENR_MDMAEN;
    (void)RCC->AHB3ENR;

    for(unsigned int i = 0; i < n_mdma_channels; i++)
    {
        mdma_handlers[i] = nullptr;
    }

    NVIC_EnableIRQ(MDMA_IRQn);
}

void mdma_register_handler(void (*handler)(), unsigned int h_idx)
{
    CriticalGuard cg(s_mdma);

    if(h_idx >= n_mdma_channels)
        return;
    mdma_handlers[h_idx] = handler;
}

extern "C" void MDMA_IRQHandler()
{
    while(auto gisr = MDMA->GISR0)
    {
        for(unsigned int i = 0; i < n_mdma_channels; i++)
        {
            if(gisr & (1U << i))
            {
                if(mdma_handlers[i])
                    mdma_handlers[i]();
                else
                {
                    __asm__ volatile("bkpt \n" ::: "memory");
                    *(uint32_t *)(MDMA_BASE + 0x44U + 0x40U * i) = 0x1fU;   // clear all interrupt flags
                }
            }
        }
    }
}
