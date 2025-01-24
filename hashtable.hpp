#ifndef HASH_TABLE_HPP
#define HASH_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <functional>
#include <type_traits>
#include <vector>
#include <optional>
#include <bit>
#include <cassert>



// FNV-1a Hashing Algorithm with SIMD.
// New keywords [[nodiscard]] == throw an exception if the return value is unused
// noexcept, this function should not throw any exceptions. 
// inline = for commmonly called functions (especially those with simpler arithmetic), instructs
// the compiler to 'paste the code' in-line to avoid function 

// Can modify to use Neon, but I am too lazy.
[[nodiscard]] inline std::uint64_t hash_string(const std::uint8_t* data, size_t len) noexcept {
    constexpr uint32_t INITIAL = 0x811C9DC5;
    constexpr uint32_t MULTIPLIER = 0x01000193;

    uint32_t hash = INITIAL;
    size_t i = 0;

    for (; i + 4 <= len; i += 4) { // SIMD/Loop Unrolling
        hash = (hash ^ data[i]) * MULTIPLIER;
        hash = (hash ^ data[i + 1]) * MULTIPLIER;
        hash = (hash ^ data[i + 2]) * MULTIPLIER;
        hash = (hash ^ data[i + 3]) * MULTIPLIER;
    }

    for (; i < len; i++) {
        hash = (hash ^ data[i]) * MULTIPLIER;
    }

    return hash;
}

// Our HashNode cannot build from copy construction - it can either be default initalized or rvalue initialized

template<typename T>
class HashNode {
public:
    HashNode() noexcept = default; // no exceptions on our default constructor
    ~HashNode() = default; // removed virtual - largely unnecessary - no child classes
    
    HashNode(const HashNode&) = delete; // delete copy constructor - disallowing copying from lvalues.
    HashNode& operator=(const HashNode&) = delete; // delete assigment operator (copy) - disallowing copying from lvalues
    
    HashNode(HashNode&&) noexcept = default; // allow transfer ownership of resources with rvalues
    HashNode& operator=(HashNode&&) noexcept = default; // overload = operator with rvalue resource transfer

protected:
    std::unique_ptr<HashNode> next_; // pointer to the next node in our 'bucket'
    std::uint64_t hash_code_{0}; // bucket id (i.e shelf number in a library system)

    friend class HashTable<T>; // We allow access to HashTable to modify us!
    friend class HashMap<T>; // We allow access to HashMap to modify us!
};

template<typename T>
class HashTable {
public:
    explicit HashTable(size_t initial_size = 0) {
        if (initial_size > 0) {
            initialize(std::bit_ceil(initial_size));
        }
    }

    HashTable(const HashTable&) = delete; 
    HashTable& operator=(const HashTable&) = delete; 

    HashTable(HashTable&&) noexcept = default; 
    HashTable& operator=(HashTable&&) noexcept = default; 

    void insert(std::unique_ptr<HashNode<T>> node) {
        if (buckets_.empty()) {
            initialize(MIN_CAPACITY);
        }

        size_t pos = node->hash_code_ & mask_;
        node->next = std::move(buckets_[pos]);
        buckets_[pos] = std::move(node);
        size_++;
    }
    
    HashNode<T>* lookup(
        const HashNode<T>* key, 
        const std::function<bool(const HashNode<T>*, const HashNode<T>*)>& comparator
    ) const {
        if (buckets_.empty()) {
            return nullptr;
        }

        size_t pos = key->hash_code_ & mask_;
        HashNode<T>* current = buckets_[pos].get();

        while (current) {
            if (comparator(current, key)) { 
                return current;  
            }
            current = current->next_.get();
        }
        return nullptr;
    }

    std::unique_ptr<HashNode<T>> remove(
        const HashNode<T>& key, 
        const std::function<bool(const HashNode<T>&, const HashNode<T>&)>& comparator;
    ) {
            if (buckets.empty()) {
                return nullptr;
            }

            size_t pos = key->hashcode & mask;
            const HashNode<T>* current = buckets[pos].get();
            const HashNode<T>* prev = nullptr;

            while (current) {
                if (comparator(current, key)) { 
                    if (prev) {
                        auto node = std::move(prev->next); 
                        prev->next = std::move(node->next);
                        return node;
                    } else {
                        auto node = std::move(buckets[pos]);
                        buckets[pos] = std::move(node->next);
                        return node;
                    }
                }
                prev = current;
                current = current->next_.get();
            }
        return nullptr;
    }
    
    [[nodiscard]] size_t size() const noexcept { return size_; } 
    [[nodiscard]] size_t capacity() const noexcept { return mask_ + 1; } 
    bool empty() const noexcept { return size_ == 0; } 

private:
    std::vector<std::unique_ptr<HashNode<T>>> buckets_; 
    size_t mask_{0}; 
    size_t size_{0}; 

    static constexpr size_t MIN_CAPACITY = 4;

    void initialize(size_t capacity) { 
        assert(std::has_single_bit(capacity));  
        capacity = std::max(capacity, MIN_CAPACITY); 
        
        buckets_.resize(capacity);
        mask_ = capacity - 1; 
        size_ = 0; 
    }
};

template<typename T>
class HashMap {
public:
    HashMap() = default;
    ~HashMap() = default;

    HashMap(const HashMap&) = delete; // lets delete copy  consructors , encorce RAII
    HashMap& operator=(const HashMap&) = delete; // lets delete copy init , encorce RAII

    HashMap(HashMap&&) noexcept = default; // default is memory ownership transfer
    HashMap& operator=(HashMap&&) noexcept = default; // default is memory ownership transfer

    void insert(std::unique_ptr<HashNode<T>> node) {
        if (primary_table_.empty()) {
            primary_table_ = HashTable<T>(MIN_CAPACITY);
        }

        primary_table_.insert(std::move(node));

        if (!resizing_table_) {
            size_t load_factor = primary_table_.size() / primary_table_.capacity();
            if (load_factor >= MAX_LOAD_FACTOR) {
                start_resize();
            }
        }
        help_resize();
    }

    HashNode<T>* find(
        const HashNode<T>* key,
        const std::function<bool(const HashNode<T>*, const HashNode<T>*)>& comparator)
    {
        help_resize();
        
        if (auto node = primary_table_.lookup(key, comparator)) {
            return node.get();
        }
        if (resizing_table_) {
            if (auto node = resizing_table_->lookup(key, comparator)) {
                return node.get();
            }
        }
        return nullptr;
    }

    std::unique_ptr<HashNode<T>> remove(
        const HashNode<T>* key,
        const std::function<bool(const HashNode<T>*, const HashNode<T>*)>& comparator)
    {
        help_resize();
        
        if (auto node = primary_table_.remove(key, comparator)) {
            return node;
        }
        if (resizing_table_) {
            if (auto node = resizing_table_->remove(key, comparator)) {
                return node;
            }
        }
        return nullptr;
    }

    [[nodiscard]] size_t size() const noexcept {
        return primary_table_.size() + 
               (resizing_table_ ? resizing_table_->size() : 0);
    }
    
    bool empty() const noexcept { return size() == 0; }

private:
    static constexpr size_t RESIZE_WORK_CHUNK = 128;
    static constexpr size_t MAX_LOAD_FACTOR = 8;
    static constexpr size_t MIN_CAPACITY = 4;

    HashTable<T> primary_table_;
    std::optional<HashTable<T>> resizing_table_;
    size_t resizing_pos_{0};

    void help_resize() {
        if (!resizing_table_) {
            return;
        }

        size_t work_done = 0;
        while (work_done < RESIZE_WORK_CHUNK && !resizing_table_->empty()) {
            // Move nodes from resizing table to primary table
            if (resizing_pos_ >= resizing_table_->capacity()) {
                resizing_pos_ = 0;
            }

            auto& bucket = resizing_table_->buckets_[resizing_pos_];
            if (bucket) {
                auto node = std::move(bucket);
                bucket = std::move(node->next_);
                node->next_ = nullptr;
                primary_table_.insert(std::move(node));
                resizing_table_->size_--;
                work_done++;
            } else {
                resizing_pos_++;
            }
        }

        if (resizing_table_->empty()) {
            resizing_table_.reset();
            resizing_pos_ = 0;
        }
    }

    void start_resize() {
        assert(!resizing_table_);
        size_t new_capacity = primary_table_.capacity() * 2;
        resizing_table_.emplace(std::move(primary_table_));
        primary_table_ = HashTable<T>(new_capacity);
        resizing_pos_ = 0;
    }
};

#endif // HASH_TABLE_HPP