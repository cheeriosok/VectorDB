#pragma once;

#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <cstdint>

class HNSWNode {
private:
    std::mutex node_lock_;  
    std::chrono::steady_clock::time_point timestamp_;
    std::atomic<bool> deleted_;
    size_t id_;
    size_t level_;
    float norm_;
    static constexpr size_t DIM = 128;
    std::array<float, DIM> embedding_;
    std::vector<std::vector<std::weak_ptr<HNSWNode>>> neighbors_;
    float dot(const std::array<float, DIM>& a, const std::array<float, DIM>& b) {
        float sum = 0.0f;
        for (size_t i = 0; i < DIM; ++i) {
            sum += a[i] * b[i];
        }
        return sum;
    }

public:
    HNSWNode(size_t id, size_t level, const std::array<float, DIM>& embedding) : 
        id_(id),
        level_(level), 
        embedding_(embedding), 
        norm_(sqrt(dot(embedding, embedding))),
        timestamp_(std::chrono::steady_clock::now()),
        deleted_(false),
        neighbors_(level + 1) {}
    
    void del() {
        HNSWNode.deleted_ = true;
    }
    
};



