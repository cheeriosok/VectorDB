#ifndef ENTRY_MANAGER_HPP
#define ENTRY_MANAGER_HPP

#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "response_serializer.hpp"

struct Entry {
    std::string value;
    std::unique_ptr<class ZSet> zset;
    int64_t heap_idx = -1;
};

class EntryManager {
public:
    static void destroy_entry(Entry* entry) {
        if (!entry) return;
        entry->value.clear();
        if (entry->zset) entry->zset.reset();
        delete entry;
    }

    static void set_entry_ttl(Entry& entry, int64_t ttl_ms, std::vector<int64_t>& heap) {
        if (ttl_ms < 0) return;

        auto expire_at = get_monotonic_usec() + ttl_ms * 1000;
        if (entry.heap_idx == -1) {
            heap.push_back(expire_at);
            entry.heap_idx = heap.size() - 1;
        } else {
            heap[entry.heap_idx] = expire_at;
        }
    }

private:
    static uint64_t get_monotonic_usec() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }
};

#endif 
