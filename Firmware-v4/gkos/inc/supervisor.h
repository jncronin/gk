#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include "process.h"
#include "_gk_proccreate.h"
extern PProcess p_supervisor;

bool init_supervisor();

/* Returns whether or not the supervisor is active (and therefore intercepting events) */
bool supervisor_is_active();
bool supervisor_is_active_for_point(unsigned int x, unsigned int y);

void supervisor_set_active(bool active, const gk_supervisor_visible_region *regs, size_t nregs);

/* Gracefully shuts down the system */
void supervisor_shutdown_system();

/* Update the userspace info about the system, and ping gk_menu */
int supervisor_update_userpace();

#endif
