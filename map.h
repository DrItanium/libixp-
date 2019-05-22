#ifndef LIBJYQ_MAP_H__
#define LIBJYQ_MAP_H__
/* C Implementation copyright ©2006-2010 Kris Maglione <maglione.k at Gmail>
 * C++ Implementation copyright (c)2019 Joshua Scoggins
 * See LICENSE file for license details.
 */

#include <functional>
#include <map>
#include <any>
#include <utility>
#include <shared_mutex>
#include "types.h"
#include "thread.h"


namespace jyq {
    template<typename K, typename V>
    class Map {
        public:
            using BackingStore = std::map<K, V>;
            using iterator = typename BackingStore::iterator;
            using const_iterator = typename BackingStore::const_iterator;
        public:
            Map() = default;
            ~Map() = default;
            V* get(const K& key) {
                std::shared_lock<std::shared_mutex> rlock(_lock);
                try {
                    return &_map.at(key);
                } catch (std::out_of_range&) {
                    return nullptr;
                }
            }
            std::optional<V> rm(const K& key) {
                // get the value out of the map and then erase the entry from
                // it
                std::unique_lock<std::shared_mutex> wlock(_lock);
                if (std::optional<V&> value = this->get(key); value) {
                    std::optional<V> result(value.value());
                    _map.erase(key);
                    return result;
                } else {
                    return std::nullopt;
                }
            }
            bool insert(const K key, V value) {
                return _map.insert({key, value});
            }
            template<typename ... Args>
            auto emplace(Args&& ... args) {
                return _map.emplace(std::forward<Args>(args)...);
            }
            template<typename T>
            void exec(std::function<void(T, iterator)> fn, T context) {
                std::shared_lock<std::shared_mutex> rlock(_lock);
                for (auto it = _map.begin(); it != _map.end(); ++it) {
                    fn(context, it);
                }
            }
        private:
            BackingStore _map;
            std::shared_mutex _lock;
    };

} // end namespace jyq

#endif // end LIBJYQ_MAP_H__
