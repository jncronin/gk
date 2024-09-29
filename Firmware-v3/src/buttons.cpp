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
