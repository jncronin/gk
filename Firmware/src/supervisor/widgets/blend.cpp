#include "widget.h"

color_t DoBlend(color_t fg, color_t bg)
{
    if(sizeof(color_t) == 1)
    {
        // AL44 'blend' - just return fg if >=50% opacity else bg
        if((fg & 0xf0) >= 0x80)
            return fg;
        else
            return bg;
    }
}