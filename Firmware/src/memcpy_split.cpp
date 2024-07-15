#include "osringbuffer.h"
#include "cache.h"

size_t memcpy_split_src(void *dst, const void *src, size_t n, size_t src_ptr, size_t split_ptr)
{
    // part 1
    auto p1_size = n;
    if(p1_size > (split_ptr - src_ptr))
    {
        p1_size = split_ptr - src_ptr;
    }
    memcpy(dst, &reinterpret_cast<const char *>(src)[src_ptr], p1_size);

    // part 2
    n -= p1_size;
    if(n)
    {
        memcpy(&reinterpret_cast<char *>(dst)[p1_size], src, n);
        return n;
    }
    else
    {
        return src_ptr + p1_size;
    }
}

size_t memcpy_split_dest(void *dst, const void *src, size_t n, size_t dest_ptr, size_t split_ptr,
    bool clean_dest)
{
    // part 1
    auto p1_size = n;
    if(p1_size > (split_ptr - dest_ptr))
    {
        p1_size = split_ptr - dest_ptr;
    }
    memcpy(&reinterpret_cast<char *>(dst)[dest_ptr], src, p1_size);
    if(clean_dest)
        CleanOrInvalidateM7Cache((uint32_t)&reinterpret_cast<char *>(dst)[dest_ptr], p1_size, CacheType_t::Data);

    // part 2
    n -= p1_size;
    if(n)
    {
        memcpy(dst, &reinterpret_cast<const char *>(src)[p1_size], n);
        if(clean_dest)
            CleanOrInvalidateM7Cache((uint32_t)dst, n, CacheType_t::Data);
        return n;
    }
    else
    {
        return dest_ptr + p1_size;
    }
}

