#ifndef OSNET_DHCP_H
#define OSNET_DHCP_H

#define DHCPDISCOVER        1
#define DHCPOFFER           2
#define DHCPREQUEST         3
#define DHCPDECLINE         4
#define DHCPACK             5
#define DHCPNACK            6
#define DHCPRELEASE         7
#define DHCPINFORM          8

#define OFFSET_XID          4
#define OFFSET_SECS         8
#define OFFSET_FLAGS        10
#define OFFSET_CIADDR       12
#define OFFSET_YIADDR       16
#define OFFSET_SIADDR       20
#define OFFSET_GIADDR       24
#define OFFSET_CHADDR       28
#define OFFSET_SNAME        44
#define OFFSET_FILE         108
#define OFFSET_OPTIONS      236

#define DHCP_MAGIC_NB       0x63825363

#endif
