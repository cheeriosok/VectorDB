#ifndef ZSET_HPP
#define ZSET_HPP

#include <memory>
#include <string_view>
#include "avl_tree.hpp"
#include "hash_table.hpp"

namespace ds {

// Forward declarations
class ZSet;

// Node structure that preserves the memory layout
class alignas(max_align_t) ZNode {
    friend class ZSet;
    
private:
    AVLNode tree_;
    HNode hash_;
    double score_;
    size_t name_len_;
    // Flexible array member simulation in C++
    char name_[1];  // Actually variable length

    // Private constructor - only created through factory function
    ZNode(double score, size_t len) 
        : score_(score), name_len_(len) {}

public:
    // Custom allocation to handle flexible array member
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

    // Accessors
    [[nodiscard]] std::string_view name() const {
        return std::string_view(name_, name_len_);
    }
    [[nodiscard]] double score() const { return score_; }

    // No copy/move
    ZNode(const ZNode&) = delete;
    ZNode& operator=(const ZNode&) = delete;
};

class ZSet {
public:
    ZSet() = default;
    ~ZSet() { dispose(); }

    // No copy/move
    ZSet(const ZSet&) = delete;
    ZSet& operator=(const ZSet&) = delete;

    // Core operations
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
    
    // Comparison helpers
    static bool less(const AVLNode* lhs, const AVLNode* rhs);
    static bool less(const AVLNode* lhs, double score, std::string_view name);
};

// Implementation of key comparison functions
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

// Helper for hash comparisons
struct HKey {
    HNode node;
    std::string_view name;

    explicit HKey(std::string_view n) 
        : name(n) {
        node.hcode = str_hash(
            reinterpret_cast<const uint8_t*>(name.data()), 
            name.length()
        );
    }
};

inline bool hcmp(const HNode* node, const HNode* key) {
    if (node->hcode != key->hcode) return false;
    
    auto* znode = container_of(node, ZNode, hash_);
    auto* hkey = container_of(key, HKey, node);
    
    return znode->name() == hkey->name;
}

} // namespace ds

#endif // ZSET_HPP