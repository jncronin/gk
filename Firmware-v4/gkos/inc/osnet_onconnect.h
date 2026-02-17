#ifndef OSNET_ONCONNECT_H
#define OSNET_ONCONNECT_H

class NetInterface;
class IP4Addr;

class OnConnectScript
{
    public:
        virtual int OnConnect(NetInterface *, const IP4Addr &addr) = 0;
        virtual int OnDisconnect(NetInterface *);

        virtual ~OnConnectScript() = default;
};

class NTPOnConnectScript : public OnConnectScript
{
    protected:
        id_t ntp_thread_id = 0;
        int ntp_socket = -1;

    public:
        int OnConnect(NetInterface *iface, const IP4Addr &addr);
        int OnDisconnect(NetInterface *);
};

#endif
