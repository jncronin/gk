#include <ext4.h>
#include <ext4_mbr.h>
#include <ext4_blockdev.h>
#include <ext4_mkfs.h>

#include <cstring>
#include <cstdlib>

#include "scheduler.h"
#include "sd.h"
#include "osqueue.h"
#include "ext4_thread.h"
#include "cache.h"
#include "gk_conf.h"
#include "process.h"

#include "vblock.h"
#include "vmem.h"
#include "pmem.h"

#include <sys/stat.h>
#include <_sys_dirent.h>

#define EXT4_DEBUG      1

// checks lwext remains in sync with our exported dir types
static_assert(EXT4_DE_UNKNOWN == DT_UNKNOWN);
static_assert(EXT4_DE_REG_FILE == DT_REG);
static_assert(EXT4_DE_DIR == DT_DIR);
static_assert(EXT4_DE_CHRDEV == DT_CHR);
static_assert(EXT4_DE_BLKDEV == DT_BLK);
static_assert(EXT4_DE_FIFO == DT_FIFO);
static_assert(EXT4_DE_SOCK == DT_SOCK);
static_assert(EXT4_DE_SYMLINK == DT_LNK);

extern char _sext4_data, _eext4_data;

extern bool sd_ready;
extern uint64_t sd_size;
static bool unmounted = false;

static int sd_open(struct ext4_blockdev *bdev);
static int sd_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
    uint32_t blk_cnt);
static int sd_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id,
    uint32_t blk_cnt);
static int sd_close(struct ext4_blockdev *bdev);

// override the definition from lwext4 here
#define static __attribute__((aligned(32))) static
EXT4_BLOCKDEV_STATIC_INSTANCE(sd, 512, 0, sd_open, sd_bread, sd_bwrite, sd_close, nullptr, nullptr);
#undef static

static ext4_blockdev sd_part;

/* message queue */
static FixedQueue<ext4_message, 8> ext4_queue;

extern "C" void *ext4_user_buf_alloc(size_t n)
{
    if(n > VBLOCK_512M)
    {
        klog("ext4: buffer size too large: %llu\n", n);
        return nullptr;
    }
    else if(n > VBLOCK_4M)
    {
        n = VBLOCK_512M;
    }
    else if(n > VBLOCK_64k)
    {
        n = VBLOCK_4M;
    }
    else
    {
        n = VBLOCK_64k;
    }

    auto vb = vblock_alloc(n, false, true, false, 0, 0, vblock, true);
    if(vb.valid)
    {
#if EXT4_DEBUG
        {
            klog("ext4: user_buf_alloc %x bytes @%x\n", n, vb.base);
        }
#endif
        return (void *)vb.base;
    }

    return (void*)nullptr;
}

extern "C" void ext4_user_buf_free(void *ptr, size_t n)
{
    if(n > VBLOCK_512M)
    {
        klog("ext4: buffer size too large: %llu\n", n);
        return;
    }
    else if(n > VBLOCK_4M)
    {
        n = VBLOCK_512M;
    }
    else if(n > VBLOCK_64k)
    {
        n = VBLOCK_4M;
    }
    else
    {
        n = VBLOCK_64k;
    }

    VMemBlock reg;
    reg.base = (uintptr_t)ptr;
    reg.length = n;
    reg.valid = true;
    vblock_free(reg, vblock, true);

#if EXT4_DEBUG
    {
        klog("ext4: user_buf_free %x bytes @%x\n", n, reg.base);
    }
#endif
}

static int get_mbr_entry()
{
	struct ext4_mbr_bdevs bdevs;
    int r = ext4_mbr_scan(&sd, &bdevs);
    if (r != EOK) {
        klog("ext4_mbr_scan error\n");
        return -2;
    }
    r = -1;
    {
        for (int i = 0; i < 4; i++)
        {
            klog("mbr_entry %d:\n", i);
            if (!bdevs.partitions[i].bdif)
            {
                klog("\tempty/unknown\n");
                continue;
            }
            else if(r == -1)
            {
                r = i;
                sd_part = bdevs.partitions[i];
            }

            klog("\toffset:  0x%" PRIx32 ", %" PRIu32 "MB\n",
                (uint32_t)bdevs.partitions[i].part_offset,
                (uint32_t)(bdevs.partitions[i].part_offset / (1024 * 1024)));
            klog("\tsize:    0x%" PRIx32 ", %" PRIu32 "MB\n",
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
    klog("ext4: registering partition %d\n", mbr_entry);
    int r = ext4_device_register(&sd_part, "sd");
    if(r != EOK)
    {
        klog("ext4: register failed %d\n", r);
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

    klog("ext4: mounting on / %s\n", readonly ? "RO" : "RW");

    int r = ext4_mount("sd", "/", readonly);
    if(r != EOK)
    {
        klog("ext4: mount failed %d\n", r);
        return r;
    }

    klog("ext4: mount complete\n");

#if !GK_EXT_READONLY
#if GK_EXT_USE_JOURNAL 
    r = ext4_recover("/");
    if(r != EOK)
    {
        klog("ext4: recover failed %d\n", r);
    }

    r = ext4_journal_start("/");
    if(r != EOK)
    {
        klog("ext4: journal_start failed %d\n", r);
    }
#endif

    // ensure we have the appropriate basic directory structure
    ext4_dir_mk("/bin");
    ext4_dir_mk("/lib");
    ext4_dir_mk("/etc");
    ext4_dir_mk("/opt");
#endif

    unmounted = false;

    {
        klog("ext4: mounted /\n");
        return 0;
    }
}

static void handle_open_message(ext4_message &msg)
{
    // try and load in file system
    ext4_file f;
    ext4_dir d;

    auto p = msg.p.lock();
    if(!p)
    {
        // message from zombie process - ignore it
        return;
    }

    // convert newlib flags to lwext4 flags
    bool is_opendir = (msg.params.open_params.mode == S_IFDIR) && (msg.params.open_params.f == O_RDONLY);

    auto extret = ext4_fopen2(&f, msg.params.open_params.pathname, msg.params.open_params.flags);
    {
        if(extret == EOK)
        {
            CriticalGuard cg(p->open_files.sl);

            if(is_opendir)
            {
                p->open_files.f[msg.params.open_params.f] = nullptr;

                msg.ss_p->ival1 = -1;
                msg.ss_p->ival2 = ENOTDIR;
                msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
            }
            auto lwfile = reinterpret_cast<LwextFile *>(
                p->open_files.f[msg.params.open_params.f].get());
            lwfile->f = f;
            msg.ss_p->ival1 = msg.params.open_params.f;
            msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
        }
        else
        {
            if(extret == ENOENT)
            {
                // try and open as directory
                extret = ext4_dir_open(&d, msg.params.open_params.pathname);
            }
            {
                CriticalGuard cg(p->open_files.sl);

                if(extret == EOK)
                {
                    auto lwfile = reinterpret_cast<LwextFile *>(
                        p->open_files.f[msg.params.open_params.f].get());
                    lwfile->d = d;
                    lwfile->is_dir = true;
                    msg.ss_p->ival1 = msg.params.open_params.f;
                    msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
                }
                else
                {
#if EXT4_DEBUG
                    {
                        klog("ext4_fopen: open(%s) failing with %d\n",
                            msg.params.open_params.pathname, extret);
                    }
#endif
                    p->open_files.f[msg.params.open_params.f] = nullptr;

                    msg.ss_p->ival1 = -1;
                    msg.ss_p->ival2 = extret;
                    msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
                }
            }
        }
    }
}

static void handle_read_message(ext4_message &msg)
{
    size_t br;
    int extret;

#if EXT4_DEBUG
    {
        klog("ext4: read(%x, %d)\n", (uint32_t)(uintptr_t)msg.params.rw_params.buf,
            msg.params.rw_params.nbytes);
    }

#endif

    {
        //SharedMemoryGuard(msg.params.rw_params.buf, msg.params.rw_params.nbytes, false, true);
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
        //SharedMemoryGuard(msg.params.rw_params.buf, msg.params.rw_params.nbytes, true, false);
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

static inline void copy_dmaaware(void *dest, const void *src, size_t n)
{
    memcpy(dest, src, n);

    //BKPT();
#if 0
#if DEBUG_EXT
    {
        klog("copy_dmaaware: dest: %x, src: %x, len %d, coreid: %d\n",
            (uint32_t)(uintptr_t)dest, (uint32_t)(uintptr_t)src, n, GetCoreID());
    }
#endif

    // handle M4 copying to M7 DTCM
    auto pstat = (uint32_t)(uintptr_t)dest;
    if((pstat >= 0x20000000) && (pstat < 0x20020000) && (GetCoreID() == 1))
    {
        SharedMemoryGuard smg(dest, n, false, true, true);
        const auto dmac = MDMA_Channel4;
        while(dmac->CCR & MDMA_CCR_EN);
        dmac->CTCR = MDMA_CTCR_SWRM |
            ((n - 1U) << MDMA_CTCR_TLEN_Pos) |
            (2U << MDMA_CTCR_DINCOS_Pos) |
            (2U << MDMA_CTCR_SINCOS_Pos) |
            (2U << MDMA_CTCR_DSIZE_Pos) |
            (2U << MDMA_CTCR_SSIZE_Pos) |
            (2U << MDMA_CTCR_DINC_Pos) |
            (2U << MDMA_CTCR_SINC_Pos);
        dmac->CBNDTR = n;
        dmac->CSAR = (uint32_t)(uintptr_t)&src;
        dmac->CDAR = pstat;
        dmac->CBRUR = 0U;
        dmac->CLAR = 0U;
        dmac->CTBR = MDMA_CTBR_DBUS;
        dmac->CMAR = 0U;
        dmac->CCR = MDMA_CCR_EN;
        dmac->CCR = MDMA_CCR_EN | MDMA_CCR_SWRQ;
        while(!(dmac->CISR & MDMA_CISR_CTCIF)) {}

        // debug
#if DEBUG_EXT
        {
            klog("copy_dmaaware: cisr: %x\n", dmac->CISR);
        }
#endif
        dmac->CIFCR = 0x1fU;
    }
    else
    {
        SharedMemoryGuard smg(dest, n, false, true);
        memcpy(dest, src, n);
    }
#endif
}

static void handle_ftruncate_message(ext4_message &msg)
{
    auto extret = ext4_ftruncate(msg.params.ftruncate_params.e4f,
        msg.params.ftruncate_params.length);
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

static void handle_fstat_message(ext4_message &msg)
{
    struct stat buf = { 0 };

    int extret;

    uint32_t t;
    auto fname = msg.params.fstat_params.pathname;
    auto f = msg.params.fstat_params.e4f;
    auto d = msg.params.fstat_params.e4d;

    [[maybe_unused]] auto ino = f ? f->inode : d->f.inode;
    {
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
        buf.st_ino = f ? f->inode : d->f.inode;
        buf.st_mode = f ? _IFREG : _IFDIR;
        
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
        buf.st_size = f ? f->fsize : d->f.fsize;
        buf.st_blksize = 512;
        buf.st_blocks = f ? ((f->fsize + 511) / 512) : 0; // round up
    }

    // handle M4 copying to M7 DTCM
#if DEBUG_EXT
    {
        klog("fstat: precopy\n");
    }
#endif
    copy_dmaaware(msg.params.fstat_params.st, &buf, sizeof(struct stat));
    msg.ss_p->ival1 = 0;
    msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);

#if DEBUG_EXT
    {
        klog("fstat: %s: blksize: %d, blocks: %d, ino: %d, mode: %x, size: %d\n",
            msg.params.fstat_params.pathname,
            buf.st_blksize,
            buf.st_blocks,
            buf.st_ino,
            buf.st_mode,
            buf.st_size);
    }
#endif
    
    return;

_err:
    {
        klog("ext4_fstat: fstat(%s) failing\n", msg.params.fstat_params.pathname);
    }
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

void handle_mkdir_message(ext4_message &msg)
{
    auto extret = ext4_dir_mk(msg.params.open_params.pathname);
    free((void *)msg.params.open_params.pathname);
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

void handle_readdir_message(ext4_message &msg)
{
    auto extret = ext4_dir_entry_next(msg.params.readdir_params.e4d);
    if(extret == nullptr)
    {
        msg.ss_p->ival1 = 0;
        msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
    }
    else
    {
        dirent nde = { 0 };
        nde.d_ino = extret->inode;
        nde.d_off = 0;
        nde.d_reclen = sizeof(dirent);
        nde.d_type = extret->inode_type;
        strncpy(nde.d_name, (const char *)extret->name, 255);
        nde.d_name[255] = 0;
        copy_dmaaware(msg.params.readdir_params.de, &nde, sizeof(dirent));
        msg.ss_p->ival1 = 1;
        msg.ss->Signal(SimpleSignal::Set, thread_signal_lwext);
    }
}

void handle_unlink_message(ext4_message &msg)
{
    auto extret = ext4_fremove(msg.params.unlink_params.pathname);
    free((void *)msg.params.unlink_params.pathname);
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

void handle_unmount_message(ext4_message &msg)
{
    auto extret = ext4_umount("/");
    unmounted = true;
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

        if(unmounted)
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

            case ext4_message::msg_type::Ftruncate:
                handle_ftruncate_message(msg);
                break;

            case ext4_message::msg_type::Lseek:
                handle_lseek_message(msg);
                break;

            case ext4_message::msg_type::Close:
                handle_close_message(msg);
                break;

            case ext4_message::msg_type::Mkdir:
                handle_mkdir_message(msg);
                break;

            case ext4_message::msg_type::ReadDir:
                handle_readdir_message(msg);
                break;

            case ext4_message::msg_type::Unlink:
                handle_unlink_message(msg);
                break;

            case ext4_message::msg_type::Unmount:
                handle_unmount_message(msg);
                break;
        }
    }
}

void init_ext4()
{
    Schedule(Thread::Create("ext4", ext4_thread, nullptr, true, GK_NPRIORITIES - 1, p_kernel));
}

int sd_open(ext4_blockdev *bdev)
{
    while(!sd_ready)
        Yield();

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
    
#if EXT4_DEBUG >= 2
    {
        klog("sd_bread: %x, %u, %u\n", (uint32_t)(uintptr_t)buf,
            (uint32_t)blk_id, blk_cnt);
    }
#endif

    auto sdr = sd_transfer(blk_id, blk_cnt, buf, true);
    //InvalidateM7Cache((uint32_t)buf, blk_cnt * 512, CacheType_t::Data);
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

    //CleanM7Cache((uint32_t)buf, blk_cnt * 512, CacheType_t::Data);
    auto sdr = sd_transfer(blk_id, blk_cnt, (void *)buf, false);
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
