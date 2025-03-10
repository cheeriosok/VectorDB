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

public:
    explicit ZSet(size_t threads = 4) : thread_pool_(threads) {} // initialize thread pool with a default of 4 worker threads

    std::future<bool> add(std::string_view name, double score) { // asynchronously adds a new entry
        return thread_pool_.enqueue([this, name, score] { // enqueues the add operation in the thread pool
            return add_internal(name, score); // calls the internal add function
        });
    }

    // bool add_internal(std::string_view name, double score) { // internal function to add a node synchronously
    //     std::lock_guard<std::mutex> lock(mutex_);

    //     if (ZNode* node = lookup(name)) { // check if the node already exists
    //         update_score(node, score); // update the score if found
    //         return false; // return false indicating an update instead of an insertion
    //     }
    //     auto node = std::make_shared<ZNode>(name, score); // create a new znode with unique_ptr
    //     hash.insert(std::string(name), node.get()); // insert the node into the hash table for fast lookups
    //     tree.set(std::string(name), score); // Use AVLTree `set` instead of `insert`
    //     return true; // return true indicating a successful insertion
    // }

    bool add_internal(std::string_view name, double score) {
        std::lock_guard<std::mutex> lock(mutex_);
    
        if (ZNode* node = lookup(name)) {
            update_score(node, score);
            return false;
        }
    
        auto node = std::make_shared<ZNode>(std::string(name), score);
    
        std::cerr << "Inserting into hash: " << node->get_key() << std::endl;
        if (node->get_key().empty()) {
            std::cerr << "ERROR: Created node has empty key!" << std::endl;
            return false;
        }
    
        hash.insert(node->get_key(), node.get());
        std::cerr << "Inserted into hash, now inserting into tree: " << node->get_key() << std::endl;
        
        tree.set(node->get_key(), score);
        
        std::cerr << "Added node successfully: " << node->get_key() << std::endl;
        return true;
    }
    

    ZNode* lookup(std::string_view name) {
        std::cerr << "Looking up: " << name << std::endl;
        ZNode** node_ptr = hash.find(std::string(name));
    
        if (!node_ptr) {
            std::cerr << "lookup: No node found for: " << name << std::endl;
            return nullptr;
        }
    
        if (*node_ptr == nullptr) {
            std::cerr << "lookup: Found corrupted node (nullptr) for: " << name << std::endl;
            return nullptr;
        }
    
        if ((*node_ptr)->get_key().empty()) {
            std::cerr << "lookup: Found corrupted node with empty key for: " << name << std::endl;
            return nullptr;
        }
    
        return *node_ptr;
    }
    

    std::future<ZNode*> pop(std::string_view name) { // asynchronously removes and returns a node
        return thread_pool_.enqueue([this, name] { // enqueues the pop operation in the thread pool
            return pop_internal(name); // calls the internal pop function
        });
    }

    // ZNode* pop_internal(std::string_view name) { // internal function to remove a node synchronously
    //     std::lock_guard<std::mutex> lock(mutex_); // lock
    //     if (ZNode* node = hash.remove(std::string(name)).value_or(nullptr)) {
    //         // check if the node exists in the hash table
    //         auto shared_node = std::make_shared<ZNode>(*node); // wrap in shared_ptr
    //         tree.del(node->get_key());            
    //         return node; // return the removed node
    //     }
    //     return nullptr; // return nullptr if the node was not found
    // }

    ZNode* pop_internal(std::string_view name) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ZNode* node = hash.remove(std::string(name)).value_or(nullptr)) {
            auto shared_node = std::make_shared<ZNode>(node->get_key(), node->get_value()); // Fix: Avoid deleted copy constructor
            tree.del(node->get_key());
            return node;
        }
        return nullptr;
    }

    void update_score(ZNode* node, double new_score) {
        if (!node) {
            std::cerr << "update_score: Received a nullptr!" << std::endl;
            return;
        }
    
        std::string key = node->get_key();
        if (key.empty()) {
            std::cerr << "update_score: Node key is empty!" << std::endl;
            return;
        }
    
        std::cerr << "Entering update_score for: " << key << std::endl;
    
        { // 🔥 First lock section
            std::cerr << "Acquiring lock to check hash for: " << key << std::endl;
            std::lock_guard<std::mutex> lock(mutex_);
            if (!hash.find(key)) {
                std::cerr << "Key not found in hash: " << key << std::endl;
                return;
            }
        } // 🔥 Lock released
    
        // 🔥 Modify AVLTree outside of mutex lock
        std::cerr << "Removing from tree: " << key << std::endl;
        tree.del(key);  
    
        std::cerr << "Updating value: " << key << " -> " << new_score << std::endl;
        node->set_value(new_score);
    
        std::cerr << "Inserting back into tree: " << key << std::endl;
        tree.set(key, new_score);
    
        std::cerr << "Exiting update_score for: " << key << std::endl;
    }
    
    ZNode* query(double score, std::string_view name, int64_t offset) {
        return tree.exists(std::string(name)) ? lookup(name) : nullptr;
    }
};