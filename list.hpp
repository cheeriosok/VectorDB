#ifndef LIST_HPP
#define LIST_HPP

#include <memory>
#include <type_traits>

namespace ds {

template<typename T>
class DoublyLinkedList;

template<typename T>
class ListNode {
public:
    ListNode() noexcept { link_self(); }
    explicit ListNode(T value) : data_(std::move(value)) { link_self(); }
    
    [[nodiscard]] const T& data() const noexcept { return data_; }
    T& data() noexcept { return data_; }
    
    [[nodiscard]] bool is_linked() const noexcept { return next_ != this; }
    
    // Operations
    void unlink() noexcept {
        prev_->next_ = next_;
        next_->prev_ = prev_;
        link_self();
    }
    
    void insert_before(ListNode& node) noexcept {
        node.prev_ = prev_;
        node.next_ = this;
        prev_->next_ = &node;
        prev_ = &node;
    }
    
    void insert_after(ListNode& node) noexcept {
        node.next_ = next_;
        node.prev_ = this;
        next_->prev_ = &node;
        next_ = &node;
    }

private:
    T data_{};
    ListNode* prev_{nullptr};
    ListNode* next_{nullptr};
    
    void link_self() noexcept {
        prev_ = this;
        next_ = this;
    }
    
    friend class DoublyLinkedList<T>;
};

template<typename T>
class DoublyLinkedList {
public:
    class Iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        
        explicit Iterator(ListNode<T>* node) noexcept : current_(node) {}
        
        reference operator*() noexcept { return current_->data(); }
        pointer operator->() noexcept { return &current_->data(); }
        
        Iterator& operator++() noexcept {
            current_ = current_->next_;
            return *this;
        }
        
        Iterator operator++(int) noexcept {
            Iterator tmp = *this;
            ++*this;
            return tmp;
        }
        
        Iterator& operator--() noexcept {
            current_ = current_->prev_;
            return *this;
        }
        
        Iterator operator--(int) noexcept {
            Iterator tmp = *this;
            --*this;
            return tmp;
        }
        
        bool operator==(const Iterator& other) const noexcept {
            return current_ == other.current_;
        }
        
        bool operator!=(const Iterator& other) const noexcept {
            return !(*this == other);
        }
        
    private:
        ListNode<T>* current_;
        friend class DoublyLinkedList;
    };
    
    DoublyLinkedList() = default;
    ~DoublyLinkedList() = default;
    
    DoublyLinkedList(const DoublyLinkedList&) = delete;
    DoublyLinkedList& operator=(const DoublyLinkedList&) = delete;
    
    DoublyLinkedList(DoublyLinkedList&&) noexcept = default;
    DoublyLinkedList& operator=(DoublyLinkedList&&) noexcept = default;
    
    [[nodiscard]] Iterator begin() noexcept { return Iterator(head_.next_); }
    [[nodiscard]] Iterator end() noexcept { return Iterator(&head_); }
    
    [[nodiscard]] bool empty() const noexcept { return !head_.is_linked(); }
    
    void push_front(ListNode<T>& node) noexcept {
        head_.insert_after(node);
    }
    
    void push_back(ListNode<T>& node) noexcept {
        head_.insert_before(node);
    }

private:
    ListNode<T> head_;
};

} // namespace ds

#endif // LIST_HPP