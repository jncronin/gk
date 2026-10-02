#ifndef WAITMULTIPLE_H
#define WAITMULTIPLE_H

#include "kernel_time.h"

/* This method handles waiting on multiple primitives.
    The logic is to check all, and if not available add to the waiting list for that
        primitive, if such a thing exists.  If available stop the loop.
    If none available then Block (possibly with a timeout).  If woken check the loop again.
    
    Once we exit the loop (either something was selected or we timed out), then remove
        outselves from the waiting list for the primitives, if applicable. 
        
    Returns index of the passed primitive, or -1 for time out.
*/

template <class... T> int WaitMultipleTimed(kernel_time tout, T &... args)
{
    int passed_on = 0;
    while(true)
    {
        passed_on = 0;
        int i = 0;

        ([&]
        {
            if(passed_on)
                return;
            
            if(args.TryAcquire(tout))
                passed_on = i + 1;
            
            i++;
        } (), ...);

        if(passed_on || clock_cur() > tout)
            break;

        Block(tout);
    }

    ([&]
    {
        args.RemoveFromWaitList();
    } (), ...);

    return passed_on - 1;
}

#endif
