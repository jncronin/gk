#include <stdio.h>

extern "C" size_t fwrite(const void *, size_t, size_t n_items, FILE *)
{
    return n_items;
}

extern "C" int fputs(const char *, FILE *)
{
    return 0;
}

extern "C" int fputc(int c, FILE *)
{
    return c;
}
