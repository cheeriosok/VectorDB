#ifndef ENTRY_MANAGER_HPP
#define ENTRY_MANAGER_HPP

#include <vector>        // std::vector
#include <cstdint>       // int64_t, uint64_t
#include <memory>        // std::unique_ptr
#include <unordered_map> // std::unordered_map
#include <functional>    // std::function (used in thread pool)
#include "response_serializer.hpp" 
#include "heap.hpp"               
#include "thread_pool.hpp"         
#include "entry.hpp"               
#include "common.hpp"           


class EntryManager {
    public:
        // function to safely destroy an entry
        static void destroy_entry(Entry* entry) {
            if (!entry) return; // return if entry is null
    
            entry->value.clear(); // clear the value stored in the entry
            if (entry->zset) { // check if the entry has a sorted set
                entry->zset->clear(); // clear the sorted set
                entry->zset.reset(); // reset the unique pointer
            }
            delete entry; // free memory allocated for entry
        }
    
        // function to delete an entry asynchronously using a thread pool
        static void delete_entry_async(Entry* entry, ThreadPool& pool) {
            if (!entry) return; // return if entry is null
    
            pool.enqueue([entry]() { // enqueue a task in the thread pool
                destroy_entry(entry); // call destroy_entry within the async task
            });
        }
    
        // function to set time-to-live (ttl) for an entry
        static void set_entry_ttl(Entry& entry, 
                                int64_t ttl_ms, 
                                std::vector<HeapItem>& heap) {
            if (ttl_ms < 0) { // if ttl is negative, remove from heap
                if (entry.heap_idx != static_cast<size_t>(-1)) { // check if entry has an active heap index
                    remove_from_heap(entry, heap); // remove from heap
                }
                return;
            }
    
            auto expire_at = get_monotonic_usec() +  // calculate expiration time
                            static_cast<uint64_t>(ttl_ms) * 1000;
            
            if (entry.heap_idx == static_cast<size_t>(-1)) { // if entry is not already in heap
                add_to_heap(entry, expire_at, heap); // add to heap with expiration time
            } else {
                update_heap(entry, expire_at, heap); // update expiration time in heap
            }
        }
    
    private:
        // function to remove an entry from the heap
        static void remove_from_heap(Entry& entry, 
                                   std::vector<HeapItem>& heap) {
            size_t pos = entry.heap_idx; // get the heap index of the entry
            heap[pos] = heap.back(); // move last element to the removed position
            heap.pop_back(); // remove last element from heap
            
            if (pos < heap.size()) { // reheapify if necessary
                heap_update(heap.data(), pos, heap.size());
            }
            entry.heap_idx = -1; // mark entry as not in heap
        }
    
        // function to add an entry to the heap with expiration time
        static void add_to_heap(Entry& entry, 
                              uint64_t expire_at, 
                              std::vector<HeapItem>& heap) {
            HeapItem item{expire_at, &entry.heap_idx}; // create heap item with expiration time
            heap.push_back(item); // add item to heap
            heap_update(heap.data(), heap.size() - 1, heap.size()); // reheapify to maintain order
        }
    
        // function to update an entry's expiration time in the heap
        static void update_heap(Entry& entry, 
                              uint64_t expire_at, 
                              std::vector<HeapItem>& heap) {
            heap[entry.heap_idx].val = expire_at; // update expiration time in heap
            heap_update(heap.data(), entry.heap_idx, heap.size()); // reheapify heap
        }
    };

#endif
