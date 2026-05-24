#include "syscalls_int.h"
#include "osnet.h"

int syscall_wifi_clearknownnetworks(int *_errno)
{
    auto ret = net_clear_known_networks();
    if(ret != 0)
        *_errno = EFAULT;
    return ret;
}

int syscall_wifi_addopennetwork(const char *ssid, int ch, int *_errno)
{
    if(!ssid)
    {
        *_errno = EINVAL;
        return -1;
    }

    auto n = std::make_shared<WifiNetInterface::wifi_network>();
    if(!n)
    {
        *_errno = ENOMEM;
        return -1;
    }

    n->ssid = std::string(ssid);
    n->ch = ch;
    n->cred_type = WifiNetInterface::credentials_type::Open;
    
    auto ret = net_add_known_network(n);
    if(ret != 0)
    {
        *_errno = EFAULT;
        return -1;
    }

    return 0;
}

int syscall_wifi_addpsknetwork(const char *ssid, int ch, const char *psk, int *_errno)
{
    if(!ssid || !psk)
    {
        *_errno = EINVAL;
        return -1;
    }

    auto n = std::make_shared<WifiNetInterface::wifi_network>();
    if(!n)
    {
        *_errno = ENOMEM;
        return -1;
    }

    n->ssid = std::string(ssid);
    n->ch = ch;
    n->cred_type = WifiNetInterface::credentials_type::PSK;

    auto c = std::make_shared<WifiNetInterface::psk_credentials>();
    if(!c)
    {
        *_errno = ENOMEM;
        return -1;
    }
    c->password = std::string(psk);
    n->credentials = c;
    
    auto ret = net_add_known_network(n);
    if(ret != 0)
    {
        *_errno = EFAULT;
        return -1;
    }

    return 0;
}
