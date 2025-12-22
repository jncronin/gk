#include <cstring>

extern "C" size_t strlen(const char *s)
{
    size_t ret = 0;
    while(*s++)
        ret++;
    return ret;
}
