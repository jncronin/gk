#ifndef SYSCALLS_H
#define SYSCALLS_H

enum syscall_no
{
    StartFirstThread = 0
};

extern "C" {
    void SyscallHandler(syscall_no num, void *r1, void *r2, void *r3);
}

#endif
