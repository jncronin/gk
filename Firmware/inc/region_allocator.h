#ifndef REGION_ALLOCATOR_H
#define REGION_ALLOCATOR_H

#include <string>
#include <vector>
#include <stdexcept>

const int REG_ID_SRAM4 = 4;

template <class T, int reg_id> struct RegionAllocator
{
    public:
        typedef T             value_type;
        typedef size_t        size_type;
        typedef ptrdiff_t     difference_type;

#if __cplusplus <= 201703L
        // These were removed for C++20.
        typedef T*       pointer;
        typedef const T* const_pointer;
        typedef T&       reference;
        typedef const T& const_reference;

        template<typename U>
        struct rebind
        { typedef RegionAllocator<U, reg_id> other; };
#endif

        constexpr T* allocate(std::size_t n)
        {
            throw std::runtime_error("unimplemented");
        }

        constexpr void deallocate(T* p, std::size_t n)
        {
            throw std::runtime_error("unimplemented");
        }
};

template <class T> using SRAM4RegionAllocator = RegionAllocator<T, REG_ID_SRAM4>;

template <class CharT> using SRAM4BasicString = std::basic_string<CharT, std::char_traits<CharT>, SRAM4RegionAllocator<CharT>>;
using SRAM4String = SRAM4BasicString<char>;
template <class T> using SRAM4Vector = std::vector<T, SRAM4RegionAllocator<T>>;

#endif
