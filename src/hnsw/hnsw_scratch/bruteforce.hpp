/*
    Basic brute-force nearest neighbor search index that stores fixed-size vectors in contiguous memory and compares a query
    against all stored vectors using a user-defined distance function. It's thread safe and supports put/del/load/search by label.
*/

#pragma once
#include <unordered_map>
#include <fstream>
#include <mutex>
#include <algorithm>
#include <assert.h>

template <typename dist_t>
class BruteforceSearch : public AlgorithmInterface<dist_t>
{
public:
    char *data_;
    size_t data_size_;

    size_t element_count_;
    size_t element_stride_;

    size_t capacity_;

    DISTFUNC<dist_t> distance_function_;
    void *distance_function_parameters_;
    std::mutex index_lock;

    std::unordered_map<label_type, size_t> label_to_index_;
    // default constructor for loading an index via loadIndex
    BruteforceSearch(SpaceInterface<dist_t> *s)
        : data_(nullptr),
          capacity_(0),
          element_count_(0),
          element_stride_(0),
          data_size_(0),
          distance_function_parameters_(nullptr)
    {
    }

    // constructor for loading a saved index from disk
    BruteforceSearch(SpaceInterface<dist_t> *s, const std::string &location)
        : data_(nullptr),
          capacity_(0),
          element_count_(0),
          element_stride_(0),
          data_size_(0),
          distance_function_parameters_(nullptr)
    {
        loadIndex(location, s);
    }

    // constructor for creating a new empty index with size up to maxElements
    BruteforceSearch(SpaceInterface<dist_t> *space, size_t maxElements)
    {
        capacity_ = maxElements;
        data_size_ = space->get_data_size();
        distance_function_ = space->get_dist_func();
        distance_function_parameters_ = space->get_distance_function_parameters_ram();
        element_stride_ = data_size_ + sizeof(label_type);
        data_ = (char *)malloc(maxElements * element_stride_);
        if (data_ == nullptr)
            throw std::runtime_error("Not enough memory: BruteforceSearch failed to allocate data");
        element_count_ = 0;
    }

    ~BruteforceSearch()
    {
        free(data_);
    }

    /*
    --AddPoint:--
    1. Locks the index
    2. If label exists, it overwrites the old data at the same location
    3. If label is new:
        3a. If capacity is full: throws
        3b. Otherwise, appends to the end
    4. Stores:
        - Vector → data_ + idx * element_stride_
        -  Label → data_ + idx * element_stride_ + data_size_
    */

    void addPoint(const void *datapoint, label_type label, bool replace_deleted = false)
    {
        int idx;
        {
            std::unique_lock<std::mutex> lock(index_lock);

            auto search = label_to_index_.find(label);
            if (search != label_to_index_.end())
            {
                idx = search->second;
            }
            else
            {
                if (element_count_ >= capacity_)
                {
                    throw std::runtime_error("The number of elements exceeds the specified limit\n");
                }
                idx = element_count_;
                label_to_index_[label] = idx;
                element_count_++;
            }
        }
        memcpy(data_ + element_stride_ * idx + data_size_, &label, sizeof(label_type));
        memcpy(data_ + element_stride_ * idx, datapoint, data_size_);
    }

    /*
    --RemovePoint--
    1. Locks the index
    2. Removes label from the map
    3. Swaps last element into deleted element’s slot (in-place compaction)
    4. Updates label_to_index_ for the moved element
    5. Decrements element_count_
    */

    void removePoint(label_type cur_external)
    {
        std::unique_lock<std::mutex> lock(index_lock);

        auto found = label_to_index_.find(cur_external);
        if (found == label_to_index_.end())
        {
            return;
        }

        label_to_index_.erase(found);

        size_t cur_c = found->second;
        label_type label = *((label_type *)(data_ + element_stride_ * (element_count_ - 1) + data_size_));
        label_to_index_[label] = cur_c;
        memcpy(data_ + element_stride_ * cur_c,
               data_ + element_stride_ * (element_count_ - 1),
               data_size_ + sizeof(label_type));
        element_count_--;
    }

    // Brute-Force KNN. Check if k <= element_count_....
    // Initialize a MaxHeap to track top K.
    // For the first K elements...
    // Compute distance to Query
    // Push into heap if allowed by filterfnc
    // Commputes distance from rest of dataset, if better
    // than worst topResults than add and maintain K by popping the worst
    std::priority_queue<std::pair<dist_t, label_type>>
    SearchKNN(const void *query, size_t K, BaseFilterFunctor *filter = nullptr) const
    {
        assert(k <= element_count_);
        std::priority_queue<std::pair<dist_t, label_type>> topResults;
        if (element_count_ == 0)
            return topResults;
        for (int i = 0; i < K; i++)
        {
            dist_t dist = distance_function_(query, data_ + element_stride_ * i, distance_function_parameters_);
            label_type label = *((label_type *)(data_ + element_stride_ * i + data_size_));
            if ((!filter) || (*filter)(label))
            {
                topResults.emplace(dist, label);
            }
        }
        dist_t lastdist = topResults.empty() ? std::numeric_limits<dist_t>::max() : topResults.top().first;
        for (int i = K; i < element_count_; i++)
        {
            dist_t dist = distance_function_(query, data_ + element_stride_ * i, distance_function_parameters_);
            if (dist <= lastdist)
            {
                label_type label = *((label_type *)(data_ + element_stride_ * i + data_size_));
                if ((!filter) || (*filter)(label))
                {
                    topResults.emplace(dist, label);
                }
                if (topResults.size() > K)
                    topResults.pop();

                if (!topResults.empty())
                {
                    lastdist = topResults.top().first;
                }
            }
        }
        return topResults;
    }

    /*
        Opens file, writes plain old data - saving
        a. capacity, b. element size, c. count and d. raw data block
    */
    void saveIndex(const std::string &location)
    {
        std::ofstream output(location, std::ios::binary);
        std::streampos position;

        writeBinaryPOD(output, capacity_);
        writeBinaryPOD(output, element_stride_);
        writeBinaryPOD(output, element_count_);

        output.write(data_, capacity_ * element_stride_);

        output.close();
    }

    /*
        Opens file, reads a, capacity, b.element size, count, distance function and its parameters amnd
        recomputes element_size and allocates memory, reads block, and clsoes file.
    */
    void loadIndex(const std::string &location, SpaceInterface<DistanceType> *space)
    {
        // open the file at 'location' in binary mode for reading  and create a variable to store current file pos
        std::ifstream input(location, std::ios::binary);
        std::streampos position;

        /* reads stored max capacity, vector dimensions, and # of stored elements */
        readBinaryPOD(input, capacity_);
        readBinaryPOD(input, element_stride_);
        readBinaryPOD(input, element_count_);

        // get # of bytes per vec, distance func from space, and any other params require.
        data_size_ = space->get_data_size();
        distance_function_ = space->get_dist_func();
        distance_function_parameters_ = space->get_distance_function_parameters_ram();
        element_stride_ = data_size_ + sizeof(label_type);
        data_ = (char *)malloc(capacity_ * element_stride_);

        if (data_ == nullptr)
            throw std::runtime_error("Not enough memory: loadIndex failed to allocate data");

        input.read(data_, capacity_ * element_stride_);
        input.close();
    }
};
