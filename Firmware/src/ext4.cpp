#include <ext4.h>
#include <ext4_mbr.h>
#include <ext4_blockdev.h>
#include <ext4_mkfs.h>

#include <cstring>
#include <cstdlib>

#include "memblk.h"
#include "scheduler.h"
#include "sd.h"

#include "SEGGER_RTT.h"

extern Spinlock s_rtt;

extern Scheduler s;
extern Process kernel_proc;
extern char _sext4_data, _eext4_data;

extern bool sd_ready;
extern uint64_t sd_size;

#define EXT4_DATA    __attribute__((section(".ext4_data")))

static int sd_open(struct ext4_blockdev *bdev);
static int sd_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
    uint32_t blk_cnt);
static int sd_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id,
    uint32_t blk_cnt);
static int sd_close(struct ext4_blockdev *bdev);

// override the definition from lwext4 here
#define static EXT4_DATA static
EXT4_BLOCKDEV_STATIC_INSTANCE(sd, 512, 0, sd_open, sd_bread, sd_bwrite, sd_close, nullptr, nullptr);
#undef static

EXT4_DATA static ext4_blockdev sd_part;

/* Handle buffer allocations to return regions in AXISRAM */
EXT4_DATA static uint32_t buf_min = 0xffffffffUL;
EXT4_DATA static uint32_t buf_max = 0UL;

extern "C" void *ext4_user_buf_alloc(size_t n)
{
    auto reg = memblk_allocate(n, AXISRAM);
    if(!reg.valid)
        return nullptr;
    
    bool update = false;
    if(reg.address < buf_min)
    {
        buf_min = reg.address;
        update = true;
    }
    if((reg.address + reg.length) > buf_max)
    {
        buf_max = reg.address + reg.length;
        update = true;
    }
    if(update)
    {
        SetMPUForCurrentThread(MPUGenerate(buf_min, buf_max - buf_min, 7, false,
            RW, RO, N_NC_S));
    }

    return (void*)reg.address;
}

extern "C" void ext4_user_buf_free(void *ptr, size_t n)
{
    MemRegion reg;
    reg.address = (uint32_t)(uintptr_t)ptr;
    reg.length = n;
    reg.rt = MemRegionType::AXISRAM;
    reg.valid = false;
    memblk_deallocate(reg);
}

static int get_mbr_entry()
{
	struct ext4_mbr_bdevs bdevs;
    int r = ext4_mbr_scan(&sd, &bdevs);
    if (r != EOK) {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ext4_mbr_scan error\n");
        return -2;
    }
    r = -1;
    {
        CriticalGuard cg(s_rtt);
        for (int i = 0; i < 4; i++)
        {
            SEGGER_RTT_printf(0, "mbr_entry %d:\n", i);
            if (!bdevs.partitions[i].bdif)
            {
                SEGGER_RTT_printf(0, "\tempty/unknown\n");
                continue;
            }
            else if(r == -1)
            {
                r = i;
                sd_part = bdevs.partitions[i];
            }

            SEGGER_RTT_printf(0, "\toffeset: 0x%" PRIx32 ", %" PRIu32 "MB\n",
                (uint32_t)bdevs.partitions[i].part_offset,
                (uint32_t)(bdevs.partitions[i].part_offset / (1024 * 1024)));
            SEGGER_RTT_printf(0, "\tsize:    0x%" PRIx32 ", %" PRIu32 "MB\n",
                (uint32_t)bdevs.partitions[i].part_size,
                (uint32_t)(bdevs.partitions[i].part_size / (1024 * 1024)));
        }
    }

    return r;
}

static bool prepare_ext4()
{
    int mbr_entry = get_mbr_entry();
    if(mbr_entry < 0)
        return false;
    
    // now check for valid fs
    int r = ext4_device_register(&sd_part, "sd");
    if(r != EOK)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ext4: register failed %d\n", r);
        return false;
    }

    r = ext4_mount("sd", "/", false);
    if(r != EOK)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ext4: mount failed %d\n", r);
        return false;
    }

    r = ext4_recover("/");
    if(r != EOK)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ext4: recover failed %d\n", r);
        return false;
    }

    r = ext4_journal_start("/");
    if(r != EOK)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ext4: journal_start failed %d\n", r);
        return false;
    }

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ext4: mounted /\n");
        return true;
    }
}

void ext4_thread(void *_p)
{
    (void)_p;

    // test lwext4
    prepare_ext4();

    ext4_file f;
    if(ext4_fopen(&f, "/README", "r") == EOK)
    {
        auto fs = ext4_fsize(&f);

        char *strbuf = (char *)alloca(fs + 1);
        memset(strbuf, 0, fs + 1);
        size_t br;
        ext4_fread(&f, strbuf, fs, &br);
        ext4_fclose(&f);

        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "/README: %s\n", strbuf);
        }
    }
    else
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ext4: couldn't open README\n");
    }


    while(true)
    {

    }
}

void init_ext4()
{
    uint32_t data_start = (uint32_t)&_sext4_data;
    uint32_t data_end = (uint32_t)&_eext4_data;

    s.Schedule(Thread::Create("sd", ext4_thread, nullptr, true, 5, kernel_proc, 
        Either, InvalidMemregion(),
        MPUGenerate(data_start, data_end - data_start, 6, false, RW, NoAccess, N_NC_S)));
}

int sd_open(ext4_blockdev *bdev)
{
    while(!sd_ready);

    if(bdev->part_size == 0 || bdev->bdif->ph_bcnt == 0)
    {
        bdev->part_size = sd_size;
        bdev->bdif->ph_bcnt = bdev->part_size / bdev->bdif->ph_bsize;
    }

    return EOK;
}

int sd_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
    uint32_t blk_cnt)
{
    (void)bdev;
    if(!blk_cnt)
        return EOK;

    auto sdr = sd_perform_transfer(blk_id, blk_cnt, buf, true);
    SCB_InvalidateDCache_by_Addr(buf, blk_cnt * 512);
    if(sdr)
    {
        return EIO;
    }
    else
    {
        return EOK;
    }
}

int sd_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id,
    uint32_t blk_cnt)
{
    (void)bdev;
    if(!blk_cnt)
        return EOK;

    SCB_CleanDCache_by_Addr((uint32_t *)buf, blk_cnt * 512);
    auto sdr = sd_perform_transfer(blk_id, blk_cnt, (void *)buf, false);
    if(sdr)
    {
        return EIO;
    }
    else
    {
        return EOK;
    }
}

int sd_close(struct ext4_blockdev *bdev)
{
    (void)bdev;
    return EOK;
}
