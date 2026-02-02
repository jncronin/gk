#ifndef KLOG_BUFFER_H
#define KLOG_BUFFER_H

#include <cstddef>
#include <cstdlib>

void init_klogbuffer();
extern "C" ssize_t log_fwrite(const void *buf, size_t count);
int klogbuffer_purge_uart();
void init_klogbuffer_thread();

#endif
