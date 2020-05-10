/*
MIT License

Copyright (c) 2020 Razvan Dan David

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

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