#ifndef SDIF_H
#define SDIF_H

#include <cstdint>
#include "osmutex.h"
#include <stm32mp2xx.h>
#include <array>

#define SDCLK 200000000

#define SDCLK_IDENT     200000
#define SDCLK_DS        25000000
#define SDCLK_HS        50000000

class SDIF
{
    public:
        volatile uint32_t *pwr_valid_reg;
        volatile uint32_t *rcc_reg;

        int (*supply_on)() = nullptr;
        int (*supply_off)() = nullptr;
        int (*io_3v3)() = nullptr;
        int (*io_1v8)() = nullptr;
        int (*io_0)() = nullptr;

        unsigned int default_io_voltage = 3300;
        unsigned int default_supply_current = 200;

        unsigned int iface_id = 0;

        SDMMC_TypeDef *iface;

        int clock_speed = 0;
        int clock_period_ns = 0;

        uint32_t cid[4] = { 0 };
        uint32_t csd[4] = { 0 };
        uint32_t rca;
        uint32_t scr[2];
        bool is_sdio = false;
        bool is_mem = false;
        bool is_4bit = false;
        bool sd_ready = false;
        bool is_hc = false;
        bool is_1v8 = false;
        bool vswitch_failed = false;
        bool tfer_inprogress = false;
        bool dma_ready = false;
        uint32_t sd_status = 0;
        uint32_t sd_dcount = 0;
        uint32_t sd_idmabase = 0;
        uint32_t sd_idmasize = 0;
        uint32_t sd_idmactrl = 0;
        uint32_t sd_dctrl = 0;
        unsigned int sdio_n_extra_funcs = 0;
        bool cmd5_s18a = false;

        bool sd_multi = false;
        uint64_t sd_size = 0;

        uint32_t cmd6_buf[512/32];
        uint8_t cccr[0x16];

        int reset();
        int set_clock(int freq, bool ddr = false);
        uint64_t get_size() const;
        uint32_t csd_extract(int startbit, int endbit) const;
        int read_cccr();
        int read_cccr(unsigned int reg, uint8_t *val_out = nullptr);
        int write_cccr(unsigned int reg, uint8_t v);

        int sdio_enable_function(unsigned int func, bool enable);
        int sdio_enable_func_int(unsigned int func, bool enable);
        int sdio_set_func_block_size(unsigned int func, size_t block_size);

        template <unsigned int n_bytes> int sdio_read_tuple(unsigned int *addr,
            uint8_t *tuple_id, size_t *len_out,
            std::array<uint8_t, n_bytes> &d)
        {
            if(!addr)
                return -1;
            if(!is_sdio)
                return -1;
            if(!sd_ready)
                return -1;
            if(*addr < 0x1000 || *addr >= 0x18000)
                return -1;
            
            uint8_t tid, len;
            unsigned int cur_addr = *addr;
            auto ret = read_cccr(cur_addr++, &tid);
            if(ret != 0)
                return ret;
            if(tuple_id) *tuple_id = tid;
            if(tid == 0xff)
            {
                // end of chain
                *addr = 0;
                if(len_out) *len_out = 0;
                return 0;
            }

            ret = read_cccr(cur_addr++, &len);
            if(ret != 0)
                return ret;
            if(len_out) *len_out = len;

            for(unsigned int i = 0; i < len; i++)
            {
                uint8_t cb;
                ret = read_cccr(cur_addr++, &cb);
                if(ret != 0)
                    return ret;
                if(i < n_bytes)
                    d[i] = cb;
            }

            *addr = cur_addr;
            return 0;
        }

        enum class resp_type { None, R1, R1b, R2, R3, R4, R4b, R5, R6, R7 };
        enum class data_dir { None, ReadBlock, WriteBlock, ReadStream, WriteStream };

        int sd_issue_command(uint32_t command, resp_type rt, uint32_t arg = 0, uint32_t *resp = nullptr, 
            bool with_data = false,
            bool ignore_crc = false,
            int timeout_retry = 10);
};

extern SDIF sdmmc[2];
void init_sdmmc1();
void init_sdmmc2();

#endif
