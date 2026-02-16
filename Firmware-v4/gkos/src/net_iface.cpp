#include "osnet.h"
#include "logger.h"
#include "thread.h"
#include "process.h"
#include "scheduler.h"
#include <memory>

extern PProcess p_net;

static void *iface_task(void *_iface);

struct iface_list
{
    Mutex m;
    std::unordered_map<std::string, NetInterface *> list;
};

static iface_list ifaces;

int net_register_interface(NetInterface *iface)
{
    auto [ ret, iface_name ] = net_register_interface_internal(iface);
    if(ret != 0)
    {
        klog("net: register_interface_internal failed: %d\n", ret);
        return -1;
    }

    sched.Schedule(Thread::Create(iface->DeviceName(), iface_task, (void *)iface, true,
        GK_PRIORITY_NORMAL, p_net));

    klog("net: registered %s\n", iface->Name().c_str());
    return 0;
}

void *iface_task(void *_iface)
{
    auto iface = reinterpret_cast<WifiNetInterface *>(_iface);
    if(!iface)
    {
        klog("net: wifi: iface invalid\n");
        return nullptr;
    }

    iface->RunTaskLoop();

    net_deregister_interface_internal(iface);

    return nullptr;
}

void NetInterface::RunTaskLoop()
{
    while(true)
    {
        netiface_msg msg;
        if(events.Pop(&msg, clock_cur() + kernel_time_from_ms(200)))
        {
            switch(msg.msg_type)
            {
                case netiface_msg::netiface_msg_type::Deregister:
                    return;

                case netiface_msg::netiface_msg_type::HardwareEvent:
                    if(HardwareEvent(msg) != 0)
                    {
                        klog("net: HardwareEvent() failed\n");
                        return;
                    }
                    break;

                case netiface_msg::netiface_msg_type::Activate:
                    Activate();
                    break;

                case netiface_msg::netiface_msg_type::Deactivate:
                    Deactivate();
                    break;
            }
        }
        if(IdleTask() != 0)
        {
            klog("net: IdleTask() failed\n");
            return;
        }
    }
}

std::pair<int, std::string> net_register_interface_internal(NetInterface *iface)
{
    MutexGuard mg(ifaces.m);
    for(auto try_id = 0U; try_id < 32; try_id++)
    {
        std::string try_name = iface->DeviceType() + std::to_string(try_id);
        if(ifaces.list.find(try_name) == ifaces.list.end())
        {
            iface->name = try_name;
            ifaces.list[try_name] = iface;
            return std::make_pair(0, try_name);
        }
    }

    return std::make_pair(-1, "");
}

int net_deregister_interface_internal(NetInterface *iface)
{
    if(!iface)
        return -1;
    
    MutexGuard mg(ifaces.m);
    auto iter = ifaces.list.find(iface->Name());
    if(iter == ifaces.list.end())
        return -1;
    ifaces.list.erase(iter);
    return 0;
}

int NetInterface::IdleTask()
{
    return 0;
}

int NetInterface::HardwareEvent(const netiface_msg &)
{
    return 0;
}

int NetInterface::Activate()
{
    active = true;
    return 0;
}

int NetInterface::Deactivate()
{
    active = false;
    return 0;
}

bool NetInterface::GetDeviceActive() const
{
    return active;
}

bool NetInterface::GetLinkActive() const
{
    return connected;
}

std::string NetInterface::DeviceName() const
{
    return "unknown device";
}

std::string NetInterface::DeviceType() const
{
    return "eth";
}

std::string NetInterface::Name() const
{
    return name;
}

int NetInterface::SetActive(bool _active)
{
    if(_active)
        events.Push({ .msg_type = netiface_msg::netiface_msg_type::Activate });
    else
        events.Push({ .msg_type = netiface_msg::netiface_msg_type::Deactivate });
    return 0;
}

const HwAddr &NetInterface::GetHwAddr() const
{
    return hwaddr;
}
