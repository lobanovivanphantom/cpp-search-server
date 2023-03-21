#pragma once
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <algorithm>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
    private:
        std::lock_guard<std::mutex> guard_;
    public:
        Value& ref_to_value;
        Access(std::map<Key, Value>& map, std::mutex& mut, const Key key) :
            guard_(mut), ref_to_value(map[key]) {
        }
    };

    explicit ConcurrentMap(size_t bucket_count) :concurrent_map_(bucket_count), mut_vec_(bucket_count), size_map_(bucket_count) {

    }
    Access operator[](const Key& key) {
        uint64_t key_u = static_cast<uint64_t>(key);
        uint64_t num_map = key_u % size_map_;
        return Access(concurrent_map_[num_map], mut_vec_[num_map], key);

    }

    std::map<Key, Value> BuildOrdinaryMap() {
        for (size_t i = 1; i < size_map_; ++i) {
            std::lock_guard<std::mutex> guard(mut_vec_[i]);
            concurrent_map_[0].merge(concurrent_map_[i]);
        }
        return concurrent_map_[0];
    }
private:
    std::vector<std::map<Key, Value>> concurrent_map_;
    std::vector<std::mutex> mut_vec_;
    uint64_t size_map_;
    std::mutex one_mutex_;
};