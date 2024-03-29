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

    while(true)
    {
        auto ffret = ff(tar_header, 512, f);
        if(ffret == 0)
            return 0;   // EOF
        if(ffret != 512)
        {
            CriticalGuard cg(s_rtt);
            SEGGER_RTT_printf(0, "fs_provision: tar header read failed\n");
            return -1;
        }

        // EOF marked by two consecutive zero sectors
        if(is_zero_sector(tar_header))
        {
            n_zero_sectors++;
            if(n_zero_sectors >= 2)
                return 0;       // EOF
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
        const char *fsptr = &tar_header[124];
        while(*fsptr)
        {
            fsize *= 8;
            fsize += (uint64_t)(*fsptr - '0');
            fsptr++;
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
            uint64_t fsize_to_read = (fsize + 511ULL) & ~511ULL;

            auto mem = memblk_allocate(fsize_to_read, MemRegionType::AXISRAM);
            if(!mem.valid)
                mem = memblk_allocate(fsize_to_read, MemRegionType::SDRAM);
            if(!mem.valid)
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "fs_provision: couldn't allocate buffer\n");
                return -1;
            }

            ffret = ff((void *)mem.address, fsize_to_read, f);
            if(ffret != fsize_to_read)
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "fs_provision: failed to read file: %d\n", ffret);
                memblk_deallocate(mem);
                return -1;
            }

            // write to ext4
            int fd = deferred_call(syscall_open, fname.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0);
            if(fd < 0)
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "fs_provision: fopen failed: %d\n", errno);
                memblk_deallocate(mem);
                return -1;
            }
            auto bw = deferred_call(syscall_write, fd, (char *)mem.address, (int)fsize);
            close(fd);            
            memblk_deallocate(mem);
            if(bw != (int)fsize)
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "fs_provision: fwrite failed: %d, expected %d\n",
                    bw, (int)fsize);
                return -1;
            }
        }
    }
}

int fs_provision()
{
    // mount a read-only FatFS system from partition 0
    if(!prep_fake_mbr())
        return -1;

    // Get the most recently provisioned file from ext4
    unsigned long long int c_prov_tstamp = 0ULL;
    auto er = deferred_call(syscall_open, "/provision.txt", O_RDONLY, 0);
    if(er >= 0)
    {
        char buf[32];
        memset(buf, 0, 32);
        auto br = deferred_call(syscall_read, er, buf, 31);
        if(br > 0)
        {
            c_prov_tstamp = atoll(buf);
        }
        close(er);
    }

    {
        CriticalGuard cg(s_rtt);
        SEGGER_RTT_printf(0, "fs_provision: c_prov_tstamp: %d\n", (int)c_prov_tstamp);
    }
    
    FATFS fs;
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
    unsigned long long max_provisioned = 0ULL;

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
            [[maybe_unused]] bool is_tar = false;
            [[maybe_unused]] bool is_targz = false;

            auto fnlen = strlen(fi.fname);
            unsigned long long int tstamp = 0LL;
            if(fnlen > 3 && !strncmp("gk-", fi.fname, 3))
            {
                int tstamp_start = 0, tstamp_end = 0;
                if(fnlen > 7 && !strncmp(".tar", &fi.fname[fnlen - 4], 4))
                {
                    is_tar = true;
                    tstamp_start = 3;
                    tstamp_end = fnlen - 4;
                }
                if(fnlen > 10 && !strncmp(".tar.gz", &fi.fname[fnlen - 7], 7))
                {
                    is_targz = true;
                    tstamp_start = 3;
                    tstamp_end = fnlen - 7;
                }

                if(tstamp_start)
                {
                    char ctstamp[32];
                    auto tstamp_len = std::min(32, tstamp_end - tstamp_start);
                    memcpy(ctstamp, &fi.fname[tstamp_start], tstamp_len);
                    ctstamp[31] = 0;

                    tstamp = atoll(ctstamp);
                }
            }

            {
                CriticalGuard cg(s_rtt);
                if(is_tar)
                    SEGGER_RTT_printf(0, "fs_provision: is_tar tstamp: %d\n", (int)tstamp);
                if(is_targz)
                    SEGGER_RTT_printf(0, "fs_provision: is_targz: %d\n", (int)tstamp);
                if(!is_tar && !is_targz)
                    SEGGER_RTT_printf(0, "fs_provision: unsupported file\n");
            }

            // check tstamp against already installed files
            if(tstamp <= c_prov_tstamp)
            {
                continue;
            }

            // install files
            if(is_tar)
            {
                FIL fp;
                fr = f_open(&fp, fi.fname, FA_READ);
                if(fr != FR_OK)
                {
                    CriticalGuard cg(s_rtt);
                    SEGGER_RTT_printf(0, "fs_provision: couldn't f_open %s: %d\n", fi.fname, fr);
                    had_failure = true;
                    break;
                }
                auto fs_p_ret = fs_provision_tarball(direct_fread, direct_lseek, &fp);
                f_close(&fp);


                if(fs_p_ret == 0)
                {
                    {
                        CriticalGuard cg(s_rtt);
                        SEGGER_RTT_printf(0, "fs_provision: provisioned %d\n", (int)tstamp);
                    }
                    max_provisioned = std::max(max_provisioned, tstamp);
                }
                else
                {
                    {
                        CriticalGuard cg(s_rtt);
                        SEGGER_RTT_printf(0, "fs_provision: failed to provision %d\n", (int)tstamp);
                    }
                    had_failure = true;
                    break;
                }
            }
        }
    }
    f_closedir(&dp);
    f_unmount("/");

    // Write out provision.txt
    if(!had_failure && max_provisioned)
    {
        er = deferred_call(syscall_open, "/provision.txt", O_CREAT | O_TRUNC | O_WRONLY, 0);
        if(er >= 0)
        {
            char buf[32];
            memset(buf, 0, 32);
            sprintf(buf, "%llu", max_provisioned);
            auto blen = (int)strlen(buf);
            auto bw = deferred_call(syscall_write, er, buf, blen + 1);
            if(bw == blen + 1)
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "fs_provision: completed provisioning\n");
            }
            else
            {
                CriticalGuard cg(s_rtt);
                SEGGER_RTT_printf(0, "fs_provision: failed to write /provision.txt : %d\n", bw);
            }

            close(er);
        }
    }

    return 0;
}