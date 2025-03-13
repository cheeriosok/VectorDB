#ifndef ENTRY_MANAGER_HPP
#define ENTRY_MANAGER_HPP

#include <cstdint>       
#include <memory>        
#include <unordered_map>
#include <unordered_set>
#include <functional>            
#include <string>        
#include <optional>      
#include <chrono>        
#include <iostream>
#include <type_traits>
#include <mutex>
#include "common.hpp"
#include "response_serializer.hpp" 
#include "src/heap.hpp"               
#include "src/thread_pool.hpp"   
#include "src/zset.hpp"             
#include "src/hashtable.hpp"

template <typename T>
struct IsValidType : std::disjunction<
    std::is_same<T, std::string>,                             
    std::is_same<T, int64_t>,                                 
    std::is_same<T, double>,                                  
    std::is_same<T, std::vector<std::string>>,               
    std::is_same<T, std::unordered_map<std::string, std::string>>, 
    std::is_same<T, std::unordered_set<std::string>>,        
    std::is_same<T, std::unique_ptr<ZSet>>> {};              

    inline uint64_t get_monotonic_usec() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

class EntryBase {
public:
    virtual ~EntryBase() = default;
    virtual void print() const = 0;
    size_t heap_idx = static_cast<size_t>(-1);
    std::string key;
};

template <typename T, typename = std::enable_if_t<IsValidType<T>::value>>
class Entry : public EntryBase {
public:    
    T value; 

    explicit Entry(std::string k, T v) : value(std::move(v)) {
        key = std::move(k);
    }

    Entry(Entry&&) noexcept = default;
    Entry& operator=(Entry&&) noexcept = default;

    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;

    void print() const override {
        if constexpr (std::is_same<T, std::unique_ptr<ZSet>>::value) {
            std::cout << "Key: " << key << " -> ZSet (Sorted Set) {" << std::endl;
            if (value) {
                std::lock_guard<std::mutex> lock(value->mutex_);
                for (const auto& node : value->nodes_) {
                    std::cout << "  " << node->get_key() << ": " << node->get_value() << std::endl;
                }
            } else {
                std::cout << "  (empty ZSet)" << std::endl;
            }
            std::cout << "}" << std::endl;
        } else {
            std::cout << "Key: " << key << " -> " << value << std::endl;
        }
    }
};

class EntryManager {
private:
    HMap<std::string, std::shared_ptr<EntryBase>> db_;  
    BinaryHeap<uint64_t> heap_;                         
    ThreadPool thread_pool_;                            
    mutable std::mutex mutex_;                        

public:
    EntryManager(size_t thread_pool_size = 4)
        : heap_(std::less<uint64_t>()), thread_pool_(thread_pool_size) {}


    std::shared_ptr<EntryBase> find_entry(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto entry = db_.find(key); 
        return entry ? *entry : nullptr;
    }
        
    
    template <typename T>
    std::shared_ptr<Entry<T>> create_entry(std::string key, T value) {
        static_assert(IsValidType<T>::value, "Invalid Redis type");

        std::lock_guard<std::mutex> lock(mutex_);
        auto entry = std::make_shared<Entry<T>>(std::move(key), std::move(value));
        db_.insert(entry->key, entry);
        return entry;
    }

    void delete_entry(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto entry = db_.find(key);
        if (entry) {
            if ((*entry)->heap_idx != static_cast<size_t>(-1)) {
                remove_from_heap(**entry);
            }
            db_.remove(key); 
        }
    }
    
    void clear_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        db_.clear();  
        heap_.clear();  
    }

    void set_entry_ttl(EntryBase& entry, int64_t ttl_ms) {
        if (ttl_ms <= 0) {  
            delete_entry(entry.key);
            return;
        }

        uint64_t expire_at = get_monotonic_usec() + static_cast<uint64_t>(ttl_ms) * 1000;

        if (entry.heap_idx == static_cast<size_t>(-1)) {
            add_to_heap(entry, expire_at);
        } else {
            update_heap(entry, expire_at);
        }
    }

    int64_t get_expiry_time(EntryBase& entry) {
        uint64_t now = get_monotonic_usec();
        
        if (entry.heap_idx == static_cast<size_t>(-1) || entry.heap_idx >= heap_.size()) {
            return -1;  
        }

        uint64_t expire_at = heap_[entry.heap_idx].value();
        int64_t remaining_time = static_cast<int64_t>((expire_at - now) / 1000);  

        return remaining_time > 0 ? remaining_time : -1; 
    }

    void remove_from_heap(EntryBase& entry) {
        if (entry.heap_idx >= heap_.size()) return;
        heap_.pop();
        entry.heap_idx = static_cast<size_t>(-1);
    }

    void add_to_heap(EntryBase& entry, uint64_t expire_at) {
        heap_.push(HeapItem<uint64_t>(expire_at, &entry.heap_idx));
        entry.heap_idx = heap_.size() - 1;
        heap_.update(entry.heap_idx);
    }

    void update_heap(EntryBase& entry, uint64_t new_expire_at) {
        if (entry.heap_idx >= heap_.size()) return;
        heap_[entry.heap_idx].set_value(new_expire_at);
        heap_.update(entry.heap_idx);
    }

    

    void delete_entry_async(const std::string& key) {
        thread_pool_.enqueue([this, key]() { 
            delete_entry(key); 
        });
        thread_pool_.wait_for_tasks(); 
    }
    

    // void print_db() const {
    //     std::lock_guard<std::mutex> lock(mutex_);
    //     for (const auto& [key, entry] : db_) {
    //         entry->print();
    //     }
    // }
};

#endif // ENTRY_MANAGER_HPP
