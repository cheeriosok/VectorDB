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
// typedef unsigned int unsigned int;
// typedef unsigned int unsigned int;
// typedef size_t labeltype;
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
    │  - sizeof(unsigned int)      │
    │  - link_capacity_level0_ *   │ 
    │ sizeof(unsigned in           │
    ├──────────────────────────────┤
    │ [vector data]                │  ← offset = data_offset_
    │  - float or other dims       │
    ├──────────────────────────────┤
    │ [label (external ID)]        │  ← offset = label_offset_
    │  - size_t, user-defined      │
    └──────────────────────────────┘
    */

    size_t element_stride_{0}; // total memory for node at L0 = link0_stride_ + data_size_ + sizeof(unsigned int)
    size_t link0_offset_{0}; // Always 0. Link list comes first in level 0 layout!
    size_t link0_stride_{0}; // size of link block at lvl 0 (neighborlist) -> link_capacity_level0_ * sizeof(unsigned int) + sizeof(unsigned int)
    size_t data_offset_{0}; // off
    size_t data_size_{0}; // Size in bytes of the vector/embedding of each lements. (determined by space / dimension * sizeof(float))
    size_t label_offset_{0}; // offset of label (label in this case is user-defined key such as an SKU
    // label_size is basically the rest of the assigned mem or even (element_stride - (data_offset_+data_size))
    char *level0_data_{nullptr} // contiguous memory block for level-0 nodes. 
    
    // Memory Layout for Level 1
    char **link_blocks_{nullptr} // pointer array for upper-level link blocks (level 0 is NOT included)
    size_t link_stride_{0}; // size of link block in upper levels -> link_stride_ = link_capacity_upper_ * sizeof(unsigned int) + sizeof(unsigned int);

    // Concurrency Primitives!
    mutable std::vector<std::mutex> label_locks_; // using MAX_LABEL_OPERATION_LOCKS i.e striped locking for label->ID ops
    std::mutex global_lock_; // For rare global operations such as updating entry_id_ or max_leveL_ 
    std::vector<std::mutex> link_locks_;// One lock per node for link list updates during graph mutation

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
    mutable std::atomic<long> metric_distance_computations_{0}; // metric_ (metric variablee) -> how many distance func calls in this search?
    mutable std::atomic<long> metric_hops_{0}; // how many hops did we take to get to our query?

    // Visited List Pool
    std::unique_ptr<VisitedListPool> visited_pool_{nullptr}; // check visited_list_pool.hpp for details


    // Deleted Element Management
    bool reuse_deleted_ = false;                 // flag to replace deleted elements (marked as deleted) during insertions
    std::mutex deleted_elements_lock_;                  // lock for deleted_elements
    std::unordered_set<unsigned int> deleted_elements_; // contains internal ids of deleted elements

    // Does nothing -> Just for generic template usage
    HierarchicalNSW(SpaceInterface<dist_t> *space) {}

    HierarchicalNSW(SpaceInterface<dist_t> *space,
                    const std::string &location,
                    bool nmslib = false,
                    size_t capacity = 0,
                    bool reuse_deleted = false)
                    : reuse_deleted_(reuse_deleted) 
                    { loadIndex(location, space, capacity); }

    HierarchicalNSW(SpaceInterface<dist_t> *space,
                    size_t capacity,
                    size_t M = 16,
                    size_t efConstruction = 200,
                    size_t random_seed = 100,
                    bool reuse_deleted = false) :
                    label_locks_(MAX_LABEL_OPERATION_LOCKS),
                    link_locks_(capacity),
                    element_levels_(capacity),
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
                        element_stride_ = link0_stride_ + data_size_ + sizeof(size_t);
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
        link_blocks_ = nullptr;
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

    inline size_t* getExternalLabelp(unsigned int internal_id) const {
        return (size_t *)(level0_data_ + internal_id * element_stride_ + label_offset_);
    }

    inline size_t setExternalLabel(unsigned int internal_id, size_t label) const {
        memcpy((level0_data_ + (internal_id * element_stride_) + label_offset_), &label, sizeof(size_t));
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


    unsigned int* get_neighbors_L0(unsigned int internal_id) const {
        return (unsigned int*)(level0_data_ + internal_id * element_stride_ + link0_offset_);
    }
    
    unsigned int* get_neighbors_L0(unsigned int internal_id, char* level0_data_) const {
        return (unsigned int*)(level0_data_ + internal_id * element_stride_ + link0_offset_);
    }
    
    unsigned int* get_neighbors(unsigned int internal_id, int level) const {
        return (unsigned int*)(linkLists_[internal_id] + (level - 1) * link_stride_);
    }
    
    unsigned int* get_neighbors_at_level(unsigned int internal_id, int level) const {
        return level == 0 ? get_neighbors_L0(internal_id) : get_neighbors(internal_id, level);
    }
    


    bool isMarkedDeleted(unsigned int internalId) const {
        unsigned char *current = ((unsigned char*)get_linklist0(internalId)) + 2;
        return *ll_cur & DELETE_MARK;
    }


    unsigned short int getListCount(unsigned int * ptr) const {
        return *((unsigned short int *)ptr);
    }


    void setListCount(unsigned int * ptr, unsigned short int size) const {
        *((unsigned short int*)(ptr))=*((unsigned short int *)&size);
    }

    // This method searches one level/layr of the HNSW graph starting from start_id, for the closest neighbors to data_point.
    std::priority_queue<std::pair<dist_t, unsigned int>, std::vector<std::pair<dist_t, unsigned int>>, CompareByFirst>
    searchBaseLayer(unsigned int start_id, const void *data_point, int layer) {
        VisitedList *Visited_List = visited_pool_->getFreeVisitedList();
        unsigned short int* Visited_Array = Visited_List->visitedAt;
        unsigned short int Visited_Array_Tag = Visited_List->currentVisited;
        
        std::priority_queue<std::pair<dist_t, unsigned int>, std::vector<std::pair<dist_t, unsigned int>>, CompareByFirst> Top_K;
        std::priority_queue<std::pair<dist_t, unsigned int>, std::vector<std::pair<dist_t, unsigned int>>, CompareByFirst> K_Set;

        dist_t lower_bound;
        if (!isMarkedDeleted(start_id)) {
            dist_t distance = distance_function(data_point, getDataByInternalId(start_id), distance_function_parameters_);
            Top_K.emplace(distance, start_id);
            lower_bound = distance;
            K_Set.emplace(-distance, start_id);
        } else {
            lower_bound = std::numeric_limits<dist_t>::max();
            K_Set.emplace(lower_bound, start_id);
        }
        Visited_Array[start_id] = Visited_Array_Tag;

        while(!K_Set.empty()) {
            std::pair<dist_t, unsigned int> current_pair = K_Set.top();
            if ((-current_pair.first) > lower_bound && Top_K.size() == efConstruction_) {
                break;
            }
            K_Set.pop();
            unsigned int current_node_id = current_pair.second;
            std::unique_lock <std::mutex> lock(link_locks_[current_node_id]);

            int *data;
            
            if (layer == 0) {
                data = (int *)get_neighbors_L0(current_node_id);
            } else {
                data = (int *)get_neighbors_at_level(current_node_id, layer);
            }
            size_t size = getListCount((unsigned int*) data);
            unsigned int *datal = (unsigned int *) (data + 1);
            for (size_t j = 0; j < size; j++) {
                unsigned int K_id = *(datal + j);
                if (Visited_Array[K_id] == Visited_Array_Tag) continue;
                Visited_Array[K_id] = Visited_Array_Tag;
                char *current_obj1 = (getDataByInternalId(K_id));

                dist_t dist1 = distance_function_(data_point, current_obj1, distance_function_parameters_);
                if (Top_K.size() < efConstruction_ || lower_bound > dist1) {
                    K_Set.emplace(-dist1, K_id);

                    if (!isMarkedDeleted(K_id))
                        Top_K.emplace(dist1, K_id);

                    if (Top_K.size() > efConstruction_)
                        Top_K.pop();

                    if (!Top_K.empty())
                        lower_bound = Top_K.top().first;
                }
            }
        }
        visited_pool_->releaseVisitedList(Visited_List);

        return Top_K;
    }
    void getNeighborsByHeuristic2 (
        std::priority_queue<std::pair<dist_t, unsigned int>, std::vector<std::pair<dist_t, unsigned int>>, CompareByFirst> &Top_K, 
        const size_t M) {
        if (Top_K.size() < M) {
            return;
        }
        std::priority_queue<std::pair<dist_t, unsigned int>> queue_closest;
        std::vector<std::pair<dist_t, unsigned int>> return_list;
        while (Top_K.size() > 0) {
            queue_closest.emplace(-Top_K.top().first, Top_K.top().second);
            Top_K.pop();
        }
        while (queue_closest.size()) {
            if (return_list() >= M) break;
            std::pair<dist_t, unsigned int> current_pair = queue.closest.top();
            dist_t distance_to_query = - current_pair.first;
            queue_closest.pop();
            bool flag = true;

            for (std::pair<dist_t, unsigned int> second_pair : return_list) {
                dist_t current_distance = distance_function_(getDataByInternalId(second_pair.second),
                                                             getDataByInternalId(curent_pair.second),
                                                             distance_function_parameters_);
                if (current_distance < distance_to_query) {
                    flag = false 
                    break;
                }
            }
            if (flag) return_list.push_back(current_pair);
        }
        for (std::pair<dist_t, unsigned int> current_pair : return_list)
            Top_K.emplace(-current_pair.first, current_pair.second);
        
    }

    unsigned int mutuallyConnectNewElement(
        const void *data_point,
        unsigned int current_c,
        std::priority_queue<std::pair<dist_t, unsigned int>, std::vector<std::pair<dist_t, unsigned int>>, CompareByFirst> &Top_K,
        int level,
        bool updateFlag) {
        
        size_t max_M = level ? max_M_ : maxM0_;
        getNeighborsByHeuristic2(Top_K, M_);
        if (Top_K.size() > M_) throw std::runtime_error("Should not be more than M_ candidates returned by the heuristic");
        std::vector<unsigned int> selectedNeighbors;
        selectedNeighbors.reserve(M_);
        while (Top_K.size() > 0) {
            selectedNeighbors.push_back(Top_K.top().second);
            Top_K.pop();
        }

        unsigned int next_closest_entry_point = selectedNeighbors.back();
        {
            std::unique_lock <std::mutex> lock(link_locks_[current_c], std::defer_lock);
            if (updateFlag) {
                lock.lock();
            }
            unsigned int *link_current;
            if (level == 0)
                link_current = get_neighbors_L0(current_c);
            else
                link_current = get_neighbors_at_level(current_c, level);

            if (*link_current && !updateFlag) 
                throw std::runtime_error("The newly inserted element should have a blank neighbor list");
            
            setListCount(link_current, selectedNeighbors.size());
            unsigned int *data = (unsigned int *) (link_current + 1);
            for (size_t idx = 0; idx < selectedNeighbors.size(); idx++) {
                if (data[idx] && !updateFlag)
                    throw std::runtime_error("Possible memory corruption");
                if (level > element_levels_[selectedNeighbors[idx]])
                    throw std::runtime_error("Trying to make a link on a non-existent level");

                data[idx] = selectedNeighbors[idx];
            }
        }
        for (size_t idx = 0; idx < selectedNeighbors.size(); idx++) {
            std::unique_lock <std::mutex> lock(link_locks_[selectedNeighbors[idx]]);

            unsigned int *link_other;
            if (level == 0)
                link_other = get_neighbors_L0(selectedNeighbors[idx]);
            else
                link_other = get_neighbors_at_level(selectedNeighbors[idx], level);

            size_t sizeof_link_other = getListCount(link_other);

            if (sizeof_link_other > max_M)
                throw std::runtime_error("Bad value of sizeof_link_other");
            if (selectedNeighbors[idx] == current_c)
                throw std::runtime_error("Trying to connect an element to itself");
            if (level > element_levels_[selectedNeighbors[idx]])
                throw std::runtime_error("Trying to make a link on a non-existent level");

            unsigned int *data = (unsigned int *) (link_other + 1);

            bool is_current_c_present = false;
            if (isUpdate) {
                for (size_t j = 0; j < sizeof_link_other; j++) {
                    if (data[j] == current_c) {
                        is_current_c_present = true;
                        break;
                    }
                }
            }

            // If cur_c is already present in the neighboring connections of `selectedNeighbors[idx]` then no need to modify any connections or run the heuristics.
            if (!is_current_c_present) {
                if (sizeof_link_other < max_M) {
                    data[sizeof_link_other] = current_c;
                    setListCount(link_other, sizeof_link_other + 1);
                } else {
                    dist_t max_distance = distance_function_(getDataByInternalId(current_c), 
                                                      getDataByInternalId(selectedNeighbors[idx]),
                                                      distance_function_parameters_);
                    
                    std::priority_queue<std::pair<dist_t, unsigned int>, std::vector<std::pair<dist_t, unsigned int>>, CompareByFirst> K_Set;
                    K_Set.emplace(max_distance, current_c);

                    for (size_t j = 0; j < sizeof_link_other; j++) {
                        K_Set.emplace(distance_function_(getDataByInternalId(data[j]), getDataByInternalId(selectedNeighbors[idx]), distance_function_parameters_), data[j]);
                    }

                    getNeighborsByHeuristic2(K_Set, max_M);

                    int n = 0;
                    while (K_Set.size() > 0) {
                        data[n] = K_Set.top().second;
                        K_Set.pop();
                        n++;
                    }

                    setListCount(link_other, n);

                }
            }
        }

        return next_closest_entry_point;
    }

    void resizeIndex(size_t new_max_elements) {
        if (new_max_elements < element_count_)
            throw std::runtime_error("Cannot resize, max element is less than the current number of elements");

        visited_pool_.reset(new VisitedListPool(1, new_max_elements));

        element_levels_.resize(new_max_elements);

        std::vector<std::mutex>(new_max_elements).swap(link_locks_);

        char* level0_data_new = (char *) realloc(data_level0_memory_, new_max_elements * element_stride_);
        if (level0_data_new == nullptr)
            throw std::runtime_error("Not enough memory: resizeIndex failed to allocate base layer");
        level0_data_ = level0_data_new;

        // Reallocate all other layers
        char ** link_blocks_new = (char **) realloc(link_blocks_, sizeof(void *) * new_max_elements);
        if (link_blocks_new == nullptr)
            throw std::runtime_error("Not enough memory: resizeIndex failed to allocate other layers");
            link_blocks_ = link_blocks_new;

        capacity_ = new_max_elements;
    }

    size_t indexFileSize() const {
        size_t size = 0;
        size += sizeof(link0_offset_);
        size += sizeof(capacity_);
        size += sizeof(element_count_);
        size += sizeof(element_stride_);
        size += sizeof(label_offset_);
        size += sizeof(data_offset_);
        size += sizeof(max_level_);
        size += sizeof(entry_id_);
        size += sizeof(max_M_);

        size += sizeof(max_M0_);
        size += sizeof(M_);
        size += sizeof(efSearch_);
        size += sizeof(efConstruction_);

        size += element_count_ * element_stride_;

        for (size_t i = 0; i < element_count_; i++) {
            unsigned int list_size = element_levels_[i] > 0 ? link_blocks * element_levels_[i] : 0;
            size += sizeof(list_size);
            size += list_size;
        }
        return size;
    }

    void saveIndex(const std::string &location) {
        std::ofstream output(location, std::ios::binary);
        std::streampos position;

        writeBinaryPOD(output, link0_offset_);
        writeBinaryPOD(output, capacity_);
        writeBinaryPOD(output, element_count_);
        writeBinaryPOD(output, element_stride_);
        writeBinaryPOD(output, label_offset_);
        writeBinaryPOD(output, data_offset_);
        writeBinaryPOD(output, max_level_);
        writeBinaryPOD(output, entry_id_);
        writeBinaryPOD(output, max_M_);

        writeBinaryPOD(output, max_M0_);
        writeBinaryPOD(output, M_);
        writeBinaryPOD(output, efSearch_);
        writeBinaryPOD(output, efConstruction_);

        output.write(level0_data_, element_count_ * element_stride_);

        for (size_t i = 0; i < element_count_; i++) {
            unsigned int list_size = element_levels_[i] > 0 ? link_blocks * element_levels_[i] : 0;
            writeBinaryPOD(output, list_size);
            if (list_size)
                output.write(link_blocks_[i], list_size);
        }
        output.close();
    }

    void loadIndex(const std::string &location, SpaceInterface<dist_t> *space, size_t max_elements_i = 0) {
        std::ifstream input(location, std::ios::binary);

        if (!input.is_open())
            throw std::runtime_error("Cannot open file");

        clear();
        // get file size:
        input.seekg(0, input.end);
        std::streampos total_filesize = input.tellg();
        input.seekg(0, input.beg);

        readBinaryPOD(input, link0_offset_);
        readBinaryPOD(input, capacity_);
        readBinaryPOD(input, element_count_);

        size_t capacity = capacity_i;
        if (capacity < element_count_)
            capacity = capacity_;
        max_elements_ = max_elements;
        readBinaryPOD(input, element_stride_);
        readBinaryPOD(input, label_offset_);
        readBinaryPOD(input, data_offset_);
        readBinaryPOD(input, max_level_);
        readBinaryPOD(input, entry_id_);

        readBinaryPOD(input, max_M_);
        readBinaryPOD(input, max_M0_);
        readBinaryPOD(input, M_);
        readBinaryPOD(input, efSearch_);
        readBinaryPOD(input, efConstruction_);

        data_size_ = space->get_data_size();
        distance_function_ = space->get_distance_function();
        distance_function_parameters_ = space->get_distance_function_parameters();

        auto pos = input.tellg();

        input.seekg(element_count_ * element_stride_, input.cur);
        for (size_t i = 0; i < element_count_; i++) {
            if (input.tellg() < 0 || input.tellg() >= total_filesize) {
                throw std::runtime_error("Index seems to be corrupted or unsupported");
            }

            unsigned int list_size;
            readBinaryPOD(input, list_size);
            if (list_size != 0) {
                input.seekg(list_size, input.cur);
            }
        }

        if (input.tellg() != total_filesize)
            throw std::runtime_error("Index seems to be corrupted or unsupported");

        input.clear();

        input.seekg(pos, input.beg);

        level0_data_ = (char *) malloc(capacity * element_stride_);
        if (level0_data_ == nullptr)
            throw std::runtime_error("Not enough memory: loadIndex failed to allocate level0");
        input.read(level0_data_, element_count_ * element_stride_);
        link_stride_ = max_M_ * sizeof(unsigned int) + sizeof(unsigned int);
        link0_stride_ = max_M0 * sizeof(unsigned int) + sizeof(unsigned int);
        std::vector<std::mutex>(capacity).swap(link_locks_);
        std::vector<std::mutex>(MAX_LABEL_OPERATION_LOCKS).swap(label_locks_);

        visited_pool_.reset(new VisitedListPool(1, capacity));

        link_blocks_ = (char **) malloc(sizeof(void *) * capacity);
        if (link_blocks_ == nullptr)
            throw std::runtime_error("Not enough memory: loadIndex failed to allocate linklists");
        element_levels_ = std::vector<int>(capacity);
        inv_lambda_ = 1.0 / level_lambda_;
        efSearch_ = 10;
        for (size_t i = 0; i < element_count_; i++) {
            label_map_[getExternalLabel(i)] = i;
            unsigned int list_size;
            readBinaryPOD(input, list_size);
            if (list_size == 0) {
                element_levels_[i] = 0;
                link_blocks_[i] = nullptr;
            } else {
                element_levels_[i] = list_size / link_stride_;
                link_blocks_[i] = (char *) malloc(list_size);
                if (link_blocks_[i] == nullptr)
                    throw std::runtime_error("Not enough memory: loadIndex failed to allocate linklist");
                input.read(link_blocks_[i], list_size);
            }
        }

        for (size_t i = 0; i < element_count_; i++) {
            if (isMarkedDeleted(i)) {
                deleted_count_ += 1;
                if (reuse_deleted_) deleted_elements_.insert(i);
            }
        }

        input.close();

        return;
    }
        template<typename data_t>
        std::vector<data_t> getDataByLabel(size_t label) const {
            std::unique_lock <std::mutex> label_lock(getLabelOpMutex(label));
            std::unique_lock <std::mutex> label_locks_(label_map_lock_);
            auto search = label_map_.find(label);
            if (search == label_map_.end() || isMarkedDeleted(search->second)) {
                throw std::runtime_error("Label not found");
            }
            unsigned int internalId = search->second;
            label_locks_.unlock();

            char* data_ptrv = getDataByInternalId(internalId);
            size_t dim = *((size_t *) distance_function_parameters_);
            std::vector<data_t> data;
            data_t* data_ptr = (data_t*) data_ptrv;
            for (size_t i = 0; i < dim; i++) {
                data.push_back(*data_ptr);
                data_ptr += 1;
            }
            return data;
        }


    /*
    * Marks an element with the given label deleted, does NOT really change the current graph.
    */
    void markDelete(size_t label) {
        // lock all operations with element by label
        std::unique_lock <std::mutex> label_lock(getLabelOpMutex(label));
        std::unique_lock <std::mutex> label_locks_(label_map_lock_);
        auto search = label_map_.find(label);
        if (search == label_map_.end()) {
            throw std::runtime_error("Label not found");
        }
        unsigned int internalId = search->second;
        label_locks_.unlock();

        markDeletedInternal(internalId);
    }


    /*
    * Uses the last 16 bits of the memory for the linked list size to store the mark,
    * whereas maxM0_ has to be limited to the lower 16 bits, however, still large enough in almost all cases.
    */
    void markDeletedInternal(unsigned int internalId) {
        assert(internalId < element_count_);
        if (!isMarkedDeleted(internalId)) {
            unsigned char *link_current = ((unsigned char *)get_neighbors_L0(internalId))+2;
            *link_current |= DELETE_MARK;
            num_deleted_ += 1;
            if (reuse_deleted_) {
                std::unique_lock <std::mutex> lock_deleted_elements(deleted_elements_lock_);
                deleted_elements.insert(internalId);
            }
        } else {
            throw std::runtime_error("The requested to delete element is already deleted");
        }
    }


    /*
    * Removes the deleted mark of the node, does NOT really change the current graph.
    * 
    * Note: the method is not safe to use when replacement of deleted elements is enabled,
    *  because elements marked as deleted can be completely removed by addPoint
    */
    void unmarkDelete(size_t label) {
        // lock all operations with element by label
        std::unique_lock <std::mutex> lock_label(getLabelOpMutex(label));
        std::unique_lock <std::mutex> label_locks_(label_map_lock_);
        auto search = label_map_.find(label);
        if (search == label_map_.end()) {
            throw std::runtime_error("Label not found");
        }
        unsigned int internalId = search->second;
        label_locks_.unlock();

        unmarkDeletedInternal(internalId);
    }



    /*
    * Remove the deleted mark of the node.
    */
    void unmarkDeletedInternal( unsigned int internalId) {
        assert(internalId < cur_element_count);
        if (isMarkedDeleted(internalId)) {
            unsigned char *link_current = ((unsigned char *)get_neighbors_L0(internalId)) + 2;
            *link_current &= ~DELETE_MARK;
            deleted_count_ -= 1;
            if (reuse_deleted_) {
                std::unique_lock <std::mutex> lock_deleted_elements(deleted_elements_lock_);
                deleted_elements_.erase(internalId);
            }
        } else {
            throw std::runtime_error("The requested to undelete element is not deleted");
        }
    }

    bool isMarkedDeleted(unsigned int internalId) const {
        unsigned char *link_current = ((unsigned char*)get_neighbors_L0(internalId)) + 2;
        return *link_current & DELETE_MARK;
    }


    unsigned short int getListCount(unsigned int* ptr) const {
        return *((unsigned short int *)ptr);
    }


    void setListCount(unsigned int * ptr, unsigned short int size) const {
        *((unsigned short int*)(ptr))=*((unsigned short int *)&size);
    }


    /*
    * Adds point. Updates the point if it is already in the index.
    * If replacement of deleted elements is enabled: replaces previously deleted point if any, updating it with new point
    */
   void addPoint(const void *data_point, size_t label, bool reuse_deleted = false) {
        if ((reuse_deleted_ == false) && (reuse_deleted == true)) {
            throw std::runtime_error("Replacement of deleted elements is disabled in constructor");
        }

        // lock all operations with element by label
        std::unique_lock <std::mutex> label_lock(getLabelOpMutex(label));
        if (!reuse_deleted) {
            addPoint(data_point, label, -1);
            return;
        }
        // check if there is vacant place
        unsigned_int internal_id_replaced;
        std::unique_lock <std::mutex> lock_deleted_elements(deleted_elements_lock_);
        bool is_vacant_place = !deleted_elements_.empty();
        if (is_vacant_place) {
            internal_id_replaced = *deleted_elements_.begin();
            deleted_elements_.erase(internal_id_replaced);
        }
        lock_deleted_elements.unlock();

        // if there is no vacant place then add or update point
        // else add point to vacant place
        if (!is_vacant_place) {
            addPoint(data_point, label, -1);
        } else {
            // we assume that there are no concurrent operations on deleted element
            size_t label_replaced = getExternalLabel(internal_id_replaced);
            setExternalLabel(internal_id_replaced, label);

            std::unique_lock <std::mutex> lock_table(label_map_lock_);
            label_map_.erase(label_replaced);
            label_map_[label] = internal_id_replaced;
            lock_table.unlock();

            unmarkDeletedInternal(internal_id_replaced);
            updatePoint(data_point, internal_id_replaced, 1.0);
        }
    }

    void updatePoint(const void *dataPoint, unsigned int internalId, float updateNeighborProbability) {
        memcpy(getDataByInternalId(internalId), dataPoint, data_size_);

        int max_levelCopy = max_level_;
        unsigned int entry_id_copy = entry_id_;
        if (entry_id_copy == internalId && element_count_ == 1)
            return;
        int element_level = element_levels_[internalId];
        std::uniform_real_distribution<float> distribution(0.0, 1.0);
        for (int layer = 0; layer <= element_level; layer++) {
            std::unordered_set<unsigned int> candidate_set;
            std::unordered_set<unsigned int> neighbor_set;
            std::vector<unsigned int> list_one_hop = getConnectionsWithLock(internalId, layer);
            if (list_one_hop.size() == 0)
                continue;

            candidate_set.insert(internalId);

            for (auto&& element_one_hop : list_one_hop) {
                candidate_set.insert(element_one_hop);

                if (distribution(update_rng_) > updateNeighborProbability)
                    continue;

                neighbor_set.insert(element_one_hop);

                std::vector<unsigned int> list_two_hop = getConnectionsWithLock(element_one_hop, layer);
                for (auto&& element_two_hop : list_two_hop) {
                    candidate_set.insert(element_two_hop);
                }
            }

            for (auto&& neighbor : neighbor_set) {
                std::priority_queue<std::pair<dist_t, unsigned int>, std::vector<std::pair<dist_t, unsigned int>>, CompareByFirst> candidates;
                size_t size = candidate_set.find(neighbor) == candidate_set.end() ? candidate_set.size() : candidate_set.size() - 1;  // candidate_set guaranteed to have size >= 1
                size_t elements_to_keep = std::min(efConstruction_, size);
                for (auto&& candidate : candidate_set) {
                    if (candidate == neighbor)
                        continue;

                    dist_t distance = distance_function_(getDataByInternalId(neighbor), getDataByInternalId(candidate), distance_function_parameters_);
                    if (candidates.size() < elements_to_keep) {
                        candidates.emplace(distance, candidate);
                    } else {
                        if (distance < candidates.top().first) {
                            candidates.pop();
                            candidates.emplace(distance, candidate);
                        }
                    }
                }

                getNeighborsByHeuristic2(candidates, layer == 0 ? max_M0_ : max_M_);

                {
                    std::unique_lock <std::mutex> lock(link_locks_[neighbor]);
                    unsigned int *link_current;
                    link_current = get_neighbors_at_level(neighbor, layer);
                    size_t candidate_size = candidates.size();
                    setListCount(link_current, candidate_size);
                    unsigned int *data = (unsigned int *) (link_current + 1);
                    for (size_t idx = 0; idx < candidate_size; idx++) {
                        data[idx] = candidates.top().second;
                        candidates.pop();
                    }
                }
            }
        }

        repairConnectionsForUpdate(dataPoint, entryPointCopy, internalId, elemLevel, max_levelCopy);
    }


    // Here

// labeltype = size_t
// typedef tableint unsigned int;
// typedef linklistsizeint unsigned int;
// typedef size_t labeltype;
// typedef unsigned short int vl_type;

    void repairConnectionsForUpdate(
        const void *dataPoint,
        unsigned int entry_point_internal_id,
        unsigned int data_point_internal_id,
        int data_point_level,
        int max_level) {
        unsigned int current_obj = entry_point_internal_id;
        if (data_point_level < max_level) {
            dist_t current_distance = distance_function_(dataPoint, getDataByInternalId(current_obj), distance_function_parameters_);
            for (int level = max_level; level > data_point_level; level--) {
                bool changed = true;
                while (changed) {
                    changed = false;
                    unsigned int *data;
                    std::unique_lock <std::mutex> lock(link_locks_[current_obj]);
                    data = get_neighbors_at_level(current_obj, level);
                    int size = getListCount(data);
                    unsigned int *datal = (unsigned int *) (data + 1);
                    for (int i = 0; i < size; i++) {
                        unsigned int candidate = datal[i];
                        dist_t distance = distance_function_(dataPoint, getDataByInternalId(candidate), distance_function_parameters__);
                        if (distance < current_distance) {
                            current_distance = distance;
                            current_obj = candidate;
                            changed = true;
                        }
                    }
                }
            }
        }

        if (data_point_level > max_level)
            throw std::runtime_error("Level of item to be updated cannot be bigger than max level");

        for (int level = data_point_level; level >= 0; level--) {
            std::priority_queue<std::pair<dist_t, unsigned int>, std::vector<std::pair<dist_t, unsigned int>>, CompareByFirst> Top_K = searchBaseLayer(current_obj, dataPoint, level);
            std::priority_queue<std::pair<dist_t, unsigned int>, std::vector<std::pair<dist_t, unsigned int>>, CompareByFirst> Filtered_Top_K;

            while (Top_K.size() > 0) {
                if (Top_K.top().second != data_point_internal_id)
                    Filtered_Top_K.push(Top_K.top());

                Top_K.pop();
            }

            if (Filtered_Top_K.size() > 0) {
                bool entry_point_deleted = isMarkedDeleted(entry_point_internal_id);
                if (entry_point_deleted) {
                    Filtered_Top_K.emplace(distance_function_(dataPoint, getDataByInternalId(entry_point_internal_id), distance_function_parameters__), entry_point_internal_id);
                    if (Filtered_Top_K.size() > efConstruction_)
                        Filtered_Top_K.pop();
                }

                current_obj = mutuallyConnectNewElement(dataPoint, data_point_internal_id, Filtered_Top_K, level, true);
            }
        }
    }


    std::vector<unsigned int> getConnectionsWithLock(unsigned int internalId, int level) {
        std::unique_lock <std::mutex> lock(link_locks_[internalId]);
        unsigned int *data = get_neighbors_at_level(internalId, level);
        int size = getListCount(data);
        std::vector<unsigned int> result(size);
        unsigned int *list = (unsigned int *) (data + 1);
        memcpy(result.data(), list, size * sizeof(unsigned int));
        return result;
    }


    unsigned int addPoint(const void *data_point, size_t label, int level) {
        unsigned int current_c = 0;
        {
            // Checking if the element with the same label already exists
            // if so, updating it *instead* of creating a new element.
            std::unique_lock <std::mutex> lock_table(label_map_lock_);
            auto search = label_map_.find(label);
            if (search != label_map_.end()) {
                unsigned int existingInternalId = search->second;
                if (reuse_deleted_) {
                    if (isMarkedDeleted(existingInternalId)) {
                        throw std::runtime_error("Can't use addPoint to update deleted elements if replacement of deleted elements is enabled.");
                    }
                }
                lock_table.unlock();

                if (isMarkedDeleted(existingInternalId)) {
                    unmarkDeletedInternal(existingInternalId);
                }
                updatePoint(data_point, existingInternalId, 1.0);

                return existingInternalId;
            }

            if (element_count_ >= capacity_) {
                throw std::runtime_error("The number of elements exceeds the specified limit");
            }

            current_c = element_count_;
            element_count_++;
            label_map_[label] = current_c;
        }

        std::unique_lock <std::mutex> lock_element(link_locks_[current_c]);
        int current_level = getRandomLevel(level_lambda_);
        if (level > 0) current_level = level;

        element_levels_[current_c] = current_level;

        std::unique_lock <std::mutex> temporary_lock(global_lock_);
        int max_level_copy = max_level_;
        if (current_level <= max_level_copy) temporary_lock.unlock();
        unsigned int current_obj = entry_id_;
        unsigned int entry_id_copy = entry_id_;

        memset(level0_data_ + current_c * element_stride_ + link0_offset_, 0, element_stride_);

        // Initialisation of the data and label
        memcpy(getExternalLabeLp(current_c), &label, sizeof(size_t));
        memcpy(getDataByInternalId(current_c), data_point, data_size_);

        if (current_level) {
            link_blocks_[current_c] = (char *) malloc(link_stride_ * current_level + 1);
            if (link_blocks_[current_c] == nullptr)
                throw std::runtime_error("Not enough memory: addPoint failed to allocate linklist");
            memset(link_blocks_[current_c], 0, link_stride_ * current_level + 1);
        }

        if ((signed)current_obj != -1) {
            if (current_level < max_level_copy) {
                dist_t current_distance = distance_function_(data_point, getDataByInternalId(current_obj), distance_function_parameters__);
                for (int level = max_level_copy; level > current_level; level--) {
                    bool changed = true;
                    while (changed) {
                        changed = false;
                        unsigned int *data;
                        std::unique_lock <std::mutex> lock(link_locks_[current_obj]);
                        data = get_neighbors(current_obj, level);
                        int size = getListCount(data);

                        unsigned int *datal = (unsigned int *) (data + 1);
                        for (int i = 0; i < size; i++) {
                            unsigned int candidate = datal[i];
                            if (candidate < 0 || candidate > capacity_)
                                throw std::runtime_error("candidate error!");
                            dist_t distance = distance_function_(data_point, getDataByInternalId(candidate), distance_function_parameters__);
                            if (distance < current_distance) {
                                current_distance = distance;
                                current_obj = candidate;
                                changed = true;
                            }
                        }
                    }
                }
            }

            bool entry_id_deleted = isMarkedDeleted(entry_id_copy);
            for (int level = std::min(curlevel, max_level_copy); level >= 0; level--) {
                if (level > max_level_copy || level < 0)  // possible?
                    throw std::runtime_error("Level error");
                std::priority_queue<std::pair<dist_t, unsigned int>, std::vector<std::pair<dist_t, unsigned int>>, CompareByFirst> Top_K = searchBaseLayer(current_obj, data_point, level);
                if (entry_id_deleted) {
                    Top_K.emplace(distance_function_(data_point, getDataByInternalId(entry_id_copy), distance_function_parameters__), entry_id_copy);
                    if (Top_K.size() > efConstruction_)
                        Top_K.pop();
                }
                current_obj = mutuallyConnectNewElement(data_point, cur_c, Top_K, level, false);
            }
        } else {
            // Do nothing for the first element
            entry_id_ = 0;
            max_level_ = current_level;
        }

        // Releasing lock for the maximum level
        if (curlevel > max_levelcopy) {
            entry_id_ = current_c;
            max_level_ = current_level;
        }
        return current_c;
    }


    std::priority_queue<std::pair<dist_t, size_t>> searchKnn(const void *query_data, size_t k, BaseFilterFunctor* isIdAllowed = nullptr) const {
        std::priority_queue<std::pair<dist_t, size_t>> result;
        if (element_count_ == 0) return result;

        unsigned int current_obj = entry_id_;
        dist_t current_distance = distance_function_(query_data, getDataByInternalId(entry_id_), distance_function_parameters__);

        for (int level = max_level_; level > 0; level--) {
            bool changed = true;
            while (changed) {
                changed = false;
                unsigned int *data;

                data = (unsigned int *) get_neighbors(current_obj, level);
                int size = getListCount(data);
                metric_hops++;
                metric_distance_computations+=size;

                unsigned int *datal = (unsigned int *) (data + 1);
                for (int i = 0; i < size; i++) {
                    unsigned int candidate = datal[i];
                    if (candidate < 0 || candidate > capacity_)
                        throw std::runtime_error("cand error");
                    dist_t distance = distance_function_(query_data, getDataByInternalId(candidate), distance_function_parameters__);
                    if (distance < current_distance) {
                        current_distance = distance;
                        current_obj = candidate;
                        changed = true;
                    }
                }
            }
        }

        std::priority_queue<std::pair<dist_t, unsigned int>, std::vector<std::pair<dist_t, unsigned int>>, CompareByFirst> Top_K;
        bool bare_bone_search = !deleted_count_ && !isIdAllowed;
        if (bare_bone_search) {
            Top_K = searchBaseLayerST<true>(
                    current_obj, query_data, std::max(efSearch_, k), isIdAllowed);
        } else {
            Top_K = searchBaseLayerST<false>(
                    current_obj, query_data, std::max(efSearch_, k), isIdAllowed);
        }

        while (Top_K.size() > k) {
            Top_K.pop();
        }
        while (Top_K.size() > 0) {
            std::pair<dist_t, unsigned int> top = Top_K.top();
            result.push(std::pair<dist_t, size_t>(top.first, getExternalLabel(top.second)));
            Top_K.pop();
        }
        return result;
    }


    std::vector<std::pair<dist_t, size_t >>
    searchStopConditionClosest(
        const void *query_data,
        BaseSearchStopCondition<dist_t>& stop_condition,
        BaseFilterFunctor* isIdAllowed = nullptr) const {
        std::vector<std::pair<dist_t, size_t >> result;
        if (cur_element_count == 0) return result;

        unsigned int current_obj = enterpoint_node_;
        dist_t current_distance = distance_function_(query_data, getDataByInternalId(enterpoint_node_), distance_function_parameters__);

        for (int level = max_level_; level > 0; level--) {
            bool changed = true;
            while (changed) {
                changed = false;
                unsigned int *data;

                data = (unsigned int *) get_linklist(current_obj, level);
                int size = getListCount(data);
                metric_hops++;
                metric_distance_computations+=size;

                unsigned int *datal = (unsigned int *) (data + 1);
                for (int i = 0; i < size; i++) {
                    unsigned int candidate = datal[i];
                    if (candidate < 0 || candidate > capacity_)
                        throw std::runtime_error("cand error");
                    dist_t distance = distance_function_(query_data, getDataByInternalId(cand), distance_function_parameters__);

                    if (distance < current_distance) {
                        current_distance = distance;
                        current_obj = candidate;
                        changed = true;
                    }
                }
            }
        }

        std::priority_queue<std::pair<dist_t, unsigned int>, std::vector<std::pair<dist_t, unsigned int>>, CompareByFirst> Top_K;
        Top_K = searchBaseLayerST<false>(current_obj, query_data, 0, isIdAllowed, &stop_condition);

        size_t size = Top_K.size();
        result.resize(size);
        while (!Top_K.empty()) {
            result[--size] = Top_K.top();
            Top_K.pop();
        }

        stop_condition.filter_results(result);

        return result;
    }


    void checkIntegrity() {
        int connections_checked = 0;
        std::vector <int > inbound_connections_num(element_count_, 0);
        for (int i = 0; i < element_count_; i++) {
            for (int l = 0; l <= element_levels_[i]; l++) {
                unsigned int *link_current = get_neighbors_at_level(i, l);
                int size = getListCount(link_current);
                unsigned int *data = (unsigned int *) (link_current + 1);
                std::unordered_set<unsigned int> this_set;
                for (int j = 0; j < size; j++) {
                    assert(data[j] < element_count_);
                    assert(data[j] != i);
                    inbound_connections_num[data[j]]++;
                    this_set.insert(data[j]);
                    connections_checked++;
                }
                assert(this_set.size() == size);
            }
        }
        if (element_count_ > 1) {
            int min1 = inbound_connections_num[0], max1 = inbound_connections_num[0];
            for (int i=0; i < element_count_; i++) {
                assert(inbound_connections_num[i] > 0);
                min1 = std::min(inbound_connections_num[i], min1);
                max1 = std::max(inbound_connections_num[i], max1);
            }
            std::cout << "Min inbound: " << min1 << ", Max inbound:" << max1 << "\n";
        }
        std::cout << "integrity ok, checked " << connections_checked << " connections\n";
    }
};


