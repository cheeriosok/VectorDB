#ifndef AVL_TREE_HPP
#define AVL_TREE_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <algorithm>

template<typename T>
class AVLNode {
public:
    explicit AVLNode() noexcept : 
        depth(1), 
        count(1), 
        left(nullptr), 
        right(nullptr), 
        parent(nullptr) {}
        
    virtual ~AVLNode() = default;

    AVLNode(const AVLNode&) = delete;
    AVLNode& operator=(const AVLNode&) = delete;
    
    AVLNode(AVLNode&&) noexcept = default;
    AVLNode& operator=(AVLNode&&) noexcept = default;

protected:
    uint32_t depth{1};
    uint32_t count{1};
    std::unique_ptr<AVLNode> left;
    std::unique_ptr<AVLNode> right;
    AVLNode* parent{nullptr};

    friend class AVLTree<T>;
};

template<typename T>
class AVLTree {
public:
    AVLTree() = default;
    ~AVLTree() = default;

    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    AVLTree(AVLTree&&) noexcept = default;
    AVLTree& operator=(AVLTree&&) noexcept = default;

    AVLNode<T>* fix(AVLNode<T>* node) {
        while (true) {
            updateNode(node);
            uint32_t l = getDepth(node->left.get());
            uint32_t r = getDepth(node->right.get());

            AVLNode<T>** from = nullptr;
            if (node->parent) {
                from = (node->parent->left.get() == node)
                    ? &node->parent->left : &node->parent->right;
            }

            if (l == r + 2) {
                auto temp = fixLeft(std::unique_ptr<AVLNode<T>>(node));
                node = temp.get();
                if (from) {
                    *from = std::move(temp);
                }
            } else if (l + 2 == r) {
                auto temp = fixRight(std::unique_ptr<AVLNode<T>>(node));
                node = temp.get();
                if (from) {
                    *from = std::move(temp);
                }
            }

            if (!from) {
                return node;
            }
            node = node->parent;
        }
    }

    AVLNode<T>* remove(AVLNode<T>* node) {
        if (!node->right) {
            AVLNode<T>* parent = node->parent;
            if (node->left) {
                node->left->parent = parent;
            }
            if (parent) {
                if (parent->left.get() == node) {
                    parent->left = std::move(node->left);
                } else {
                    parent->right = std::move(node->left);
                }
                return fix(parent);
            }
            return node->left.get();
        } else {
            AVLNode<T>* victim = node->right.get();
            while (victim->left) {
                victim = victim->left.get();
            }
            AVLNode<T>* root = remove(victim);
            
            std::swap(node->depth, victim->depth);
            std::swap(node->count, victim->count);
            std::swap(node->left, victim->left);
            std::swap(node->right, victim->right);
            std::swap(node->parent, victim->parent);
            
            if (node->left) node->left->parent = node;
            if (node->right) node->right->parent = node;
            
            return root;
        }
    }

    AVLNode<T>* offset(AVLNode<T>* node, int64_t offset) {
        int64_t pos = 0;
        
        while (offset != pos) {
            if (pos < offset && pos + getCount(node->right.get()) >= offset) {
                node = node->right.get();
                pos += getCount(node->left.get()) + 1;
            } else if (pos > offset && pos - getCount(node->left.get()) <= offset) {
                node = node->left.get();
                pos -= getCount(node->right.get()) + 1;
            } else {
                AVLNode<T>* parent = node->parent;
                if (!parent) {
                    return nullptr;
                }
                if (parent->right.get() == node) {
                    pos -= getCount(node->left.get()) + 1;
                } else {
                    pos += getCount(node->right.get()) + 1;
                }
                node = parent;
            }
        }
        return node;
    }

    static uint32_t getDepth(const AVLNode<T>* node) noexcept {
        return node ? node->depth : 0;
    }

    static uint32_t getCount(const AVLNode<T>* node) noexcept {
        return node ? node->count : 0;
    }
    
private:
    std::unique_ptr<AVLNode<T>> root_;
    
    static std::unique_ptr<AVLNode<T>> rotateLeft(std::unique_ptr<AVLNode<T>> node) {
        auto newRoot = std::move(node->right);
        auto* newRootPtr = newRoot.get();
        
        newRootPtr->parent = node->parent;
        
        if (newRootPtr->left) {
            newRootPtr->left->parent = node.get();
        }
        
        node->right = std::move(newRootPtr->left);
        node->parent = newRootPtr;
        
        newRootPtr->left = std::move(node);
        
        updateNode(newRootPtr->left.get());
        updateNode(newRootPtr);
        
        return newRoot;
    }

    static std::unique_ptr<AVLNode<T>> rotateRight(std::unique_ptr<AVLNode<T>> node) {
        auto newRoot = std::move(node->left);
        auto* newRootPtr = newRoot.get();
        
        newRootPtr->parent = node->parent;
        
        if (newRootPtr->right) {
            newRootPtr->right->parent = node.get();
        }
        
        node->left = std::move(newRootPtr->right);
        node->parent = newRootPtr;
        
        newRootPtr->right = std::move(node);
        
        updateNode(newRootPtr->right.get());
        updateNode(newRootPtr);
        
        return newRoot;
    }

    static void updateNode(AVLNode<T>* node) noexcept {
        if (node) {
            node->depth = 1 + std::max(getDepth(node->left.get()), 
                                     getDepth(node->right.get()));
            node->count = 1 + getCount(node->left.get()) + 
                             getCount(node->right.get());
        }
    }

    static std::unique_ptr<AVLNode<T>> fixLeft(std::unique_ptr<AVLNode<T>> root) {
        if (getDepth(root->left->left.get()) < getDepth(root->left->right.get())) {
            root->left = rotateLeft(std::move(root->left));
        }
        return rotateRight(std::move(root));
    }

    static std::unique_ptr<AVLNode<T>> fixRight(std::unique_ptr<AVLNode<T>> root) {
        if (getDepth(root->right->right.get()) < getDepth(root->right->left.get())) {
            root->right = rotateRight(std::move(root->right));
        }
        return rotateLeft(std::move(root));
    }
};

#endif // AVL_TREE_HPP