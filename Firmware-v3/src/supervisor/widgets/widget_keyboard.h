#ifndef WIDGET_KEYBOARD_H
#define WIDGET_KEYBOARD_H

#include "_gk_scancodes.h"

class KeyboardWidget : public GridWidget
{
    protected:
        class Keybutton : public ButtonWidget
        {
            protected:
                int key;
                int gx, gy;

                friend class KeyboardWidget;

                KeyboardWidget *kbd;

                static void _OnClick(Widget *w, coord_t x, coord_t y);
                static void _OnClickBegin(Widget *w, coord_t x, coord_t y);

            public:
                Keybutton(int _key, std::string text, float _x, float _y, float _w = 1.0f, float _h = 1.0f);
        };

        friend class Keybutton;

        Keybutton btns[85] =
        {
            Keybutton(GK_SCANCODE_ESCAPE, "Esc", 0, 0),
            Keybutton(GK_SCANCODE_F1, "F1", 1.1, 0, 0.9),
            Keybutton(GK_SCANCODE_F2, "F2", 2.0, 0, 0.9),
            Keybutton(GK_SCANCODE_F3, "F3", 2.9, 0, 0.9),
            Keybutton(GK_SCANCODE_F4, "F4", 3.8, 0, 0.9),
            Keybutton(GK_SCANCODE_F5, "F5", 4.8, 0, 0.9),
            Keybutton(GK_SCANCODE_F6, "F6", 5.7, 0, 0.9),
            Keybutton(GK_SCANCODE_F7, "F7", 6.6, 0, 0.9),
            Keybutton(GK_SCANCODE_F8, "F8", 7.5, 0, 0.9),
            Keybutton(GK_SCANCODE_F9, "F9", 8.5, 0, 0.9),
            Keybutton(GK_SCANCODE_F10, "F10", 9.4, 0, 0.9),
            Keybutton(GK_SCANCODE_F11, "F11", 10.3, 0, 0.9),
            Keybutton(GK_SCANCODE_F12, "F12", 11.2, 0, 0.9),
            Keybutton(GK_SCANCODE_PRINTSCREEN, "PrtSc", 12.2, 0),
            Keybutton(GK_SCANCODE_PAUSE, "Pause", 13.2, 0),
            Keybutton(GK_SCANCODE_INSERT, "Ins", 14.2, 0),
            Keybutton(GK_SCANCODE_DELETE, "Del", 15.2, 0),

            Keybutton(GK_SCANCODE_GRAVE, "`", 0, 1.0),
            Keybutton(GK_SCANCODE_1, "1", 1.0, 1.0),
            Keybutton(GK_SCANCODE_2, "2", 2.0, 1.0),
            Keybutton(GK_SCANCODE_3, "3", 3.0, 1.0),
            Keybutton(GK_SCANCODE_4, "4", 4.0, 1.0),
            Keybutton(GK_SCANCODE_5, "5", 5.0, 1.0),
            Keybutton(GK_SCANCODE_6, "6", 6.0, 1.0),
            Keybutton(GK_SCANCODE_7, "7", 7.0, 1.0),
            Keybutton(GK_SCANCODE_8, "8", 8.0, 1.0),
            Keybutton(GK_SCANCODE_9, "9", 9.0, 1.0),
            Keybutton(GK_SCANCODE_0, "0", 10.0, 1.0),
            Keybutton(GK_SCANCODE_MINUS, "-", 11.0, 1.0),
            Keybutton(GK_SCANCODE_EQUALS, "=", 12.0, 1.0),
            Keybutton(GK_SCANCODE_BACKSPACE, "<-", 13.0, 1.0, 2.2),
            Keybutton(GK_SCANCODE_HOME, "Home", 15.2, 1.0),

            Keybutton(GK_SCANCODE_TAB, "Tab", 0.0, 2.0, 1.5),
            Keybutton(GK_SCANCODE_Q, "Q", 1.5, 2.0),
            Keybutton(GK_SCANCODE_W, "W", 2.5, 2.0),
            Keybutton(GK_SCANCODE_E, "E", 3.5, 2.0),
            Keybutton(GK_SCANCODE_R, "R", 4.5, 2.0),
            Keybutton(GK_SCANCODE_T, "T", 5.5, 2.0),
            Keybutton(GK_SCANCODE_Y, "Y", 6.5, 2.0),
            Keybutton(GK_SCANCODE_U, "U", 7.5, 2.0),
            Keybutton(GK_SCANCODE_I, "I", 8.5, 2.0),
            Keybutton(GK_SCANCODE_O, "O", 9.5, 2.0),
            Keybutton(GK_SCANCODE_P, "P", 10.5, 2.0),
            Keybutton(GK_SCANCODE_LEFTBRACKET, "[", 11.5, 2.0),
            Keybutton(GK_SCANCODE_RIGHTBRACKET, "]", 12.5, 2.0),
            Keybutton(GK_SCANCODE_BACKSLASH, "\\", 13.5, 2.0, 1.7),
            Keybutton(GK_SCANCODE_PAGEUP, "PgUp", 15.2, 2.0),

            Keybutton(GK_SCANCODE_CAPSLOCK, "Caps", 0.0, 3.0, 1.8),
            Keybutton(GK_SCANCODE_A, "A", 1.8, 3.0),
            Keybutton(GK_SCANCODE_S, "S", 2.8, 3.0),
            Keybutton(GK_SCANCODE_D, "D", 3.8, 3.0),
            Keybutton(GK_SCANCODE_F, "F", 4.8, 3.0),
            Keybutton(GK_SCANCODE_G, "G", 5.8, 3.0),
            Keybutton(GK_SCANCODE_H, "H", 6.8, 3.0),
            Keybutton(GK_SCANCODE_J, "J", 7.8, 3.0),
            Keybutton(GK_SCANCODE_K, "K", 8.8, 3.0),
            Keybutton(GK_SCANCODE_L, "L", 9.8, 3.0),
            Keybutton(GK_SCANCODE_SEMICOLON, ";", 10.8, 3.0),
            Keybutton(GK_SCANCODE_APOSTROPHE, "\'", 11.8, 3.0),
            Keybutton(GK_SCANCODE_RETURN, "Enter", 12.8, 3.0, 15.2-12.8),
            Keybutton(GK_SCANCODE_PAGEDOWN, "PgDn", 15.2, 3.0),

            Keybutton(GK_SCANCODE_LSHIFT, "Shift", 0.0, 4.0, 2.0),
            Keybutton(GK_SCANCODE_Z, "Z", 2.0, 4.0),
            Keybutton(GK_SCANCODE_X, "X", 3.0, 4.0),
            Keybutton(GK_SCANCODE_C, "C", 4.0, 4.0),
            Keybutton(GK_SCANCODE_V, "V", 5.0, 4.0),
            Keybutton(GK_SCANCODE_B, "B", 6.0, 4.0),
            Keybutton(GK_SCANCODE_N, "N", 7.0, 4.0),
            Keybutton(GK_SCANCODE_M, "M", 8.0, 4.0),
            Keybutton(GK_SCANCODE_COMMA, ",", 9.0, 4.0),
            Keybutton(GK_SCANCODE_PERIOD, ".", 10.0, 4.0),
            Keybutton(GK_SCANCODE_SLASH, "/", 11.0, 4.0),
            Keybutton(GK_SCANCODE_RSHIFT, "Shift", 12.0, 4.0, 2.2),
            //Keybutton(273, "^", 14.2, 4.0),
            Keybutton(GK_SCANCODE_END, "End", 15.2, 4.0),

            Keybutton(GK_SCANCODE_LCTRL, "Ctrl", 0.0, 5.0, 1.4),
            //Keybutton(311, "Wnd", 1.4, 5.0),
            Keybutton(GK_SCANCODE_LALT, "Alt", 2.4, 5.0, 1.1),
            Keybutton(GK_SCANCODE_SPACE, "", 3.5, 5.0, 9.0-3.5),
            Keybutton(GK_SCANCODE_RALT, "Alt", 9.0, 5.0, 1.4),
            //Keybutton(314, "Menu", 10.4, 5.0, 11.8-10.4),
            Keybutton(GK_SCANCODE_RCTRL, "Ctrl", 11.8, 5.0, 13.2-11.8),
            //Keybutton(276, "<-", 13.2, 5.0),
            //Keybutton(274, "Down", 14.2, 5.0),
            //Keybutton(275, "->", 15.2, 5.0),


            // The following need icons not text - put in separate array
            Keybutton(GK_SCANCODE_UP, "^", 14.2, 4.0),
            Keybutton(GK_SCANCODE_LGUI, "Wnd", 1.4, 5.0),
            Keybutton(GK_SCANCODE_APPLICATION, "Menu", 10.4, 5.0, 11.8-10.4),
            Keybutton(GK_SCANCODE_LEFT, "<-", 13.2, 5.0),
            Keybutton(GK_SCANCODE_DOWN, "Down", 14.2, 5.0),
            Keybutton(GK_SCANCODE_RIGHT, "->", 15.2, 5.0),

        };

    public:
        KeyboardWidget();
        void (*OnKeyboardButtonClick)(Widget *w, coord_t x, coord_t y, int key);
        void (*OnKeyboardButtonClickBegin)(Widget *w, coord_t x, coord_t y, int key);

        bool HandleMove(int x, int y);
};


#endif
