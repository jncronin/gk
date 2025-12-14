#ifndef COMPLETION_LIST_H
#define COMPLETION_LIST_H

#include <unordered_map>
#include "osmutex.h"

template <typename T, typename _Hash = std::hash<T>> class CompletionList
{
    protected:
        std::unordered_map<T, void *, _Hash> m;

    public:
        Spinlock sl;
        int _set(const T &id, void *v)
        {
            m[id] = v;
            return 0;
        }

        int Set(const T &id, void *v)
        {
            CriticalGuard cg(sl);
            return _set(id, v);
        }

        std::pair<bool, void *> _get(const T &id)
        {
            auto iter = m.find(id);
            if(iter == m.end())
                return std::make_pair(false, nullptr);
            else
                return std::make_pair(true, iter->second);
        }

        std::pair<bool, void *> Get(const T &id)
        {
            CriticalGuard cg(sl);
            return _get(id);
        }
};

template <class T> inline void hash_combine(std::size_t &s, const T &v)
{
    std::hash<T> h;
    s^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
}

struct pidtid_hash
{
    std::size_t operator()(const pidtid &v) const
    {
        std::size_t ret = 0;
        hash_combine(ret, v.pid);
        hash_combine(ret, v.tid);
        return ret;
    }
};

constexpr bool operator==(const pidtid &a, const pidtid &b)
{
    if(a.pid != b.pid)
        return false;
    return a.tid == b.tid;
}

extern CompletionList<id_t> ProcessExitCodes;
extern CompletionList<pidtid, pidtid_hash> ThreadExitCodes;

#endif
