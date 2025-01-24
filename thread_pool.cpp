#include "thread_pool.hpp"
#include <stdexcept>

namespace threading {

ThreadPool::ThreadPool(size_t num_threads) {
    if (num_threads == 0) {
        throw std::invalid_argument("Thread pool size must be positive");
    }

    try {
        threads_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            threads_.emplace_back(&ThreadPool::worker, this);
        }
    } catch (...) {
        stop_ = true;
        condition_.notify_all();
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        throw;
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void ThreadPool::worker() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] {
                return stop_ || !tasks_.empty();
            });

            if (stop_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

size_t ThreadPool::queue_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

void ThreadPool::wait_all() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] {
        return tasks_.empty();
    });
}

} // namespace threading