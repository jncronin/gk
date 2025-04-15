# gkos scheduler #

The gkos scheduler exists in the Scheduler class (scheduler.h/scheduler.cpp) with the assembly code task switcher in switcher.cpp.

The scheduler is a priority-based round-robin scheduler supporting priority inheritance.  Task switches are invoked upon the PendSV interrupt.  It is currently triggered by the Cortex-M Systick however an experimental tickless mode is also available.  The Yield() function also triggers the PendSV handler and is used extensively throughout the kernel (e.g. by blocking code or by code which unblocks a higher priority task).

Choice of next thread to execute occurs within the PendSV handler, therefore is ideally as fast as possible.  Also within this function is code that unblocks any currently waiting threads of a higher priority than the current thread.
