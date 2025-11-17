#ifndef WEAK_PTR_UNORDERED_SET_H
#define WEAK_PTR_UNORDERED_SET_H

/* Prior to C++26 P1901R2 (not currently implemented by any library) we cannot store
    weak_ptr objects in an unordered_set due to the lack of a hash operator.
    
    Therefore we define a new set where we add real shared_ptrs, but only store
    weak_ptrs to them.
    
    Essentially, the process here is to:
        hash the shared_ptr
        check whether the hash exists in an unordered_set of hashes
        if not, store the hash _and_ add the weak_ptr to a vector which can be
         iterated */

#include <vector>
#include <memory>
#include <unordered_set>

template <class Key> class WeakPtrUnorderedSet
{
    protected:
        // the values are already hashed, so don't bother doing any more
        struct hash_hasher
        {
            std::size_t operator()(const std::size_t &v) const { return v; }
        };
        std::unordered_set<std::size_t, hash_hasher> hashes;

    public:
        std::vector<std::weak_ptr<Key>> entries;

        using iter_t = std::vector<std::weak_ptr<Key>>::iterator;

        std::pair<iter_t, bool> insert(std::shared_ptr<Key> &v)
        {
            auto hash = std::hash<std::shared_ptr<Key>>()(v);
            auto [_,inserted] = hashes.insert(hash);
            if(inserted)
            {
                auto ret_iter = entries.insert(entries.end(), std::weak_ptr<Key>(v));
                return std::make_pair(ret_iter, true);
            }
            return std::make_pair(entries.end(), false);
        }

        iter_t begin() { return entries.begin(); }
        iter_t end() { return entries.end(); }

        void clear()
        {
            entries.clear();
            hashes.clear();
        }
};

#endif
