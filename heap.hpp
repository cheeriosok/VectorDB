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
    explicit HeapItem(T value) : value_(std::move(value)) {} // constructor with T value argument -> take rvalue as input, and position_ref remians null

    // classic delete copy operations
    HeapItem(const HeapItem&) = delete;
    HeapItem& operator=(const HeapItem&) = delete;

    // set move operations as default
    HeapItem(HeapItem&& other) noexcept = default;
    HeapItem& operator=(HeapItem&& other) noexcept = default;

    // getter
    [[nodiscard]] const T& value() const noexcept { return value_; } //throw compiler warning if return value unused
    T& value() noexcept { return value_; } //non-const accessor
    // setter
    void set_position(std::size_t* pos) noexcept { position_ref_ = pos; } // set position_ref_ pointer to track the item's position in the heap.

private:
    T value_{}; // value in heap
    std::size_t* position_ref_{nullptr}; // pointer to the position of this item in heap

    friend class BinaryHeap<T>; // provide access/mod to BinaryHeap
};

template<typename T, typename Compare = std::less<T>>
class BinaryHeap {
public:
    BinaryHeap() = default;
    explicit BinaryHeap(const Compare& comp = Compare{}) : compare_(comp) {} // This constructor takes in a comparator function!
    
    void push(HeapItem<T> item) { // push method = push HeapItem<T> item to the back of our vector - we sift locations up accordingly thereafter
        items_.push_back(std::move(item));
        sift_up(items_.size() - 1);
    }

    [[nodiscard]] const HeapItem<T>& top() const { // Top method = read index 0 of items, read-only (immutable)
        if (empty()) {
            throw std::out_of_range("Heap is empty");
        }
        return items_[0];
    }

    HeapItem<T> pop() { 
        if (empty()) {
            throw std::out_of_range("Heap is empty");
        }
        
        HeapItem<T> result = std::move(items_[0]); // move our items_[0] to our result. Since we popped our element - we must 
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
    std::vector<HeapItem<T>> items_; // So we have a vector of HeapItems with any type
    Compare compare_; // and a comparator function (for our MinHeap functionality)
    
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
    // If the item at the present node gets popped, we must redefine who takes its place! We 
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
    
    /*
    Heap (tree view)              Array representation
          10                         [10, 20, 15, 30, 40]
         /  \                       
       20   15
      /  \
    30   40
    
    This is a MinHeap, and the array representation allows us to get parent of, left child and right child of any node with pointer arithmetic.

    */
    static constexpr std::size_t parent(std::size_t i) noexcept { // As you can see, given 30, (idx = 3), the parent of 30 is 20 (idx = 1).
        return (i + 1) / 2 - 1;
    }
    
    static constexpr std::size_t left_child(std::size_t i) noexcept { // As you can see, given 20, (idx = 1), the left child is 30 or (idx = 3).
        return i * 2 + 1;
    }
    
    static constexpr std::size_t right_child(std::size_t i) noexcept {  // As you can see the right child of 20 (idx = 1) is 40 (idx = 4)
        return i * 2 + 2;
    }
};

} // namespace ds

#endif // HEAP_HPP