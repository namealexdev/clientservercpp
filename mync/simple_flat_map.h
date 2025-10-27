#ifndef SIMPLE_FLAT_MAP_H
#define SIMPLE_FLAT_MAP_H

#include <cctype>
#include <vector>
#include <unistd.h>
#include <cstdint>
#include "snowflakeidgen.h"

template<typename Value>
class simple_flat_map {
public:
    struct Entry {
        uint64_t key = -1;
        bool occupied = false;
        Value value;
    };

    explicit simple_flat_map(size_t capacity = 1024) : entries(capacity) {}

    void insert(int key, Value val) {
        size_t index = hash(key);
        size_t start = index;
        while (entries[index].occupied && entries[index].key != key) {
            index = (index + 1) % entries.size();
            if (index == start) {
                // Table is full, need to resize (not implemented for simplicity)
                return;
            }
        }


        entries[index].key = key;
        entries[index].value = std::move(val);
        entries[index].occupied = true;
    }

    Value* find(int key) {
        size_t index = hash(key);
        size_t start = index;
        do {
            if (!entries[index].occupied) return nullptr;
            if (entries[index].key == key && entries[index].occupied) {
                return &entries[index].value;
            }
            index = (index + 1) % entries.size();
        } while (index != start);
        return nullptr;
    }

private:
    std::vector<Entry> entries;

    size_t hash(uint64_t key) const {

        return static_cast<size_t>(key) % entries.size();
    }

    SnowflakeGenerator genuuid_;
};

#include <unordered_map>
// это локальный буфер, для каждого epoll, тут синхронизация потоков не нужна!
template<typename Value>
class socket_um {
public:

    explicit socket_um(size_t capacity = 100) : entries(capacity) {}

    void insert(const Value& val) {
        connection_counter_++;
        entries.emplace(key, std::move(val));
    }

    Value* find(int key) {
        size_t index = hash(key);
        size_t start = index;
        do {
            if (!entries[index].occupied) return nullptr;
            if (entries[index].key == key && entries[index].occupied) {
                return &entries[index].value;
            }
            index = (index + 1) % entries.size();
        } while (index != start);
        return nullptr;
    }

private:
    uint64_t connection_counter_ = 0;
    std::unordered_map<uint64_t, Value> entries;


#endif // SIMPLE_FLAT_MAP_H
