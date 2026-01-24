#ifndef THREADLIST_H
#define THREADLIST_H

#include "osspinlock.h"
#include <memory>
#include <map>
#include "ostypes.h"

class Thread;
class Process;
class Mutex;
class Condition;
class RwLock;
class UserspaceSemaphore;

template <class T, class ret_T> struct ThreadProcListMember
{
    using PT = std::shared_ptr<T>;
    PT v{};
    ret_T retval = (ret_T)0;
    bool has_ended = false;

    typedef T value_type;
    typedef PT pointer_type;
    typedef ret_T return_type;

    operator PT() { return v; }
};

struct ProcessListMember : public ThreadProcListMember<Process, int>
{
    id_t ppid = 0;
};

using ThreadListMember = ThreadProcListMember<Thread, void *>;

template <class T> class IDList
{    
    public:
        protected:
        std::map<id_t, T> list;
        id_t next_id = 1;

    public:
        Spinlock sl;

        id_t _register(T v)
        {
            auto ret = next_id;
            list[next_id++] = v;
            return ret;
        }

        id_t Register(T v)
        {
            CriticalGuard cg(sl);
            return _register(v);
        }

        T _get(id_t id)
        {
            if(!id)
                return T();
            auto iter = list.find(id);
            if(iter == list.end())
            {
                return T();
            }
            return iter->second;
        }

        T Get(id_t id)
        {
            if(!id)
                return T();
            CriticalGuard cg(sl);
            return _get(id);
        }

        bool _exists(id_t id)
        {
            if(!id)
                return false;
            auto iter = list.find(id);
            return iter != list.end();
        }

        bool Exists(id_t id)
        {
            if(!id)
                return false;
            CriticalGuard cg(sl);
            return _exists(id);
        }

        void _delete(id_t id)
        {
            auto iter = this->list.find(id);
            if(iter != this->list.end())
            {
                this->list.erase(iter);
            }
        }

        void Delete(id_t id)
        {
            CriticalGuard cg(this->sl);
            _delete(id);
        }
};

template <typename T> class PTIDList : public IDList<T>
{
    public:
        using PT = T::pointer_type;

        template <class... Args> PT Create(Args... a)
        {
            auto p = std::make_shared<typename T::value_type>(a...);
            T container_val;
            container_val.v = p;
            
            p->id = this->Register(container_val);
            return p;
        }

        void _setexitcode(id_t id, const T::return_type &ret)
        {
            auto iter = this->list.find(id);
            if(iter != this->list.end())
            {
                iter->second.retval = ret;
                iter->second.has_ended = true;
            }
        }

        void SetExitCode(id_t id, const T::return_type &ret)
        {
            CriticalGuard cg(this->sl);
            _setexitcode(id, ret);
        }
};

template <typename T> class SyncPrimIDList : public IDList<std::shared_ptr<T>>
{
    public:
        using PT = std::shared_ptr<T>;
        
        template <class... Args> PT Create(Args... a)
        {
            auto p = std::make_shared<T>(a...);
            p->id = this->Register(p);
            return p;
        }
};

using ThreadList_t = PTIDList<ThreadListMember>;
using ProcessList_t = PTIDList<ProcessListMember>;
using MutexList_t = SyncPrimIDList<Mutex>;
using CondList_t = SyncPrimIDList<Condition>;
using RwLockList_t = SyncPrimIDList<RwLock>;
using UserspaceSemaphoreList_t = SyncPrimIDList<UserspaceSemaphore>;

extern ThreadList_t ThreadList;
extern ProcessList_t ProcessList;
extern MutexList_t MutexList;
extern CondList_t ConditionList;
extern RwLockList_t RwLockList;
extern UserspaceSemaphoreList_t UserspaceSemaphoreList;

#endif
