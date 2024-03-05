#include "osnet.h"

bool SocketBuffer::Alloc()
{
    recvbuf = net_allocate_sbuf();
    if(!recvbuf)
        return false;
    sendbuf = net_allocate_sbuf();
    if(!sendbuf)
    {
        net_deallocate_sbuf(recvbuf);
        return false;
    }

    send_rptr = 0;
    send_wptr = 0;
    recv_rptr = 0;
    recv_wptr = 0;

    return true;
}

SocketBuffer::~SocketBuffer()
{
    if(recvbuf)
        net_deallocate_sbuf(recvbuf);
    if(sendbuf)
        net_deallocate_sbuf(sendbuf);    
}

SocketBuffer &SocketBuffer::operator=(SocketBuffer &&other)
{
    sendbuf = other.sendbuf;
    recvbuf = other.recvbuf;
    send_rptr = other.send_rptr;
    send_wptr = other.send_wptr;
    recv_rptr = other.recv_rptr;
    recv_wptr = other.recv_wptr;

    other.sendbuf = nullptr;
    other.recvbuf = nullptr;

    return *this;
}

SocketBuffer::SocketBuffer(SocketBuffer &&other)
{
    *this = std::move(other);
}

size_t SocketBuffer::AvailableRecvSpace() const
{
    if(!recvbuf) return 0;
    return (recv_rptr > recv_wptr) ? (recv_rptr - recv_wptr) : (buflen - (recv_wptr - recv_rptr));
}

size_t SocketBuffer::RecvBytesAvailable() const
{
    if(!recvbuf) return 0;
    return buflen - AvailableRecvSpace();
}

size_t SocketBuffer::AvailableSendSpace() const
{
    if(!sendbuf) return 0;
    return (send_rptr > send_wptr) ? (send_rptr - send_wptr) : (buflen - (send_wptr - send_rptr));
}
