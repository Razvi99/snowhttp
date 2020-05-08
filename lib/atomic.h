#pragma once

#include <mutex>
#include <queue>
#include <map>

namespace atomic {
    template<class value_type, class container>
    class queue {
    public:
        void push(value_type &val) {
            std::lock_guard<std::mutex> lock(mutex);
            queue.push(val);
        }

        void pop() { queue.pop(); }

        auto front() { return queue.front(); }

        bool empty() { return queue.empty(); }

    private:
        std::mutex mutex;
        std::queue<value_type, container> queue;
    };

    template<class key_type, class value_type, class compare = std::less<key_type>, class alloc = std::allocator<std::pair<const key_type, value_type>>>
    class map {

    public:
        auto erase(const key_type &k) {
            std::lock_guard<std::mutex> lock(mutex);
            return map.erase(k);
        }

        auto find(const key_type &k) {
            std::lock_guard<std::mutex> lock(mutex);
            return map.find(k);
        }

        auto insert(const std::pair<key_type, value_type> &n) {
            std::lock_guard<std::mutex> lock(mutex);
            return map.insert(n);
        }

        auto end() { return map.end(); }

        auto empty() { return map.empty(); }


    private:
        std::mutex mutex;
        std::map<key_type, value_type, compare, alloc> map;
    };
}