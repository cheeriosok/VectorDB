#ifndef ENTRY_MANAGER_HPP
#define ENTRY_MANAGER_HPP

#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "response_serializer.hpp"

// Modern entry managment with RAII
class EntryManager {
    public:
        static void destroy_entry(Entry* entry) {
            if (!entry) return;
    
            entry->value.clear();
            if (entry->zset) {
                entry->zset->clear();
                entry->zset.reset();
            }
            delete entry;
        }
    
        static void delete_entry_async(Entry* entry, ThreadPool& pool) {
            if (!entry) return;
    
            pool.enqueue([entry]() {
                destroy_entry(entry);
            });
        }
    
        static void set_entry_ttl(Entry& entry, 
                                int64_t ttl_ms, 
                                std::vector<HeapItem>& heap) {
            if (ttl_ms < 0) {
                if (entry.heap_idx != static_cast<size_t>(-1)) {
                    remove_from_heap(entry, heap);
                }
                return;
            }
    
            auto expire_at = get_monotonic_usec() + 
                            static_cast<uint64_t>(ttl_ms) * 1000;
            
            if (entry.heap_idx == static_cast<size_t>(-1)) {
                add_to_heap(entry, expire_at, heap);
            } else {
                update_heap(entry, expire_at, heap);
            }
        }
    
    private:
        static void remove_from_heap(Entry& entry, 
                                   std::vector<HeapItem>& heap) {
            size_t pos = entry.heap_idx;
            heap[pos] = heap.back();
            heap.pop_back();
            
            if (pos < heap.size()) {
                heap_update(heap.data(), pos, heap.size());
            }
            entry.heap_idx = -1;
        }
    
        static void add_to_heap(Entry& entry, 
                              uint64_t expire_at, 
                              std::vector<HeapItem>& heap) {
            HeapItem item{expire_at, &entry.heap_idx};
            heap.push_back(item);
            heap_update(heap.data(), heap.size() - 1, heap.size());
        }
    
        static void update_heap(Entry& entry, 
                              uint64_t expire_at, 
                              std::vector<HeapItem>& heap) {
            heap[entry.heap_idx].val = expire_at;
            heap_update(heap.data(), entry.heap_idx, heap.size());
        }
    };