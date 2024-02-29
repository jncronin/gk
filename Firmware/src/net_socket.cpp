#include "osnet.h"

Socket::Socket()
{
    recvbuf = net_allocate_sbuf();
    sendbuf = net_allocate_sbuf();
}

Socket::~Socket()
{
    if(recvbuf)
        net_deallocate_sbuf(recvbuf);
    if(sendbuf)
        net_deallocate_sbuf(sendbuf);
}
