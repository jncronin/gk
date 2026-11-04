#include "osnet.h"
#include "syscalls_int.h"
#include "process.h"

static inline int net_deferred_return(int retval, int *_errno)
{
    if(retval == -2)
    {
        auto t = GetCurrentThreadForCore();
        while(!t->ss.Wait(SimpleSignal::Set, 0U));
        if(t->ss_p.ival1 != 0)
        {
            if(_errno)
                *_errno = t->ss_p.ival2;
        }
        return t->ss_p.ival1;
    }
    else
    {
        return retval;
    }
}

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
                                if(!tcpsck)
                                {
                                    klog("socket: couldn't create new TCPSocket\n");
                                    *_errno = ENOMEM;
                                    return -1;
                                }
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
                                if(!udpsck)
                                {
                                    klog("socket: couldn't create UDPSocket\n");
                                    *_errno = ENOMEM;
                                    return -1;
                                }
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
    auto p = GetCurrentProcessForCore();
    {
        CriticalGuard cg(p->open_files.sl);

        auto fildes = p->open_files.get_free_fildes();

        if(fildes == -1)
        {
            delete sck;
            *_errno = EMFILE;
            return -1;
        }

        sck->sockfd = fildes;
        auto sfile = std::make_shared<SocketFile>(sck);
        p->open_files.f[fildes] = sfile;

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

// TODO: have this return a shared_ptr and use ThreadDeletionPreventionGuards in the syscall
Socket *fildes_to_sck(int fildes)
{
    auto p = GetCurrentProcessForCore();
    {
        CriticalGuard cg(p->open_files.sl);
        if(fildes < 0 || fildes >= GK_MAX_OPEN_FILES)
        {
            return nullptr;
        }
        if(p->open_files.f[fildes] == nullptr)
        {
            return nullptr;
        }
        if(p->open_files.f[fildes]->GetType() != FileType::FT_Socket)
        {
            return nullptr;
        }
        return reinterpret_cast<SocketFile *>(p->open_files.f[fildes].get())->sck;
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

    return net_deferred_return(sck->BindAsync(addr, addrlen, _errno), _errno);
}

int bind(int sockfd, const sockaddr *addr, socklen_t addrlen)
{
    return deferred_call(syscall_bind, sockfd, addr, addrlen).first;
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

    /* If userspace call then we need to first get the socket data in kernel memory
        (because net_ functions occur in a separate process) then copy back
        to userspace on success. */
    void *kbuf;
    sockaddr *ksrc_addr;
    socklen_t *kaddrlen;
    if((uintptr_t)buf < UH_START)
    {
        kbuf = malloc(len);
        if(!kbuf)
        {
            *_errno = ENOMEM;
            return -1;
        }
        klog("recvfrom: allocate %zu bytes for kbuf @ %p\n", len, kbuf);
    }
    else
    {
        kbuf = (void *)buf;
    }
    if(src_addr && (uintptr_t)src_addr < UH_START)
    {
        if(!addrlen || !*addrlen)
        {
            if(kbuf && kbuf != buf)
            {
                free(kbuf);
            }
            *_errno = EINVAL;
            return -1;
        }
        ksrc_addr = (sockaddr *)malloc(*addrlen);
        klog("recvfrom: allocate %zu bytes for ksrc_addr @ %p\n", *addrlen, ksrc_addr);
        if(!ksrc_addr)
        {
            if(kbuf && kbuf != buf)
            {
                free(kbuf);
            }
            *_errno = ENOMEM;
            return -1;
        }
    }
    else
    {
        ksrc_addr = src_addr;
    }
    if(addrlen && (uintptr_t)addrlen < UH_START)
    {
        kaddrlen = (socklen_t *)malloc(sizeof(socklen_t));
        if(!kaddrlen)
        {
            if(kbuf && kbuf != buf)
            {
                free(kbuf);
            }
            if(ksrc_addr && ksrc_addr != src_addr)
            {
                free(ksrc_addr);
            }
            *_errno = ENOMEM;
            return -1;
        }
        klog("recvfrom: allocate %zu bytes for kaddrlen @ %p\n", sizeof(socklen_t), kaddrlen);
        *kaddrlen = *addrlen;
    }
    else
    {
        kaddrlen = addrlen;
    }

    GetCurrentThreadForCore()->ss.Signal(SimpleSignal::Set, 0U);
    auto ret = net_deferred_return(sck->RecvFromAsync(kbuf, len, flags, ksrc_addr, kaddrlen, _errno), _errno);

    // copy kernel buffers to userspace
    if(kbuf && kbuf != buf)
    {
        if(ret >= 0)
            memcpy(buf, kbuf, (size_t)ret);
        klog("recvfrom: free kbuf @ %p\n", kbuf);
        free(kbuf);
    }
    if(ksrc_addr && ksrc_addr != src_addr)
    {
        if(ret >= 0)
            memcpy(src_addr, ksrc_addr, *kaddrlen);
        klog("recvfrom: free ksrc_addr @ %p\n", ksrc_addr);
        free(ksrc_addr);
    }
    if(kaddrlen && kaddrlen != addrlen)
    {
        if(ret >= 0)
            *addrlen = *kaddrlen;
        klog("recvfrom: free kaddrlen @ %p\n", kaddrlen);
        free(kaddrlen);
    }

    return ret;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
    struct sockaddr *src_addr, socklen_t *addrlen)
{
    return deferred_call(syscall_recvfrom, sockfd, buf, len, flags, src_addr, addrlen).first;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
    struct sockaddr *src_addr, socklen_t *addrlen, kernel_time until)
{
    return deferred_call(syscall_recvfrom, until, sockfd, buf, len, flags, src_addr, addrlen).first;
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

    /* If userspace call then we need to copy the data into kernel memory.
        An optimization here would be to copy direct to a pbuf. */
    void *kbuf;
    sockaddr *kdest_addr;
    if((uintptr_t)buf < UH_START)
    {
        kbuf = malloc(len);
        if(!kbuf)
        {
            *_errno = ENOMEM;
            return -1;
        }
        klog("sendto: allocate %zu bytes for kbuf @ %p\n", len, kbuf);
        memcpy(kbuf, buf, len);
    }
    else
    {
        kbuf = (void *)buf;
    }

    if(dest_addr && (uintptr_t)dest_addr < UH_START)
    {
        kdest_addr = (sockaddr *)malloc(addrlen);
        if(!kdest_addr)
        {
            if(kbuf && kbuf != buf)
            {
                free(kbuf);
            }
            *_errno = ENOMEM;
            return -1;
        }
        memcpy(kdest_addr, dest_addr, addrlen);
        klog("sendto: allocate %zu bytes for kdest_addr @ %p\n", addrlen, kdest_addr);
    }
    else
    {
        kdest_addr = (sockaddr *)dest_addr;
    }

    GetCurrentThreadForCore()->ss.Signal(SimpleSignal::Set, 0U);
    auto ret = net_deferred_return(sck->SendToAsync(kbuf, len, flags, kdest_addr, addrlen, _errno), _errno);

    if(kbuf && kbuf != buf)
    {
        klog("sendto: free kbuf @ %p\n", kbuf);
        free(kbuf);
    }
    if(kdest_addr && kdest_addr != dest_addr)
    {
        klog("sendto: free kdest_addr @ %p\n", kdest_addr);
        free(kdest_addr);
    }

    return ret;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
    const sockaddr *dest_addr, socklen_t addrlen)
{
    return deferred_call(syscall_sendto, sockfd, buf, len, flags, dest_addr, addrlen).first;
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
    
    return net_deferred_return(sck->ListenAsync(backlog, _errno), _errno);
}

int listen(int sockfd, int backlog)
{
    return deferred_call(syscall_listen, sockfd, backlog).first;
}

int syscall_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int *_errno)
{
    auto sck = fildes_to_sck(sockfd);
    if(!sck)
    {
        *_errno = EBADF;
        return -1;
    }
    
    return net_deferred_return(sck->AcceptAsync(addr, addrlen, _errno), _errno);
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return deferred_call(syscall_accept, sockfd, addr, addrlen).first;
}

#include "syscalls_int.h"
int close(int fd)
{
    // call twice, to allow graceful shutdown of socket if possible
    auto retpt1 = deferred_call(syscall_close1, fd).first;
    if(retpt1 != 0)
        return retpt1;
    return deferred_call(syscall_close2, fd).first;
}

int syscall_setsockopt(int sockfd, int level, int option_name, const void *opt_val, socklen_t opt_len,
    int *_errno)
{
    ThreadDeletionPreventionGuard tdpg;
    auto sck = fildes_to_sck(sockfd);
    if(!sck)
    {
        *_errno = EBADF;
        return -1;
    }

    switch(option_name)
    {
        case SO_REUSEADDR:
            if(sck->is_bound)
            {
                *_errno = EISCONN;
                return -1;
            }
            if(opt_len < sizeof(int))
            {
                *_errno = EINVAL;
                return -1;
            }
            if(!opt_val)
            {
                *_errno = EINVAL;
                return -1;
            }
            sck->reuseaddr = *(const int *)opt_val != 0;
            return 0;
    }

    *_errno = EINVAL;
    return -1;
}

int setsockopt(int sockfd, int level, int option_name, const void *opt_val, socklen_t opt_len)
{
    int _errno = 0;
    auto ret = syscall_setsockopt(sockfd, level, option_name, opt_val, opt_len, &_errno);
    if(_errno)
        errno = _errno;
    return ret;
}
