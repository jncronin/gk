#include "osnet.h"
#include "syscalls_int.h"
#include "process.h"

int Socket::BindAsync(const sockaddr *addr, socklen_t addrlen, int *_errno)
{
    *_errno = ENOTSUP;
    return -1;
}

int Socket::RecvFromAsync(void *buf, size_t len, int flags,
    struct sockaddr *src_addr, socklen_t *addrlen, int *_errno)
{
    *_errno = ENOTSUP;
    return -1;
}

int Socket::SendToAsync(const void *buf, size_t len, int flags,
    const struct sockaddr *dest_addr, socklen_t addrlen, int *_errno)
{
    *_errno = ENOTSUP;
    return -1;
}

int Socket::SendPendingData()
{
    return NET_NOTSUPP;
}

int Socket::ListenAsync(int backlog, int *_errno)
{
    *_errno = EOPNOTSUPP;
    return -1;
}

int Socket::AcceptAsync(sockaddr *addr, socklen_t *addrlen, int *_errno)
{
    *_errno = EOPNOTSUPP;
    return -1;
}

int Socket::CloseAsync(int *_errno)
{
    return 0;
}

void Socket::HandleWaitingReads()
{
    
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
                protocol = IPPROTO_RAW;
            }
            else if(type == SOCK_DGRAM)
            {
                protocol = IPPROTO_UDP;
            }
            else if(type == SOCK_STREAM)
            {
                protocol = IPPROTO_TCP;
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
                            {
                                auto tcpsck = new TCPSocket();
                                tcpsck->is_dgram = false;
                                sck = tcpsck;
                            }

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
                            {
                                auto udpsck = new UDPSocket();
                                udpsck->is_dgram = true;
                                sck = udpsck;
                            }
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
        CriticalGuard cg(p->sl);

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

int socket(int domain, int type, int protocol)
{
    int _errno = EOK;
    auto ret = syscall_socket(domain, type, protocol, &_errno);
    if(_errno)
        errno = _errno;
    return ret;
}

Socket *fildes_to_sck(int fildes)
{
    auto t = GetCurrentThreadForCore();
    auto &p = t->p;
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

int syscall_bind(int sockfd, const sockaddr *addr, socklen_t addrlen, int *_errno)
{
    auto sck = fildes_to_sck(sockfd);
    if(!sck)
    {
        *_errno = EBADF;
        return -1;
    }
    if(!addr)
    {
        *_errno = EINVAL;
        return -1;
    }

    return sck->BindAsync(addr, addrlen, _errno);
}

int bind(int sockfd, const sockaddr *addr, socklen_t addrlen)
{
    return deferred_call(syscall_bind, sockfd, addr, addrlen);
}

int syscall_recvfrom(int sockfd, void *buf, size_t len, int flags,
    struct sockaddr *src_addr, socklen_t *addrlen, int *_errno)
{
    auto sck = fildes_to_sck(sockfd);
    if(!sck)
    {
        *_errno = EBADF;
        return -1;
    }
    if(!buf)
    {
        *_errno = EINVAL;
        return -1;
    }

    return sck->RecvFromAsync(buf, len, flags, src_addr, addrlen, _errno);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
    struct sockaddr *src_addr, socklen_t *addrlen)
{
    return deferred_call(syscall_recvfrom, sockfd, buf, len, flags, src_addr, addrlen);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    return recvfrom(sockfd, buf, len, flags, nullptr, nullptr);
}

int syscall_sendto(int sockfd, const void *buf, size_t len, int flags,
    const sockaddr *dest_addr, socklen_t addrlen, int *_errno)
{
    auto sck = fildes_to_sck(sockfd);
    if(!sck)
    {
        *_errno = EBADF;
        return -1;
    }
    if(!buf)
    {
        *_errno = EINVAL;
        return -1;
    }

    return sck->SendToAsync(buf, len, flags, dest_addr, addrlen, _errno);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
    const sockaddr *dest_addr, socklen_t addrlen)
{
    return deferred_call(syscall_sendto, sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    return sendto(sockfd, buf, len, flags, nullptr, 0);
}

int syscall_listen(int sockfd, int backlog, int *_errno)
{
    auto sck = fildes_to_sck(sockfd);
    if(!sck)
    {
        *_errno = EBADF;
        return -1;
    }
    
    return sck->ListenAsync(backlog, _errno);
}

int listen(int sockfd, int backlog)
{
    return deferred_call(syscall_listen, sockfd, backlog);
}

int syscall_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int *_errno)
{
    auto sck = fildes_to_sck(sockfd);
    if(!sck)
    {
        *_errno = EBADF;
        return -1;
    }
    
    return sck->AcceptAsync(addr, addrlen, _errno);
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return deferred_call(syscall_accept, sockfd, addr, addrlen);
}

#include "syscalls_int.h"
int close(int fd)
{
    // call twice, to allow graceful shutdown of socket if possible
    auto retpt1 = deferred_call(syscall_close1, fd);
    if(retpt1 != 0)
        return retpt1;
    return deferred_call(syscall_close2, fd);
}
