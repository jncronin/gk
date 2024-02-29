#include "osnet.h"
#include "syscalls_int.h"

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

int syscall_socket(int domain, int type, int protocol, int *_errno)
{
    // sanity check arguments
    if(domain != AF_UNIX && domain != AF_INET)
    {
        *_errno = EAFNOSUPPORT;
        return -1;
    }

    if(protocol == 0)
    {
        if(domain == AF_UNIX)
            protocol = IPPROTO_RAW;
        else if(domain == AF_INET)
        {
            if(type == SOCK_RAW)
            {
                domain = IPPROTO_RAW;
            }
            else if(type == SOCK_DGRAM)
            {
                domain = IPPROTO_UDP;
            }
            else if(type == SOCK_STREAM)
            {
                domain = IPPROTO_TCP;
            }
        }
    }

    Socket *sck = nullptr;

    switch(domain)
    {
        case AF_INET:
            switch(protocol)
            {
                case IPPROTO_TCP:
                    switch(type)
                    {
                        case SOCK_STREAM:
                            // TODO: sck = new TCPSocket();
                            break;

                        default:
                            *_errno = EPROTOTYPE;
                            return -1;
                    }
                    break;

                case IPPROTO_UDP:
                    switch(type)
                    {
                        case SOCK_DGRAM:
                            sck = new UDPSocket();
                            break;

                        default:
                            *_errno = EPROTOTYPE;
                            return -1;
                    }
                    break;

                case IPPROTO_RAW:
                    switch(type)
                    {
                        case SOCK_RAW:
                            // TODO: sck = new RawSocket();
                            break;

                        default:
                            *_errno = EPROTOTYPE;
                            return -1;
                    }
                    break;

                default:
                    *_errno = EPROTONOSUPPORT;
                    return -1;
            }
            break;

        case AF_UNIX:
            *_errno = EPROTONOSUPPORT;
            return -1;

        default:
            *_errno = EAFNOSUPPORT;
            return -1;
    }

    if(!sck)
    {
        *_errno = EPROTOTYPE;
        return -1;
    }

    // get available sockfd
    auto t = GetCurrentThreadForCore();
    auto p = t->p;
    {
        CriticalGuard cg(p.sl);

        int fildes = get_free_fildes(p);
        if(fildes == -1)
        {
            *_errno = EMFILE;
            return -1;
        }

        sck->sockfd = fildes;
        auto sfile = new SocketFile(sck);
        p.open_files[fildes] = sfile;

        return fildes;
    }
}

Socket *fildes_to_sck(int fildes)
{
    auto t = GetCurrentThreadForCore();
    auto p = t->p;
    {
        CriticalGuard cg(p.sl);
        if(fildes < 0 || fildes >= GK_MAX_OPEN_FILES)
        {
            return nullptr;
        }
        if(p.open_files[fildes] == nullptr)
        {
            return nullptr;
        }
        if(p.open_files[fildes]->GetType() != FileType::FT_Socket)
        {
            return nullptr;
        }
        return reinterpret_cast<SocketFile *>(p.open_files[fildes])->sck;
    }
}
