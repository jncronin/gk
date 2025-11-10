#include "clocks.h"

uintptr_t _clocks_cur_s_address()
{
    // TODO: update after setting up our own handlers
    return 0x0e0bfe00;      // SSBL-A address
}
