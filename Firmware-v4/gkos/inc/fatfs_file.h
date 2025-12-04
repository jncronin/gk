#ifndef FATFS_FILE_H
#define FATFS_FILE_H

#include <string>
#include "osfile.h"

int fatfs_open(const std::string &fname, PFile *f, bool for_read, bool for_write);

#endif
