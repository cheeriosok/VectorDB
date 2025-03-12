#ifndef ZSET_HPP
#define ZSET_HPP

#include "avl.hpp" // include the avl tree header file
#include "hashtable.hpp" // include the custom hash table header file
#include "thread_pool.hpp" // include the thread pool header file
#include <memory> // include memory management utilities like unique_ptr
#include <mutex>
#include <future>
using ZNode = AVLNode<std::string, double>;

class ZSet { // define the zset class
private:
    AVLTree<std::string, double> tree; // avl tree to maintain ordered scores
    HMap<std::string, ZNode*> hash; // Assuming HMap<K, V> uses string keys
    ThreadPool thread_pool_; // thread pool for handling asynchronous tasks
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<ZNode>> nodes_; // Keeps ZNode alive

public:
    explicit ZSet(size_t threads = 4) : thread_pool_(threads) {} // initialize thread pool with a default of 4 worker threads

    std::future<bool> add(std::string_view name, double score) { // asynchronously adds a new entry
        return thread_pool_.enqueue([this, name, score] { // enqueues the add operation in the thread pool
            return add_internal(name, score); // calls the internal add function
        });
    }

    ZNode* lookup(std::string_view name) {
        std::cerr << "Looking up: " << name << std::endl;

        ZNode** node_ptr = hash.find(std::string(name));
        if (!node_ptr || !*node_ptr) { 
            std::cerr << "lookup: No valid node found for: " << name << std::endl;
            return nullptr;
        }
        std::cerr << "lookup: Successfully found node for: " << name << " with score: " << (*node_ptr)->get_value() << std::endl;
        return *node_ptr;
    }
    
    bool add_internal(std::string_view name, double score) {
        std::lock_guard<std::mutex> lock(mutex_);
    
        if (ZNode* node = lookup(name)) {
            update_score(node, score);
            return false;
        }
    
        // Create a shared pointer to ensure ownership
        auto node = std::make_shared<ZNode>(std::string(name), score);
    
        if (node->get_key().empty()) {
            std::cerr << "ERROR: Created node has empty key after construction!" << std::endl;
            return false;
        }
    
        std::cerr << "Created node: " << node->get_key() << " with score: " << node->get_value() << std::endl;
    
        nodes_.emplace_back(node); // 🔥 Store shared_ptr to maintain ownership
        hash.insert(node->get_key(), node.get()); // 🔥 Store raw pointer in hash table
        tree.set(node->get_key(), score);
    
        std::cerr << "Added node successfully: " << node->get_key() << std::endl;
        return true;
    }
    
    

    std::future<ZNode*> pop(std::string_view name) { // asynchronously removes and returns a node
        return thread_pool_.enqueue([this, name] { // enqueues the pop operation in the thread pool
            return pop_internal(name); // calls the internal pop function
        });
    }

    ZNode* pop_internal(std::string_view name) {
        std::lock_guard<std::mutex> lock(mutex_);
        ZNode** node_ptr = hash.find(std::string(name)); 
        if (!node_ptr || !*node_ptr) return nullptr; // 🔥 Ensure valid pointer

        ZNode* node = *node_ptr; // Dereference the pointer to get the actual node

        hash.remove(std::string(name)); // Now remove safely
        tree.del(node->get_key());
        return node;
    }
    
    bool update_score(ZNode* node, double new_score) {
        if (!node) {
            std::cerr << "update_score: Received a nullptr!" << std::endl;
            return false;
        }
    
        std::string key = node->get_key();
        if (key.empty()) {
            std::cerr << "update_score: Node key is empty!" << std::endl;
            return false;
        }
    
        ZNode** node_ptr = hash.find(key);
        if (!node_ptr || !*node_ptr) {
            std::cerr << "update_score: Key not found in hash: " << key << std::endl;
            return false; 
        }
    
        tree.del(key);
        node->set_value(new_score);
        tree.set(key, new_score);
        
        return true; 
    }
    
    ZNode* query(double score, std::string_view name, int64_t offset) {
        return tree.exists(std::string(name)) ? lookup(name) : nullptr;
    }

    ~ZSet() {
        hash.clear(); 
        
        nodes_.clear(); 
    
        thread_pool_.shutdown();  // Shutdown the thread pool if it has an explicit shutdown mechanism.
    }    
    
};

#endif