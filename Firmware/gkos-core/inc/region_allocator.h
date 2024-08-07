#ifndef REGION_ALLOCATOR_H
#define REGION_ALLOCATOR_H

#include <string>
#include <vector>
#include <stdexcept>
#include <unordered_set>

const int REG_ID_SRAM4 = 4;

extern void *malloc_region(size_t n, int reg_id);
extern void free_region(void *ptr, int reg_id);
extern void *realloc_region(void *ptr, size_t size, int reg_id);
extern void *calloc_region(size_t nmemb, size_t size, int reg_id);

template <class T, int reg_id> struct GKOS_FUNC(RegionAllocator)
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
        { typedef GKOS_FUNC(RegionAllocator)<U, reg_id> other; };
#endif

        constexpr T* allocate(std::size_t n)
        {
            return reinterpret_cast<T*>(GKOS_FUNC(malloc_region)(n * sizeof(T), reg_id));
        }

        constexpr void deallocate(T* p, std::size_t n)
        {
            free_region(p, reg_id);
        }
};

template <class T> using GKOS_FUNC(SRAM4RegionAllocator) = GKOS_FUNC(RegionAllocator)<T, REG_ID_SRAM4>;

template <class CharT> using GKOS_FUNC(SRAM4BasicString) = std::basic_string<CharT, std::char_traits<CharT>, GKOS_FUNC(SRAM4RegionAllocator)<CharT>>;
using GKOS_FUNC(SRAM4String) = GKOS_FUNC(SRAM4BasicString)<char>;
template <class T> using GKOS_FUNC(SRAM4Vector) = std::vector<T, GKOS_FUNC(SRAM4RegionAllocator)<T>>;
template <class T> using GKOS_FUNC(SRAM4UnorderedSet) = std::unordered_set<T, std::hash<T>, std::equal_to<T>, GKOS_FUNC(SRAM4RegionAllocator)<T>>;

#endif
