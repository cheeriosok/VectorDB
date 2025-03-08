#ifndef AVL_TREE_HPP
#define AVL_TREE_HPP

// Our node class contains two unique pointers to left and right, a raw pointer to parent, and depth + weight values for rebalancing.
// depth = (how far down the tree we are), weight = how many nodes in its subtree (nodes below this node). AVLTree will modify these 2
// so I will call it a friend class.

#include <memory>    
#include <algorithm> 
#include <cstdint>   
#include <utility>   
#include <cstddef>

template<typename T>
class AVLNode {
public:
    AVLNode () noexcept :
        depth(1);
        weight(1):
        left(nullptr);
        right(nullptr);
        parent(nullptr); {}

    virtual ~AVLNode() = default;

    AVLNode(const AVLNode&) = delete;
    AVLNode& operator= (const AVLNode&) = delete;

    AVLNode(AVLNode&&) noexcept = default;
    AVLNode& operator= (AVLNode&&) noexcept = default;

protected:
    uint32_t depth {1};
    uint32_t weight {1};
    std::unique_ptr<AVLNode> left;
    std::unique_ptr<AVLNode> right;
    AVLNode* parent {nullptr};

    friend class AVLTree<T>;
};

// Our Tree contains the root_ node, fixLeft, fixRight, RotateLeft, RotateRight, fix (which calls fixLeft and fixRight), offset, remove and insert methods for our AVL Tree.

template<typename T>
class AVLTree {
public:
    AVLTree() = default;
    ~AVLTree() = default;

    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    AVLTree(AVLTree&&) = default;
    AVLTree& operator= (AVLTree&&) = default;

public:
    AVLNode<T>* fix(AVLNode<T>* node) {
        while (true) {
            updateNode(node); // Update weight and depth of each node
            // get Depths of children
            uint32_t leftDepth = getDepth(node->left.get()); 
            uint32_t rightDepth = getDepth(node->right.get());
            // from will point to another unique pointer. If node is on parent's left side then from is parents left uniqueptr
            // if node is on parents right side then """. after rotation, we need to update the parent's pointer to point to the new root of subtree
            AVLNode<T>** from = nullptr;
            if (node->parent) {
                from = (node->parent->left.get() == node)
                    ? &node->parent->left : &node->parent->right;
            }
            // if leftDepth is 2 over rightDepth then leftDepth is too heavy. Call fixLeft on root. 
            if (leftDepth == rightDepth + 2) {
                auto temp = fixLeft(std::unique_ptr<AVLNode<T>>(node)); // Call fixleft to fix our left with node as the parent.
                node = temp.get(); // get raw pointer to this new root 
                /*So here, if we have a parent, **from**, (when dereferenced), will correspond to a unique pointer of the parent of the old unrotated subtree 
                (either left if node was on left of parent or right if on right) and when assigning to temp is essentially reinitialziing to now point to new node of rotated tree */
                if (from) { 
                    *from = std::move(temp);
                }
            // if leftDepth is 2 over lefttDepth then rightDepth is too heavy. Call fixRight on root. 
            } else if (leftDepth + 2 == rightDepth) {
                auto temp = fixRight(std::unique_ptr<AVLNode<T>>(node));
                node = temp.get();
                if (from) {
                    *from = std::move(temp);
                }
            }
            // 
            if (!from) {
                return node;
            }
            node = node->parent;
        }
    }
    
    // Fairly complicated function using recursive calls to swap the minimum element of a node (i.e downstream a subtree) with current node
    // If node has no right child, then it readjusts parent and left nodes between both left and parent nodes respectively to exclude node
    // If node has a right child, then recursive calls to get the minimum element downstream the node of interests are called and all
    // vaues will be swapped once the minimum is found. 

    AVLNode<T>* remove(AVLNode<T>* node) {
        if (!node->right) { // Node has no right child!
            AVLNode<T>* parent = node->parent; // Copy of nodes parent pointer
            if (node->left) { // has a left child?
                node->left->parent = parent; // make left childs parent pointer to point to root parent (skipping root entirely)
            }
            if (parent) { // if !root
                if (parent->left.get() == node) { // If our node is on parent's left
                    parent->left = std::move(node->left); // move node's left to paren't left (instead of pointing to node)
                } else { // If our node is on parent's right
                    parent->right = std::move(node->left); // move node's left to parents right (instead of pointing to node)
                }
                return fix(parent); // rebalance parent if necessary 
            }
            return node->left.get(); // Our right doesn't exist, so we return left (taking place of node)
        } else {
            AVLNode<T>* victim = node->right.get(); // node->right exists, and victim is a raw pointer to right child.
            while (victim->left) { // traverse as left of victim as you can
                victim = victim->left.get();
            }
            AVLNode<T>* root = remove(victim);
            
            std::swap(node->depth, victim->depth);
            std::swap(node->weight, victim->weight);
            std::swap(node->left, victim->left);
            std::swap(node->right, victim->right);
            std::swap(node->parent, victim->parent);
            
            if (node->left) node->left->parent = node;
            if (node->right) node->right->parent = node;
            
            return root;
        }
    }
    // finds node at a position in an in-order traversal. node corresponds to start node to search from
    // offset corresponds to target poisition we need to find. 
    AVLNode<T>* offset(AVLNode<T>* node, int64_t target_pos) {
        // keep track of our current position in the in-order traversal
        int64_t pos = 0;
        // loop until we find the target position
        while (target_pos != pos) {
            // Case 1: Target is to our right
            // if we're before our target AND it's reachable through right subtree
            if (pos < target_pos && pos + getWeight(node->right.get()) >= target_pos) {
                node = node->right.get(); // mpve to right child
                pos += getWeight(node->left.get()) + 1;
            } // Case 2: Target is to our left
            // if we're before our target AND it's reachable through left subtree 
            else if (pos > target_pos && pos - getWeight(node->left.get()) <= target_pos) {
                node = node->left.get(); // move to left child
                pos -= getWeight(node->right.get()) + 1;
            } 
            // Case 2: move up tree
            else {
                AVLNode<T>* parent = node->parent;
                if (!parent) {
                    return nullptr; // target position not found in tree
                }
                // moving up from right child: subtract left subtree + node
                if (parent->right.get() == node) {
                    pos -= getWeight(node->left.get()) + 1;
                } 
                // moving up from left child: add right subtree + node
                else {
                    pos += getWeight(node->right.get()) + 1;
                }
                node = parent; // Move up to parent
            }
        }
        return node;
    }

    // helper func to get depth. if node == nullptr, then return 0
    static uint32_t getDepth(const AVLNode<T>* node) noexcept {
        return node ? node->depth : 0;
    }
    // helper func to get depth. if node == nullptr, then return 0
    static uint32_t getWeight(const AVLNode<T>* node) noexcept {
        return node ? node->weight : 0;
    }
    
private:
    std::unique_ptr<AVLNode<T>> root_;

    /*   1
          \
           2
          /
         X
    */
    static std::unique_ptr<AVLNode<T>> rotateLeft(std::unique_ptr<AVLNode<T>> node) {
        auto newRoot = std::move(node->right); // newRoot is a unique pointer holding the right child (2);
        auto* newRootPtr = newRoot.get(); // newRootPtr is a raw pointer to this same node.
        
        newRootPtr->parent = node->parent;  // Now, we're going to reinitialize 1's raw poiinter to parent to be 2's raw ptr to parent
        
        if (newRootPtr->left) { // Does our '2' have a left child X? 
            newRootPtr->left->parent = node.get();  // If it does, the left child's (X's) parent rawptr will now point to our root node (1).
        }
        
        node->right = std::move(newRootPtr->left); // now,  2's left unique pointer is owned by 1's right pointer (1 no longer points to 2 but to 2's left child) 
        node->parent = newRootPtr; // and now node's parent pointer points to 2.
        
        newRootPtr->left = std::move(node); // 2's left pointer now points to 1!
    
    /*           2 (2's parent points to 1's old parent [line 169], 2's left now points to root node 1 (line 178), 2's right is unchanged] 
                /
               1 (1's parent now points to 2 [line 176], 1's left is unaffected, 1's right points to 2's old left.)
                \
                 X
    */
        updateNode(newRootPtr->left.get()); //update old root node (1)
        updateNode(newRootPtr); // update new root node (2)
        
        return newRoot; // return unique pointer to new root node (2)!
    }
        /*   2
            /
           1  
            \  
             X   
    */
    static std::unique_ptr<AVLNode<T>> rotateRight(std::unique_ptr<AVLNode<T>> node) {
        auto newRoot = std::move(node->left); // newRoot is a unqiue pointer to 1 from 2, will use it to return 1.
        auto* newRootPtr = newRoot.get();   // get raw pointer of newRoot 
        
        newRootPtr->parent = node->parent; // 1's parent now points to 2's parent.
        
        if (newRootPtr->right) { // Does 1 have a right child?
            newRootPtr->right->parent = node.get(); // If it does, then 1's right child's parent pointer now points to 2!
        }
        
        node->left = std::move(newRootPtr->right); // Node (2)'s left child now points to X
        node->parent = newRootPtr; // 2's parent is now 1!
        
        newRootPtr->right = std::move(node); // 1's right child now points to 2
        
        /*
             1  1's right ptr points to 2 [212], 1's left pointer is the exact same, 1's parent is 2's old parent [line 203]
              \
               2  // 2's left uniqueptr points to X [209]. 2's right pointer is unchanged, 2's parent is now 1 [210]
              /
             X 
        */

        updateNode(newRootPtr->right.get());
        updateNode(newRootPtr);
        
        return newRoot;
    }
    // updateNode simply updates the depth and weight of each node after a potential rotation.
    static void updateNode(AVLNode<T>* node) noexcept {
        if (node) {
            node->depth = 1 + std::max(getDepth(node->left.get()), 
                                     getDepth(node->right.get()));
            node->weight = 1 + getWeight(node->left.get()) + 
                             getWeight(node->right.get());
        }
    }

/* Ex1:Initial Tree:  After rotateRight:        Ex2: Initial Tree:         After rotateLeft:      After rotateRight:
          3                  2 (0)                 3                            3                        2                     
         /                   / \                   /                           /                        / \ 
       2                    1   3                 1                           2                        1   3
      /                                            \                         /
     1                                              2                       1
*/
    static std::unique_ptr<AVLNode<T>> fixLeft(std::unique_ptr<AVLNode<T>> root) {
        if (getDepth(root->left->left.get()) < getDepth(root->left->right.get())) {
            root->left = rotateLeft(std::move(root->left));
        }
        return rotateRight(std::move(root));
    }
/* Ex1: Initial Tree: After rotateLeft:    Ex2: Initial Tree:       After rotateRight:        After rotateLeft;
            3                 2                  3                          3                           2
             \               / \                  \                          \                         / \
              2             1   3                  1                          2                       1   3
               \                                   /                           \ 
                1                                 2                             1
*/
    static std::unique_ptr<AVLNode<T>> fixRight(std::unique_ptr<AVLNode<T>> root) {
        if (getDepth(root->right->right.get()) < getDepth(root->right->left.get())) {
            root->right = rotateRight(std::move(root->right));
        }
        return rotateLeft(std::move(root));
    }
};

#endif 