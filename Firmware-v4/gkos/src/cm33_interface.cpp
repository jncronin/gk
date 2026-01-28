#include <stm32mp2xx.h>
#include "vmem.h"
#include "gic.h"
#include "cm33_interface.h"
#include "cm33_data.h"
#include "pins.h"
#include <cstring>
#include "osqueue.h"
#include "process.h"
#include "supervisor.h"

#define RISAF2_VMEM ((RISAF_TypeDef *)PMEM_TO_VMEM(RISAF2_BASE))
#define RAMCFG_VMEM ((RAMCFG_TypeDef *)PMEM_TO_VMEM(RAMCFG_BASE))
#define RCC_VMEM ((RCC_TypeDef *)PMEM_TO_VMEM(RCC_BASE))
#define RISAB1_VMEM ((RISAB_TypeDef *)PMEM_TO_VMEM(RISAB1_BASE))
#define RISAB2_VMEM ((RISAB_TypeDef *)PMEM_TO_VMEM(RISAB2_BASE))
#define RISAB3_VMEM ((RISAB_TypeDef *)PMEM_TO_VMEM(RISAB3_BASE))
#define RISAB4_VMEM ((RISAB_TypeDef *)PMEM_TO_VMEM(RISAB4_BASE))
#define RISC_VMEM ((RISC_TypeDef *)PMEM_TO_VMEM(RISC_BASE))
#define CA35SYSCFG_VMEM ((CA35SYSCFG_TypeDef *)PMEM_TO_VMEM(CA35SYSCFG_BASE))
#define EXTI1_VMEM ((EXTI_TypeDef *)PMEM_TO_VMEM(EXTI1_BASE))
#define PWR_VMEM ((PWR_TypeDef *)PMEM_TO_VMEM(PWR_BASE))
#define RAMCFG_SRAM1_VMEM ((RAMCFG_TypeDef *)PMEM_TO_VMEM(RAMCFG_SRAM1_BASE))
#define RAMCFG_SRAM2_VMEM ((RAMCFG_TypeDef *)PMEM_TO_VMEM(RAMCFG_SRAM2_BASE))

#define GPIOC_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOC_BASE))
#define GPIOG_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOG_BASE))
#define GPIOH_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOH_BASE))
#define GPIOJ_VMEM ((GPIO_TypeDef *)PMEM_TO_VMEM(GPIOJ_BASE))

static volatile cm33_data_kernel *dk = (volatile cm33_data_kernel *)0xfffffd0030060000;
static SimpleSignal cm33_event;

static float input_tilt_centre = 30.0f;
cm33_joy_calib input_joy_calib[2] =
{
    { .left = -32767, .right = 32767, .top = 32767, .bottom = -32767, .middle_x = 0, .middle_y = 0 },
    { .left = -32767, .right = 32767, .top = 32767, .bottom = -32767, .middle_x = 0, .middle_y = 0 },
};
static bool input_tilt_enable = true;

static void cm33_irq(exception_regs *, uint64_t);

static void sram_setup()
{
    // Enable SRAM1 and 2
    RCC_VMEM->SRAM1CFGR |= RCC_SRAM1CFGR_SRAM1EN;
    RCC_VMEM->SRAM2CFGR |= RCC_SRAM2CFGR_SRAM2EN;
    __asm__ volatile ("dsb sy\n" ::: "memory");

    // RISAB3 (SRAM1) and RISAB4 (SRAM2) each have 32 pages of 8 blocks each
    for(auto risab : { RISAB1_VMEM, RISAB2_VMEM, RISAB3_VMEM, RISAB4_VMEM })
    {
        for(unsigned int page = 0; page < 32; page++)
        {
            // Secure accesses only
            risab->PGSECCFGR[page] = 0xffU;
            // Privileged accesses only
            risab->PGPRIVCFGR[page] = 0xffU;
            // CID filtering
            risab->PGCIDCFGR[page] = RISAB_PGCIDCFGR_CFEN;
        }

        // Give unprivileged (SRAM1) or privileged (SRAM2) read/write access to CIDs 0,1,2
        for(unsigned int cid = 0; cid < 3; cid++)
        {
            risab->CID[cid].PRIVCFGR = (risab == RISAB3_VMEM) ? 0U : 0xffffffffU;
            risab->CID[cid].RDCFGR = 0xffffffffU;
            risab->CID[cid].WRCFGR = 0xffffffffU;
        }
    }

    // Wipe SRAM1 and SRAM2
    RAMCFG_SRAM1_VMEM->ERKEYR = 0xca;
    __asm__ volatile("dsb sy\n" ::: "memory");
    RAMCFG_SRAM1_VMEM->ERKEYR = 0x53;
    __asm__ volatile("dsb sy\n" ::: "memory");
    RAMCFG_SRAM1_VMEM->CR = RAMCFG_CR_SRAMER;
    __asm__ volatile("dsb sy\n" ::: "memory");
    while(RAMCFG_SRAM1_VMEM->ISR & RAMCFG_ISR_SRAMBUSY);
    __asm__ volatile("dsb sy\n" ::: "memory");

    RAMCFG_SRAM2_VMEM->ERKEYR = 0xca;
    __asm__ volatile("dsb sy\n" ::: "memory");
    RAMCFG_SRAM2_VMEM->ERKEYR = 0x53;
    __asm__ volatile("dsb sy\n" ::: "memory");
    RAMCFG_SRAM2_VMEM->CR = RAMCFG_CR_SRAMER;
    __asm__ volatile("dsb sy\n" ::: "memory");
    while(RAMCFG_SRAM2_VMEM->ISR & RAMCFG_ISR_SRAMBUSY);
    __asm__ volatile("dsb sy\n" ::: "memory");
}

static void reset_cm33()
{
    klog("cm33: reset\n");

    /* CM33 runs code from OSPI and uses SRAM2 as data area and SRAM1 for comms with A35

        It uses TIM6 as a 200 Hz/5 ms timer (it sets this up itself)

        It has access to I2C1 for interface with the IO expander and LSM6DSL

        It has access to ADC1 for joystick readings (we save ADC2 for the CA35 because
            it allows VCORE/CPU etc voltage readings too)
    */

    /* Hold CM33 in reset */
    RCC_VMEM->CPUBOOTCR &= ~RCC_CPUBOOTCR_BOOT_CPU2;
    (void)RCC_VMEM->CPUBOOTCR;
    RCC_VMEM->C2RSTCSETR = RCC_C2RSTCSETR_C2RST;
    while(RCC_VMEM->C2RSTCSETR & RCC_C2RSTCSETR_C2RST);

    // Give CM33 secure access to OSPI
    RISAF2_VMEM->REG[0].CFGR = 0;
    RISAF2_VMEM->REG[0].STARTR = 0x3f8000;
    RISAF2_VMEM->REG[0].ENDR = 0x3fffff;
    RISAF2_VMEM->REG[0].CIDCFGR = 7U;    // TRACE/CPU0/CPU1
    RISAF2_VMEM->REG[0].CFGR = 0xf0101;  // all privilege, secure, enable

    sram_setup();

    // Enable the ADC, use root clock 46 (HSI64)
    PWR_VMEM->CR1 |= PWR_CR1_ASV;
    __asm__ volatile("dsb sy\n" ::: "memory");

    RCC_VMEM->FINDIVxCFGR[46] = 0;       // disable
    RCC_VMEM->PREDIVxCFGR[46] = 0;       // div 1
    RCC_VMEM->XBARxCFGR[46] = 0x48;      // enabled, hsi64_ker_ck
    RCC_VMEM->FINDIVxCFGR[46] = 0x40;    // enabled, div 1
    __asm__ volatile("dsb sy\n" ::: "memory");

    RCC_VMEM->ADC12CFGR |= RCC_ADC12CFGR_ADC12EN;
    RCC_VMEM->ADC12CFGR &= ~RCC_ADC12CFGR_ADC12RST;
    __asm__ volatile("dsb sy\n" ::: "memory");

    /* Pins for CM33 to use:
        JOY_A_X             ANA0        V5          ADC1_INP0, ADC2_INP0
        JOY_A_Y             ANA1        V6          ADC1_INP1, ADC2_INP1

        JOY_B_X             PC9         U8          ADC1_INP8, ADC2_INP8
        JOY_B_Y             PG4         AA4         ADC1_INP4, ADC2_INP4

        BTN_MCU_VOLUP       PH2         V13
        BTN_MCU_VOLDOWN     PJ0         U15
    */
    const pin JOY_B_X { GPIOC_VMEM, 9 };
    const pin JOY_B_Y { GPIOG_VMEM, 4 };
    const pin BTN_MCU_VOLUP { GPIOH_VMEM, 2 };
    const pin BTN_MCU_VOLDOWN { GPIOJ_VMEM, 0 };

    RCC_VMEM->GPIOCCFGR |= RCC_GPIOCCFGR_GPIOxEN;
    RCC_VMEM->GPIOGCFGR |= RCC_GPIOGCFGR_GPIOxEN;
    RCC_VMEM->GPIOHCFGR |= RCC_GPIOHCFGR_GPIOxEN;
    RCC_VMEM->GPIOJCFGR |= RCC_GPIOJCFGR_GPIOxEN;
    __asm__ volatile("dsb sy\n" ::: "memory");

    JOY_B_X.set_secure(false);
    JOY_B_Y.set_secure(false);
    JOY_B_X.set_as_analog();
    JOY_B_Y.set_as_analog();
    BTN_MCU_VOLUP.set_as_input();
    BTN_MCU_VOLDOWN.set_as_input();

    // Start up the CM33 code running from QSPI @ 0x603fc000
    // Boot in secure mode
    RCC_VMEM->SYSCPU1CFGR |= RCC_SYSCPU1CFGR_SYSCPU1EN;
    (void)RCC_VMEM->SYSCPU1CFGR;
    CA35SYSCFG_VMEM->M33_TZEN_CR |= CA35SYSCFG_M33_TZEN_CR_CFG_SECEXT;
    CA35SYSCFG_VMEM->M33_INITSVTOR_CR = 0x603f8000;

    /* Start CPU2 */
    RCC_VMEM->CPUBOOTCR |= RCC_CPUBOOTCR_BOOT_CPU2;

    /* Allow CM33 to send interrupts to us - EXTI1 channel 64 */
    EXTI1_VMEM->RTSR3 |= (1U << 0);
    EXTI1_VMEM->C1IMR3 |= (1U << 0);

    unsigned int cm33_sev_irq = 252U;
    gic_set_handler(cm33_sev_irq, cm33_irq);
    gic_set_target(cm33_sev_irq, GIC_ENABLED_CORES);
    gic_set_enable(cm33_sev_irq);
}

static void cm33_send_cmd(uint32_t cmd)
{
    while(dk->cr)
    {
        if(dk->sr & CM33_DK_SR_FAIL)
            return;
    }
    dk->cr = cmd;
    while(dk->cr)
    {
        if(dk->sr & CM33_DK_SR_FAIL)
            return;
    }
}

static void cm33_handle_message(uint32_t msg)
{
    switch(msg & CM33_DK_MSG_MASK)
    {
        case CM33_DK_MSG_PRESS:
        case CM33_DK_MSG_RELEASE:
        case CM33_DK_MSG_LONGPRESS:
        case CM33_DK_MSG_REPEAT:
            {
                auto p = (supervisor_is_active() && p_supervisor) ? p_supervisor : GetFocusProcess();
                if(p)
                {
                    p->HandleInputEvent(msg);
                }
            }
            break;

        default:
            klog("cm33: unhandled message: %x\n", msg);
            break;
    }
}

static void *cm33_manager_thread(void *)
{
    bool is_init = false;
    reset_cm33();

    while(true)
    {
        if(!is_init)
        {
            if(cm33_event.Wait(SimpleSignal::Set, 0, clock_cur() + kernel_time_from_ms(100)))
            {
                if(dk->sr & CM33_DK_SR_READY)
                {
                    is_init = true;
                    klog("cm33: status %x\n", dk->sr);
                }
                else
                {
                    continue;
                }
            }
            else
            {
                klog("cm33: did not reset, trying again\n");
                reset_cm33();
                continue;
            }
        }

        cm33_event.Wait(SimpleSignal::Set, 0, clock_cur() + kernel_time_from_ms(20));
        if(dk->sr & CM33_DK_SR_FAIL)
        {
            klog("cm33: fail, resetting\n");
            is_init = false;
            reset_cm33();
        }

        // program calibration data, if necessary
        if(input_tilt_enable && !(dk->sr & CM33_DK_SR_TILT_ENABLE))
        {
            cm33_send_cmd(CM33_DK_CMD_TILT_ENABLE);
        }
        else if(!input_tilt_enable && (dk->sr & CM33_DK_SR_TILT_ENABLE))
        {
            cm33_send_cmd(CM33_DK_CMD_TILT_DISABLE);
        }
        if(dk->joy_a_calib.left != input_joy_calib[0].left)
            dk->joy_a_calib.left = input_joy_calib[0].left;
        if(dk->joy_a_calib.right != input_joy_calib[0].right)
            dk->joy_a_calib.right = input_joy_calib[0].right;
        if(dk->joy_a_calib.top != input_joy_calib[0].top)
            dk->joy_a_calib.top = input_joy_calib[0].top;
        if(dk->joy_a_calib.bottom != input_joy_calib[0].bottom)
            dk->joy_a_calib.bottom = input_joy_calib[0].bottom;
        if(dk->joy_a_calib.middle_x != input_joy_calib[0].middle_x)
            dk->joy_a_calib.middle_x = input_joy_calib[0].middle_x;
        if(dk->joy_a_calib.middle_y != input_joy_calib[0].middle_y)
            dk->joy_a_calib.middle_y = input_joy_calib[0].middle_y;

        if(dk->joy_b_calib.left != input_joy_calib[1].left)
            dk->joy_b_calib.left = input_joy_calib[1].left;
        if(dk->joy_b_calib.right != input_joy_calib[1].right)
            dk->joy_b_calib.right = input_joy_calib[1].right;
        if(dk->joy_b_calib.top != input_joy_calib[1].top)
            dk->joy_b_calib.top = input_joy_calib[1].top;
        if(dk->joy_b_calib.bottom != input_joy_calib[1].bottom)
            dk->joy_b_calib.bottom = input_joy_calib[1].bottom;
        if(dk->joy_b_calib.middle_x != input_joy_calib[1].middle_x)
            dk->joy_b_calib.middle_x = input_joy_calib[1].middle_x;
        if(dk->joy_b_calib.middle_y != input_joy_calib[1].middle_y)
            dk->joy_b_calib.middle_y = input_joy_calib[1].middle_y;

        if(dk->tilt_zero != input_tilt_centre)
            dk->tilt_zero = input_tilt_centre;

        // check for messages
        if(dk->rb_paddr)
        {
            auto msgs = (volatile uint32_t *)(0xfffffd0000000000ULL + (uintptr_t)dk->rb_paddr);

            while(dk->rb_r_ptr != dk->rb_w_ptr)
            {
                cm33_handle_message(msgs[dk->rb_r_ptr]);
                auto new_r_ptr = dk->rb_r_ptr + 1;
                if(new_r_ptr >= dk->rb_size)
                    new_r_ptr = 0;
                dk->rb_r_ptr = new_r_ptr;
            }
        }
    }
}

void cm33_irq(exception_regs *, uint64_t)
{
    cm33_event.Signal();
    EXTI1_VMEM->RPR3 = (1U << 0);
    __DMB();
}

void init_cm33_interface()
{
    sram_setup();

    Schedule(Thread::Create("cm33", cm33_manager_thread, nullptr, true, GK_PRIORITY_HIGH,
        p_kernel));
}
