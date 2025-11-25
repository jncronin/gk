#ifndef SD_H
#define SD_H

#include <cstdint>
#include "osmutex.h"

void init_sd();

enum sd_mode_t { LwExt4, MSC };
int sd_set_mode(sd_mode_t mode);
sd_mode_t sd_get_mode();

bool sd_get_ready();
void sd_reset();
uint64_t sd_get_size();

#define SD_NOT_READY    -1
#define SD_INPROG       -2

struct sd_request
{
    uint32_t block_start;
    uint32_t block_count;
    void *mem_address;
    bool is_read;
    SimpleSignal *completion_event;
    int *res_out;
};

int sd_transfer_async(const sd_request &req);
int sd_transfer(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read);

#endif
