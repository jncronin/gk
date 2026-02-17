#include "osnet.h"
#include "osnet_onconnect_userprocess.h"
#include "thread.h"
#include "process.h"
#include "scheduler.h"
#include "clocks.h"
#include "syscalls_int.h"

static void *user_process_manager_thread(void *);

extern PProcess p_net;

struct manager_thread_data
{
    IP4Addr addr;
    UserProcessOnConnectScript *ocs;
};

int UserProcessOnConnectScript::OnConnect(NetInterface *iface, const IP4Addr &addr)
{
    OnDisconnect(iface);

    auto mtd = new manager_thread_data();
    mtd->addr = addr;
    mtd->ocs = this;

    auto t = Thread::Create(iface->Name() + "_" + name + "_manager",
        user_process_manager_thread, mtd, true, GK_PRIORITY_NORMAL, p_net);
    manager_thread_id = t->id;
    Schedule(t);

    return 0;
}

void *user_process_manager_thread(void *p)
{
    auto mtd = reinterpret_cast<manager_thread_data *>(p);
    auto addr = mtd->addr;
    auto ocs = mtd->ocs;
    delete(mtd);

    ocs->ManagerThread(addr);

    return nullptr;
 }

 int UserProcessOnConnectScript::ManagerThread(const IP4Addr &addr)
 {
    unsigned int backoff_ms = 0;
    kernel_time last_spawn_time = kernel_time_invalid();
    kernel_time last_exit_time = kernel_time_invalid();

    while(true)
    {
        last_spawn_time = clock_cur();
        auto spawn_ret = SpawnProcess(addr);
        if(spawn_ret != 0)
        {
            klog("net: SpawnProcess() failed: %d\n", spawn_ret);
        }
        else
        {
            syscall_waitpid(process_id, nullptr, 0, &errno);
        }
        last_exit_time = clock_cur();

        // did we exit really quickly?
        auto runtime = last_exit_time - last_spawn_time;
        if(runtime < kernel_time_from_ms(500))
        {
            if(backoff_ms < 60000)
            {
                if(backoff_ms == 0)
                    backoff_ms = 1;
                else
                    backoff_ms *= 10;
            }
        }
        else
        {
            // ran for a reasonable time - reset backoff
            backoff_ms = 0;
        }

        klog("net: userspace manager thread: ran for %llu ms, backoff set to %u\n", 
            kernel_time_to_ms(runtime), backoff_ms);

        Block(clock_cur() + kernel_time_from_ms(backoff_ms));
    }
}

int UserProcessOnConnectScript::OnDisconnect(NetInterface *)
{
    if(manager_thread_id != 0)
    {
        Thread::Kill(manager_thread_id, nullptr);
        manager_thread_id = 0;
    }
    if(process_id != 0)
    {
        Process::Kill(process_id);
        process_id = 0;
    }
    return 0;
}

TelnetOnConnectScript::TelnetOnConnectScript()
{
    name = "telnet";
}

int TelnetOnConnectScript::SpawnProcess(const IP4Addr &addr)
{
    id_t pid;
    proccreate_t pc = { 0 };

    const char *argv[] = {
        "--inet-addr", addr.ToString().c_str(),
        "-d"
    };
    constexpr auto argc = sizeof(argv) / sizeof(argv[0]);
    const char *cwd = "";

    pc.argv = argv;
    pc.argc = argc;
    pc.cwd = cwd;
    
    auto ret = syscall_proccreate("/tools-0.1.1-gk/bin/telnetd", &pc, (pid_t *)&pid, &errno);
    if(ret == 0)
    {
        process_id = pid;
    }
    return ret;
}
