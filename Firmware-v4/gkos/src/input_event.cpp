#include "process.h"
#include "cm33_data.h"
#include "_gk_scancodes.h"
#include "supervisor.h"
#include "vmem.h"

extern PMemBlock process_kernel_info_page;
extern PProcess p_gksupervisor;

int Process::HandleTouchAsMouseEvent(unsigned int msg_type, unsigned int x, unsigned int y)
{
    // scale to screen coordinates
    x = (x * screen.screen_w) / GK_SCREEN_WIDTH;
    y = (y * screen.screen_h) / GK_SCREEN_HEIGHT;

    Event ev;
    switch(msg_type)
    {
        case CM33_DK_MSG_TOUCHPRESS:
            ev.type = Event::event_type_t::MouseDown;
            ev.mouse_data.x = x;
            ev.mouse_data.y = y;
            ev.mouse_data.buttons = 1;
            ev.mouse_data.is_rel = true;
            events.Push(ev);
            return 0;

        case CM33_DK_MSG_TOUCHMOVE:
            ev.type = Event::event_type_t::MouseMove;
            ev.mouse_data.x = x;
            ev.mouse_data.y = y;
            ev.mouse_data.buttons = 1;
            ev.mouse_data.is_rel = true;
            events.Push(ev);
            return 0;

        case CM33_DK_MSG_TOUCHRELEASE:
            ev.type = Event::event_type_t::MouseUp;
            ev.mouse_data.x = x;
            ev.mouse_data.y = y;
            ev.mouse_data.buttons = 1;
            ev.mouse_data.is_rel = true;
            events.Push(ev);
            return 0;

        default:
            return -1;
    }
    
}

int Process::HandleInputEvent(unsigned int cm33_cmd)
{
    auto kinfo = (gk_kernel_info *)PMEM_TO_VMEM(process_kernel_info_page.base);

    auto key = cm33_cmd & CM33_DK_MSG_CONTENTS;
    auto action = cm33_cmd & CM33_DK_MSG_MASK;

    switch(action)
    {
        case CM33_DK_MSG_TOUCHMOVE:
        case CM33_DK_MSG_TOUCHPRESS:
        case CM33_DK_MSG_TOUCHRELEASE:
            if(keymap.touch_is_mouse)
                return HandleTouchAsMouseEvent(action, key & 1023U, (key >> 10) & 1023U);
            else
                return 0;
    }

    if(key >= GK_NUMKEYS)
    {
        return -1;
    }

    auto scode = keymap.gamepad_to_scancode[key];

    /* Power, menu (always), and _unhandled_ volup/down go to supervisor */
    if(scode == 0 || (key == GK_KEYPOWER) || (key == GK_KEYMENU))
    {
        unsigned short sc = 0;

        switch(key)
        {            
            case GK_KEYPOWER:
                sc = GK_SCANCODE_POWER;
                break;

            case GK_KEYMENU:
                sc = GK_SCANCODE_MENU;
                break;

            case GK_KEYVOLUP:
                sc = GK_SCANCODE_VOLUMEUP;
                break;

            case GK_KEYVOLDOWN:
                sc = GK_SCANCODE_VOLUMEDOWN;
                break;
        }

        if(sc)
        {
            Event ev;
            ev.type = (action == CM33_DK_MSG_RELEASE) ? Event::event_type_t::KeyUp :
                Event::event_type_t::KeyDown;
            ev.key = sc;
            p_gksupervisor->events.Push(ev);
        }

        return 0;
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
                kinfo->joystick_buttons |= btn;
                events.Push({ .type = Event::event_type_t::ButtonDown, .key = gkey });
                break;

            case CM33_DK_MSG_RELEASE:
                gamepad_buttons &= ~btn;
                kinfo->joystick_buttons &= ~btn;
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