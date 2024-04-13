#include <ext4.h>
#include <ext4_mbr.h>
#include <ext4_blockdev.h>
#include <ext4_mkfs.h>

#include <cstring>
#include <cstdlib>

#include "memblk.h"
#include "scheduler.h"
#include "sd.h"
#include "osqueue.h"
#include "SEGGER_RTT.h"
#include "ext4_thread.h"
#include "cache.h"
#include "gk_conf.h"
#include "ossharedmem.h"
#include "process.h"

#include <sys/stat.h>
#include <_sys_dirent.h>

// checks lwext remains in sync with our exported dir types
static_assert(EXT4_DE_UNKNOWN == DT_UNKNOWN);
static_assert(EXT4_DE_REG_FILE == DT_REG);
static_assert(EXT4_DE_DIR == DT_DIR);
static_assert(EXT4_DE_CHRDEV == DT_CHR);
static_assert(EXT4_DE_BLKDEV == DT_BLK);
static_assert(EXT4_DE_FIFO == DT_FIFO);
static_assert(EXT4_DE_SOCK == DT_SOCK);
static_assert(EXT4_DE_SYMLINK == DT_LNK);

extern Spinlock s_rtt;

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

/* message queue */
__attribute__((section(".sram4"))) static FixedQueue<ext4_message, 8> ext4_queue;

extern "C" void *ext4_user_buf_alloc(size_t n)
{
    auto reg = memblk_allocate(n, AXISRAM);
    if(!reg.valid)
        reg = memblk_allocate(n, SDRAM);
    if(!reg.valid)
        return nullptr;

#if EXT4_DEBUG
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ext4: user_buf_alloc %x bytes @%x\n", n, reg.address);
    }
#endif

    return (void*)reg.address;
}

extern "C" void ext4_user_buf_free(void *ptr, size_t n)
{
    MemRegion reg;
    reg.address = (uint32_t)(uintptr_t)ptr;
    reg.length = n;
    reg.rt = reg.address >= 0x60000000 ? MemRegionType::SDRAM : MemRegionType::AXISRAM;
    reg.valid = true;

#if EXT4_DEBUG
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ext4: user_buf_free %x bytes @%x\n", n, reg.address);
    }
#endif

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

static int do_mount();

static int prepare_ext4()
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
        return r;
    }

    return do_mount();
}

static int do_mount()
{
#if GK_EXT_READONLY
    const bool readonly = true;
#else
    const bool readonly = false;
#endif

    int r = ext4_mount("sd", "/", readonly);
    if(r != EOK)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ext4: mount failed %d\n", r);
        return r;
    }

#if !GK_EXT_READONLY
#if GK_EXT_USE_JOURNAL 
    r = ext4_recover("/");
    if(r != EOK)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ext4: recover failed %d\n", r);
    }

    r = ext4_journal_start("/");
    if(r != EOK)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ext4: journal_start failed %d\n", r);
    }
#endif

    // ensure we have the appropriate basic directory structure
    ext4_dir_mk("/bin");
    ext4_dir_mk("/lib");
    ext4_dir_mk("/etc");
    ext4_dir_mk("/opt");
#endif

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "ext4: mounted /\n");
        return 0;
    }
}

static void handle_open_message(ext4_message &msg)
{
    // try and load in file system
    ext4_file f;
    char fmode[8];

    // convert newlib flags to lwext4 flags
    auto pflags = msg.params.open_params.flags & (O_RDONLY | O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_RDWR);
    switch(pflags)
    {
        case O_RDONLY:
            strcpy(fmode, "r");
            break;

        case O_WRONLY | O_CREAT | O_TRUNC:
            strcpy(fmode, "w");
            break;

        case O_WRONLY | O_CREAT | O_APPEND:
            strcpy(fmode, "a");
            break;

        case O_RDWR:
            strcpy(fmode, "r+");
            break;

        case O_RDWR | O_CREAT | O_TRUNC:
            strcpy(fmode, "w+");
            break;

        case O_RDWR | O_CREAT | O_APPEND:
            strcpy(fmode, "a+");
            break;

        default:
            msg.ss_p->ival1 = -1;
            msg.ss_p->ival2 = EINVAL;
            msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
            return;
    }

    auto extret = ext4_fopen(&f, msg.params.open_params.pathname, fmode);
    {
        CriticalGuard cg(msg.params.open_params.p->sl);

        if(extret == EOK)
        {
            auto lwfile = reinterpret_cast<LwextFile *>(
                msg.params.open_params.p->open_files[msg.params.open_params.f]);
            lwfile->f = f;
            msg.ss_p->ival1 = msg.params.open_params.f;
            msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
        }
        else
        {
            delete msg.params.open_params.p->open_files[msg.params.open_params.f];
            msg.params.open_params.p->open_files[msg.params.open_params.f] = nullptr;

            msg.ss_p->ival1 = -1;
            msg.ss_p->ival2 = extret;
            msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
        }
    }
}

static void handle_read_message(ext4_message &msg)
{
    size_t br;
    int extret;

    {
        SharedMemoryGuard(msg.params.rw_params.buf, msg.params.rw_params.nbytes, true, false);
        extret = ext4_fread(msg.params.rw_params.e4f,
            msg.params.rw_params.buf, msg.params.rw_params.nbytes,
            &br);
    }
    if(extret == EOK)
    {
        msg.ss_p->ival1 = static_cast<int>(br);
        msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
    }
    else
    {
        msg.ss_p->ival1 = -1;
        msg.ss_p->ival2 = extret;
        msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
    }
}

static void handle_write_message(ext4_message &msg)
{
    size_t bw;
    int extret;

    {
        SharedMemoryGuard(msg.params.rw_params.buf, msg.params.rw_params.nbytes, false, true);
        extret = ext4_fwrite(msg.params.rw_params.e4f,
            msg.params.rw_params.buf, msg.params.rw_params.nbytes,
            &bw);
    }

    if(extret == EOK)
    {
        msg.ss_p->ival1 = static_cast<int>(bw);
        msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
    }
    else
    {
        msg.ss_p->ival1 = -1;
        msg.ss_p->ival2 = extret;
        msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
    }
}

struct timespec lwext_time_to_timespec(uint32_t t)
{
    timespec ret;
    ret.tv_nsec = 0;
    ret.tv_sec = t;
    return ret;
}

static void handle_fstat_message(ext4_message &msg)
{
    struct stat buf = { 0 };

    int extret;

    uint32_t t;
    auto fname = msg.params.fstat_params.pathname;
    auto f = msg.params.fstat_params.e4f;
    auto pstat = (uint32_t)(uintptr_t)msg.params.fstat_params.st;

    if((extret = ext4_atime_get(fname, &t)) != EOK)
        goto _err;
    buf.st_atim = lwext_time_to_timespec(t);

    if((extret = ext4_ctime_get(fname, &t)) != EOK)
        goto _err;
    buf.st_ctim = lwext_time_to_timespec(t);

    if((extret = ext4_mtime_get(fname, &t)) != EOK)
        goto _err;
    buf.st_mtim = lwext_time_to_timespec(t);

    buf.st_dev = 0;
    buf.st_ino = f->inode;
    buf.st_mode = _IFREG;
    
    uint32_t mode;
    if((extret = ext4_mode_get(fname, &mode)) != EOK)
        goto _err;
    buf.st_mode |= mode;
    buf.st_nlink = 1;
    
    uint32_t uid;
    uint32_t gid;
    if((extret = ext4_owner_get(fname, &uid, &gid)) != EOK)
        goto _err;
    buf.st_uid = static_cast<uid_t>(uid);
    buf.st_gid = static_cast<gid_t>(gid);

    buf.st_rdev = 0;
    buf.st_size = f->fsize;
    buf.st_blksize = 512;
    buf.st_blocks = (f->fsize + 511) / 512; // round up

    // handle M4 copying to M7 DTCM
    if((pstat >= 0x20000000) && (pstat < 0x20020000) && (GetCoreID() == 1))
    {
        const auto dmac = MDMA_Channel4;
        while(dmac->CCR & MDMA_CCR_EN);
        dmac->CTCR = MDMA_CTCR_SWRM |
            ((sizeof(struct stat) - 1U) << MDMA_CTCR_TLEN_Pos) |
            (2U << MDMA_CTCR_DINCOS_Pos) |
            (2U << MDMA_CTCR_SINCOS_Pos) |
            (2U << MDMA_CTCR_DSIZE_Pos) |
            (2U << MDMA_CTCR_SSIZE_Pos) |
            (2U << MDMA_CTCR_DINC_Pos) |
            (2U << MDMA_CTCR_SINC_Pos);
        dmac->CBNDTR = sizeof(struct stat);
        dmac->CSAR = (uint32_t)(uintptr_t)&buf;
        dmac->CDAR = pstat;
        dmac->CBRUR = 0U;
        dmac->CLAR = 0U;
        dmac->CTBR = MDMA_CTBR_DBUS;
        dmac->CMAR = 0U;
        dmac->CCR = MDMA_CCR_EN;
        dmac->CCR = MDMA_CCR_EN | MDMA_CCR_SWRQ;
        while(!(dmac->CISR & MDMA_CISR_CTCIF));
        dmac->CIFCR = 0x1fU;
    }
    else
    {
        SharedMemoryGuard(msg.params.fstat_params.st, sizeof(struct stat), false, true);
        *msg.params.fstat_params.st = buf;
    }
    msg.ss_p->ival1 = 0;
    msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
    
    return;

_err:
    msg.ss_p->ival1 = -1;
    msg.ss_p->ival2 = extret;
    msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
}

void handle_lseek_message(ext4_message &msg)
{
    auto f = msg.params.lseek_params.e4f;
    auto extret = ext4_fseek(f,
        msg.params.lseek_params.offset,
        msg.params.lseek_params.whence);
    if(extret == EOK)
    {
        msg.ss_p->ival1 = f->fpos;
        msg.ss->Signal();
    }
    else
    {
        msg.ss_p->ival1 = -1;
        msg.ss_p->ival2 = extret;
        msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
    }
}

void handle_close_message(ext4_message &msg)
{
    auto &f = msg.params.close_params.e4f;
    auto extret = ext4_fclose(&f);
    if(extret == EOK)
    {
        msg.ss_p->ival1 = 0;
        msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
    }
    else
    {
        msg.ss_p->ival1 = -1;
        msg.ss_p->ival2 = extret;
        msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
    }
}

void *ext4_thread(void *_p)
{
    (void)_p;

    // test lwext4
    prepare_ext4();

    while(true)
    {
        ext4_message msg;
        if(!ext4_queue.Pop(&msg))
            continue;
        
        switch(msg.type)
        {
            case ext4_message::msg_type::Open:
                handle_open_message(msg);
                break;

            case ext4_message::msg_type::Read:
                handle_read_message(msg);
                break;

            case ext4_message::msg_type::Write:
                handle_write_message(msg);
                break;

            case ext4_message::msg_type::Fstat:
                handle_fstat_message(msg);
                break;

            case ext4_message::msg_type::Lseek:
                handle_lseek_message(msg);
                break;

            case ext4_message::msg_type::Close:
                handle_close_message(msg);
                break;
        }
    }
}

void init_ext4()
{
    uint32_t data_start = (uint32_t)&_sext4_data;
    uint32_t data_end = (uint32_t)&_eext4_data;

    Schedule(Thread::Create("ext4", ext4_thread, nullptr, true, GK_NPRIORITIES - 1, kernel_proc, 
        PreferM4, InvalidMemregion(),
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
    InvalidateM7Cache((uint32_t)buf, blk_cnt * 512, CacheType_t::Data);
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

    CleanM7Cache((uint32_t)buf, blk_cnt * 512, CacheType_t::Data);
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

bool ext4_send_message(ext4_message &msg)
{
    return ext4_queue.Push(msg);
}
