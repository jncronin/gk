#include <thread.h>
#include <sd.h>
#include <usb.h>
#include <gk_conf.h>
#include <cstring>
#include <ff.h>
#include <diskio.h>
#include <string>
#include <ext4_thread.h>
#include "syscalls_int.h"
#include "SEGGER_RTT.h"
#include "zlib.h"
#include "process.h"

#define FS_PROVISION_BLOCK_SIZE     (4*1024*1024)

/* The SD card is split into two parts to allow on-the-fly provisioning.
    We have a small FAT filesystem which is exported via USB MSC.
    
    At startup, any new provisioning files found on this FS are decompressed
    and copied to the main filesystem, after which:
     - the boot process continues
     - the small FAT fs is exported via USB to allow further updates without
        needing to unmount to main filesystem.  Then to further provision the
        system needs restarting.

    Provisioning files are of the name gk-`unix timestamp`.tar.gz, we track
    the latest installed filestamp.

    All of this needs to occur before allowing the vfs to access files,
    therefore.
*/

char fake_usb_mbr[512];
SRAM4_DATA static volatile bool fake_mbr_prepped = false;
SRAM4_DATA static volatile unsigned int fake_mbr_sector_count = 0;
SRAM4_DATA static volatile unsigned int fake_mbr_lba = 0;
static FATFS fs;
static FIL fp;
extern Spinlock s_rtt;

/* Generates a fake MBR which is exported whenever USB MSC asks to read
    sector 0 */
static bool prep_fake_mbr()
{
    if(fake_mbr_prepped)
        return true;
    
    auto sret = sd_perform_transfer(0, 1, fake_usb_mbr, true);
    if(sret != 0)
        return false;
    
    /* Partition 0 is always the provisioning partition, therefore zero
        out the rest */
    memset(&fake_usb_mbr[0x1ce], 0, 16*3);

    /* Copy to stack to avoid alignment issues */
    uint32_t p0[4];
    memcpy(p0, &fake_usb_mbr[0x1be], 16);

    /* Get end of first partition */
    auto p0_lba = p0[2];
    auto p0_len = p0[3];
    auto p0_end = p0_lba + p0_len;

    /* Check fits on disk */
    if(p0_end > sd_get_size()/512U)
    {
        return false;
    }

    fake_mbr_lba = p0_lba;
    fake_mbr_sector_count = p0_end;
    fake_mbr_prepped = true;

    return true;
}

unsigned int fake_mbr_get_sector_count()
{
    if(!fake_mbr_prepped)
        prep_fake_mbr();
    if(!fake_mbr_prepped)
        return 0U;
    return fake_mbr_sector_count;
}

const char *fake_mbr_get_mbr()
{
    if(!fake_mbr_prepped)
        prep_fake_mbr();
    if(!fake_mbr_prepped)
        return nullptr;
    return fake_usb_mbr;
}

bool fake_mbr_check_extents(unsigned int lba, unsigned int sector_count)
{
    if(!fake_mbr_prepped)
        prep_fake_mbr();
    if(!fake_mbr_prepped)
        return false;

    if(lba < fake_mbr_lba || lba >= fake_mbr_sector_count)
        return false;
    auto end = lba + sector_count;
    if(end <= fake_mbr_lba || end > fake_mbr_sector_count)
        return false;
    
    return true;
}

/* wrappers for fatfs */
DSTATUS disk_initialize(BYTE pdrv)
{
    if(!prep_fake_mbr())
        return RES_NOTRDY;
    return RES_OK;
}

DSTATUS disk_status(BYTE pdrv)
{
    return disk_initialize(pdrv);
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if(!prep_fake_mbr())
        return RES_NOTRDY;
    
    unsigned int act_lba = sector + fake_mbr_lba;
    if(!fake_mbr_check_extents(act_lba, count))
        return RES_PARERR;
    
    auto sret = sd_perform_transfer(act_lba, count, buff, true);
    if(sret != 0)
        return RES_ERROR;
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if(!prep_fake_mbr())
        return RES_NOTRDY;
    
    unsigned int act_lba = sector + fake_mbr_lba;
    if(!fake_mbr_check_extents(act_lba, count))
        return RES_PARERR;

    auto sret = sd_perform_transfer(act_lba, count, (void *)buff, false);
    if(sret != 0)
        return RES_ERROR;
    return RES_OK;
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff)
{
    return RES_OK;
}

typedef size_t (*fread_func)(void *ptr, size_t size, void *f);
typedef ssize_t (*lseek_func)(void *f, size_t offset);

static size_t direct_fread(void *ptr, size_t size, void *f)
{
    UINT br;
    auto fr = f_read((FIL *)f, ptr, size, &br);
    if(fr != FR_OK)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "direct_fread: f_read failed %d\n", fr);
        return 0;
    }

    return br;
}

static ssize_t direct_lseek(void *f, size_t offset)
{
    auto fr = f_lseek((FIL *)f, offset);
    if(fr != FR_OK)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "direct_lseek: f_lseek failed %d\n", fr);
        return -1;
    }
    return offset;
}

static size_t gz_fread(void *ptr, size_t size, void *f)
{
    auto ret = gzread((gzFile)f, ptr, size);
    return ret;
}

static ssize_t gz_lseek(void *f, size_t offset)
{
    return gzseek((gzFile)f, offset, SEEK_SET);
}

/* written using function pointers to allow us to easily add a gzip     
    version later */
static char tar_header[512];

static bool is_zero_sector(const char *hdr)
{
    for(int i = 0; i < 512; i++)
    {
        if(hdr[i])
            return false;
    }
    return true;
}

static int fs_provision_tarball(fread_func ff, lseek_func lf, void *f)
{
    lf(f, 0);

    int n_zero_sectors = 0;

    // Get scratch region for transferring data
    auto mem = memblk_allocate(FS_PROVISION_BLOCK_SIZE, MemRegionType::SDRAM);
    if(!mem.valid)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "fs_provision: couldn't allocate buffer\n");
        return -1;
    }
    memset((void *)mem.address, 0, mem.length);

    while(true)
    {
        auto ffret = ff(tar_header, 512, f);
        if(ffret == 0)
        {
            memblk_deallocate(mem);
            return 0;   // EOF
        }
        if(ffret != 512)
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "fs_provision: tar header read failed\n");
            memblk_deallocate(mem);
            return -1;
        }

        // EOF marked by two consecutive zero sectors
        if(is_zero_sector(tar_header))
        {
            n_zero_sectors++;
            if(n_zero_sectors >= 2)
            {
                memblk_deallocate(mem);
                return 0;       // EOF
            }
            continue;
        }
        else
        {
            n_zero_sectors = 0;
        }

        // check we are ustar
        if(strncmp("ustar", &tar_header[257], 5))
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "fs_provision: tar not ustar\n");
            memblk_deallocate(mem);
            return -1;
        }

        auto type = tar_header[156];
        std::string fname;
        if(tar_header[345])
            fname = "/" + std::string(&tar_header[345]) + std::string(&tar_header[0]);
        else
            fname = "/" + std::string(&tar_header[0]);

        // get file size, octal encoded over 12 bytes
        uint64_t fsize = 0;
        int nfsize = 0;
        const char *fsptr = &tar_header[124];
        while(*fsptr != 0 && *fsptr != ' ')
        {
            fsize *= 8;
            fsize += (uint64_t)(*fsptr - '0');
            fsptr++;
            nfsize++;
            if(nfsize >= 12)
                break;
        }
        
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "fs_provision: %s (%d bytes), type %d\n", fname.c_str(),
                (int)fsize, type);
        }

        if(type == 0 || type == '0')
        {
            // regular file

            // get total to read
            uint64_t total_fsize_to_read = (fsize + 511ULL) & ~511ULL;

            // open ext4 file
            int fd = deferred_call(syscall_open, fname.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0);
            if(fd < 0)
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "fs_provision: fopen failed: %d\n", errno);
                memblk_deallocate(mem);
                return -1;
            }

            // load in 4 MiB increments
            uint64_t offset = 0;

            while(true)
            {
                auto fsize_to_read = total_fsize_to_read - offset;
                if(fsize_to_read == 0U)
                    break;
                if(fsize_to_read > mem.length)
                    fsize_to_read = mem.length;
                
                ffret = ff((void *)mem.address, fsize_to_read, f);
                if(ffret != fsize_to_read)
                {
                    CriticalGuard cg(s_rtt);
                    SEGGER_RTT_printf(0, "fs_provision: failed to read file: %d\n", ffret);
                    memblk_deallocate(mem);
                    return -1;
                }

                auto fsize_to_write = fsize - offset;
                if(fsize_to_write == 0U)
                    break;
                if(fsize_to_write > mem.length)
                    fsize_to_write = mem.length;

                auto bw = deferred_call(syscall_write, fd, (char *)mem.address, (int)fsize_to_write);
                if(bw != (int)fsize_to_write)
                {
                    CriticalGuard cg(s_rtt);
                    SEGGER_RTT_printf(0, "fs_provision: fwrite failed: %d, expected %d\n",
                        bw, (int)fsize);
                    memblk_deallocate(mem);
                    return -1;
                }

                {
                    CriticalGuard cg(s_rtt);
                    SEGGER_RTT_printf(0, "fs_provision: read %d, wrote %d at offset %d\n",
                        (int)fsize_to_read, (int)fsize_to_write, (int)offset);
                }

                offset += fsize_to_read;
            }
            close(fd);
        }
    }
}

int fs_provision()
{
    // mount a read-only FatFS system from partition 0
    if(!prep_fake_mbr())
        return -1;
    
    //FATFS fs;
    auto fr = f_mount(&fs, "", 0);
    if(fr != FR_OK)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "fs_provision: f_mount failed %d\n", fr);
        return fr;
    }

    // List root directory
    DIR dp;
    fr = f_opendir(&dp, "/");
    if(fr != FR_OK)
    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "fs_provision: f_opendir failed %d\n", fr);
        return fr;
    }

    bool had_failure = false;

    while(true)
    {
        if(had_failure)
            break;
        
        FILINFO fi;
        fr = f_readdir(&dp, &fi);
        if(fr != FR_OK || fi.fname[0] == 0)
            break;
        
        if(!(fi.fattrib & AM_DIR))
        {
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "fs_provision: found file %s (%d bytes)\n", fi.fname, (unsigned int)fi.fsize);
            }

            // Get file type
            bool is_tar = false;
            bool is_targz = false;

            auto fnlen = strlen(fi.fname);
            if(fnlen > 4 && !strncmp(".tar", &fi.fname[fnlen - 4], 4))
            {
                is_tar = true;
            }
            if(fnlen > 7 && !strncmp(".tar.gz", &fi.fname[fnlen - 7], 7))
            {
                is_targz = true;
            }

            {
                CriticalGuard cg(s_rtt);
                if(is_tar)
                    SEGGER_RTT_printf(0, "fs_provision: is_tar\n");
                if(is_targz)
                    SEGGER_RTT_printf(0, "fs_provision: is_targz\n");
                if(!is_tar && !is_targz)
                    SEGGER_RTT_printf(0, "fs_provision: unsupported file\n");
            }

            // install files
            if(is_tar || is_targz)
            {
                int fs_p_ret = 0;
                //FIL fp;
                fr = f_open(&fp, fi.fname, FA_READ);
                if(fr != FR_OK)
                {
                    CriticalGuard cg(s_rtt);
                    SEGGER_RTT_printf(0, "fs_provision: couldn't f_open %s: %d\n", fi.fname, fr);
                    had_failure = true;
                    break;
                }

                if(is_tar)
                {
                    fs_p_ret = fs_provision_tarball(direct_fread, direct_lseek, &fp);
                    f_close(&fp);
                }
                else if(is_targz)
                {
                    // try and get free process file handle
                    int fd;

                    {
                        auto t = GetCurrentThreadForCore();
                        auto &p = t->p;
                        CriticalGuard _p(p->sl);
                        fd = get_free_fildes(p);
                        if(fd < 0)
                        {
                            CriticalGuard cg(s_rtt);
                            SEGGER_RTT_printf(0, "fs_provision: couldn't assign fildes\n");
                            f_close(&fp);
                        }
                        else
                        {
                            p.open_files[fd] = new FatfsFile(&fp, std::string(fi.fname));
                        }
                    }

                    if(fd >= 0)
                    {
                        auto gzf = gzdopen(fd, "r");
                        if(gzf == NULL)
                        {
                            CriticalGuard cg(s_rtt);
                            SEGGER_RTT_printf(0, "fs_provision: gzdopen failed\n");
                            close(fd);
                        }
                        else
                        {
                            // sensible buffer sizes bearing in mind it allocates 3x this
                            gzbuffer(gzf, 8192*4);

                            fs_p_ret = fs_provision_tarball(gz_fread, gz_lseek, gzf);
                            gzclose(gzf);
                        }
                    }
                }

                if(fs_p_ret == 0)
                {
                    {
                        CriticalGuard cg(s_rtt);
                        SEGGER_RTT_printf(0, "fs_provision: provisioned %s\n", fi.fname);
                    }

                    // delete file
                    if(f_unlink(fi.fname) != FR_OK)
                    {
                        CriticalGuard cg(s_rtt);
                        SEGGER_RTT_printf(0, "fs_provision: failed to delete %s: %d\n",
                            fi.fname, f_unlink(fi.fname));
                    }
                }
                else
                {
                    {
                        CriticalGuard cg(s_rtt);
                        SEGGER_RTT_printf(0, "fs_provision: failed to provision %s\n", fi.fname);
                    }
                    had_failure = true;
                    break;
                }
            }
        }
    }
    f_closedir(&dp);
    f_unmount("/");

    return 0;
}

/* We implement these as wrappers for zlib gz_* interface to allocate buffers suitable
    for SDMMC DMA access */
#include <map>
SRAM4_DATA static std::map<uint32_t, MemRegion> gz_malloc_regions;

extern "C" void *gz_malloc_buffer(size_t n)
{
    auto mr = memblk_allocate(n, MemRegionType::SDRAM);
    if(!mr.valid)
        mr = memblk_allocate(n, MemRegionType::SDRAM);
    if(!mr.valid)
    {
        __asm__ volatile ("bkpt \n" ::: "memory");
        return nullptr;
    }

    memset((void *)mr.address, 0, mr.length);
    
    gz_malloc_regions[mr.address] = mr;
    return (void *)mr.address;
}

extern "C" void gz_free_buffer(void *address)
{
    auto iter = gz_malloc_regions.find((uint32_t)(uintptr_t)address);
    if(iter != gz_malloc_regions.end())
    {
        auto mr = iter->second;
        memblk_deallocate(mr);
        gz_malloc_regions.erase(iter);
    }
    else
    {
        __asm__ volatile ("bkpt \n" ::: "memory");
    }
}

extern "C" void *zcalloc(void *opaque, unsigned items, unsigned size)
{
    auto len = items * size;
    auto ret = gz_malloc_buffer(len);
    if(ret)
    {
        memset(ret, 0, len);
    }
    return ret;
}

extern "C" void zcfree(void *opaque, void *ptr)
{
    gz_free_buffer(ptr);
}
