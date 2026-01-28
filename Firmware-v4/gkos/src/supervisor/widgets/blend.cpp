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

// Alpha multiplier specialisations
template<> unsigned char MultiplyAlpha(unsigned char col, unsigned char alpha)
{
    // AL44
    if(alpha == 0) return 0;
    if(alpha == 255) return col;

    auto orig_alpha = (int)(col & 0xf0U);
    auto new_alpha = (int)alpha * orig_alpha;   // /256 * /256 = /63356
    new_alpha /= 4096;  // now /16
    return ((unsigned char)(new_alpha << 4)) | (col & 0x0fU);
}
