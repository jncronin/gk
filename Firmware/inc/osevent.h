#ifndef OSEVENT_H
#define OSEVENT_H

struct Event
{
    enum event_type_t {
        KeyDown,
        KeyUp,
        MouseDown,
        MouseDrag,
        MouseUp,
    };

    event_type_t type;

    union
    {
        char key;
        struct 
        {
            unsigned short int x, y;
        } mouse_data;
    };
};



#endif
