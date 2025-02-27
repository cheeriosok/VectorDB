#ifndef ZSET_HPP
#define ZSET_HPP

#include <memory>
#include <string_view>
#include <cstring>
#include <cassert>
#include "avl_tree.hpp"
#include "hash_table.hpp"

namespace ds {

// Node structure that preserves the memory layout
class alignas(max_align_t) ZNode {
    friend class ZSet;

private:
    AVLNode tree_;
    HNode hash_;
    double score_;
    size_t name_len_;
    char name_[1];  // Flexible array member simulation in C++

    // Private constructor - only created through factory function
    ZNode(double score, size_t len) 
        : score_(score), name_len_(len) {}

public:
    static ZNode* create(std::string_view name, double score) {
        void* mem = ::operator new(sizeof(ZNode) + name.length());
        ZNode* node = new(mem) ZNode(score, name.length());
        node->tree_ = AVLNode{};
        node->hash_ = HNode{};
        node->hash_.hcode = str_hash(
            reinterpret_cast<const uint8_t*>(name.data()), 
            name.length()
        );
        std::memcpy(node->name_, name.data(), name.length());
        return node;
    }

    static void destroy(ZNode* node) {
        node->~ZNode();
        ::operator delete(node);
    }

    [[nodiscard]] std::string_view name() const {
        return std::string_view(name_, name_len_);
    }

    [[nodiscard]] double score() const { return score_; }

    ZNode(const ZNode&) = delete;
    ZNode& operator=(const ZNode&) = delete;
};

class ZSet {
public:
    ZSet() = default;
    ~ZSet() { dispose(); }

    ZSet(const ZSet&) = delete;
    ZSet& operator=(const ZSet&) = delete;

    bool add(std::string_view name, double score);
    ZNode* lookup(std::string_view name) const;
    ZNode* pop(std::string_view name);
    ZNode* query(double score, std::string_view name, int64_t offset) const;

    void dispose() {
        if (root_) {
            dispose_tree(root_);
            root_ = nullptr;
        }
        hmap_.dispose();
    }

private:
    AVLNode* root_{nullptr};
    HMap hmap_{};

    static void dispose_tree(AVLNode* node);
    void update_score(ZNode* node, double new_score);
    void tree_add(ZNode* node);

    static bool less(const AVLNode* lhs, const AVLNode* rhs);
    static bool less(const AVLNode* lhs, double score, std::string_view name);
};

inline bool ZSet::less(const AVLNode* lhs, const AVLNode* rhs) {
    auto* zl = container_of(lhs, ZNode, tree_);
    auto* zr = container_of(rhs, ZNode, tree_);

    if (zl->score_ != zr->score_) {
        return zl->score_ < zr->score_;
    }
    return zl->name() < zr->name();
}

inline bool ZSet::less(const AVLNode* lhs, double score, std::string_view name) {
    auto* zl = container_of(lhs, ZNode, tree_);
    if (zl->score_ != score) {
        return zl->score_ < score;
    }
    return zl->name() < name;
}

inline bool hcmp(const HNode* node, const HNode* key) {
    if (node->hcode != key->hcode) return false;

    auto* znode = container_of(node, ZNode, hash_);
    auto* hkey = container_of(key, HKey, node);

    return znode->name() == hkey->name;
}

void ZSet::tree_add(ZNode* node) {
    if (!root_) {
        root_ = &node->tree_;
        return;
    }

    AVLNode* cur = root_;
    while (true) {
        AVLNode** from = less(&node->tree_, cur) ? 
            &cur->left : &cur->right;
        if (!*from) {
            *from = &node->tree_;
            node->tree_.parent = cur;
            root_ = avl_fix(&node->tree_);
            break;
        }
        cur = *from;
    }
}

void ZSet::update_score(ZNode* node, double new_score) {
    if (node->score_ == new_score) {
        return;
    }
    root_ = avl_del(&node->tree_);
    node->score_ = new_score;
    avl_init(&node->tree_);
    tree_add(node);
}

bool ZSet::add(std::string_view name, double score) {
    if (ZNode* node = lookup(name)) {
        update_score(node, score);
        return false;
    }

    ZNode* node = ZNode::create(name, score);
    assert(node);
    hmap_.insert(&node->hash_);
    tree_add(node);
    return true;
}

ZNode* ZSet::lookup(std::string_view name) const {
    if (!root_) {
        return nullptr;
    }

    HKey key(name);
    if (HNode* found = hm_lookup(&hmap_, &key.node, hcmp)) {
        return container_of(found, ZNode, hash_);
    }
    return nullptr;
}

ZNode* ZSet::pop(std::string_view name) {
    if (!root_) {
        return nullptr;
    }

    HKey key(name);
    if (HNode* found = hm_pop(&hmap_, &key.node, hcmp)) {
        ZNode* node = container_of(found, ZNode, hash_);
        root_ = avl_del(&node->tree_);
        return node;
    }
    return nullptr;
}

ZNode* ZSet::query(double score, std::string_view name, int64_t offset) const {
    AVLNode* found = nullptr;
    AVLNode* cur = root_;

    while (cur) {
        if (less(cur, score, name)) {
            cur = cur->right;
        } else {
            found = cur;
            cur = cur->left;
        }
    }

    if (found) {
        found = avl_offset(found, offset);
    }
    return found ? container_of(found, ZNode, tree_) : nullptr;
}

void ZSet::dispose_tree(AVLNode* node) {
    if (!node) {
        return;
    }
    dispose_tree(node->left);
    dispose_tree(node->right);
    ZNode::destroy(container_of(node, ZNode, tree_));
}

} // namespace ds

#endif // ZSET_HPP
