#ifndef ENTRY_MANAGER_HPP
#define ENTRY_MANAGER_HPP

#include <cstdint>       // int64_t, uint64_t
#include <memory>        // std::unique_ptr
#include <unordered_map> // std::unordered_map
#include <functional>    // std::function (used in thread pool)
#include "response_serializer.hpp" 
#include "src/heap.hpp"               
#include "src/thread_pool.hpp"   
#include "src/zset.hpp"      // Sorted Set (ZSet) support
#include "common.hpp"           
#include <string>        // std::string
#include <memory>        // std::unique_ptr
#include <optional>      // std::optional
#include <chrono>        // For std::chrono

inline uint64_t get_monotonic_usec() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

class Entry {
public:
    std::string key;                      // The key for this entry
    std::string value;                    // The value associated with the key
    std::unique_ptr<ZSet> zset;            // Optional sorted set (if applicable)
    size_t heap_idx = static_cast<size_t>(-1); // Heap index for TTL management (-1 if not in heap)

    // Constructor for string-based key-value pairs
    explicit Entry(std::string k, std::string v)
        : key(std::move(k)), value(std::move(v)), zset(nullptr) {}

    // Constructor for sorted set entries
    explicit Entry(std::string k, std::unique_ptr<ZSet> zs)
        : key(std::move(k)), zset(std::move(zs)) {}

    // Destructor to manage memory
    ~Entry() = default;

    // Disable copying to prevent accidental duplication
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;

    // Move constructor and move assignment for efficiency
    Entry(Entry&&) noexcept = default;
    Entry& operator=(Entry&&) noexcept = default;
};

class EntryManager {
public:
    // Function to safely destroy an entry
    static void destroy_entry(Entry* entry) {
        if (!entry) return;
        delete entry;
    }

    // Function to delete an entry asynchronously using a thread pool
    static void delete_entry_async(Entry* entry, ThreadPool& pool) {
        if (!entry) return;
        pool.enqueue([entry]() { destroy_entry(entry); });
    }

    // Function to set time-to-live (TTL) for an entry
    static void set_entry_ttl(Entry& entry, int64_t ttl_ms, BinaryHeap<uint64_t>& heap) {
        if (ttl_ms <= 0) {  // ✅ Remove entry for both `ttl_ms == 0` and `ttl_ms == -1`
            if (entry.heap_idx != static_cast<size_t>(-1)) {
                remove_from_heap(entry, heap);
            }
            return;
        }
    
        uint64_t expire_at = get_monotonic_usec() + static_cast<uint64_t>(ttl_ms) * 1000;
    
        if (entry.heap_idx == static_cast<size_t>(-1)) {
            add_to_heap(entry, expire_at, heap);
        } else {
            update_heap(entry, expire_at, heap);  // ✅ Update TTL instead of re-inserting
        }
    }
    
    
    
    

private:
    // Function to remove an entry from the heap
   // Function to remove an entry from the heap
    static void remove_from_heap(Entry& entry, BinaryHeap<uint64_t>& heap) {

        size_t pos = entry.heap_idx;

        if (pos >= heap.size()) {
            std::cerr << "remove_from_heap: Invalid heap index for entry: " << entry.key << std::endl;
            return;
        }

        // Swap with last element if not already last
        if (pos != heap.size() - 1) {
            std::swap(heap[pos], heap[heap.size() - 1]);
        }

        // Remove last element
        heap.pop();

        // If swapped, restore heap order
        if (pos < heap.size()) {
            heap.update(pos);
        }

        // Mark entry as removed from the heap
        entry.heap_idx = static_cast<size_t>(-1);
    }

    // Function to add an entry to the heap with expiration time
    // Function to add an entry to the heap
    static void add_to_heap(Entry& entry, uint64_t expire_at, BinaryHeap<uint64_t>& heap) {
        heap.push(HeapItem<uint64_t>(expire_at, &entry.heap_idx));
        entry.heap_idx = heap.size() - 1; // Store heap index
        heap.update(entry.heap_idx); // Restore heap order
    }

    // Function to update an entry's expiration time in the heap
   // Function to update an entry's expiration time in the heap
   static void update_heap(Entry& entry, uint64_t new_expire_at, BinaryHeap<uint64_t>& heap){
    if (entry.heap_idx >= heap.size()) {
        std::cerr << "update_heap: Invalid heap index for entry: " << entry.key << std::endl;
        return;
    }
    heap[entry.heap_idx].set_value(new_expire_at); 
    heap.update(entry.heap_idx);  
    }


};

#endif // ENTRY_MANAGER_HPP
