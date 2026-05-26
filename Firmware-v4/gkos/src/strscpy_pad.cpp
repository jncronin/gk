#include "linux_types.h"

ssize_t strscpy_pad(char *dest, const char *src, size_t count)
{
    if(!src)
        return -EINVAL;
    if(!dest)
        return -EINVAL;

    if(count == 0)
    {
        if(*src)
            return -E2BIG;
        return 0;
    }

    ssize_t ret = 0;
    bool is_str = true;

    while(count--)
    {
        auto s = is_str ? *src++ : 0;
        ret++;

        if(!count && s)
        {
            *dest++ = 0;
            return -E2BIG;
        }
        *dest++ = s;
        if(!s)
            is_str = false;
    }
    return ret;
}
