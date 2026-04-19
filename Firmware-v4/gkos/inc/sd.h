#ifndef SD_H
#define SD_H

#include <cstdint>
#include <memory>
#include "osmutex.h"
#include "block_dev.h"

void init_sd();

enum sd_mode_t { LwExt4, MSC };
int sd_set_mode(sd_mode_t mode);
sd_mode_t sd_get_mode();

bool sd_get_ready();
void sd_reset();
uint64_t sd_get_size();

#define SD_NOT_READY    -1
#define SD_INPROG       -2

int sd_unmount();
int sd_transfer(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read);

std::shared_ptr<BlockDevice> sd_get_device();

#endif
