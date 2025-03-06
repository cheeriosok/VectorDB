#include "avl.hpp" // include the avl tree header file
#include "hashtable.hpp" // include the custom hash table header file
#include "thread_pool.hpp" // include the thread pool header file
#include <memory> // include memory management utilities like unique_ptr

class ZSet { // define the zset class
private:
    AVLTree<std::unique_ptr<ZNode>> tree; // avl tree to maintain ordered scores
    HMap<ZNode*> hash; // hash table for fast name-based lookup
    threading::ThreadPool thread_pool_; // thread pool for handling asynchronous tasks

public:
    ZSet(size_t threads = 4) : thread_pool_(threads) {} // initialize thread pool with a default of 4 worker threads

    std::future<bool> add(std::string_view name, double score) { // asynchronously adds a new entry
        return thread_pool_.enqueue([this, name, score] { // enqueues the add operation in the thread pool
            return add_internal(name, score); // calls the internal add function
        });
    }

    bool add_internal(std::string_view name, double score) { // internal function to add a node synchronously
        if (ZNode* node = lookup(name)) { // check if the node already exists
            update_score(node, score); // update the score if found
            return false; // return false indicating an update instead of an insertion
        }
        auto node = std::make_unique<ZNode>(name, score); // create a new znode with unique_ptr
        hash.insert(node.get()); // insert the node into the hash table for fast lookups
        tree.insert(std::move(node)); // insert into the avl tree for ordered storage
        return true; // return true indicating a successful insertion
    }

    ZNode* lookup(std::string_view name) const { // retrieves a node based on its name
        return hash.find(name); // return the node if it exists in the hash table
    }

    std::future<ZNode*> pop(std::string_view name) { // asynchronously removes and returns a node
        return thread_pool_.enqueue([this, name] { // enqueues the pop operation in the thread pool
            return pop_internal(name); // calls the internal pop function
        });
    }

    ZNode* pop_internal(std::string_view name) { // internal function to remove a node synchronously
        if (ZNode* node = hash.remove(name)) { // check if the node exists in the hash table
            tree.remove(node); // remove from the avl tree
            return node; // return the removed node
        }
        return nullptr; // return nullptr if the node was not found
    }

    void update_score(ZNode* node, double new_score) { // updates the score of an existing node
        tree.remove(node); // remove the node from the avl tree
        node->score = new_score; // update the score
        tree.insert(node); // reinsert the node into the avl tree with the new score
    }

    ZNode* query(double score, std::string_view name, int64_t offset) const { // queries a node based on score and name
        return tree.query(score, name, offset); // return the node from the avl tree using its query function
    }
};