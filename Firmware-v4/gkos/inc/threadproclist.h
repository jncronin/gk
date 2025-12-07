#ifndef THREADLIST_H
#define THREADLIST_H

#include "osspinlock.h"
#include <memory>
#include <map>
#include "ostypes.h"

template <class T, class MemberType = std::weak_ptr<T>> class IDList
{
    public:
        using PT = std::shared_ptr<T>;

        template<typename U> struct is_shared_ptr : std::false_type {};
        template<typename U> struct is_shared_ptr<std::shared_ptr<U>> : std::true_type {};

    protected:
        std::map<id_t, MemberType> list;
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
            auto iter = list.find(id);
            if(iter == list.end())
            {
                return nullptr;
            }
            if constexpr(is_shared_ptr<MemberType>::value)
                return iter->second;
            else
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
using MutexList_t = IDList<Mutex, std::shared_ptr<Mutex>>;
using CondList_t = IDList<Condition, std::shared_ptr<Condition>>;
using RwLockList_t = IDList<RwLock, std::shared_ptr<RwLock>>;
using UserspaceSemaphoreList_t = IDList<UserspaceSemaphore, std::shared_ptr<UserspaceSemaphore>>;

extern ThreadList_t ThreadList;
extern ProcessList_t ProcessList;
extern MutexList_t MutexList;
extern CondList_t ConditionList;
extern RwLockList_t RwLockList;
extern UserspaceSemaphoreList_t UserspaceSemaphoreList;

#endif
