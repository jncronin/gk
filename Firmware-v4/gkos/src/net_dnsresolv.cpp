/* Invoke a userspace runtime helper to handle dns lookups */
#include <string>
#include <unordered_map>
#include "osnet.h"
#include "osmutex.h"
#include "osfile.h"
#include <fcntl.h>
#include "errno.h"
#include "process.h"
#include "elf.h"
#include "syscalls_int.h"

static const std::string resolver = "/dnsresolv-0.1.1-gk/bin/dnsresolv";

struct addrlist
{
    std::vector<IP4Addr> ip4addrs;
};

static std::unordered_map<std::string, addrlist> dns_addrs;
static PMutex m_resolver;
static PMutex m_resolver_cache;

static const addrlist check_cache(const std::string &host)
{
    MutexGuard mg(*m_resolver_cache);
    auto iter = dns_addrs.find(host);
    if(iter == dns_addrs.end())
    {
        return addrlist();
    }
    else
    {
        return iter->second;
    }
}

std::vector<IP4Addr> net_dns_lookup(const std::string &host)
{
    MutexGuard mg(*m_resolver);

    /* check cache */
    auto cache_resp = check_cache(host);
    if(!cache_resp.ip4addrs.empty())
    {
        return cache_resp.ip4addrs;
    }

    /* call userspace function instead */
    auto proc_fd = OpenFile(resolver.c_str(), O_RDONLY, 0, &errno);
    if(proc_fd == nullptr)
    {
        klog("net_dns_lookup: failed to open resolver process\n");
        return std::vector<IP4Addr>();
    }
    auto p_resolver = Process::Create("resolver", false, GetCurrentPProcessForCore());
    Thread::threadstart_t resolver_ep;
    auto ret = elf_load_fildes(proc_fd, *p_resolver, &resolver_ep);
    if(ret != 0)
    {
        klog("net_dns_lookup: elf_load_fildes failed: %d\n", ret);
        proc_fd->Close(&errno);
        proc_fd->Close2(&errno);
        return std::vector<IP4Addr>();
    }

    p_resolver->env.cwd = "/";
    p_resolver->env.args.clear();
    p_resolver->env.args.push_back(host);

    auto t_resolver = Thread::Create("resolver", resolver_ep, nullptr, false, GK_PRIORITY_VERYHIGH, p_resolver);
    if(!t_resolver)
    {
        klog("net_dns_lookup: failed to create resolver thread\n");
        proc_fd->Close(&errno);
        proc_fd->Close2(&errno);
        return std::vector<IP4Addr>();
    }

    sched.Schedule(t_resolver);

    /* Wait for completion */
    syscall_waitpid(p_resolver->id, nullptr, 0, &errno);

    proc_fd->Close(&errno);
    proc_fd->Close2(&errno);

    /* The cache should now have been populated via syscalls */
    return check_cache(host).ip4addrs;
}

int syscall_adddnsentry(const char *host, size_t nentries, const __syscall_ipaddr *addrs, int *_errno)
{
    if(!addrs)
        nentries = 0;

    if(!host)
    {
        *_errno = EINVAL;
        return -1;
    }

    addrlist alist;
    for(auto i = 0u; i < nentries; i++)
    {
        const auto centry = &addrs[i];
        if(centry->ver == 4)
        {
            alist.ip4addrs.push_back(centry->addr[0]);
        }        
    }
    std::string s_host(host);

    MutexGuard mg(*m_resolver_cache);
    dns_addrs[s_host] = alist;
    return 0;
}

int init_dns()
{
    m_resolver = MutexList.Create();
    m_resolver_cache = MutexList.Create();

    if(m_resolver && m_resolver_cache)
        return 0;
    else
        return -1;
}
