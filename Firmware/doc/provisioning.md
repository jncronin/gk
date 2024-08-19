# gkos provisioning #

gkos uses the ext4 filesystem internally on its SD card.  In order to allow simple provisioning, a FAT filesystem is also exported that the user can see directly as a USB mass storage device.  This allows the user to add files to the gk without having to unmount the internal ext4 device, connect it to their host computer and potentially find ext4 drivers for said computer.

The SD card block 0 returned to the host PC when connected as a USB mass storage device is alterered from what actually exists to hide the ext4 partition.  Read/writes to the ext4 partition sectors are also disabled.  This means the gk can continue running, loading and saving to the ext4 parition whilst also conencted and exporting a FAT filesystem to a host PC.  Thus, provisioning files (simple .tar.gz files) can be copied to the gk whilst it is running.

On gk startup, the FAT filesystem is checked for provisioning files.  If any are found, they are uncompressed and loaded onto the ext4 drive.  The provisioning file is then deleted from the FAT drive.  Only after this is the FAT filesystem exported again via USB.  Optionally, at provision time, a file from the ext4 filesystem can also be copied back to the FAT system (useful, for example, for accessing crash logs).  See fs_provision.cpp for details.
