#ifndef COMPLETION_LIST_H
#define COMPLETION_LIST_H

#include <unordered_map>
#include "osmutex.h"

template <typename T_id, typename T_val, typename _Hash = std::hash<T_id>> class CompletionList
{
    protected:
        std::unordered_map<T_id, T_val, _Hash> m;

    public:
        Spinlock sl;
        int _set(const T_id &id, T_val v)
        {
            m[id] = v;
            return 0;
        }

        int Set(const T_id &id, T_val v)
        {
            CriticalGuard cg(sl);
            return _set(id, v);
        }

        std::pair<bool, T_val> _get(const T_id &id)
        {
            auto iter = m.find(id);
            if(iter == m.end())
                return std::make_pair(false, T_val{});
            else
                return std::make_pair(true, iter->second);
        }

        std::pair<bool, T_val> Get(const T_id &id)
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

extern CompletionList<id_t, int> ProcessExitCodes;
extern CompletionList<pidtid, void *, pidtid_hash> ThreadExitCodes;

#endif
