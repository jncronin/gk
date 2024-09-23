#ifndef FS_PROVISION_H
#define FS_PROVISION_H

unsigned int fake_mbr_get_sector_count();
const char *fake_mbr_get_mbr();
bool fake_mbr_check_extents(unsigned int lba, unsigned int sector_count);

int fs_provision();


#endif
