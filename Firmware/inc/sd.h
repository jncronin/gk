#ifndef SD_H
#define SD_H

#include <cstdint>
#include "osmutex.h"

void init_sd();

int sd_async_complete();

int sd_read_block(uint32_t block_addr, void *ptr);
int sd_read_block_async(uint32_t block_addr, void *ptr);
int sd_read_blocks(uint32_t block_addr, uint32_t block_count, void *ptr);
int sd_read_blocks_async(uint32_t block_addr, uint32_t block_count, void *ptr);
int sd_read_block_poll(uint32_t block_addr, void *ptr);

int sd_write_block(uint32_t block_addr, const void *ptr);
int sd_write_block_async(uint32_t block_addr, const void *ptr);
int sd_write_blocks(uint32_t block_addr, uint32_t block_count, const void *ptr);
int sd_write_blocks_async(uint32_t block_addr, uint32_t block_count, const void *ptr);
int sd_write_block_poll(uint32_t block_addr, const void *ptr);

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

int sd_perform_transfer_async(const sd_request &req);
int sd_perform_transfer(uint32_t block_start, uint32_t block_count,
    void *mem_address, bool is_read, int nretries = 10);

#endif
