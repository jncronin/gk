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

#include "sdif.h"

#include <sys/stat.h>
#include <_sys_dirent.h>

#define EXT4_DEBUG      0

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

static bool unmounted = true;

static int sd_open(struct ext4_blockdev *bdev);
static int sd_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
    uint32_t blk_cnt);
static int sd_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id,
    uint32_t blk_cnt);
static int sd_close(struct ext4_blockdev *bdev);

// override the definition from lwext4 here
#define static __attribute__((aligned(64))) static
EXT4_BLOCKDEV_STATIC_INSTANCE(sd, 512, 0, sd_open, sd_bread, sd_bwrite, sd_close, nullptr, nullptr);
#undef static

static ext4_blockdev sd_part;

static Mutex m_ext4;

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
            klog("ext4: user_buf_alloc %llx bytes @%llx\n", n, vb.base);
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
        klog("ext4: user_buf_free %llx bytes @%llx\n", n, reg.base);
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
#endif

    unmounted = false;

    {
        klog("ext4: mounted /\n");
        return 0;
    }
}

static int check_mounted()
{
    extern bool usb_israwsd;
    if(usb_israwsd)
        return -1;
    if(unmounted)
        prepare_ext4();
    if(unmounted)
        return -1;
    else
        return 0;
}

int gk_ext4_open(const char *pathname, int flags, int mode, int fd, int *_errno)
{
    MutexGuard mg(m_ext4);
    if(check_mounted() != 0)
    {
        *_errno = ENOSYS;
        return -1;
    }

    // try and load in file system
    ext4_file f;
    ext4_dir d;

    auto p = GetCurrentProcessForCore();

    // convert newlib flags to lwext4 flags
    bool is_opendir = (mode == S_IFDIR) && (flags == O_RDONLY);

    auto extret = ext4_fopen2(&f, pathname, flags);
    {
        if(extret == EOK)
        {
            CriticalGuard cg(p->open_files.sl);

            if(is_opendir)
            {
                p->open_files.f[fd] = nullptr;

                *_errno = ENOTDIR;
                return -1;
            }
            auto lwfile = reinterpret_cast<LwextFile *>(
                p->open_files.f[fd].get());
            lwfile->f = f;
#if EXT4_DEBUG
            klog("ext4: opened %s\n", pathname);
#endif
            return fd;
        }
        else
        {
            if(extret == ENOENT)
            {
                // try and open as directory
                extret = ext4_dir_open(&d, pathname);
            }
            {
                CriticalGuard cg(p->open_files.sl);

                if(extret == EOK)
                {
                    auto lwfile = reinterpret_cast<LwextFile *>(
                        p->open_files.f[fd].get());
                    lwfile->d = d;
                    lwfile->is_dir = true;
#if EXT4_DEBUG
                    klog("ext4: opened %s as directory\n", pathname);
#endif
                    return fd;
                }
                else
                {
#if EXT4_DEBUG
                    {
                        klog("ext4_fopen: open(%s) failing with %d\n",
                            pathname, extret);
                    }
#endif
                    p->open_files.f[fd] = nullptr;

                    *_errno = extret;
                    return -1;
                }
            }
        }
    }
}

int gk_ext4_read(ext4_file &e4f, char *buf, int nbytes, int *_errno)
{
    MutexGuard mg(m_ext4);
    if(check_mounted() != 0)
    {
        *_errno = ENOSYS;
        return -1;
    }

    size_t br;
    int extret;

#if EXT4_DEBUG
    {
        klog("ext4: read(%p, %d)\n", (uint32_t)(uintptr_t)buf,
            nbytes);
    }

#endif

    extret = ext4_fread(&e4f, buf, nbytes, &br);

    if(extret == EOK)
    {
        return static_cast<int>(br);
    }
    else
    {
        *_errno = extret;
        return -1;
    }
}

int gk_ext4_write(ext4_file &e4f, const char *buf, int nbytes, int *_errno)
{
    MutexGuard mg(m_ext4);
    if(check_mounted() != 0)
    {
        *_errno = ENOSYS;
        return -1;
    }

    size_t bw;
    int extret;

    extret = ext4_fwrite(&e4f, buf, nbytes, &bw);

    if(extret == EOK)
    {
        return static_cast<int>(bw);
    }
    else
    {
        *_errno = extret;
        return -1;
    }
}

struct timespec lwext_time_to_timespec(uint32_t t)
{
    timespec ret;
    ret.tv_nsec = 0;
    ret.tv_sec = t;
    return ret;
}

int gk_ext4_ftruncate(ext4_file &e4f, off_t length, int *_errno)
{
    MutexGuard mg(m_ext4);
    if(check_mounted() != 0)
    {
        *_errno = ENOSYS;
        return -1;
    }

    auto extret = ext4_ftruncate(&e4f, length);

    if(extret == EOK)
    {
        return 0;
    }
    else
    {
        *_errno = extret;
        return -1;
    }
}

int gk_ext4_fstat(ext4_file &e4f, ext4_dir &e4d, bool is_dir, struct stat *st, const char *pathname, int *_errno)
{
    MutexGuard mg(m_ext4);
    if(check_mounted() != 0)
    {
        *_errno = ENOSYS;
        return -1;
    }

    int extret;

    uint32_t t;
    auto fname = pathname;
    auto f = is_dir ? nullptr : &e4f;
    auto d = is_dir ? &e4d : nullptr;

    [[maybe_unused]] auto ino = f ? f->inode : d->f.inode;
    {
        if((extret = ext4_atime_get(fname, &t)) != EOK)
            goto _err;
        st->st_atim = lwext_time_to_timespec(t);

        if((extret = ext4_ctime_get(fname, &t)) != EOK)
            goto _err;
        st->st_ctim = lwext_time_to_timespec(t);

        if((extret = ext4_mtime_get(fname, &t)) != EOK)
            goto _err;
        st->st_mtim = lwext_time_to_timespec(t);

        st->st_dev = 0;
        st->st_ino = f ? f->inode : d->f.inode;
        st->st_mode = f ? _IFREG : _IFDIR;
        
        uint32_t mode;
        if((extret = ext4_mode_get(fname, &mode)) != EOK)
            goto _err;
        st->st_mode |= mode;
        st->st_nlink = 1;
        
        uint32_t uid;
        uint32_t gid;
        if((extret = ext4_owner_get(fname, &uid, &gid)) != EOK)
            goto _err;
        st->st_uid = static_cast<uid_t>(uid);
        st->st_gid = static_cast<gid_t>(gid);

        st->st_rdev = 0;
        st->st_size = f ? f->fsize : d->f.fsize;
        st->st_blksize = 512;
        st->st_blocks = f ? ((f->fsize + 511) / 512) : 0; // round up
    }

#if DEBUG_EXT
    {
        klog("fstat: %s: blksize: %d, blocks: %d, ino: %d, mode: %x, size: %d\n",
            pathname,
            st->st_blksize,
            st->st_blocks,
            st->st_ino,
            st->st_mode,
            st->st_size);
    }
#endif
    
    return 0;

_err:
    {
        klog("ext4_fstat: fstat(%s) failing\n", pathname);
    }
    *_errno = extret;
    return -1;
}

int gk_ext4_lseek(ext4_file &e4f, off_t offset, int whence, int *_errno)
{
    MutexGuard mg(m_ext4);
    if(check_mounted() != 0)
    {
        *_errno = ENOSYS;
        return -1;
    }

    auto f = &e4f;
    auto extret = ext4_fseek(f, offset, whence);

    if(extret == EOK)
    {
        return f->fpos;
    }
    else
    {
        *_errno = extret;
        return -1;
    }
}

int gk_ext4_close(ext4_file &e4f, int *_errno)
{
    MutexGuard mg(m_ext4);
    if(check_mounted() != 0)
    {
        *_errno = ENOSYS;
        return -1;
    }

    auto &f = e4f;
    auto extret = ext4_fclose(&f);
    if(extret == EOK)
    {
        return 0;
    }
    else
    {
        *_errno = extret;
        return -1;
    }
}

int gk_ext4_mkdir(const char *pathname, int mode, int *_errno)
{
    MutexGuard mg(m_ext4);
    if(check_mounted() != 0)
    {
        *_errno = ENOSYS;
        return -1;
    }

    auto extret = ext4_dir_mk(pathname);
    if(extret == EOK)
    {
        return 0;
    }
    else
    {
        *_errno = extret;
        return -1;
    }
}

int gk_ext4_readdir(ext4_dir &e4d, struct dirent *de, int *_errno)
{
    MutexGuard mg(m_ext4);
    if(check_mounted() != 0)
    {
        *_errno = ENOSYS;
        return -1;
    }

    auto extret = ext4_dir_entry_next(&e4d);
    if(extret == nullptr)
    {
        // 0 = end of stream without errno set
        return 0;
    }
    else
    {
        de->d_ino = extret->inode;
        de->d_off = 0;
        de->d_reclen = sizeof(dirent);
        de->d_type = extret->inode_type;
        strncpy(de->d_name, (const char *)extret->name, 255);
        de->d_name[255] = 0;
        return 1;
    }
}

int gk_ext4_unlink(const char *pathname, int *_errno)
{
    MutexGuard mg(m_ext4);
    if(check_mounted() != 0)
    {
        *_errno = ENOSYS;
        return -1;
    }

    auto extret = ext4_fremove(pathname);
    if(extret == EOK)
    {
        return 0;
    }
    else
    {
        *_errno = extret;
        return -1;
    }
}

int gk_ext4_link(const char *oldname, const char *newname, int *_errno)
{
    MutexGuard mg(m_ext4);
    if(check_mounted() != 0)
    {
        *_errno = ENOSYS;
        return -1;
    }

    auto extret = ext4_flink(oldname, newname);
    if(extret == EOK)
    {
        return 0;
    }
    else
    {
        *_errno = extret;
        return -1;
    }
}

int gk_ext4_unmount(int *_errno)    
{
    MutexGuard mg(m_ext4);
    if(check_mounted() != 0)
    {
        *_errno = ENOSYS;
        return -1;
    }

    auto extret = ext4_umount("/");
    unmounted = true;
    if(extret == EOK)
    {
        return 0;
    }
    else
    {
        *_errno = extret;
        return -1;
    }
}

int sd_open(ext4_blockdev *bdev)
{
    extern bool usb_israwsd;
    if(usb_israwsd)
    {
        return ENXIO;
    }
    bdev->part_size = sd_get_size();
    bdev->bdif->ph_bcnt = bdev->part_size / bdev->bdif->ph_bsize;

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
