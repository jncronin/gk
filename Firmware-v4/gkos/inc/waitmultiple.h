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

template <class... T> ssize_t WaitMultipleTimed(kernel_time tout, T &... args)
{
    static_assert(sizeof...(args) <= (sizeof(size_t) * 8 - 1));
    ssize_t passed_on = 0;
    ssize_t failed_on = 0;
    auto t = GetCurrentThreadForCore();
    if(!t)
        return 0;
    
    while(true)
    {
        passed_on = 0;
        int i = 0;
        t->blocking.set_may_sleep(true);

        ([&]
        {
            if(passed_on || failed_on)
                return;
            
            auto curret = args.TryAcquire(tout);
            if(curret > 0)
            {
                passed_on = (ssize_t)1 << i;
            }
            else if(curret < 0)
            {
                failed_on = (ssize_t)1 << i;
            }
            
            i++;
        } (), ...);

        if(passed_on || failed_on || (kernel_time_is_valid(tout) && clock_cur() > tout))
            break;

        if(t->blocking.block_if_may_sleep(tout))
        {
            Yield();
        }
    }

    ([&]
    {
        args.RemoveFromWaitList();
    } (), ...);

    t->blocking.set_may_sleep(false);

    if(passed_on)
        return passed_on;
    else
        return -failed_on;
}

template <class... T> size_t WaitMultiple(T &... args)
{
    return WaitMultipleTimed(kernel_time_invalid(), args...);
}

#endif
