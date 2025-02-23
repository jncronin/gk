#include "process.h"
#include "_gk_scancodes.h"

extern Process p_supervisor;

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

void Process::HandleJoystickEvent(unsigned int x, unsigned int y)
{
    if(joystick_is_joystick)
    {
        if(x != joy_last_x)
        {
            events.Push({ Event::event_type_t::AxisMotion, .axis_data = { 2, (short int)x }});
        }
        if(y != joy_last_y)
        {
            events.Push({ Event::event_type_t::AxisMotion, .axis_data = { 3, (short int)y }});
        }
    }
    joy_last_x = x;
    joy_last_y = y;
}

void Process::HandleTouchEvent(unsigned int x, unsigned int y, TouchEventType type)
{
    // scale to screen size
    x = (x * screen_w) / 640;
    y = (y * screen_h) / 480;

    if(touch_is_mouse)
    {
        switch(type)
        {
            case TouchEventType::Press:
                events.Push({ Event::event_type_t::MouseDown,
                    .mouse_data = { .x = (int16_t)x, .y = (int16_t)y, .is_rel = false, .buttons = 1 }});
                break;
            case TouchEventType::Drag:
                events.Push({ Event::event_type_t::MouseMove,
                    .mouse_data = { .x = (int16_t)x, .y = (int16_t)y, .is_rel = false, .buttons = 1 }});
                break;
            case TouchEventType::Release:
                events.Push({ Event::event_type_t::MouseUp,
                    .mouse_data = { .x = (int16_t)x, .y = (int16_t)y, .is_rel = false, .buttons = 1 }});
                break;
        }
    }
}

void Process::HandleTiltEvent(int x, int y)
{
    if(tilt_is_joystick)
    {
        events.Push({ Event::event_type_t::AxisMotion, .axis_data = { 0, (short int)x }});
        events.Push({ Event::event_type_t::AxisMotion, .axis_data = { 1, (short int)y }});
    }
    if(tilt_is_keyboard)
    {
        char new_state = 0;
        if(x < 0) new_state |= 1;
        if(x > 0) new_state |= 2;
        if(y > 0) new_state |= 4;
        if(y < 0) new_state |= 8;

        if(new_state != tilt_keyboard_state)
        {
            auto state_change = new_state ^ tilt_keyboard_state;
            for(int i = 0; i < 4; i++)
            {
                if(state_change & (1 << i) && gamepad_to_scancode[GK_KEYTILTLEFT + i])
                {
                    auto evtype = (new_state & (0x1 << i)) ? Event::event_type_t::KeyDown : Event::event_type_t::KeyUp;
                    events.Push({ evtype, .key = gamepad_to_scancode[GK_KEYTILTLEFT + i] });
                }
            }
            tilt_keyboard_state = new_state;
        }
    }
}

void Process::HandleGamepadEvent(Process::GamepadKey key, bool pressed, bool ongoing_press)
{
    CriticalGuard cg(sl);
    if(gamepad_is_mouse && gamepad_to_scancode[(int)key] == 0)
    {
        if(ongoing_press || pressed)
        {
            switch(key)
            {
                case GamepadKey::Left:
                    events.Push({ Event::event_type_t::MouseMove,
                        .mouse_data = { .x = -1, .y = 0, .is_rel = 1, .buttons = mouse_buttons } });
                    break;
                case GamepadKey::Right:
                    events.Push({ Event::event_type_t::MouseMove,
                        .mouse_data = { .x = 1, .y = 0, .is_rel = 1, .buttons = mouse_buttons } });
                    break;
                case GamepadKey::Up:
                    events.Push({ Event::event_type_t::MouseMove,
                        .mouse_data = { .x = 0, .y = -1, .is_rel = 1, .buttons = mouse_buttons } });
                    break;
                case GamepadKey::Down:
                    events.Push({ Event::event_type_t::MouseMove,
                        .mouse_data = { .x = 0, .y = 1, .is_rel = 1, .buttons = mouse_buttons } });
                    break;
                default:
                    break;
            }
        }
        if(pressed)
        {
            switch(key)
            {
                case GamepadKey::A:
                    mouse_buttons |= 1U;
                    events.Push({ Event::event_type_t::MouseDown,
                        .mouse_data = { .x = 0, .y = 0, .is_rel = 0, .buttons = 1U } });
                    break;
                case GamepadKey::B:
                    mouse_buttons |= 2U;
                    events.Push({ Event::event_type_t::MouseDown,
                        .mouse_data = { .x = 0, .y = 0, .is_rel = 0, .buttons = 2U } });
                    break;
                default:
                    break;
            }
        }
        if(!pressed && !ongoing_press)
        {
            switch(key)
            {
                case GamepadKey::A:
                    mouse_buttons &= ~1U;
                    events.Push({ Event::event_type_t::MouseUp,
                        .mouse_data = { .x = 0, .y = 0, .is_rel = 0, .buttons = 1U } });
                    break;
                case GamepadKey::B:
                    mouse_buttons &= ~2U;
                    events.Push({ Event::event_type_t::MouseUp,
                        .mouse_data = { .x = 0, .y = 0, .is_rel = 0, .buttons = 2U } });
                    break;
                default:
                    break;
            }
        }
    }
    if(gamepad_is_joystick && gamepad_to_scancode[(int)key] == 0)
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
                        .axis_data = { 1, get_axis_value(gamepad_buttons, GamepadKey::Down, GamepadKey::Up) }});
                    break;
                default:
                    break;
            }
        }
        else if(!ongoing_press)
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
                default:
                    break;
            }
        }
    }
    if(gamepad_is_keyboard)
    {
        auto scode = gamepad_to_scancode[(int)key];
        if(scode)
        {
            if(pressed)
            {
                events.Push({ Event::event_type_t::KeyDown, .key = scode });
            }
            else if(!ongoing_press)
            {
                events.Push({ Event::event_type_t::KeyUp, .key = scode });
            }
        }
        else if(this != &p_supervisor &&
            (key == Process::GamepadKey::VolUp || key == Process::GamepadKey::VolDown))
        {
            p_supervisor.HandleGamepadEvent(key, pressed, ongoing_press);
        }
    }
    else if(this != &p_supervisor &&
        (key == Process::GamepadKey::VolUp || key == Process::GamepadKey::VolDown))
    {
        p_supervisor.HandleGamepadEvent(key, pressed, ongoing_press);
    }
}