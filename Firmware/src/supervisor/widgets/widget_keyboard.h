#ifndef WIDGET_KEYBOARD_H
#define WIDGET_KEYBOARD_H

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
            Keybutton(27, "Esc", 0, 0),
            Keybutton(282, "F1", 1.1, 0, 0.9),
            Keybutton(283, "F2", 2.0, 0, 0.9),
            Keybutton(284, "F3", 2.9, 0, 0.9),
            Keybutton(285, "F4", 3.8, 0, 0.9),
            Keybutton(286, "F5", 4.8, 0, 0.9),
            Keybutton(287, "F6", 5.7, 0, 0.9),
            Keybutton(288, "F7", 6.6, 0, 0.9),
            Keybutton(289, "F8", 7.5, 0, 0.9),
            Keybutton(290, "F9", 8.5, 0, 0.9),
            Keybutton(291, "F10", 9.4, 0, 0.9),
            Keybutton(292, "F11", 10.3, 0, 0.9),
            Keybutton(293, "F12", 11.2, 0, 0.9),
            Keybutton(316, "PrtSc", 12.2, 0),
            Keybutton(19, "Pause", 13.2, 0),
            Keybutton(277, "Ins", 14.2, 0),
            Keybutton(127, "Del", 15.2, 0),

            Keybutton('`', "`", 0, 1.0),
            Keybutton('1', "1", 1.0, 1.0),
            Keybutton('2', "2", 2.0, 1.0),
            Keybutton('3', "3", 3.0, 1.0),
            Keybutton('4', "4", 4.0, 1.0),
            Keybutton('5', "5", 5.0, 1.0),
            Keybutton('6', "6", 6.0, 1.0),
            Keybutton('7', "7", 7.0, 1.0),
            Keybutton('8', "8", 8.0, 1.0),
            Keybutton('9', "9", 9.0, 1.0),
            Keybutton('0', "0", 10.0, 1.0),
            Keybutton(45, "-", 11.0, 1.0),
            Keybutton(43, "+", 12.0, 1.0),
            Keybutton(8, "<-", 13.0, 1.0, 2.2),
            Keybutton(278, "Home", 15.2, 1.0),

            Keybutton(9, "Tab", 0.0, 2.0, 1.5),
            Keybutton('q', "Q", 1.5, 2.0),
            Keybutton('w', "W", 2.5, 2.0),
            Keybutton('e', "E", 3.5, 2.0),
            Keybutton('r', "R", 4.5, 2.0),
            Keybutton('t', "T", 5.5, 2.0),
            Keybutton('y', "Y", 6.5, 2.0),
            Keybutton('u', "U", 7.5, 2.0),
            Keybutton('i', "I", 8.5, 2.0),
            Keybutton('o', "O", 9.5, 2.0),
            Keybutton('p', "P", 10.5, 2.0),
            Keybutton('[', "[", 11.5, 2.0),
            Keybutton(']', "]", 12.5, 2.0),
            Keybutton('\\', "\\", 13.5, 2.0, 1.7),
            Keybutton(280, "PgUp", 15.2, 2.0),

            Keybutton(301, "Caps", 0.0, 3.0, 1.8),
            Keybutton('a', "A", 1.8, 3.0),
            Keybutton('s', "S", 2.8, 3.0),
            Keybutton('d', "D", 3.8, 3.0),
            Keybutton('f', "F", 4.8, 3.0),
            Keybutton('g', "G", 5.8, 3.0),
            Keybutton('h', "H", 6.8, 3.0),
            Keybutton('j', "J", 7.8, 3.0),
            Keybutton('k', "K", 8.8, 3.0),
            Keybutton('l', "L", 9.8, 3.0),
            Keybutton(';', ";", 10.8, 3.0),
            Keybutton('\'', "\'", 11.8, 3.0),
            Keybutton(13, "Enter", 12.8, 3.0, 15.2-12.8),
            Keybutton(281, "PgDn", 15.2, 3.0),

            Keybutton(304, "Shift", 0.0, 4.0, 2.0),
            Keybutton('z', "Z", 2.0, 4.0),
            Keybutton('x', "X", 3.0, 4.0),
            Keybutton('c', "C", 4.0, 4.0),
            Keybutton('v', "V", 5.0, 4.0),
            Keybutton('b', "B", 6.0, 4.0),
            Keybutton('n', "N", 7.0, 4.0),
            Keybutton('m', "M", 8.0, 4.0),
            Keybutton(',', ",", 9.0, 4.0),
            Keybutton('.', ".", 10.0, 4.0),
            Keybutton('/', "/", 11.0, 4.0),
            Keybutton(303, "Shift", 12.0, 4.0, 2.2),
            //Keybutton(273, "^", 14.2, 4.0),
            Keybutton(279, "End", 15.2, 4.0),

            Keybutton(306, "Ctrl", 0.0, 5.0, 1.4),
            //Keybutton(311, "Wnd", 1.4, 5.0),
            Keybutton(308, "Alt", 2.4, 5.0, 1.1),
            Keybutton(' ', "", 3.5, 5.0, 9.0-3.5),
            Keybutton(307, "Alt", 9.0, 5.0, 1.4),
            //Keybutton(314, "Menu", 10.4, 5.0, 11.8-10.4),
            Keybutton(305, "Ctrl", 11.8, 5.0, 13.2-11.8),
            //Keybutton(276, "<-", 13.2, 5.0),
            //Keybutton(274, "Down", 14.2, 5.0),
            //Keybutton(275, "->", 15.2, 5.0),


            // The following need icons not text - put in separate array
            Keybutton(273, "^", 14.2, 4.0),
            Keybutton(311, "Wnd", 1.4, 5.0),
            Keybutton(314, "Menu", 10.4, 5.0, 11.8-10.4),
            Keybutton(276, "<-", 13.2, 5.0),
            Keybutton(274, "Down", 14.2, 5.0),
            Keybutton(275, "->", 15.2, 5.0),

        };

    public:
        KeyboardWidget();
        void (*OnKeyboardButtonClick)(Widget *w, coord_t x, coord_t y, int key);
        void (*OnKeyboardButtonClickBegin)(Widget *w, coord_t x, coord_t y, int key);

        bool HandleMove(int x, int y);
};


#endif
