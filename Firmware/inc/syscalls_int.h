#ifndef SYSCALLS_INT_H
#define SYSCALLS_INT_H

#include <sys/types.h>
#include <sys/stat.h>

int syscall_fstat(int file, struct stat *st);
int syscall_write(int file, char *buf, int nbytes);

#endif
