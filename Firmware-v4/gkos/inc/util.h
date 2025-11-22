#ifndef UTIL_H
#define UTIL_H

#include <type_traits>

template <typename T> constexpr size_t highest_bit_set(T x)
{
    if(x == 0)
        return ~((T)0);
    size_t ret = 0;
    using uT = std::make_unsigned_t<T>;
    auto xu = (uT)x;
    auto high_bit = (uT)1 << (sizeof(T) * 8 - 1);
    while((xu & high_bit) == 0)
    {
        xu <<= 1;
        ret++;
    }
    return sizeof(T) * 8 - ret - 1;
}

template <typename T> constexpr T align_power_2(T x)
{
    auto hb = highest_bit_set(x);
    auto hb_shift = (T)1 << hb;
    return (x + (hb_shift - 1)) & ~(hb_shift - 1);
}

#endif
