#ifndef WAITMULTIPLE_H
#define WAITMULTIPLE_H

#include "kernel_time.h"

/* This method handles waiting on multiple primitives.
    The logic is to check all, and if not available add to the waiting list for that
        primitive, if such a thing exists.
    If none available then Block (possibly with a timeout).  If woken check the loop again.
    
    Once we exit the loop (either something was selected or we timed out), then remove
        outselves from the waiting list for the primitives, if applicable. 
        
    Returns bitmap representing the (possibly multiple) acquired primitives.
*/

template <class... T> size_t WaitMultipleTimed(kernel_time tout, T &... args)
{
    static_assert(sizeof...(args) <= (sizeof(size_t) * 8));
    size_t passed_on = 0;
    while(true)
    {
        passed_on = 0;
        int i = 0;

        ([&]
        {            
            if(args.TryAcquire(tout))
            {
                passed_on |= (size_t)1 << i;
            }
            
            i++;
        } (), ...);

        if(passed_on || (kernel_time_is_valid(tout) && clock_cur() > tout))
            break;

        Block(tout);
    }

    ([&]
    {
        args.RemoveFromWaitList();
    } (), ...);

    return passed_on;
}

#endif
