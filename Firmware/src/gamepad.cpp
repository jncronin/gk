#include "process.h"

static inline short int get_axis_value(unsigned int btns, Process::GamepadKey pos, Process::GamepadKey neg)
{
    unsigned int _pos = 1U << (int)pos;
    unsigned int _neg = 1U << (int)neg;
    btns &= (_pos | _neg);
    if(btns == (_pos | _neg))
        return 0;
    else if(btns == _pos)
        return INT16_MAX;
    else
        return INT16_MIN;
}

void Process::HandleGamepadEvent(Process::GamepadKey key, bool pressed)
{
    CriticalGuard cg(sl);
    if(gamepad_is_joystick)
    {
        if(pressed)
        {
            gamepad_buttons |= 1U << (int)key;
            switch(key)
            {
                case GamepadKey::A:
                case GamepadKey::B:
                case GamepadKey::X:
                case GamepadKey::Y:
                    events.Push({ Event::event_type_t::ButtonDown, .key = (unsigned short)key });
                    break;
                case GamepadKey::Left:
                case GamepadKey::Right:
                    events.Push({ Event::event_type_t::AxisMotion,
                        .axis_data = { 0, get_axis_value(gamepad_buttons, GamepadKey::Right, GamepadKey::Left) }});
                    break;
                case GamepadKey::Up:
                case GamepadKey::Down:
                    events.Push({ Event::event_type_t::AxisMotion,
                        .axis_data = { 0, get_axis_value(gamepad_buttons, GamepadKey::Down, GamepadKey::Up) }});
                    break;
            }
        }
        else
        {
            gamepad_buttons &= ~(1U << (int)key);
            switch(key)
            {
                case GamepadKey::A:
                case GamepadKey::B:
                case GamepadKey::X:
                case GamepadKey::Y:
                    events.Push({ Event::event_type_t::ButtonUp, .key = (unsigned short)key });
                    break;
                case GamepadKey::Left:
                case GamepadKey::Right:
                    events.Push({ Event::event_type_t::AxisMotion,
                        .axis_data = { 0, get_axis_value(gamepad_buttons, GamepadKey::Right, GamepadKey::Left) }});
                    break;
                case GamepadKey::Up:
                case GamepadKey::Down:
                    events.Push({ Event::event_type_t::AxisMotion,
                        .axis_data = { 0, get_axis_value(gamepad_buttons, GamepadKey::Down, GamepadKey::Up) }});
                    break;
            }
        }
    }
    if(gamepad_is_keyboard)
    {
        if(pressed)
        {
            events.Push({ Event::event_type_t::KeyDown, .key = gamepad_to_scancode[(int)key] });
        }
        else
        {
            events.Push({ Event::event_type_t::KeyUp, .key = gamepad_to_scancode[(int)key] });
        }
    }
}