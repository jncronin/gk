#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <unordered_map>
#include <list>
#include <functional>
#include <memory>

template <typename Key, typename T,
    typename Hash = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>,
    typename MapAllocator = std::allocator<std::pair<const Key, T>>,
    typename ListAllocator = std::allocator<typename std::unordered_map<Key, T>::iterator> >
        class LRUCache
{
    protected:
        std::list<std::pair<Key, T>, ListAllocator> item_list;
        std::unordered_map<Key, decltype(item_list.begin()), Hash, KeyEqual, MapAllocator> item_map;
        size_t _max_size;

    public:
        LRUCache(size_t max_size)
        {
            _max_size = max_size;
        }

        bool try_get(const Key &key, T *value)
        {
            auto iter = item_map.find(key);
            if(iter == item_map.end())
            {
                return false;
            }
            else
            {
                *value = iter->second->second;
                item_list.splice(item_list.begin(), item_list, iter->second);
                return true;
            }
        }

        void push(const Key &key, const T &value)
        {
            auto iter = item_map.find(key);
            if(iter != item_map.end())
            {
                // already exists, delete so can move to front
                item_list.erase(iter->second);
                item_map.erase(iter);
            }
            item_list.push_front(std::make_pair(key, value));
            item_map.insert(std::make_pair(key, item_list.begin()));    // first item guaranteed to be one we just inserted

            // do we overrun max_size?
            while(item_map.size() > _max_size)
            {
                auto del_iter = item_list.end();
                del_iter--;     // get last item
                item_map.erase(del_iter->first);
                item_list.pop_back();
            }
        }

        void clear()
        {
            item_map.clear();
            item_list.clear();
        }

        size_t size()
        {
            return item_map.size();
        }
};

#include "osallocator.h"

template<typename Key, typename T, int region_id> using LRUCacheRegion = 
    LRUCache<Key, T, std::hash<Key>, std::equal_to<Key>,
        GKAllocator<std::pair<const Key, T>, region_id>,
        GKAllocator<typename std::unordered_map<Key, T>::iterator, region_id>>;

#endif
