#include <cstring>
#include <cstdint>

extern "C"
void *memcpy(void *dest, const void *src, size_t n)
{
    auto d = (uint8_t *)dest;
    auto s = (const uint8_t *)src;

    while(n--)
    {
        *d++ = *s++;
    }

    return dest;
}
