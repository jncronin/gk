#include "stm32h7rsxx.h"
#include "pins.h"
#include "supervisor.h"
#include "debounce.h"
#include "_gk_scancodes.h"

constexpr const pin BTN_MENU { GPIOM, 2 };
constexpr const pin BTN_VOLUP { GPIOM, 1 };
constexpr const pin BTN_VOLDOWN { GPIOG, 3 };      // TODO changed to PA12 in v2
constexpr const pin BTN_ONOFF { GPIOM, 3 };
constexpr const pin BTN_L { GPIOE, 15 };
constexpr const pin BTN_R { GPIOM, 0 };
constexpr const pin BTN_U { GPIOG, 15 };
constexpr const pin BTN_D { GPIOE, 14 };
constexpr const pin BTN_A { GPIOD, 4 };
constexpr const pin BTN_B { GPIOD, 5 };
constexpr const pin BTN_X { GPIOC, 2 };
constexpr const pin BTN_Y { GPIOE, 10 };
constexpr const pin BTN_JOY { GPIOD, 2 };

constexpr const pin JOY_X { GPIOC, 4 };        // ADC12_INP4
constexpr const pin JOY_Y { GPIOA, 6 };        // ADC12_INP3

#define dma GPDMA1_Channel13

static Debounce<5, 20, 20, 1000, Process::GamepadKey>
    db_MENU(BTN_MENU, Process::GamepadKey::Menu),
    db_VOLUP(BTN_VOLUP, Process::GamepadKey::VolUp),
    db_VOLDOWN(BTN_VOLDOWN, Process::GamepadKey::VolDown),
    db_ONOFF(BTN_ONOFF, Process::GamepadKey::Power),
    db_L(BTN_L, Process::GamepadKey::Left),
    db_R(BTN_R, Process::GamepadKey::Right),
    db_U(BTN_U, Process::GamepadKey::Up),
    db_D(BTN_D, Process::GamepadKey::Down),
    db_A(BTN_A, Process::GamepadKey::A),
    db_B(BTN_B, Process::GamepadKey::B),
    db_X(BTN_X, Process::GamepadKey::X),
    db_Y(BTN_Y, Process::GamepadKey::Y),
    db_JOY(BTN_JOY, Process::GamepadKey::Joy);

static int longpress_count = 0;

static __attribute__((aligned(32))) volatile uint16_t adc_vals[16];

void init_buttons()
{
    BTN_MENU.set_as_input();
    BTN_VOLUP.set_as_input();
    BTN_VOLDOWN.set_as_input();
    BTN_ONOFF.set_as_input();
    BTN_L.set_as_input();
    BTN_R.set_as_input();
    BTN_U.set_as_input();
    BTN_D.set_as_input();
    BTN_A.set_as_input();
    BTN_B.set_as_input();
    BTN_X.set_as_input();
    BTN_Y.set_as_input();
    BTN_JOY.set_as_input();
    pin_set(JOY_X, 3);
    pin_set(JOY_Y, 3);

    // Set up ADC, clocked from 24 MHz HSE
    RCC->AHB1ENR |= RCC_AHB1ENR_ADC12EN;
    (void)RCC->AHB1ENR;
    RCC->AHB1RSTR = RCC_AHB1RSTR_ADC12RST;
    (void)RCC->AHB1RSTR;
    RCC->AHB1RSTR = 0;
    (void)RCC->AHB1RSTR;

    // Disable deep power down
    ADC1->CR = 0;
    (void)ADC1->CR;

    // Enable regulator
    ADC1->CR |= ADC_CR_ADVREGEN;
    (void)ADC1->CR;
    delay_ms(1);

    ADC12_COMMON->CCR = //ADC_CCR_TSEN |
        (8U << ADC_CCR_PRESC_Pos) |         // /32
        ADC_CCR_DMACFG |                    // DMA circular mode
        (0xfU << ADC_CCR_DELAY_Pos);
    ADC1->DIFSEL = 0;

    // Enable calibration
    ADC1->CR |= ADC_CR_ADCAL;
    (void)ADC1->CR;
    while(ADC1->CR & ADC_CR_ADCAL);

    // Enable ADC
    ADC1->ISR = ADC_ISR_ADRDY;
    ADC1->CR |= ADC_CR_ADEN;
    (void)ADC1->CR;
    while(!(ADC1->ISR & ADC_ISR_ADRDY));

    ADC1->CFGR = 
        ADC_CFGR_CONT |
        ADC_CFGR_OVRMOD |
        ADC_CFGR_DMACFG |
        ADC_CFGR_DMAEN;

    // x128 and 3 bit shift gives ~5ms update interval
    ADC1->CFGR2 = (0U << ADC_CFGR2_OVSS_Pos) |
        (3U << ADC_CFGR2_OVSR_Pos) |
        ADC_CFGR2_ROVSE;
    ADC1->SQR1 = (3U << ADC_SQR1_L_Pos) |   // 2 conversions
        (4U << ADC_SQR1_SQ1_Pos) |          // JOY_X
        (3U << ADC_SQR1_SQ2_Pos) |          // JOY_Y
        (3U << ADC_SQR1_SQ2_Pos) |          // JOY_Y
        (16U << ADC_SQR1_SQ2_Pos);          // temperature
    ADC1->SQR2 = 0;
    ADC1->SQR3 = 0;
    ADC1->SQR4 = 0;
    const unsigned smpr = 5u;
    ADC1->SMPR1 = (smpr << ADC_SMPR1_SMP0_Pos) |
        (smpr << ADC_SMPR1_SMP1_Pos) |
        (smpr << ADC_SMPR1_SMP2_Pos) |
        (smpr << ADC_SMPR1_SMP3_Pos) |
        (smpr << ADC_SMPR1_SMP4_Pos) |
        (smpr << ADC_SMPR1_SMP5_Pos) |
        (smpr << ADC_SMPR1_SMP6_Pos) |
        (smpr << ADC_SMPR1_SMP7_Pos) |
        (smpr << ADC_SMPR1_SMP8_Pos) |
        (smpr << ADC_SMPR1_SMP9_Pos);
    ADC1->SMPR2 = (smpr << ADC_SMPR2_SMP10_Pos) |
        (smpr << ADC_SMPR2_SMP11_Pos) |
        (smpr << ADC_SMPR2_SMP12_Pos) |
        (smpr << ADC_SMPR2_SMP13_Pos) |
        (smpr << ADC_SMPR2_SMP14_Pos) |
        (smpr << ADC_SMPR2_SMP15_Pos) |
        (1 << ADC_SMPR2_SMP16_Pos) |
        (smpr << ADC_SMPR2_SMP17_Pos) |
        (smpr << ADC_SMPR2_SMP18_Pos);


    // Set up DMA
    RCC->AHB1ENR |= RCC_AHB1ENR_GPDMA1EN;
    (void)RCC->AHB1ENR;

    dma->CCR = 0;
    dma->CTR1 = DMA_CTR1_DAP |
        (3U << DMA_CTR1_DBL_1_Pos) |
        DMA_CTR1_DINC |
        (1U << DMA_CTR1_DDW_LOG2_Pos) |
        (0U << DMA_CTR1_SBL_1_Pos) |
        (1U << DMA_CTR1_SDW_LOG2_Pos);
    dma->CTR2 = (0U << DMA_CTR2_REQSEL_Pos);
    dma->CTR3 = 0;
    dma->CBR1 = 8U |
        DMA_CBR1_BRDDEC |
        (1U << DMA_CBR1_BRC_Pos);
    dma->CSAR = (uint32_t)(uintptr_t)&ADC1->DR;
    dma->CDAR = (uint32_t)(uintptr_t)adc_vals;
    dma->CBR2 = (8U << DMA_CBR2_BRDAO_Pos);     // -8 every block
    dma->CLLR = 4U; // anything not zero
    dma->CCR |= DMA_CCR_EN;

    ADC1->CR |= ADC_CR_ADSTART;

    while(true)
    {
        delay_ms(5);
        SCB_InvalidateDCache_by_Addr(adc_vals, 32);
        klog("adc: %u %u %u %u\n", adc_vals[0], adc_vals[1], adc_vals[2], adc_vals[3]);        
    }


    // Set up lptim2 for debouncing
    RCC->APB4ENR |= RCC_APB4ENR_LPTIM2EN;
    (void)RCC->APB4ENR;
    LPTIM2->CR = 0;
    LPTIM2->CR = LPTIM_CR_RSTARE;
    (void)LPTIM2->CR;
    LPTIM2->CR = 0;

    LPTIM2->CFGR = 5UL << LPTIM_CFGR_PRESC_Pos;     // /32 => 1 MHz tick
    LPTIM2->DIER = LPTIM_DIER_ARRMIE;
    LPTIM2->CR = LPTIM_CR_ENABLE;
    LPTIM2->ARR = 4999;                              // Reload every 5 ms

    NVIC_EnableIRQ(LPTIM2_IRQn);
    LPTIM2->CR = LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT;
}

static inline Process &recv_proc()
{
    // TODO
    /*if(supervisor_is_active(nullptr, nullptr, nullptr, nullptr))
    {
        return p_supervisor;
    }
    else*/
    {
        return *focus_process;
    }
}

static inline void handle_state_change(Process::GamepadKey exti, bool pressed)
{
    Event::event_type_t et = pressed ? Event::KeyDown : Event::KeyUp;

    switch(exti)
    {
        case Process::GamepadKey::Power:
            p_supervisor.events.Push({ et, .key = GK_SCANCODE_POWER });
            break;
        case Process::GamepadKey::Menu:
            p_supervisor.events.Push({ et, .key = GK_SCANCODE_MENU });
            break;
        case Process::GamepadKey::Left:
        case Process::GamepadKey::Right:
        case Process::GamepadKey::Up:
        case Process::GamepadKey::Down:
        case Process::GamepadKey::A:
        case Process::GamepadKey::B:
        case Process::GamepadKey::X:
        case Process::GamepadKey::Y:
        case Process::GamepadKey::VolUp:
        case Process::GamepadKey::VolDown:
        case Process::GamepadKey::Joy:
            recv_proc().HandleGamepadEvent(exti, pressed);
            break;
        default:
            break;
    }
}

template<int tick_period_ms,
    int high_time_ms,
    int low_time_ms,
    int long_press_ms> void handle_debounce_event(Debounce<tick_period_ms, high_time_ms, low_time_ms, long_press_ms, Process::GamepadKey> &db)
{
    auto v = db.tick();
    auto val = db.get_val();
    if(v & pin_state::NewStableState)
    {
        handle_state_change(val, v & pin_state::StableLow);
    }
    else if(v & pin_state::StableLow && (!(longpress_count % (100 / low_time_ms))))
    {
        // long presses
        switch(val)
        {
            case Process::GamepadKey::Left:
            case Process::GamepadKey::Right:
            case Process::GamepadKey::Up:
            case Process::GamepadKey::Down:
            case Process::GamepadKey::A:
            case Process::GamepadKey::B:
            case Process::GamepadKey::X:
            case Process::GamepadKey::Y:
            case Process::GamepadKey::VolUp:
            case Process::GamepadKey::VolDown:
            case Process::GamepadKey::Joy:
                recv_proc().HandleGamepadEvent(val, false, true);
                break;
            default:
                break;
        }
    }
}

extern "C" void LPTIM2_IRQHandler()
{
    handle_debounce_event(db_MENU);
    handle_debounce_event(db_VOLUP);
    handle_debounce_event(db_VOLDOWN);
    handle_debounce_event(db_L);
    handle_debounce_event(db_R);
    handle_debounce_event(db_U);
    handle_debounce_event(db_D);
    handle_debounce_event(db_A);
    handle_debounce_event(db_B);
    handle_debounce_event(db_X);
    handle_debounce_event(db_Y);
    handle_debounce_event(db_JOY);

    longpress_count++;

    LPTIM2->ICR = LPTIM_ICR_ARRMCF;
    __DMB();
}
