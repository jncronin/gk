#include "stm32h7xx.h"
#include "pins.h"
#include "supervisor.h"
#include "debounce.h"

constexpr const pin BTN_MENU { GPIOB, 4 };
constexpr const pin BTN_VOLUP { GPIOB, 11 };
constexpr const pin BTN_VOLDOWN { GPIOB, 12 };
constexpr const pin BTN_ONOFF { GPIOH, 3 };
constexpr const pin BTN_L { GPIOH, 5 };
constexpr const pin BTN_R { GPIOH, 6 };
constexpr const pin BTN_U { GPIOH, 7 };
constexpr const pin BTN_D { GPIOH, 8 };
constexpr const pin BTN_A { GPIOH, 9 };
constexpr const pin BTN_B { GPIOH, 10 };
constexpr const pin BTN_X { GPIOH, 13 };
constexpr const pin BTN_Y { GPIOH, 15 };

static Debounce db_MENU(BTN_MENU),
    db_VOLUP(BTN_VOLUP),
    db_VOLDOWN(BTN_VOLDOWN),
    db_ONOFF(BTN_ONOFF),
    db_L(BTN_L),
    db_R(BTN_R),
    db_U(BTN_U),
    db_D(BTN_D),
    db_A(BTN_A),
    db_B(BTN_B),
    db_X(BTN_X),
    db_Y(BTN_Y);

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

    // Set up lptim2 for debouncing
    RCC->APB4ENR |= RCC_APB4ENR_LPTIM2EN;
    (void)RCC->APB4ENR;
    LPTIM2->CR = 0;
    LPTIM2->CR = LPTIM_CR_RSTARE;
    (void)LPTIM2->CR;
    LPTIM2->CR = 0;

    LPTIM2->CFGR = 4UL << LPTIM_CFGR_PRESC_Pos;     // /16 => 1 MHz tick
    LPTIM2->IER = LPTIM_IER_ARRMIE;
    LPTIM2->CR = LPTIM_CR_ENABLE;
    LPTIM2->ARR = 4999;                              // Reload every 5 ms

    NVIC_EnableIRQ(LPTIM2_IRQn);
    LPTIM2->CR = LPTIM_CR_ENABLE | LPTIM_CR_CNTSTRT;
}

static inline Process &recv_proc()
{
    if(supervisor_is_active(nullptr, nullptr, nullptr, nullptr))
    {
        return p_supervisor;
    }
    else
    {
        return *focus_process;
    }
}

static inline void handle_state_change(int exti, bool pressed)
{
    Event::event_type_t et = pressed ? Event::KeyDown : Event::KeyUp;

    switch(exti)
    {
        case 3:
            p_supervisor.events.Push({ et, .key = 0 });
            break;
        case 4:
            p_supervisor.events.Push({ et, .key = ' ' });
            break;
        case 5:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::Left, pressed);
            break;
        case 6:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::Right, pressed);
            break;
        case 7:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::Up, pressed);
            break;
        case 8:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::Down, pressed);
            break;
        case 9:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::A, pressed);
            break;
        case 10:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::B, pressed);
            break;
        case 11:
            p_supervisor.events.Push({ et, .key = 'u' });
            break;
        case 12:
            p_supervisor.events.Push({ et, .key = 'd' });
            break;
        case 13:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::X, pressed);
            break;
        case 14:
            recv_proc().HandleGamepadEvent(Process::GamepadKey::Y, pressed);
            break;
    }
}

static inline void handle_state_change(const pin &p)
{
    handle_state_change(p.pin, !p.value());
}

template<int tick_period_ms,
    int high_time_ms,
    int low_time_ms,
    int long_press_ms> void handle_debounce_event(Debounce<tick_period_ms, high_time_ms, low_time_ms, long_press_ms> &db)
{
    auto v = db.tick();
    if(v & pin_state::NewStableState)
    {
        handle_state_change(db.get_pin().pin, v & pin_state::StableLow);
    }
    else if(v & pin_state::StableLow && (!(longpress_count % (100 / low_time_ms))))
    {
        // long presses
        switch(db.get_pin().pin)
        {
            case 5:
                recv_proc().HandleGamepadEvent(Process::GamepadKey::Left, false, true);
                break;
            case 6:
                recv_proc().HandleGamepadEvent(Process::GamepadKey::Right, false, true);
                break;
            case 7:
                recv_proc().HandleGamepadEvent(Process::GamepadKey::Up, false, true);
                break;
            case 8:
                recv_proc().HandleGamepadEvent(Process::GamepadKey::Down, false, true);
                break;
            case 9:
                recv_proc().HandleGamepadEvent(Process::GamepadKey::A, false, true);
                break;
            case 10:
                recv_proc().HandleGamepadEvent(Process::GamepadKey::B, false, true);
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

    longpress_count++;

    LPTIM2->ICR = LPTIM_ICR_ARRMCF;
    __DMB();
}
