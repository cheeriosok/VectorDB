#pragma once

#include "visited_list_pool.hpp"
#include "hnswlib.hpp"
#include <atomic>
#include <random>
#include <stdlib.h>
#include <assert.h>
#include <unordered_set>
#include <list>
#include <memory>

template <typename dist_t>
class HierarchicalNSW : public AlgorithmInterface<dist_t>
{
public:
    static const unsigned int MAX_LABEL_OPERATION_LOCKS = 65536; // Hash labels into a fixed # of mutexes (striped locking)
    static const unsigned char DELETE_MARK = 0x01; // (flag for marking elements as deleted)

    // Index Metadata & Graph Structure
    size_t capacity_{0}; // total allotted capaicty (max num elements)
    mutable std::atomic<size_t> element_count_{0}; // current number of elements
    mutable std::atomic<size_t> deleted_count_{0}; // number of elements MARKED deleted     
    int max_level_{0}; // highest/max level in the multi-layer HNSW
    unsigned int entry_id_{0}; // ID NOde of the current entry point in the graph
    std::vector<int> element_levels_; // keeps level of each element

    // Link Graph Parameters
    size_t link_degree_{0}; // target degree for each node - otherwise known as "M" in HNSW . 
    size_t link_capacity_upper_{0}; // max degree capacity at upper levels (typically equiv to link degree_).
    size_t link_capacity_level0_{0}; // max degree capacity at level 0, (typically 2M)
    size_t beam_build_{0}; // neighbors to evaluate during insertion - efConstruction in whitepaper
    size_t beam_query_{0}; // neighbors to evaluate during search - efSearch in whitepaper

    // Probabilistic Level Generator Parameters
    double level_lambda_{0.0}, inv_lambda_{0.0}; // ;ambda for exponential level assignment and inverse of level_lambda, respectively

    // Memory Layout for Level 0 (Contiguous)
    size_t element_stride_{0}; // full size of one element
    size_t link_stride_{0}; // size of link block in upper levels ???????
    size_t link0_stride_{0}; // 
};