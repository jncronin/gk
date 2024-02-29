#include <osnet.h>
#include <unordered_set>

SRAM4_DATA static std::unordered_set<UDPSocket> open_udp_sockets;

int net_handle_udp_packet(const IP4Packet &pkt)
{
    return NET_NOTSUPP;
}

int UDPSocket::HandlePacket(const char *pkt, size_t n)
{
    return -1;
}

int UDPSocket::SendData(const char *d, size_t n, const void *addr, size_t addrlen)
{
    return -1;
}

int UDPSocket::RecvData(char *d, size_t n, void *addr, size_t addrlen, SimpleSignal &ss)
{
    return -1;
}
