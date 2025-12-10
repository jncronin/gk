#ifndef THREADLIST_H
#define THREADLIST_H

#include "osspinlock.h"
#include <memory>
#include <map>
#include "ostypes.h"

template <class T> class IDList
{
    public:
        using PT = std::shared_ptr<T>;

        protected:
        std::map<id_t, PT> list;
        id_t next_id = 1;

    public:
        Spinlock sl;
        template <class... Args> PT Create(Args... a)
        {
            auto p = std::make_shared<T>(a...);
            p->id = Register(p);
            return p;
        }

        id_t _register(PT v)
        {
            auto ret = next_id;
            list[next_id++] = v;
            return ret;
        }

        id_t Register(PT v)
        {
            CriticalGuard cg(sl);
            return _register(v);
        }

        PT _get(id_t id)
        {
            if(!id)
                return nullptr;
            auto iter = list.find(id);
            if(iter == list.end())
            {
                return nullptr;
            }
            return iter->second;
        }

        PT Get(id_t id)
        {
            if(!id)
                return nullptr;
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
