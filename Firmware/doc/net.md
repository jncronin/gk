# gkos networking #

Networking support is currently a work-in-progress for gkos however the currently supported features are:
- USB RNDIS driver
- ATWINC1500 WiFi driver
- ARP
- ICMPv4 ping
- IPv4
- TCPv4
- userspace socket library
- userspace tftpd daemon
- in-kernel telnetd test daemon (currently just echoes)
- in-kernel dhcpc client (used with WiFi driver)
- in-kernel dhcpd daemon (used with USB RNDIS driver)

see src/net_*.cpp for details
