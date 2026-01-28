#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include "process.h"
extern PProcess p_supervisor;

bool init_supervisor();

/* Returns whether or not the supervisor is active (and therefore intercepting events)
    If it is, also optionally returns the screen coordinates the supervisor is active over */
bool supervisor_is_active(unsigned int *x = nullptr, unsigned int *y = nullptr,
    unsigned int *w = nullptr, unsigned int *h = nullptr);

/* Gracefully shuts down the system */
void supervisor_shutdown_system();

#endif
