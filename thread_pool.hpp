#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <vector>
#include <memory>
#include <type_traits>
#include <stdexcept>
#include <mutex>
#include <condition_variable>

namespace threading {

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) {
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

    ~ThreadPool() {
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

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) noexcept = default;
    ThreadPool& operator=(ThreadPool&&) noexcept = default;

    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> 
    {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_) {
                throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
            }

            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return result;
    }

    [[nodiscard]] size_t thread_count() const noexcept { return threads_.size(); }
    [[nodiscard]] size_t queue_size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }

    void wait_all() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] {
            return tasks_.empty();
        });
    }

private:
    void worker() {
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

    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    bool stop_{false};
};

} // namespace threading

#endif // THREAD_POOL_HPP
