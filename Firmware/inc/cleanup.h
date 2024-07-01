#ifndef CLEANUP_H
#define CLEANUP_H

#include "thread.h"
#include <sys/types.h>
#include "osqueue.h"

struct cleanup_message
{
    bool is_thread;
    union
    {
        Thread *t;
        Process *p;
    };   
};

using CleanupQueue_t = FixedQueue<cleanup_message, 8>;

extern CleanupQueue_t CleanupQueue;

void *cleanup_thread(void *);

#endif
