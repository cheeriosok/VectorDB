#ifndef AVL_TREE_HPP
#define AVL_TREE_HPP

#include <memory>    
#include <algorithm> 
#include <cstdint>   
#include <utility>   
#include <cstddef>

// forward dec. for our friend class 
template<typename K, typename V>
class AVLTree;

template<typename K, typename V>
class AVLNode;

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

    // `SET` 
    void set(const K& key, const V& value) {
        AVLNode<K, V>* node = search(key);
        if (node) {
            node->value = value; // Update existing key
        } else {
            root_ = insert(std::move(root_), key, value);
        }
    }
    
    // `GET` 
    std::optional<V> get(const K& key) const {
        AVLNode<K, V>* node = search(key);
        return node ? std::optional<V>(node->value) : std::nullopt;
    }
        
    // `DEL key` 
    void del(const K& key) {
        if (root_) {
            root_ = remove(std::move(root_), key);
        }
    }

    // `EXISTS key` 
    bool exists(const K& key) const {
        return search(key) != nullptr;
    }


private:
    // std::unique_ptr<AVLNode<T>> fix(std::unique_ptr<AVLNode<T>> node) {
    //     AVLNode<T>* current = node.get(); // raw pointer for traversal - its simply impossible to traverse with a raw parent and unique ptr children.
    //     // I can only think of using raw for this case. 
    //     while (current) {
    //             updateNode(current); // Update weight and depth of each node
    //             // get Depths of children
    //             uint32_t leftDepth = getDepth(current->left.get()); 
    //             uint32_t rightDepth = getDepth(current->right.get());
    //             // from will point to another unique pointer. If node is on parent's left side then from is parents left uniqueptr
    //             // if node is on parents right side then """. after rotation, we need to update the parent's pointer to point to the new root of subtree
    //             std::unique_ptr<AVLNode<T>>* from = nullptr;
    //             if (current->parent) {
    //                 if (current->parent->left && current->parent->left.get() == current) {
    //                     from = &current->parent->left;
    //                 } else if (current->parent->right && current->parent->right.get() == current) {
    //                     from = &current->parent->right;
    //                 }
    // }

    //             // if leftDepth is 2 over rightDepth then leftDepth is too heavy. Call fixLeft on root. 
    //             if (leftDepth == rightDepth + 2 && (from && *from)) {
    //                     auto temp = fixLeft(std::move(*from)); // Call fixleft to fix our left with node as the parent.
    //                     current = temp.get(); // get raw pointer to this new root 
    //                     /*So here, if we have a parent, **from**, (when dereferenced), will correspond to a unique pointer of the parent of the old unrotated subtree 
    //                     (either left if node was on left of parent or right if on right) and when assigning to temp is essentially reinitialziing to now point to new node of rotated tree */
    //                     *from = std::move(temp); // assign new subtree root
    //             }
    //             // if leftDepth is 2 over lefttDepth then rightDepth is too heavy. Call fixRight on root. 
    //             else if (leftDepth + 2 == rightDepth && (from && *from)) {
    //                 auto temp = fixRight(std::move(*from));
    //                 current = temp.get();
    //                 *from = std::move(temp);
    //             }
    //             current = current->parent;
    //         }
    //         return node;
    //     }
    std::unique_ptr<AVLNode<K, V>> fix(std::unique_ptr<AVLNode<K, V>>&& node) {
        if (!node) return nullptr;

        updateNode(node.get());

        uint32_t leftDepth = getDepth(node->left.get());
        uint32_t rightDepth = getDepth(node->right.get());

        if (leftDepth > rightDepth + 1) {
            return fixLeft(std::move(node));
        } else if (rightDepth > leftDepth + 1) {
            return fixRight(std::move(node));
        }

        return std::move(node);
    }
    
    
    
    // Fairly complicated function using recursive calls to swap the minimum element of a node (i.e downstream a subtree) with current node
    // If node has no right child, then it readjusts parent and left nodes between both left and parent nodes respectively to exclude node
    // If node has a right child, then recursive calls to get the minimum element downstream the node of interests are called and all
    // vaues will be swapped once the minimum is found. 

    std::unique_ptr<AVLNode<K, V>> remove(std::unique_ptr<AVLNode<K, V>> node, const K& key) {
        if (!node) return nullptr;

        if (key < node->key) {
            node->left = remove(std::move(node->left), key);
        } else if (key > node->key) {
            node->right = remove(std::move(node->right), key);
        } else {
            if (!node->left) return std::move(node->right);
            if (!node->right) return std::move(node->left);

            // Find in-order successor (smallest key in right subtree)
            AVLNode<K, V>* successor = node->right.get();
            while (successor->left) {
                successor = successor->left.get();
            }

            node->key = successor->key;
            node->value = successor->value;
            node->right = remove(std::move(node->right), successor->key);
        }

        return fix(std::move(node)); // Ensure AVL balancing
    }
    
    

    std::unique_ptr<AVLNode<K, V>> insert(std::unique_ptr<AVLNode<K, V>>&& node, const K& key, const V& value) {
        if (!node) return std::make_unique<AVLNode<K, V>>(key, value);

        if (key < node->key) {
            node->left = insert(std::move(node->left), key, value);
            node->left->parent = node.get();
        } else if (key > node->key) {
            node->right = insert(std::move(node->right), key, value);
            node->right->parent = node.get();
        } else {
            return std::move(node); // Duplicate key, no insertion
        }

        return fix(std::move(node)); // Ensure AVL balancing
    }
    
    
     
    
    AVLNode<K, V>* search(const K& key) const {
        AVLNode<K, V>* node = root_.get();
        while (node) {
            if (key == node->key) return node;
            node = (key < node->key) ? node->left.get() : node->right.get();
        }
        return nullptr;
    }
    
    
    // // finds node at a position in an in-order traversal. node corresponds to start node to search from
    // // offset corresponds to target poisition we need to find. 
    // AVLNode<T>* offset(AVLNode<T>* node, int64_t target_pos) {
    //     if (!node) return nullptr;
    //     // keep track of our current position in the in-order traversal
    //     int64_t pos = getWeight(node->left.get()); // start at correct in-order position
    //     // loop until we find the target position
    //     while (target_pos != pos) {
    //         if (node == nullptr) return nullptr; 
    //         // Case 1: Target is to our right
    //         // if we're before our target AND it's reachable through right subtree
    //         if (pos < target_pos && node->right) {
    //             node = node->right.get(); // mpve to right child
    //             pos += getWeight(node->left.get()) + 1;
    //         } // Case 2: Target is to our left
    //         // if we're before our target AND it's reachable through left subtree 
    //         else if (target_pos < pos && node->left) { // ensure correct traversal logic
    //             node = node->left.get();
    //             pos -= getWeight(node->right.get()) + 1;
    //         }
    //         // Case 3: move up tree
    //         else {
    //             AVLNode<T>* parent = node->parent;
    //             if (!parent) {
    //                 return nullptr; // target position not found in tree
    //             }
    //             // moving up from right child: subtract left subtree + node
    //             if (parent->right.get() == node) {
    //                 pos -= getWeight(node->left.get()) + 1; // move up from right child
    //             } else {
    //                 pos += getWeight(parent->left.get()) + 1; // move up from left child
    //             }
                
    //             node = parent; // Move up to parent
    //         }
    //     }
    //     return node;
    // }

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

    if (getDepth(node->right->left.get()) > getDepth(node->right->right.get())) {
        node->right = rotateRight(std::move(node->right));
    }

    return rotateLeft(std::move(node));
}


    std::unique_ptr<AVLNode<K,V>> root_;
};


// Our node class contains two unique pointers to left and right, a raw pointer to parent, and depth + weight values for rebalancing.
// depth = (how far down the tree we are), weight = how many nodes in its subtree (nodes below this node). AVLTree will modify these 2
// so I will call it a friend class.

template<typename K, typename V>
class AVLNode {
public:
    AVLNode(const K& k, const V& v) : key(k), value(v) {}
    
    virtual ~AVLNode() = default;

    AVLNode(const AVLNode&) = delete;
    AVLNode& operator= (const AVLNode&) = delete;

    AVLNode(AVLNode&&) noexcept = default;
    AVLNode& operator= (AVLNode&&) noexcept = default;

    const std::string& get_key() const { return key; }
    double get_value() const { return value; }
    void set_value(double v) { value = v; }

private:
    K key;      
    V value;
    uint32_t depth {1};
    uint32_t weight {1};
    std::unique_ptr<AVLNode<K, V>> left;
    std::unique_ptr<AVLNode<K, V>> right;
    AVLNode<K, V>* parent {nullptr};

    friend class AVLTree<K,V>;
};



#endif 