#ifndef SYSCALLS_INT_H
#define SYSCALLS_INT_H

#include <sys/types.h>
#include <sys/stat.h>

#include "thread.h"
#include "_netinet_in.h"

int get_free_fildes(Process &p);

int syscall_fstat(int file, struct stat *st, int *_errno);
int syscall_write(int file, char *buf, int nbytes, int *_errno);
int syscall_read(int file, char *buf, int nbytes, int *_errno);
int syscall_isatty(int file, int *_errno);
off_t syscall_lseek(int file, off_t offset, int whence, int *_errno);
int syscall_open(const char *pathname, int flags, int mode, int *_errno);
int syscall_close(int file, int *_errno);
int syscall_socket(int domain, int type, int protocol, int *_errno);
int syscall_bind(int sockfd, const sockaddr *addr, socklen_t addrlen, int *_errno);
int syscall_listen(int sockfd, int backlog, int *_errno);
int syscall_accept(int sockfd, void *addr, unsigned int *addrlen, int *_errno);

#endif
