#include <thread.h>
#include <sd.h>
#include <usb.h>
#include <gk_conf.h>
#include <cstring>

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

/* Generates a fake MBR which is exported whenever USB MSC asks to read
    sector 0 */
static bool prep_fake_mbr()
{
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
