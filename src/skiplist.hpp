#pragma once
#include <iostream>
#include <vector>
#include <random>
#include <memory>
#include <mutex>
#include <limits>
#include <unordered_set>
#include <optional>
#include <thread>
#include <sstream>
#include <gtest/gtest.h>

inline std::string thread_id_str() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

template <typename Key, typename Value>
class SkipList {
private:
    static constexpr int MAX_LEVEL = 16;
    static constexpr float PROBABILITY = 0.5f;

    struct Node {
        Key key;
        Value value;
        std::vector<std::atomic<Node*>> next;
        std::atomic<bool> marked;
        std::mutex node_lock;

        Node(const Key& k, const Value& v, int level)
            : key(k), value(v), next(level), marked(false) {
            for (int i = 0; i < level; ++i) {
                next[i].store(nullptr, std::memory_order_relaxed);
            }
        }
    };

    std::unique_ptr<Node> head;
    std::unique_ptr<Node> tail;
    std::atomic<int> currentLevel;
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist;


    int randomLevel() {
        int lvl = 1;
        while (dist(gen) < PROBABILITY && lvl < MAX_LEVEL)
            ++lvl;
        return lvl;
    }

    public:
        SkipList()
        : head(std::make_unique<Node>(std::numeric_limits<Key>::min(), Value{}, MAX_LEVEL)),
        tail(std::make_unique<Node>(std::numeric_limits<Key>::max(), Value{}, MAX_LEVEL)),
        currentLevel(1),
        gen(std::random_device{}()),
        dist(0.0f, 1.0f)
    {
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->next[i].store(tail.get(), std::memory_order_relaxed);
            // 👇 This connects tail permanently (you must not delete tail manually)
        }
    }


    ~SkipList() {
        Node* node = head->next[0].load();
        while (node && node != tail.get()) {
            Node* next = node->next[0].load();
            delete node;
            node = next;
        }
    }
    
    

    bool find(const Key& key, std::vector<Node*>& preds, std::vector<Node*>& succs) {
        bool valid = true;
        Node* pred = head.get();
        for (int level = currentLevel - 1; level >= 0; --level) {
            Node* curr = pred->next[level].load(std::memory_order_acquire);
            while (true) {
                if (curr->marked.load(std::memory_order_acquire)) {
                    curr = curr->next[level].load(std::memory_order_acquire);
                    continue;
                }
                if (curr->key < key) {
                    pred = curr;
                    curr = curr->next[level].load(std::memory_order_acquire);
                } else {
                    break;
                }
            }
            preds[level] = pred;
            succs[level] = curr;
        }
        return true;
    }

    bool contains(const Key& key) const {
        Node* curr = head.get();
        for (int level = currentLevel - 1; level >= 0; --level) {
            while (true) {
                Node* next = curr->next[level].load(std::memory_order_acquire);
                if (!next) break; // ☠️ null protection
                if (next->key < key) {
                    curr = next;
                } else {
                    break;
                }
            }
        }
    
        Node* next = curr->next[0].load(std::memory_order_acquire);
        return (next && next->key == key && !next->marked.load(std::memory_order_acquire));
    }
    


    [[nodiscard]] bool add(const Key& key, const Value& value) {
        int topLevel = randomLevel();
        std::vector<Node*> preds(MAX_LEVEL), succs(MAX_LEVEL);
    
        while (true) {
            find(key, preds, succs);
    
            Node* found = succs[0];
            if (found->key == key && !found->marked.load(std::memory_order_acquire)) {
                return false;  // already exists
            }
    
            Node* newNode = new Node(key, value, topLevel);
            for (int level = 0; level < topLevel; ++level)
                newNode->next[level].store(succs[level], std::memory_order_relaxed);
    
            // 🔐 Lock & validate
            std::array<std::unique_lock<std::mutex>, MAX_LEVEL> predLocks;
            for (int level = 0; level < topLevel; ++level)
                predLocks[level] = std::unique_lock<std::mutex>(preds[level]->node_lock);
    
            bool valid = true;
            for (int level = 0; level < topLevel; ++level) {
                if (preds[level]->next[level].load(std::memory_order_acquire) != succs[level]) {
                    valid = false;
                    break;
                }
            }
    
            if (!valid) {
                delete newNode;
                continue;
            }
    
            // 🔄 Insert node
            for (int level = 0; level < topLevel; ++level)
                preds[level]->next[level].store(newNode, std::memory_order_release);
    
            // ⬆️ Update list level if needed
            int curLevel = currentLevel.load(std::memory_order_relaxed);
            while (topLevel > curLevel && !currentLevel.compare_exchange_weak(curLevel, topLevel)) {}
    
            return true;
        }
    }
    

    [[nodiscard]] bool remove(const Key& key) {
        std::vector<Node*> preds(MAX_LEVEL), succs(MAX_LEVEL);
        Node* victim = nullptr;
        bool isMarked = false;
    
        while (true) {
            find(key, preds, succs);
            victim = succs[0];
    
            if (victim->key != key)
                return false;
    
            // 🔒 Mark victim logically deleted
            if (!isMarked) {
                std::lock_guard<std::mutex> lock(victim->node_lock);
                if (victim->marked.load(std::memory_order_acquire)) return false;
                victim->marked.store(true, std::memory_order_release);
                isMarked = true;
            }
    
            // 🔐 Lock preds and validate
            std::array<std::unique_lock<std::mutex>, MAX_LEVEL> predLocks;
            bool valid = true;
            for (int level = 0; level < victim->next.size(); ++level)
                predLocks[level] = std::unique_lock<std::mutex>(preds[level]->node_lock);
    
            for (int level = 0; level < victim->next.size(); ++level) {
                if (preds[level]->next[level].load(std::memory_order_acquire) != victim) {
                    valid = false;
                    break;
                }
            }
    
            if (!valid)
                continue;
    
            // 🧹 Physically remove node
            for (int level = victim->next.size() - 1; level >= 0; --level)
                preds[level]->next[level].store(victim->next[level].load(std::memory_order_acquire), std::memory_order_release);
    
            delete victim;
            return true;
        }
    }
    


    
};
