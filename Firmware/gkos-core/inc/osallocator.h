#ifndef OSALLOCATOR_H
#define OSALLOCATOR_H

#include <cstdlib>
#include <limits>

template <int region_id> void *GKOS_FUNC(region_alloc)(size_t size);
template <int region_id> void GKOS_FUNC(region_free)(void *ptr);

template <class T, int region_id> class GKOS_FUNC(GKAllocator)
{
    public:
        // type definitions
        typedef T        value_type;
        typedef T*       pointer;
        typedef const T* const_pointer;
        typedef T&       reference;
        typedef const T& const_reference;
        typedef std::size_t    size_type;
        typedef std::ptrdiff_t difference_type;

        // rebind allocator to type U
        template <class U>
        struct rebind {
            typedef GKOS_FUNC(GKAllocator)<U, region_id> other;
        };

        // return address of values
        pointer address (reference value) const {
            return &value;
        }
        const_pointer address (const_reference value) const {
            return &value;
        }

        /* constructors and destructor
        * - nothing to do because the allocator has no state
        */
        GKOS_FUNC(GKAllocator)() throw() {
        }
        GKOS_FUNC(GKAllocator)(const GKOS_FUNC(GKAllocator)&) throw() {
        }
        template <class U>
            GKOS_FUNC(GKAllocator) (const GKOS_FUNC(GKAllocator)<U, region_id>&) throw() {
        }
        ~GKOS_FUNC(GKAllocator)() throw() {
        }

        // return maximum number of elements that can be allocated
        size_type max_size () const throw() {
            return std::numeric_limits<std::size_t>::max() / sizeof(T);
        }

        // allocate but don't initialize num elements of type T
        pointer allocate (size_type num, const void* = 0) {
            pointer ret = (pointer)region_alloc<region_id>(num * sizeof(T));
            return ret;
        }

        // initialize elements of allocated storage p with value value
        void construct (pointer p, const T& value) {
            // initialize memory with placement new
            new((void*)p)T(value);
        }

        // destroy elements of initialized storage p
        void destroy (pointer p) {
            // destroy objects by calling their destructor
            p->~T();
        }

        // deallocate storage p of deleted elements
        void deallocate (pointer p, size_type num) {
            region_free<region_id>(p);
        }
};

// return that all specializations of this allocator are interchangeable
template <class T1, class T2, int reg_id>
bool operator== (const GKOS_FUNC(GKAllocator)<T1, reg_id>&,
                const GKOS_FUNC(GKAllocator)<T2, reg_id>&) throw() {
    return true;
}

template <class T1, class T2, int reg_id>
bool operator!= (const GKOS_FUNC(GKAllocator)<T1, reg_id>&,
                const GKOS_FUNC(GKAllocator)<T2, reg_id>&) throw() {
    return false;
}

#endif
