#include "etnaviv_drv.h"
#include "etnaviv_gpu.h"
#include "etnaviv_cmdbuf.h"


/* bits is number of bits in the bitmap
    order is log2 of number of bits
*/

int bitmap_find_free_region(uint64_t * bitmap,
 	int bits,
 	int order)
{
    auto nbits = 1UL << order;
    auto trials = bits / nbits;
    if(nbits > 64)
    {
        klog("bitmap_find_free_region not implemented for order %d\n", order);
        return -EINVAL;
    }
    auto tests_per_word = 64 / nbits;
    auto test = (1UL << nbits) - 1;
    for(auto i = 0ul, shift = 0ul; i < trials; )
    {
        shift &= 63;
        auto ctest = test << shift;
        auto cword = i / tests_per_word;

        if(bitmap[cword] == ~0UL)
        {
            // full word
            i += tests_per_word;
            continue;
        }

        if((bitmap[cword] & ctest) == 0)
        {
            // free bit
            bitmap[cword] |= ctest;
            return i * nbits;
        }

        i++;
        shift += nbits;
    }

    return -ENOMEM;
}
