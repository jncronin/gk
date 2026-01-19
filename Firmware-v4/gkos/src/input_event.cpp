#include "process.h"
#include "cm33_data.h"
#include "_gk_scancodes.h"

int Process::HandleInputEvent(unsigned int cm33_cmd)
{
    auto key = cm33_cmd & CM33_DK_MSG_CONTENTS;
    auto action = cm33_cmd & CM33_DK_MSG_MASK;
    if(key >= GK_NUMKEYS)
    {
        return -1;
    }

    auto scode = keymap.gamepad_to_scancode[key];

    /* Power, menu (always), and _unhandled_ volup/down go to supervisor */
    if(scode == 0 || (key == GK_KEYPOWER) || (key == GK_KEYMENU))
    {
        switch(key)
        {
            case GK_KEYPOWER:
            case GK_KEYMENU:
            case GK_KEYVOLUP:
            case GK_KEYVOLDOWN:
                // TODO: send to supervisor
                klog("input: send to supervisor not implemented\n");
                return 0;

            default:
                // ignore
                return 0;
        }
    }

    /* Handle mouse buttons */
    if(scode >= GK_MOUSE_BUTTON && scode < (GK_MOUSE_BUTTON + 8))
    {
        unsigned char btn = 1U << (scode - GK_MOUSE_BUTTON);

        switch(action)
        {
            case CM33_DK_MSG_PRESS:
                mouse_buttons |= btn;
                events.Push({ .type = Event::event_type_t::MouseDown,
                    .mouse_data = { .x = 0, .y = 0, .is_rel = true, .buttons = btn } });
                break;

            case CM33_DK_MSG_RELEASE:
                mouse_buttons &= ~btn;
                events.Push({ .type = Event::event_type_t::MouseUp,
                    .mouse_data = { .x = 0, .y = 0, .is_rel = true, .buttons = btn } });
                break;
        }

        return 0;
    }

    /* Handle joystick buttons */
    if(scode >= GK_GAMEPAD_BUTTON && scode < (GK_GAMEPAD_BUTTON + 64))
    {
        auto gkey = (unsigned short)(scode - GK_GAMEPAD_BUTTON);
        uint64_t btn = 1U << (scode - GK_GAMEPAD_BUTTON);

        switch(action)
        {
            case CM33_DK_MSG_PRESS:
            case CM33_DK_MSG_REPEAT:
                gamepad_buttons |= btn;
                events.Push({ .type = Event::event_type_t::ButtonDown, .key = gkey });
                break;

            case CM33_DK_MSG_RELEASE:
                gamepad_buttons &= ~btn;
                events.Push({ .type = Event::event_type_t::ButtonUp, .key = gkey });
                break;
        }

        return 0;
    }

    // Else handle it as a key
    switch(action)
    {
        case CM33_DK_MSG_PRESS:
        case CM33_DK_MSG_REPEAT:
            events.Push({ .type = Event::event_type_t::KeyDown, .key = (unsigned short)scode });
            break;

        case CM33_DK_MSG_RELEASE:
            events.Push({ .type = Event::event_type_t::KeyUp, .key = (unsigned short)scode });
            break;
    }

    return 0;
}