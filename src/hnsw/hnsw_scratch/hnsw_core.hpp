#pragma once

#include "visited_list_pool.hpp"
#include "hnswlib.hpp"
#include <atomic> // thread-safe counters
#include <random> // level assignment
#include <stdlib.h> // C-style memory mgmt (Goal: Get rid of this)
#include <assert.h> // runtime-assertion checks
#include <unordered_set> // fast set lookups
#include <list> // temporary path tracking/queuing
#include <memory> // unique_ptr


// labeltype = size_t
// typedef unsigned int tableint;
// typedef unsigned int linklistsizeint;
// typedef unsigned short int vl_type;

template <typename dist_t>
class HierarchicalNSW : public AlgorithmInterface<dist_t>
{
public:
    static const unsigned int MAX_LABEL_OPERATION_LOCKS = 65536; // Hash labels into a fixed # of mutexes (striped locking)
    static const unsigned char DELETE_MARK = 0x01; // (bitmask flag for marking elements as deleted)

    // Index Metadata & Graph Structure
    size_t capacity_{0}; // total allotted capaicty (max num elements)
    mutable std::atomic<size_t> element_count_{0}; // current number of elements
    mutable std::atomic<size_t> deleted_count_{0}; // number of elements MARKED deleted     
    int max_level_{0}; // highest/max level in the multi-layer HNSW
    unsigned int entry_id_{0}; // ID NOde of the starting entry point in the entrie HNSW graph
    std::vector<int> element_levels_; // keeps level of each element, SIZE OF CAPACITY_

    // Link Graph Parameters
    size_t M_{0}; // target degree for each node - otherwise known as "M" in HNSW . 
    size_t max_M_{0}; // max degree capacity at upper levels (typically equiv to link degree_).
    size_t max_M0_{0}; // max degree capacity at level 0, (typically 2M)
    size_t efConstruction_{0}; // neighbors to evaluate during insertion - efConstruction in whitepaper
    size_t efSearch_{0}; // neighbors to evaluate during search - efSearch in whitepaper

    // Probabilistic Level Generator Parameters
    double level_lambda_{0.0}, inv_lambda_{0.0}; // lambda for exponential level assignment and inverse of level_lambda, respectively

    // Memory Layout for Level 0 (Contiguous)
    /* Element_Stride = Total Size for one block of these in L0.
    // For every node in Level0, we have this structure. 
    // level0_data_ a contig memblcok represents ALL elements in HNSW in contiguous format, each of these blocks can be offset by
    // internal_id * element_stride! ! After indexing our block of interest we can access the block data of our element by adding this to:
    ┌──────────────────────────────┐
    │ [level-0 links]              │  ← offset = link0_offset_ = 0
    │  - sizeof(linklistsizeint)   │
    │  - link_capacity_level0_ *   │ 
    │ sizeof(tableint)             │
    ├──────────────────────────────┤
    │ [vector data]                │  ← offset = data_offset_
    │  - float or other dims       │
    ├──────────────────────────────┤
    │ [label (external ID)]        │  ← offset = label_offset_
    │  - size_t, user-defined      │
    └──────────────────────────────┘
    */

    size_t element_stride_{0}; // total memory for node at L0 = link0_stride_ + data_size_ + sizeof(label_type)
    size_t link0_offset_{0}; // Always 0. Link list comes first in level 0 layout!
    size_t link0_stride_{0}; // size of link block at lvl 0 (neighborlist) -> link_capacity_level0_ * sizeof(tableint) + sizeof(linklistsizeint)
    size_t data_offset_{0}; // off
    size_t data_size_{0}; // Size in bytes of the vector/embedding of each lements. (determined by space / dimension * sizeof(float))
    size_t label_offset_{0}; // offset of label (label in this case is user-defined key such as an SKU
    // label_size is basically the rest of the assigned mem or even (element_stride - (data_offset_+data_size))
    char *level0_data_{nullptr} // contiguous memory block for level-0 nodes. 
    
    // Memory Layout for Level 1
    char **link_blocks_{nullptr} // pointer array for upper-level link blocks (level 0 is NOT included)
    size_t link_stride_{0}; // size of link block in upper levels -> link_stride_ = link_capacity_upper_ * sizeof(tableint) + sizeof(linklistsizeint);

    // Concurrency Primitives!
    mutable std::vector<std::mutex> label_locks_; // using MAX_LABEL_OPERATION_LOCKS i.e striped locking for label->ID ops
    std::mutex global_lock_; // For rare global operations such as updating entry_id_ or max_leveL_ 
    std::vector<std::mutex> link_locks__;// One lock per node for link list updates during graph mutation

    // Label to Internal ID Mapping
   mutable std::mutex label_map_lock_; // lock for label_map_
   std::unordered_map<size_t, unsigned int> label_map_;

    // Distance Function and Metadata
    DISTFUNC<dist_t> distance_function_; // ???????? Altered. Please update. 
    void *distance_function_parameters_{nullptr}; // ???????? Altered. Please update. 


    // RNG for level assignment/updates
    std::default_random_engine level_rng_; // level = -log(U) * inv_lambda_
    std::default_random_engine update_rng_; // stochastic rebalancing, much like our resize_ op in HashTable
    // Runtime Metrics (Performance Metric Collection)
    mutable std::atomic<long> metric_distance_computations{0}; // metric_ (metric variablee) -> how many distance func calls in this search?
    mutable std::atomic<long> metric_hops{0}; // how many hops did we take to get to our query?

    // Visited List Pool
    std::unique_ptr<VisitedListPool> visited_pool_{nullptr}; // check visited_list_pool.hpp for details


    // Deleted Element Management
    bool reuse_deleted_ = false;                 // flag to replace deleted elements (marked as deleted) during insertions
    std::mutex deleted_elements_lock;                  // lock for deleted_elements
    std::unordered_set<unsigned int> deleted_elements; // contains internal ids of deleted elements

    // Does nothing -> Just for generic template usage
    HierarchicalNSW(SpaceInterface<dist_t> *space) {}

    HierarchicalNSW(SpaceInterface<dist_t> *space,
                    const std::string &location,
                    bool nmslib = false,
                    size_t capacity = 0,
                    bool reuse_deleted = false)
                    : reuse_deleted_(reuse_deleted) 
                    { loadIndex(location, space, max_elements); }

    HierarchicalNSW(SpaceInterface<dist_t> *space,
                    size_t capacity,
                    size_t M = 16,
                    size_t efConstruction = 200,
                    size_t random_seed = 100,
                    bool reuse_deleted = false) :
                    label_locks_(MAX_LABEL_OPERATION_LOCKS),
                    link_locks_(capacity),
                    element_levels_(max_elements),
                    reuse_deleted_(reuse_deleted) {
                        capacity_ = capacity;
                        deleted_count_ = 0;
                        data_size_ = space->get_data_size();
                        distance_function_ = space->get_distance_function();
                        distance_function_parameters_ = space->get_distance_distance_function_parameters_();
                        if ( M <= 10000) {
                            M_ = M;
                        } else {
                            std::cerr << "WARNING: M parameter exceeds 10000 which may lead to adverse effects." << std::endl;
                            std::cerr << "M has been reduced to 10,000 for proper operation." << std::endl;
                            M_ = 10000;
                        }
                        max_M_ = M_;
                        max_M0_ = 2*M_;
                        efConstruction = std::max(efConstruction, M_);
                        efSearch = 10;

                        level_rng_.seed(random_seed);
                        update_rng_.seed(random_seed + 1);
                        
                        link0_stride_ = max_M0_ * (sizeof(unsigned int)) + (sizeof(unsigned int));
                        element_stride_ = link0_stride_ + data_size_ + sizeof(label_type);
                        link0_offset_ = 0;
                        data_offset_ = link0_offset_ + link0_stride_;
                        label_offset_ = data_offset_ + data_size_;
                        
                        size_t size_level0_data = capacity_ * element_stride_;
                        level0_data_ = (char *)malloc(size_level0_data);
                        if (level0_data_ == nullptr) {
                            std::stringstream ss;
                            ss << "Not enough memory! Requested " << size_level0_data << " bytes.";
                            throw std::runtime_error(ss.str());
                        }
                        element_count_ = 0;

                        visited_list_pool_ = std::unique_ptr<VisitedListPool>(new VisitedListPool(1, max_elements));
                        
                        entry_id_ = -1;
                        max_level_ = -1;

                        link_blocks_size = sizeof(void *) * capacity_;
                        link_blocks_ = (char **) malloc(link_blocks_size);
                        if (link_blocks_ == nullptr) {
                            std::stringstream ss;
                            ss << "Not enough memory! Requested " << link_blocks_size << " bytes.";
                            throw std::runtime_error(ss.str());
                        }

                        link_stride_ = max_M_ * (sizeof(unsigned int)) + (sizeof(unsigned int));
                        level_lambda_ = 1 / log(1.0 * M_);
                        inv_lambda_ = 1.0 / level_lambda_;
                    }

    ~HierarchicalNSW() {
        clear();
    }

    // Pretty self-explanatory - we release all elements in level0_data_, and iterate through each element and free 
    // neighbor lists in all levels != 0.
    void clear() {
        free(level0_data_);
        level0_data_ = nullptr;

        for (unsigned int i = 0; i < element_count_; i++) {
            if (element_levels_[i] > 0)
                free(link_blocks_[i]);
        }
        free(link_blocks_);
        link_blocks = nullptr;
        element_count_ = 0;
        visited_pool_.reset(nullptr);
    }

    struct CompareByFirst {
        constexpr bool operator()(std::pair<dist_t, unsigned int> const& left, std::pair<dist_t, unsigned int> const& right) const noexcept {
            return left.first < right.first;
        }
    };

    void setefSearch (size_t efSearch) {
        efSearch_ = efSearch;
    }

    inline std::mutex& getLabelOpMutex(size_t label) const {
        // calculate hash
        size_t lock_id = label & (MAX_LABEL_OPERATION_LOCKS - 1);
        return label_locks_[lock_id];
    }

    // MEMCPY signature -> void* memcpy(void* dest, const void* src, size_t count); 

    inline size_t getExternalLabel(unsigned int internal_id) const {
            size_t return_label;
            memcpy(&return_label, (level0_data_ + internal_id * element_stride_ + label_offset_), sizeof(size_t));
            // for your own reference, level0_data_ points to the contiguous block of memory holding L0 nodes. internal_id * element_stride gives us the index 
            // of the node of interest, and + label_offset_ gives us the offset where the label information is stored. Check diagram above.
            return return_label;
    }

    inline size_t setExternalLabel(unsigned int internal_id, size_t label) const {
        memcpy((level0_data_ + (internal_id * element_stride_) + label_offset_), &label, sizeof(size_t));
    }


    inline size_t* getExternalLabelp(unsigned int internal_id) const {
            return (size_t *)(level0_data_ + internal_id * element_stride_ + label_offset_);
    }

    inline char* getDataByInternalId(size_t internal_id) const {
        return (level0_data_ + internal_id * element_stride_ + data_offset_);
    }


    // my guess reverse is an input for testing and functionla programming purposes.
    int getRandomLevel(double reverse) {
        std::uniform_real_distribution<double> distribution(0.0, 1.0); // random distribution between 0,1.
        double r = -log(distribution(level_rng_) * reverse); // given seeded level_rng and distrubtion - we get a repro. rng # between 0,1. 
        return (int) r;
    }

    size_t getCapacity() {
        return capacity_;
    }

    size_t getElementCount() {
        return element_count_;
    }

    size_t getDeletedCount() {
        return deleted_count_;
    }

    std::priority_queue<std::pair<dist_t, unsigned int>, std::vector<std::pair<dist_t, unsigned int>>, CompareByFirst>
    searchBaseLayer(unsigned int search_start_id, const void *data, int layer) {
        VisitedList *visited_list = visited_pool_->getFreeVisitedList();
        unsigned short int visited_Array = visited_list->mass;
        unsigned short int visited_Array_Tag = visited_list->curV;
        
        std::priority_queue<std::pair<dist_t, tableint>, std::vector<std::pair<dist_t, tableint>>, CompareByFirst> top_candidates;
        std::priority_queue<std::pair<dist_t, tableint>, std::vector<std::pair<dist_t, tableint>>, CompareByFirst> candidateSet;

        
    }

};