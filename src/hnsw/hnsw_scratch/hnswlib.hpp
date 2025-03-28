#pragma once
#include <queue>
#include <vector>
#include <iostream>
#include <string.h>
#include <utility>
#include <string>


template<typename dist_t>
class BaseSearchStopCondition {
public:
    virtual void add_point_to_result(size_t label, const void* datapoint, dist_t dist) = 0;
    virtual void remove_point_from_result(size_t label, const void* datapoint, dist_t dist) = 0;

    [[nodiscard]] virtual bool should_stop_search(dist_t candidate_dist, dist_t lowerBound) const = 0;
    [[nodiscard]] virtual bool should_consider_candidate(dist_t candidate_dist, dist_t lowerBound) const = 0;
    [[nodiscard]] virtual bool should_remove_extra() const = 0;

    virtual void filter_results(std::vector<std::pair<dist_t, size_t>>& candidates) = 0;

    virtual ~BaseSearchStopCondition() = default;
};


template <typename T>
void writeBinaryPOD(std::ostream& out, const T& podRef) {
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    out.write(reinterpret_cast<const char*>(&podRef), sizeof(T));
}

template <typename T>
void readBinaryPOD(std::istream& in, T& podRef) {
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
    in.read(reinterpret_cast<char*>(&podRef), sizeof(T));
}

template <typename T>
class pairGreater {
 public:
    bool operator()(const T& p1, const T& p2) {
        return p1.first > p2.first;
    }
};
template <typename T>
class pairLesser {
 public:
    bool operator()(const T& p1, const T& p2) {
        return p1.first < p2.first;
    }
};

template <typename Scalar, typename DistanceFunction>
class SpaceInterface {
public:
    // These are pure virtual functions and will be overwritten by implementations in all non-core HNSW headers.
    virtual size_t get_data_size() const = 0;
    virtual size_t get_dim() const = 0;
    virtual DistanceFunction get_distance_function_() const = 0;
    virtual ~SpaceInterface() = default;
};

// This can be extended to store state for filtering (e.g. from a std::set)
class BaseFilterFunctor {
    public:
       virtual bool operator()(size_t id) { return true; }
       virtual ~BaseFilterFunctor() {};
   };

   template<typename dist_t>
class AlgorithmInterface {
public:
    virtual void addPoint(const void* datapoint, size_t label, bool replace_deleted = false) = 0;

    [[nodiscard]] virtual std::priority_queue<std::pair<dist_t, size_t>>
        SearchKNN(const void* query, size_t k, BaseFilterFunctor* filter = nullptr) const = 0;

    [[nodiscard]] virtual std::vector<std::pair<dist_t, size_t>>
        SearchKNNCloserFirst(const void* query, size_t k, BaseFilterFunctor* filter = nullptr) const;

    virtual void saveIndex(const std::string& location) = 0;

    virtual ~AlgorithmInterface() = default;
};


template<typename dist_t>
std::vector<std::pair<dist_t, size_t>>
AlgorithmInterface<dist_t>::SearchKNNCloserFirst(const void* query, size_t k,
                                                 BaseFilterFunctor* filter) const {
    auto heap = SearchKNN(query, k, filter);

    std::vector<std::pair<dist_t, size_t>> result(heap.size());
    for (size_t i = heap.size(); i > 0; --i) {
        result[i - 1] = std::move(heap.top());
        heap.pop();
    }
    return result;
}