#ifndef HEAP_HPP
#define HEAP_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <stdexcept>

namespace ds {

template<typename T>
class HeapItem {
public:
    HeapItem() = default;
    explicit HeapItem(T value) : value_(std::move(value)) {}
    
    // Accessors
    [[nodiscard]] const T& value() const noexcept { return value_; }
    T& value() noexcept { return value_; }
    
    void set_position(std::size_t* pos) noexcept { position_ref_ = pos; }
    
private:
    T value_{};
    std::size_t* position_ref_{nullptr};
    
    friend class BinaryHeap<T>;
};

template<typename T, typename Compare = std::less<T>>
class BinaryHeap {
public:
    BinaryHeap() = default;
    explicit BinaryHeap(const Compare& comp = Compare{}) : compare_(comp) {}
    
    void push(HeapItem<T> item) {
        items_.push_back(std::move(item));
        sift_up(items_.size() - 1);
    }

    [[nodiscard]] const HeapItem<T>& top() const {
        if (empty()) {
            throw std::out_of_range("Heap is empty");
        }
        return items_[0];
    }

    HeapItem<T> pop() {
        if (empty()) {
            throw std::out_of_range("Heap is empty");
        }
        
        HeapItem<T> result = std::move(items_[0]);
        if (size() > 1) {
            items_[0] = std::move(items_.back());
            items_.pop_back();
            sift_down(0);
        } else {
            items_.pop_back();
        }
        return result;
    }

    void update(std::size_t pos) {
        if (pos >= items_.size()) {
            throw std::out_of_range("Position out of range");
        }
        
        if (pos > 0 && compare_(items_[pos].value_, items_[parent(pos)].value_)) {
            sift_up(pos);
        } else {
            sift_down(pos);
        }
    }

    [[nodiscard]] bool empty() const noexcept { return items_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return items_.size(); }
    
private:
    std::vector<HeapItem<T>> items_;
    Compare compare_;
    
    void sift_up(std::size_t pos) {
        HeapItem<T> temp = std::move(items_[pos]);
        
        while (pos > 0) {
            std::size_t parent_pos = parent(pos);
            if (!compare_(temp.value_, items_[parent_pos].value_)) {
                break;
            }
            
            items_[pos] = std::move(items_[parent_pos]);
            if (items_[pos].position_ref_) {
                *items_[pos].position_ref_ = pos;
            }
            pos = parent_pos;
        }
        
        items_[pos] = std::move(temp);
        if (items_[pos].position_ref_) {
            *items_[pos].position_ref_ = pos;
        }
    }
    
    void sift_down(std::size_t pos) {
        HeapItem<T> temp = std::move(items_[pos]);
        const std::size_t len = items_.size();
        
        while (true) {
            std::size_t min_pos = pos;
            std::size_t left = left_child(pos);
            std::size_t right = right_child(pos);
            
            if (left < len && compare_(items_[left].value_, temp.value_)) {
                min_pos = left;
            }
            
            if (right < len && compare_(items_[right].value_, 
                (min_pos == pos ? temp.value_ : items_[min_pos].value_))) {
                min_pos = right;
            }
            
            if (min_pos == pos) {
                break;
            }
            
            items_[pos] = std::move(items_[min_pos]);
            if (items_[pos].position_ref_) {
                *items_[pos].position_ref_ = pos;
            }
            pos = min_pos;
        }
        
        items_[pos] = std::move(temp);
        if (items_[pos].position_ref_) {
            *items_[pos].position_ref_ = pos;
        }
    }
    
    static constexpr std::size_t parent(std::size_t i) noexcept {
        return (i + 1) / 2 - 1;
    }
    
    static constexpr std::size_t left_child(std::size_t i) noexcept {
        return i * 2 + 1;
    }
    
    static constexpr std::size_t right_child(std::size_t i) noexcept {
        return i * 2 + 2;
    }
};

} // namespace ds

#endif // HEAP_HPP