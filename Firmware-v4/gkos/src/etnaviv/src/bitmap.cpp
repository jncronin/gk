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
            // found a free range
            klog("bitmap_find_free_region: order=%d, alloc %lu bits at position %lu (cword=%lu, total bits=%d)\n",
                order, nbits, i * nbits, cword, bits);
            bitmap[cword] |= ctest;
            return i * nbits;
        }

        i++;
        shift += nbits;
    }

    klog("bitmap: find_free_region failed\n");

    return -ENOMEM;
}

void bitmap_zero(uint64_t *bmp, int bits)
{
    for(auto i = 0; i < bits/64; i++)
        bmp[i] = 0;
}

void bitmap_set (unsigned long *bitmap, unsigned int start, unsigned int nbits)
{
    // HIGHLY unoptimised.
    // TODO
    /* Essentially linux creates a uint64_t first_word variable and last_word
        variable to set the non-complete words at the beginning/end of the
        range, and then sets all words in between to ~0

        Because this is mostly used in run-once init code it hasn't been
        optimised for gkos yet.
    */
    while(nbits--)
    {
        set_bit(start++, bitmap);
    }
}
