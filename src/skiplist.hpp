#pragma once
#include <iostream>
#include <vector>
#include <random>
#include <memory>
#include <mutex>
#include <limits>
#include <optional>
#include <unordered_set>

// Thread-compatible SkipList using unique_ptr for safe ownership

template <typename Key, typename Value>
class SkipList {
    static constexpr int MAX_LEVEL = 16;
    static constexpr float PROBABILITY = 0.5f;

    struct Node {
        Key key;
        Value value;
        std::vector<std::unique_ptr<Node>> forward;
        std::vector<Node*> forward_raw;
        std::mutex node_lock;

        Node(const Key& k, const Value& v, int level)
        : key(k), value(v), forward(MAX_LEVEL), forward_raw(MAX_LEVEL, nullptr) {}
    
    };

    std::unique_ptr<Node> head;
    std::atomic<int> currentLevel;
    std::mutex global_lock;

    std::mt19937 gen;
    std::uniform_real_distribution<float> dist;

    int randomLevel() {
        int lvl = 1;
        while (dist(gen) < PROBABILITY && lvl < MAX_LEVEL)
            ++lvl;
        return lvl;
    }

public:
SkipList() {
    head = std::make_unique<Node>(std::numeric_limits<Key>::min(), Value(), MAX_LEVEL);

    // Explicitly initialize all forward pointers to nullptr
    for (int i = 0; i < MAX_LEVEL; ++i) {
        head->forward[i] = nullptr;
        head->forward_raw[i] = nullptr;
    }

    currentLevel.store(1);
    gen = std::mt19937(1337);
    dist = std::uniform_real_distribution<float>(0.0f, 1.0f);
}


    ~SkipList() {
        std::lock_guard<std::mutex> lock(global_lock);
        std::mutex level_update_lock;
        std::unique_ptr<Node>& current = head->forward[0];
        while (current) {
            current = std::move(current->forward[0]);
        }
        head.reset();
    }

    bool get(const Key& key, Value& value_out) {
        Node* current = head.get();
        int lvl = currentLevel.load();
    
        for (int i = lvl - 1; i >= 0; --i) {
            while (true) {
                if (i >= current->forward_raw.size()) break;
                Node* next = current->forward_raw[i];
                if (!next || next->key >= key) break;
                current = next;
            }
        }
    
        current = current->forward_raw[0];
        if (current && current->key == key) {
            std::lock_guard<std::mutex> lock(current->node_lock);
            value_out = current->value;
            return true;
        }
        return false;
    }
    
    void set(const Key& key, const Value& value) {
        std::cout << "[set] Thread entering set with key: " << key << std::endl;
    
        std::vector<Node*> update(MAX_LEVEL);
        Node* current = head.get();
        int lvl = currentLevel.load();
    
        // Traverse and fill update[i]
        for (int i = lvl - 1; i >= 0; --i) {
            Node* next = current->forward[i].get();
            while (next && next->key < key) {
                current = next;
                next = current->forward[i].get();
            }
            update[i] = current;
        }
    
        // Handle potential level promotion first — thread-safe
        int newLevel = randomLevel();
        int oldLevel = currentLevel.load();
        if (newLevel > oldLevel) {
            {
                static std::mutex level_update_lock;
                std::lock_guard<std::mutex> guard(level_update_lock);
                int cur = currentLevel.load();
                if (newLevel > cur) {
                    for (int i = cur; i < newLevel; ++i) {
                        update[i] = head.get();  // fill the new higher levels
                    }
                    currentLevel.store(newLevel);
                }
            }
        }
    
        // Collect all unique nodes in update[] up to newLevel
        std::unordered_set<Node*> unique_nodes;
        for (int i = 0; i < newLevel; ++i) {
            unique_nodes.insert(update[i]);
        }
    
        // Sort nodes by memory address for consistent lock ordering
        std::vector<Node*> nodes_to_lock(unique_nodes.begin(), unique_nodes.end());
        std::sort(nodes_to_lock.begin(), nodes_to_lock.end());
    
        // Acquire locks
        std::vector<std::unique_lock<std::mutex>> acquired_locks;
        std::cout << "[set] Acquiring locks..." << std::endl;
        for (Node* node : nodes_to_lock) {
            std::cout << "  [set] Locking node with key: " << node->key << std::endl;
            acquired_locks.emplace_back(node->node_lock);
        }
    
        // Check if key exists and update
        Node* next = update[0]->forward_raw[0];
        if (next && next->key == key) {
            std::cout << "[set] Key exists. Updating value for key: " << key << std::endl;
        
            // Prevent double-locking if next is already in the locked set
            bool already_locked = unique_nodes.count(next) > 0;
        
            if (!already_locked) {
                std::lock_guard<std::mutex> lock(next->node_lock);
                next->value = value;
            } else {
                // Already locked via acquired_locks
                next->value = value;
            }
        
        } else {
            std::cout << "[set] Inserting new key: " << key << " at level: " << newLevel << std::endl;
        
            auto newNode = std::make_unique<Node>(key, value, MAX_LEVEL);
            Node* newNodeRaw = newNode.get();
        
            // Set forward pointers of new node
            for (int i = 0; i < newLevel; ++i) {
                newNode->forward[i] = std::move(update[i]->forward[i]);
                newNode->forward_raw[i] = newNode->forward[i] ? newNode->forward[i].get() : nullptr;
            }
            for (int i = newLevel; i < MAX_LEVEL; ++i)
                newNode->forward_raw[i] = nullptr;
        
            // Link update[i] → newNode
            for (int i = 0; i < newLevel; ++i) {
                if (i == 0) {
                    update[i]->forward[i] = std::move(newNode);
                    update[i]->forward_raw[i] = update[i]->forward[i].get();
                } else {
                    update[i]->forward[i] = nullptr;
                    update[i]->forward_raw[i] = newNodeRaw;
                }
            }
        }
        
    
        std::cout << "[set] Done with key: " << key << std::endl;
    }
    
    
    
    void del(const Key& key) {
        std::cout << "[del] Thread deleting key: " << key << std::endl;
    
        std::vector<Node*> update(MAX_LEVEL);
        Node* current = head.get();
        int lvl = currentLevel.load();
    
        for (int i = lvl - 1; i >= 0; --i) {
            while (true) {
                if (i >= current->forward_raw.size()) break;
                Node* next = current->forward_raw[i];
                if (!next || next->key >= key) break;
                current = next;
            }
            update[i] = current;
        }
    
        std::unordered_set<Node*> seen;
        std::vector<std::unique_lock<std::mutex>> locks;
    
        for (int i = 0; i < lvl; ++i) {
            if (update[i] && seen.insert(update[i]).second) {
                std::cout << "[del] Locking node with key: " << update[i]->key << std::endl;
                locks.emplace_back(update[i]->node_lock);
            }
        }
    
        Node* target = update[0]->forward_raw[0];
        if (!target || target->key != key) {
            std::cout << "[del] Target key " << key << " not found. Skipping.\n";
            return;
        }
    
        std::cout << "[del] Locking target key: " << target->key << std::endl;
        locks.emplace_back(target->node_lock);
    
        int targetLevel = static_cast<int>(target->forward.size());
        std::cout << "[del] Unlinking target at " << targetLevel << " levels.\n";
    
        for (int i = 0; i < targetLevel; ++i) {
            if (i >= update[i]->forward_raw.size()) continue;
            if (update[i]->forward_raw[i] == target) {
                if (target->forward[i]) {
                    update[i]->forward[i] = std::move(target->forward[i]);
                    update[i]->forward_raw[i] = update[i]->forward[i].get();
                    std::cout << "  [del] Updated level " << i << " with next node\n";
                } else {
                    update[i]->forward_raw[i] = target->forward_raw[i];
                    std::cout << "  [del] Cleared level " << i << " raw pointer only\n";
                }
            }
        }
    
        while (currentLevel.load() > 1 && !head->forward_raw[currentLevel.load() - 1]) {
            std::cout << "[del] Decreasing currentLevel from " << currentLevel << std::endl;
            currentLevel.store(currentLevel.load() - 1);
        }
    
        std::cout << "[del] Done deleting key: " << key << std::endl;
    }
    
};
