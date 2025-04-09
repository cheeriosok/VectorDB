#ifndef LIST_HPP 
#define LIST_HPP

#include <mutex>

// Forward declaration of DoubleLinkedList for ListNode to friend it
template<typename T>
class DoublyLinkedList;
// Template generic for ListNode - can carry various forms of data.
template<typename T>
class ListNode {
public:
    ListNode() noexcept { link_self(); } // Default constructor creating a new node with a circular reference
    explicit ListNode(T value) : data_(std::move(value)) { link_self(); } // parameterized constructor that takes a value of T moves it into data_ and self-links the node.
    
    [[nodiscard]] const T& data() const noexcept { return data_; } // should not be discarded as a non-modifiable (const) getter, throw an error if so. 
    T& data() noexcept { return data_; } // modifiable getter, may be discarded if mutated in some way.
    
    [[nodiscard]] bool is_linked() const noexcept { return next_ != this; } // return the boolean state of ListNode's next_, [[nodiscard]] to flag unused bool.
    
    void unlink() noexcept { // remove node from list
        prev_->next_ = next_; // if node is linked, take previous node's next and link to present node's next 
        next_->prev_ = prev_; // if node is linked, take next node's prev and link it to node before our node. 
        link_self(); // now, let's take our current node and sever its connections to its sorrounding via calling circular reference helper link_self.
    }
    
    void insert_before(ListNode& node) noexcept { // our current listNode has this method to insert "node" before it.
        node.prev_ = prev_; // here, we assign 'node.prev_' to point to our prev_. 
        node.next_ = this; // then we take node's next to point to this listNode itself
        prev_->next_ = &node; // now our prev ListNode should point to our Node
        prev_ = &node; //and our prev pointer should point to the node passed in.
    }
    
    void insert_after(ListNode& node) noexcept { // our current listNode has this method to insert "node" after it. Same logic as above but reversed.
        node.prev_ = this; // ""
        node.next_ = next_; // ""
        next_->prev_ = &node; // ""
        next_ = &node; // ""
    }

private:
    T data_{}; // this is our template data_! We can hold a lot of things here with our helpful generic
    ListNode* prev_{nullptr}; // default init as nullptr. Definitely a doubleLinkedList already btw - probably should rename the class.
    ListNode* next_{nullptr}; // default init as nullptr.
    /*
    Reasons why we prefer circular referencing over default nullptr initialization for prev_ and next_:
    1. Main Reason: Minimize nullptr checks - every operation would require null checks which would clutter our code. (Consider unlink for example)
    2. Operations such as node->next_->prev_ = node->prev_; will always work without risk of dereferencing nulls.
    3. We use a sentinel node that must have a circular reference, but this is barely an issue.
    */
    void link_self() noexcept {
        prev_ = this;
        next_ = this;
    }
    
    friend class DoublyLinkedList<T>; //  allow DoublyLinkedList<T> to modify objects and call methods constructed from our interface
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
        
        Iterator& operator++() noexcept { // This actually represents the "++object" operator
            current_ = current_->next_;
            return *this;
        }
        
        Iterator operator++(int) noexcept { // This actually represents the "object++" operator
            Iterator tmp = *this;
            ++*this;
            return tmp;
        }
        
        Iterator& operator--() noexcept { // This actually represents the "--object" operator
            current_ = current_->prev_;
            return *this;
        }
        
        Iterator operator--(int) noexcept { // This actually represents the "object--" operator
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
    // We want the compiler generated default and move constructors/destructors but we don't want shallow copies due to high risk of memory mismanagement.
    DoublyLinkedList() = default; // So we generate the default constructors 
    ~DoublyLinkedList() = default; // and the destructors
    
    DoublyLinkedList(const DoublyLinkedList&) = delete; // we delete the lvalue copy constructors
    DoublyLinkedList& operator=(const DoublyLinkedList&) = delete; // and the assignment operators for copying
    
    DoublyLinkedList(DoublyLinkedList&&) noexcept = default; // but we set the move constructors as default
    DoublyLinkedList& operator=(DoublyLinkedList&&) noexcept = default; // and the operator is overloaded to only accept std::move rather than lvalue object instances
    
    [[nodiscard]] Iterator begin() noexcept { 
        std::shared_lock lock(list_mutex_);
        return Iterator(head_.next_); 
    } // beginning of list

    [[nodiscard]] Iterator end() noexcept { 
        std::shared_lock lock(list_mutex_);
        return Iterator(&head_); 
    } // end of list 
    
    [[nodiscard]] bool empty() const noexcept { 
        std::shared_lock lock(list_mutex_);
        return !head_.is_linked(); 
    } // if circular reference, our sentinel node is alone and thus the list is empty.
    
    void push_front(ListNode<T>& node) noexcept { // use ListNode insert_after method to push to the front of head.
        std::unique_lock lock(list_mutex_);
        head_.insert_after(node);
    }
    
    void push_back(ListNode<T>& node) noexcept {  // use ListNode insert_after method to push to the back of head.
        std::unique_lock lock(list_mutex_);
        head_.insert_before(node);
    }

private:
    ListNode<T> head_; // sentinel node
    mutable std::shared_mutex list_mutex_;  // ✅ Protects concurrent access
};

#endif // LIST_HPP