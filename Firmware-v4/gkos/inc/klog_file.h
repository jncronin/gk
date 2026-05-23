#ifndef KLOG_FILE_H
#define KLOG_FILE_H

#include "lockfreebuffer.h"

void init_klogfile();
int klogbuffer_purge_file(LockFreeBuffer &buf);

#endif
