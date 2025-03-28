#pragma once
#include <queue>
#include <vector>
#include <iostream>
#include <string.h>

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
   