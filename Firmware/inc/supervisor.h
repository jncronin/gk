#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include "process.h"
extern Process p_supervisor;

void init_supervisor();

/* Returns whether or not the supervisor is active (and therefore intercepting events)
    If it is, also optionally returns the screen coordinates the supervisor is active over */
bool supervisor_is_active(unsigned int *x, unsigned int *y, unsigned int *w, unsigned int *h);

#endif
