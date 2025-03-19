
/*
Reference for AVL LockFree
Issues: Too much CAS, atomicity, the performance will suffer.
Untested slop. Do not use as a reference.
*/

#ifndef AVL_TREE_HPP
#define AVL_TREE_HPP

#include <memory>    
#include <algorithm> 
#include <cstdint>   
#include <utility>   
#include <cstddef>
#include <atomic>

template<typename K, typename V>
struct AVLTree;

template<typename K, typename V>
struct AVLNode;

template<typename K, typename V>
struct AVLTree {
    AVLTree() = default;
    ~AVLTree() = default;

    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    AVLTree(AVLTree&&) = default;
    AVLTree& operator= (AVLTree&&) = default;

    std::atomic<std::shared_ptr<AVLNode<K,V>>> root_;

    // `SET` 
    void set(const K& key, const V& value) {
        std::shared_ptr<AVLNode<K, V>> current_root;
        std::shared_ptr<AVLNode<K, V>> new_root;
        std::shared_ptr<AVLNode<K, V>> node = search(key);
        
        if (node) {
            V expected = node->value;
            while (!std::atomic_compare_exchange_weak_explicit(
                &node->value,
                &expected,
                value,
                std::memory_order_release,
                std::memory_order_relaxed)) {
            }
        } else {
            do {
                current_root = root_.load(std::memory_order_acquire);
                new_root = insert(std::shared_ptr<AVLNode<K, V>>(current_root), key, value);
            } while (!root_.compare_exchange_weak(current_root, new_root, 
                     std::memory_order_release, std::memory_order_relaxed));
        }
    }
    
    // `GET` 
    std::optional<V> get(const K& key) const {
        std::shared_ptr<AVLNode<K, V>> node = search(key);
        return node ? std::optional<V>(node->value.load(std::memory_order_acquire)) : std::nullopt;
    }
        
    // `DEL key` 
    void del(const K& key) {
        std::shared_ptr<AVLNode<K, V>> current_root;
        std::shared_ptr<AVLNode<K, V>> new_root;
        
        do {
            current_root = root_.load(std::memory_order_acquire);
            if (!current_root) return;
            new_root = remove(std::shared_ptr<AVLNode<K, V>>(current_root), key);
        } while (!root_.compare_exchange_weak(current_root, new_root, 
                 std::memory_order_release, std::memory_order_relaxed));
    }

    // `EXISTS key` 
    bool exists(const K& key) const {
        return search(key) != nullptr;
    }

    std::shared_ptr<AVLNode<K, V>> fix(std::shared_ptr<AVLNode<K, V>> node) {
        if (!node) return nullptr;

        node->update();

        uint32_t leftDepth = getDepth(node->left.load(std::memory_order_acquire));
        uint32_t rightDepth = getDepth(node->right.load(std::memory_order_acquire));

        if (leftDepth > rightDepth + 1) {
            return fixLeft(std::move(node));
        } else if (rightDepth > leftDepth + 1) {
            return fixRight(std::move(node));
        }

        return node;
    }

    std::shared_ptr<AVLNode<K, V>> remove(std::shared_ptr<AVLNode<K, V>> node, const K& key) {
        if (!node) return nullptr;

        if (key < node->key) {
            std::shared_ptr<AVLNode<K, V>> current_left = node->left.load(std::memory_order_acquire);
            std::shared_ptr<AVLNode<K, V>> new_left = remove(std::shared_ptr<AVLNode<K, V>>(current_left), key);
            
            while (!node->left.compare_exchange_weak(current_left, new_left, 
                   std::memory_order_release, std::memory_order_relaxed)) {
                new_left = remove(std::shared_ptr<AVLNode<K, V>>(current_left), key);
            }
        } else if (key > node->key) {
            std::shared_ptr<AVLNode<K, V>> current_right = node->right.load(std::memory_order_acquire);
            std::shared_ptr<AVLNode<K, V>> new_right = remove(std::shared_ptr<AVLNode<K, V>>(current_right), key);
            
            while (!node->right.compare_exchange_weak(current_right, new_right, 
                   std::memory_order_release, std::memory_order_relaxed)) {
                new_right = remove(std::shared_ptr<AVLNode<K, V>>(current_right), key);
            }
        } else {
            std::shared_ptr<AVLNode<K, V>> left_child = node->left.load(std::memory_order_acquire);
            std::shared_ptr<AVLNode<K, V>> right_child = node->right.load(std::memory_order_acquire);
            
            if (!left_child) return right_child;
            if (!right_child) return left_child;

            std::shared_ptr<AVLNode<K, V>> successor = right_child;
            std::shared_ptr<AVLNode<K, V>> successor_parent = node; 
            
            std::shared_ptr<AVLNode<K, V>> successor_left = successor->left.load(std::memory_order_acquire);
            while (successor_left) {
                successor_parent = successor;
                successor = successor_left;
                successor_left = successor->left.load(std::memory_order_acquire);
            }

            node->key = successor->key;
            node->value.store(successor->value.load(std::memory_order_acquire), std::memory_order_release);

            if (successor_parent != node) {
                std::shared_ptr<AVLNode<K, V>> successor_right = successor->right.load(std::memory_order_acquire);
                
                while (!successor_parent->left.compare_exchange_weak(successor, successor_right, 
                       std::memory_order_release, std::memory_order_relaxed)) {
                    if (successor_parent->left.load(std::memory_order_acquire) != successor) {
                        return remove(node, key);
                    }
                }
            } else {
                std::shared_ptr<AVLNode<K, V>> successor_right = successor->right.load(std::memory_order_acquire);
                
                while (!node->right.compare_exchange_weak(successor, successor_right, 
                       std::memory_order_release, std::memory_order_relaxed)) {
                    successor = node->right.load(std::memory_order_acquire);
                    successor_right = successor->right.load(std::memory_order_acquire);
                }
            }
        }

        return fix(std::move(node)); 
    }
    

    std::shared_ptr<AVLNode<K, V>> insert(std::shared_ptr<AVLNode<K, V>> node, const K& key, const V& value) {
        if (!node) {
            auto new_node = std::make_shared<AVLNode<K, V>>(key, value);
            return new_node;
        }

        if (key < node->key) {
            std::shared_ptr<AVLNode<K, V>> current_left = node->left.load(std::memory_order_acquire);
            std::shared_ptr<AVLNode<K, V>> new_left = insert(std::shared_ptr<AVLNode<K, V>>(current_left), key, value);
            
            while (!node->left.compare_exchange_weak(current_left, new_left, 
                   std::memory_order_release, std::memory_order_relaxed)) {
                new_left = insert(std::shared_ptr<AVLNode<K, V>>(current_left), key, value);
            }
        } else if (key > node->key) {
            std::shared_ptr<AVLNode<K, V>> current_right = node->right.load(std::memory_order_acquire);
            std::shared_ptr<AVLNode<K, V>> new_right = insert(std::shared_ptr<AVLNode<K, V>>(current_right), key, value);
            
            while (!node->right.compare_exchange_weak(current_right, new_right, 
                   std::memory_order_release, std::memory_order_relaxed)) {
                new_right = insert(std::shared_ptr<AVLNode<K, V>>(current_right), key, value);
            }
        } else {
            V expected = node->value.load(std::memory_order_acquire);
            while (!std::atomic_compare_exchange_weak_explicit(
                &node->value,
                &expected,
                value,
                std::memory_order_release,
                std::memory_order_relaxed)) {
            }
            return node;
        }

        return fix(std::move(node)); 
    }


    std::shared_ptr<AVLNode<K, V>> search(const K& key) const {
        std::shared_ptr<AVLNode<K, V>> node = root_.load(std::memory_order_acquire);
        while (node) {
            if (key == node->key) return node;
            node = (key < node->key) ? node->left.load(std::memory_order_acquire) 
                                     : node->right.load(std::memory_order_acquire);
        }
        return nullptr;
    }
    
    static uint32_t getDepth(const std::shared_ptr<AVLNode<K, V>>& node) noexcept {
        return node ? node->depth.load(std::memory_order_acquire) : 0;
    }
    
    static uint32_t getWeight(const std::shared_ptr<AVLNode<K, V>>& node) noexcept {
        return node ? node->weight.load(std::memory_order_acquire) : 0;
    }

    // Rotate Left:
    /*     Before           After
           node (X)         newRoot (Y)   
            \                   /
            newRoot (Y)    node (X) 
            /                 \
            B                  B

    */

    std::shared_ptr<AVLNode<K, V>> rotateLeft(std::shared_ptr<AVLNode<K, V>> node) {
        if (!node) return node;
        std::shared_ptr<AVLNode<K, V>> newRoot = node->right.load(std::memory_order_acquire);
        if (!newRoot) return node;
        
        std::shared_ptr<AVLNode<K, V>> newRootLeft = newRoot->left.load(std::memory_order_acquire);
        
        while (!node->right.compare_exchange_weak(newRoot, newRootLeft, 
               std::memory_order_release, std::memory_order_relaxed)) {
            newRoot = node->right.load(std::memory_order_acquire);
            if (!newRoot) return node;
            newRootLeft = newRoot->left.load(std::memory_order_acquire);
        }
        
        while (!newRoot->left.compare_exchange_weak(newRootLeft, node, 
               std::memory_order_release, std::memory_order_relaxed)) {
            newRootLeft = newRoot->left.load(std::memory_order_acquire);
        }

        node->update();
        newRoot->update();
        return newRoot;
    } 

    // Rotate Right:
    /*     Before       After
           node (X)  newroot (Y)   
            /              \
       newRoot (Y)      node (X) 
          \                /
           B              B

    */

    std::shared_ptr<AVLNode<K, V>> rotateRight(std::shared_ptr<AVLNode<K, V>> node) {
        if (!node) return node;
        std::shared_ptr<AVLNode<K, V>> newRoot = node->left.load(std::memory_order_acquire);
        if (!newRoot) return node;

        std::shared_ptr<AVLNode<K, V>> newRootRight = newRoot->right.load(std::memory_order_acquire);
        
        while (!node->left.compare_exchange_weak(newRoot, newRootRight, 
               std::memory_order_release, std::memory_order_relaxed)) {
            newRoot = node->left.load(std::memory_order_acquire);
            if (!newRoot) return node;
            newRootRight = newRoot->right.load(std::memory_order_acquire);
        }
        
        while (!newRoot->right.compare_exchange_weak(newRootRight, node, 
               std::memory_order_release, std::memory_order_relaxed)) {
            newRootRight = newRoot->right.load(std::memory_order_acquire);
        }

        node->update();
        newRoot->update();
        return newRoot;
    }
    
    std::shared_ptr<AVLNode<K, V>> fixLeft(std::shared_ptr<AVLNode<K, V>> node) {
        if (!node) return node;
        std::shared_ptr<AVLNode<K, V>> leftNode = node->left.load(std::memory_order_acquire);
        if (!leftNode) return node;
    
        std::shared_ptr<AVLNode<K, V>> leftRight = leftNode->right.load(std::memory_order_acquire);
        std::shared_ptr<AVLNode<K, V>> leftLeft = leftNode->left.load(std::memory_order_acquire);
    
        if (getDepth(leftRight) >= getDepth(leftLeft)) {
            std::shared_ptr<AVLNode<K, V>> newLeft = rotateLeft(leftNode);
            
            while (!node->left.compare_exchange_weak(leftNode, newLeft, 
                   std::memory_order_release, std::memory_order_relaxed)) {
                leftNode = node->left.load(std::memory_order_acquire);
                if (!leftNode) return node;
                
                leftRight = leftNode->right.load(std::memory_order_acquire);
                leftLeft = leftNode->left.load(std::memory_order_acquire);
                
                if (getDepth(leftRight) >= getDepth(leftLeft)) {
                    newLeft = rotateLeft(leftNode);
                } else {
                    break;
                }
            }
        }
    
        return rotateRight(node);
    }

    std::shared_ptr<AVLNode<K, V>> fixRight(std::shared_ptr<AVLNode<K, V>> node) {
        if (!node) return node;
        std::shared_ptr<AVLNode<K, V>> rightNode = node->right.load(std::memory_order_acquire);
        if (!rightNode) return node;

        std::shared_ptr<AVLNode<K, V>> rightLeft = rightNode->left.load(std::memory_order_acquire);
        std::shared_ptr<AVLNode<K, V>> rightRight = rightNode->right.load(std::memory_order_acquire);         
        
        if (getDepth(rightLeft) >= getDepth(rightRight)) {
            std::shared_ptr<AVLNode<K, V>> newRight = rotateRight(rightNode);
            
            while (!node->right.compare_exchange_weak(rightNode, newRight, 
                   std::memory_order_release, std::memory_order_relaxed)) {
                rightNode = node->right.load(std::memory_order_acquire);
                if (!rightNode) return node;
                
                rightLeft = rightNode->left.load(std::memory_order_acquire);
                rightRight = rightNode->right.load(std::memory_order_acquire);
                
                if (getDepth(rightLeft) >= getDepth(rightRight)) {
                    newRight = rotateRight(rightNode);
                } else {
                    break;
                }
            }
        }

        return rotateLeft(node);
    }
};

template<typename K, typename V>
struct AVLNode {
public:
    AVLNode(const K& k, const V& v) : key(k), value(v), depth(1), weight(1), left(nullptr), right(nullptr) {}

    virtual ~AVLNode() = default;

    AVLNode(const AVLNode&) = delete;
    AVLNode& operator= (const AVLNode&) = delete;

    AVLNode(AVLNode&&) noexcept = default;
    AVLNode& operator= (AVLNode&&) noexcept = default;

    const K& get_key() const { return key; }
    V get_value() const { return value.load(std::memory_order_acquire); }
    void set_value(const V& v) { value.store(v, std::memory_order_release); }

    void update() {
        auto leftNode = left.load(std::memory_order_acquire);
        auto rightNode = right.load(std::memory_order_acquire);
        
        uint32_t leftDepth = leftNode ? leftNode->depth.load(std::memory_order_acquire) : 0;
        uint32_t rightDepth = rightNode ? rightNode->depth.load(std::memory_order_acquire) : 0;
        depth.store(1 + std::max(leftDepth, rightDepth), std::memory_order_release);

        uint32_t leftWeight = leftNode ? leftNode->weight.load(std::memory_order_acquire) : 0;
        uint32_t rightWeight = rightNode ? rightNode->weight.load(std::memory_order_acquire) : 0;
        weight.store(1 + leftWeight + rightWeight, std::memory_order_release);
    }


    K key;
    std::atomic<V> value;
    std::atomic<uint32_t> depth {1};
    std::atomic<uint32_t> weight {1};
    
    std::atomic<std::shared_ptr<AVLNode<K, V>>> left;
    std::atomic<std::shared_ptr<AVLNode<K, V>>> right;

    friend class AVLTree<K, V>;
};



#endif 
