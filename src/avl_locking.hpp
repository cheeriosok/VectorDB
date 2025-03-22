/*
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    NOTE TO SELF: THIS IS THE ORIGINAL AVL!!!!!!!!!!!!!!
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/


#ifndef AVL_TREE_HPP
#define AVL_TREE_HPP

#include <memory>    
#include <algorithm> 
#include <cstdint>   
#include <utility>   
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <optional> 
#include <chrono> 
#include <iostream>

// forward dec. for our friend class 
template<typename K, typename V>
class AVLTree;

template<typename K, typename V>
class AVLNode;

// Our node class contains two unique pointers to left and right, a raw pointer to parent, and depth + weight values for rebalancing.
// depth = (how far down the tree we are), weight = how many nodes in its subtree (nodes below this node). AVLTree will modify these 2
// so I will call it a friend class.

template<typename K, typename V>
class AVLNode {
public:
    AVLNode(const K& k, const V& v) : key(k), value(v) {}

    ~AVLNode() = default;

    AVLNode(const AVLNode&) = delete;
    AVLNode& operator=(const AVLNode&) = delete;

    AVLNode(AVLNode&&) noexcept = default;
    AVLNode& operator=(AVLNode&&) noexcept = default;

    K key;
    V value;
    uint32_t depth {1};
    uint32_t weight {1};
    std::unique_ptr<AVLNode<K, V>> left;
    std::unique_ptr<AVLNode<K, V>> right;
    AVLNode<K, V>* parent = nullptr;   
    mutable std::recursive_mutex node_mutex;
    friend class AVLTree<K, V>;
};

// Our Tree contains the root_ node, fixLeft, fixRight, RotateLeft, RotateRight, fix (which calls fixLeft and fixRight), offset, remove and insert methods for our AVL Tree.

template<typename K, typename V>
class AVLTree {
public:
    AVLTree() = default;
    ~AVLTree() = default;

    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    AVLTree(AVLTree&&) = default;
    AVLTree& operator= (AVLTree&&) = default;

    std::unique_ptr<AVLNode<K, V>> root_;
    mutable std::shared_mutex tree_mutex;

    // `SET` 
    void set(const K& key, const V& value) {
        // std::cout << "[SET] Trying to set key: " << key << std::endl;
        
        std::shared_lock read_lock(tree_mutex);
        AVLNode<K, V>* existingNode = search(key);
        read_lock.unlock(); // Release read lock before acquiring write lock
    
        if (existingNode) {
            std::lock_guard<std::recursive_mutex> node_lock(existingNode->node_mutex);
            existingNode->value = value;
            return;
        }
    
        std::unique_lock write_lock(tree_mutex);
        if (!existingNode) {
            // std::cout << "[SET] Inserting new key: " << key << std::endl;
            root_ = insert(std::move(root_), key, value);
        }
    }
    
    // `GET` 
    std::optional<V> get(const K& key) const {
        AVLNode<K, V>* node = search(key);
        if (!node) return std::nullopt;
        std::lock_guard<std::recursive_mutex> node_lock(node->node_mutex);
        return std::optional<V>(node->value);
    }
        
    // `DEL key` 
    void del(const K& key) {
        std::unique_lock lock(tree_mutex);
        if (!root_) return;
        root_ = remove(std::move(root_), key);
    }
    
    // `EXISTS key` 
    bool exists(const K& key) const {
        // std::cout << "[EXISTS] Checking if key exists: " << key << std::endl;
        std::shared_lock lock(tree_mutex);
        bool found = search(key) != nullptr;
        // std::cout << "[EXISTS] Key " << key << (found ? " exists." : " does not exist.") << std::endl;
        return found;
    }
    
    

///////////////////////////////////////
private:
std::unique_ptr<AVLNode<K, V>> insert(std::unique_ptr<AVLNode<K, V>> node, const K& key, const V& value) {
    if (!node) {
        // std::cout << "[INSERT] Inserting new node with key: " << key << "\n";
        return std::make_unique<AVLNode<K, V>>(key, value);
    }

    // std::cout << "[INSERT] Traversing node: " << node->key << " for key: " << key << "\n";
    std::unique_lock<std::recursive_mutex> node_lock(node->node_mutex);
    
    if (key < node->key) {
        // std::cout << "[INSERT] Going left from " << node->key << "\n";
        node->left = insert(std::move(node->left), key, value);
        node->left->parent = node.get();
    } else if (key > node->key) {
        // std::cout << "[INSERT] Going right from " << node->key << "\n";
        node->right = insert(std::move(node->right), key, value);
        node->right->parent = node.get();
    } else {
        // std::cout << "[INSERT] Duplicate key " << key << "\n";
        return std::move(node);
    }

    return fix(std::move(node));
}





    std::unique_ptr<AVLNode<K, V>> remove(std::unique_ptr<AVLNode<K, V>> node, const K& key) {
        if (!node) return nullptr;

        std::lock_guard<std::recursive_mutex> node_lock(node->node_mutex);

        if (key < node->key) {
            node->left = remove(std::move(node->left), key);
        } else if (key > node->key) {
            node->right = remove(std::move(node->right), key);
        } else {
            if (!node->left) return std::move(node->right);
            if (!node->right) return std::move(node->left);

            AVLNode<K, V>* successor = node->right.get();
            while (successor->left) {
                successor = successor->left.get();
            }

            node->key = successor->key;
            node->value = successor->value;
            node->right = remove(std::move(node->right), successor->key);
        }

        return fix(std::move(node));
    }

    AVLNode<K, V>* search(const K& key) const {
        // std::cout << "[SEARCH] Searching for key: " << key << std::endl;
        AVLNode<K, V>* node = root_.get();
        
        while (node) {
            // std::cout << "[SEARCH] At node with key: " << node->key << std::endl;
            if (key == node->key) return node;
            node = (key < node->key) ? node->left.get() : node->right.get();
        }
    
        // std::cout << "[SEARCH] Key " << key << " not found." << std::endl;
        return nullptr;
    }    
    
    std::unique_ptr<AVLNode<K, V>> fix(std::unique_ptr<AVLNode<K, V>> node) {
        if (!node) return nullptr;
    
        // std::cout << "[FIX] Fixing node: " << node->key << "\n";
        updateNode(node.get());
    
        uint32_t leftDepth = getDepth(node->left.get());
        uint32_t rightDepth = getDepth(node->right.get());
    
        if (leftDepth > rightDepth + 1) {
            // std::cout << "[FIX] Left heavy at: " << node->key << "\n";
            return fixLeft(std::move(node));
        } else if (rightDepth > leftDepth + 1) {
            // std::cout << "[FIX] Right heavy at: " << node->key << "\n";
            return fixRight(std::move(node));
        }
    
        return std::move(node);
    }
    

    
    /*
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    NOTE TO SELF: THIS IS THE ORIGINAL AVL!!!!!!!!!!!!!!
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/


    

    // helper func to get depth. if node == nullptr, then return 0
    static uint32_t getDepth(const AVLNode<K, V>* node) noexcept {
        return node ? node->depth : 0;
    }
    // helper func to get depth. if node == nullptr, then return 0
    static uint32_t getWeight(const AVLNode<K, V>* node) noexcept {
        return node ? node->weight : 0;
    }

    /*   1
          \
           2
          /
         X
    */
    std::unique_ptr<AVLNode<K, V>> rotateLeft(std::unique_ptr<AVLNode<K, V>> node) {
        if (!node || !node->right) return node;
    
        auto newRoot = std::move(node->right);
        node->right = std::move(newRoot->left);
        if (node->right) node->right->parent = node.get();
    
        newRoot->left = std::move(node);
        newRoot->parent = newRoot->left->parent;
        newRoot->left->parent = newRoot.get();
    
        updateNode(newRoot->left.get());
        updateNode(newRoot.get());
        return newRoot;
    }
        /*   2
            /
           1  
            \  
             X   
    */
    std::unique_ptr<AVLNode<K, V>> rotateRight(std::unique_ptr<AVLNode<K, V>> node) {
        if (!node || !node->left) return node;
    
        auto newRoot = std::move(node->left);
        node->left = std::move(newRoot->right);
        if (node->left) node->left->parent = node.get();
    
        newRoot->right = std::move(node);
        newRoot->parent = newRoot->right->parent;
        newRoot->right->parent = newRoot.get();
    
        updateNode(newRoot->right.get());
        updateNode(newRoot.get());
        return newRoot;
    }
    

    // updateNode simply updates the depth and weight of each node after a potential rotation.
    void updateNode(AVLNode<K, V>* node) noexcept {
        if (node) {
            node->depth = 1 + std::max(getDepth(node->left.get()), getDepth(node->right.get()));
            node->weight = 1 + getWeight(node->left.get()) + getWeight(node->right.get());
        }
    }
    

/* Ex1:Initial Tree:  After rotateRight:        Ex2: Initial Tree:         After rotateLeft:      After rotateRight:
          3                  2 (0)                 3                            3                        2                     
         /                   / \                   /                           /                        / \ 
       2                    1   3                 1                           2                        1   3
      /                                            \                         /
     1                                              2                       1
*/
std::unique_ptr<AVLNode<K, V>> fixLeft(std::unique_ptr<AVLNode<K, V>> node) {
    if (!node || !node->left) return node;

    std::lock_guard<std::recursive_mutex> node_lock(node->node_mutex);

    if (getDepth(node->left->right.get()) > getDepth(node->left->left.get())) {
        node->left = rotateLeft(std::move(node->left));  
    }

    return rotateRight(std::move(node)); 
}


/* Ex1: Initial Tree: After rotateLeft:    Ex2: Initial Tree:       After rotateRight:        After rotateLeft;
            3                 2                  3                          3                           2
             \               / \                  \                          \                         / \
              2             1   3                  1                          2                       1   3
               \                                   /                           \ 
                1                                 2                             1
*/
std::unique_ptr<AVLNode<K, V>> fixRight(std::unique_ptr<AVLNode<K, V>> node) {
    if (!node || !node->right) return node;

    std::lock_guard<std::recursive_mutex> node_lock(node->node_mutex);

    if (getDepth(node->right->left.get()) > getDepth(node->right->right.get())) {
        node->right = rotateRight(std::move(node->right)); 
    }

    return rotateLeft(std::move(node));  
}


};



/*
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    NOTE TO SELF: THIS IS THE ORIGINAL AVL!!!!!!!!!!!!!!
    !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/

#endif