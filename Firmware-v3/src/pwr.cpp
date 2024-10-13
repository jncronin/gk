#include "stm32h7rsxx.h"
#include "i2c_poll.h"
#include "gk_conf.h"

INTFLASH_FUNCTION void SWO_Init(uint32_t portBits, uint32_t cpuCoreFreqHz) {
  /*
    This functions recommends system speed of 400000000Hz and will
    use SWO clock speed of 2000000Hz

    # GDB OpenOCD commands to connect to this:
    monitor tpiu config internal - uart off 400000000
    monitor itm port 0 on

    Code Gen Ref: https://gist.github.com/mofosyne/178ad947fdff0f357eb0e03a42bcef5c
  */

  /* Setup SWO and SWO funnel (Note: SWO_BASE and SWTF_BASE not defined in stm32h743xx.h) */
  // DBGMCU_CR : Enable D3DBGCKEN D1DBGCKEN TRACECLKEN Clock Domains
  DBGMCU->CR |=  DBGMCU_CR_TRACECLKEN |
    DBGMCU_CR_DBGCKEN; // DBGMCU_CR
  // SWO_LAR & SWTF_LAR : Unlock SWO and SWO Funnel
  *((uint32_t *)(0x5c003fb0)) = 0xC5ACCE55; // SWO_LAR
  *((uint32_t *)(0x5c004fb0)) = 0xC5ACCE55; // SWTF_LAR
  // SWO_CODR  : 400000000Hz -> 2000000Hz
  // Note: SWOPrescaler = ((sysclock_Hz / SWOSpeed_Hz) - 1) --> 0x0000c7 = 199 = (400000000 / 2000000) - 1)
  *((uint32_t *)(0x5c003010)) = ((cpuCoreFreqHz /  2000000) - 1); // SWO_CODR
  // SWO_SPPR : (2:  SWO NRZ, 1:  SWO Manchester encoding)
  *((uint32_t *)(0x5c0030f0)) = 0x00000002; // SWO_SPPR
  // SWTF_CTRL : enable SWO
  *((uint32_t *)(0x5c004000)) |= 0x1; // SWTF_CTRL
}

INTFLASH_FUNCTION void SWO_PrintChar(char c, uint8_t portNo) {
  volatile int timeout;
 
  /* Check if Trace Control Register (ITM->TCR at 0xE0000E80) is set */
  if ((ITM->TCR&ITM_TCR_ITMENA_Msk) == 0) { /* check Trace Control Register if ITM trace is enabled*/
    return; /* not enabled? */
  }
  /* Check if the requested channel stimulus port (ITM->TER at 0xE0000E00) is enabled */
  if ((ITM->TER & (1ul<<portNo))==0) { /* check Trace Enable Register if requested port is enabled */
    return; /* requested port not enabled? */
  }
  timeout = 5000; /* arbitrary timeout value */
  while (ITM->PORT[0].u32 == 0) {
    /* Wait until STIMx is ready, then send data */
    timeout--;
    if (timeout==0) {
      return; /* not able to send */
    }
  }
  ITM->PORT[0].u16 = 0x08 | (c<<8);
}

INTFLASH_FUNCTION void init_uart_debug()
{
    // UART8 on PE0/1
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
    GPIOE->AFR[0] = (GPIOE->AFR[0] & ~GPIO_AFRL_AFSEL0_Msk) | (8UL << GPIO_AFRL_AFSEL0_Pos);
    GPIOE->AFR[0] = (GPIOE->AFR[0] & ~GPIO_AFRL_AFSEL1_Msk) | (8UL << GPIO_AFRL_AFSEL1_Pos);
    GPIOE->OSPEEDR |= (3U << GPIO_OSPEEDR_OSPEED0_Pos) |
        (3U << GPIO_OSPEEDR_OSPEED1_Pos);
    GPIOE->MODER = (GPIOB->MODER & ~GPIO_MODER_MODE0_Msk) | (2U << GPIO_MODER_MODE0_Pos);
    GPIOE->MODER = (GPIOB->MODER & ~GPIO_MODER_MODE1_Msk) | (2U << GPIO_MODER_MODE1_Pos);

    RCC->CCIPR2 = (RCC->CCIPR2 & ~RCC_CCIPR2_UART234578SEL_Msk) |
        (3U << RCC_CCIPR2_UART234578SEL_Pos);       // hsi64

    RCC->APB1ENR1 |= RCC_APB1ENR1_UART8EN;
    (void)RCC->APB1ENR1;

    UART8->PRESC = 2U;  // /4 -> 16MHz clock
    UART8->BRR = 139U;  // 115108 baud
    UART8->CR1 = USART_CR1_FIFOEN | USART_CR1_RE | USART_CR1_TE;
    UART8->CR1 |= USART_CR1_UE;
}

INTFLASH_FUNCTION void uart_sendchar(char c)
{
    while(!(UART8->ISR & USART_ISR_TXE_TXFNF));
    UART8->TDR = c;
}

extern "C" INTFLASH_FUNCTION int pwr_disable_regulators()
{
    // PWR init
    auto pwr_csr2 = PWR->CSR2;
    pwr_csr2 &= ~0xffU;
    pwr_csr2 |= PWR_CSR2_BYPASS;
    PWR->CSR2 = pwr_csr2;

    // Enable SWO port
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
    (void)RCC->AHB4ENR;
    GPIOB->AFR[0] &= ~GPIO_AFRL_AFSEL3_Msk;
    GPIOB->OSPEEDR |= 3U << GPIO_OSPEEDR_OSPEED3_Pos;
    GPIOB->MODER = (GPIOB->MODER & ~GPIO_MODER_MODE3_Msk) | (2U << GPIO_MODER_MODE3_Pos);

    SWO_Init(0x1, 600000000);
    SWO_PrintChar('A', 0);
    SWO_PrintChar('\n', 0);

    return 0;
}

INTFLASH_FUNCTION int pwr_set_vos_high()
{
    // boost VCORE
    uint8_t dvb2a = 0;
    auto i2c_ret = i2c_poll_register_read(0x78 >> 1, (uint8_t)0x0e, &dvb2a, 1);
    if(i2c_ret == 1)
    {
        dvb2a &= ~0x1fU;
        dvb2a |= 0x1fU;

        i2c_poll_register_write(0x78 >> 1, (uint8_t)0x0e, &dvb2a, 1);

        // check for success
        PWR->CSR4 |= PWR_CSR4_VOS;

        if(i2c_poll_register_read(0x78 >> 1, (uint8_t)0x0e, &dvb2a, 1) == 1 &&
            ((dvb2a & 0x1fU) == 0x1fU))
        {
            return 0;
        }
    }

    return -1;
}
