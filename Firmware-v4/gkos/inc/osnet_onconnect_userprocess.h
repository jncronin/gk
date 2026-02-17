#ifndef OSNET_ONCONNECT_USERPROCESS_H
#define OSNET_ONCONNECT_USERPROCESS_H

#include "process.h"
#include "thread.h"
#include "scheduler.h"

#include "osnet_onconnect.h"

class UserProcessOnConnectScript : public OnConnectScript
{
    protected:
        id_t manager_thread_id = 0;
        id_t process_id = 0;
        std::string name = "userprocess";

        virtual int SpawnProcess(const IP4Addr &addr) = 0;

    public:
        virtual int OnConnect(NetInterface *iface, const IP4Addr &addr);
        virtual int OnDisconnect(NetInterface *);

        int ManagerThread(const IP4Addr &addr);

};

class TelnetOnConnectScript : public UserProcessOnConnectScript
{
    protected:
        int SpawnProcess(const IP4Addr &addr);

    public:
        TelnetOnConnectScript();
};

#endif
