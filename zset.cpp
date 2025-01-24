#include "zset.hpp"
#include <cassert>
#include <cstring>

namespace ds {

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
    assert(node);  // For compatibility with original implementation
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
            found = cur;    // candidate
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