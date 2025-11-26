#ifndef THREADLIST_H
#define THREADLIST_H

#include "osspinlock.h"
#include <memory>
#include <map>
#include "ostypes.h"

template <class T> class IDList
{
    public:
        using WPT = std::weak_ptr<T>;
        using PT = std::shared_ptr<T>;

    protected:
        std::map<id_t, WPT> list;
        id_t next_id = 0;

    public:
        Spinlock sl;
        template <class... Args> PT Create(Args... a)
        {
            auto p = std::make_shared<T>(a...);
            p->id = Register(p);
            return p;
        }

        id_t Register(PT v)
        {
            CriticalGuard cg(sl);
            auto ret = next_id;
            list[next_id++] = v;
            return ret;
        }

        PT _get(id_t id)
        {
            auto iter = list.find(id);
            if(iter == list.end())
            {
                return nullptr;
            }
            return iter->second.lock();
        }

        PT Get(id_t id)
        {
            CriticalGuard cg(sl);
            return _get(id);
        }

        void _delete(id_t id)
        {
            auto iter = list.find(id);
            if(iter != list.end())
            {
                list.erase(iter);
            }
        }

        void Delete(id_t id)
        {
            CriticalGuard cg(sl);
            _delete(id);
        }

        bool _exists(id_t id)
        {
            auto iter = list.find(id);
            return iter != list.end();
        }

        bool Exists(id_t id)
        {
            CriticalGuard cg(sl);
            return _exists(id);
        }
};

class Thread;
class Process;
class Mutex;
class Condition;
class RwLock;
class UserspaceSemaphore;

using ThreadList_t = IDList<Thread>;
using ProcessList_t = IDList<Process>;
using MutexList_t = IDList<Mutex>;
using CondList_t = IDList<Condition>;
using RwLockList_t = IDList<RwLock>;
using UserspaceSemaphoreList_t = IDList<UserspaceSemaphore>;

extern ThreadList_t ThreadList;
extern ProcessList_t ProcessList;
extern MutexList_t MutexList;
extern CondList_t ConditionList;
extern RwLockList_t RwLockList;
extern UserspaceSemaphoreList_t UserspaceSemaphoreList;

#endif
