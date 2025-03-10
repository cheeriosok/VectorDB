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

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) { // explicit to prevent accidental conversion
        if (num_threads == 0) {
            throw std::invalid_argument("Thread pool size must be positive");
        }

        try {
            threads_.reserve(num_threads); // Makes space in the threads_ vector so we don't need to keep resizing it - important property for any container.
            for (size_t i = 0; i < num_threads; ++i) {  // for each thread...
                threads_.emplace_back(&ThreadPool::worker, this); // Constructs the thread in-place, each thread runs the worker function.
            }
        } catch (...) { // if anything fails, we'll catch all possible exceptions using ... and do the following:
            stop = true; // stop flag to let all workers know to leave the premises
            condition_.notify_all(); // tell all workers condition has been changed.
            for (auto& thread : threads_) {  // for each thread in threads_ vector...
                if (thread.joinable()) { // join all running threeads
                    thread.join(); 
                }
            }
            throw; // rethrow an exception
        }
    }

        /*
        Joining before ending reasons:
        If ThreadPool is destroyed without joining threads, the threads:
        Might try to access memory that no longer exists.
        Could cause undefined behavior or crashes.
        */
        ~ThreadPool() {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stop = true;
            }
            condition_.notify_all();
        
            for (auto& thread : threads_) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
        }
        
       
    // delete copy constructor 3 reasons - we're dealing with shared states, threads cant be copied and concurrency makes copying unsafe
    ThreadPool(const ThreadPool&) = delete; 
    ThreadPool& operator=(const ThreadPool&) = delete;
    // default move constructor - if necessary to transfer ownership we default this behavior! 
    ThreadPool(ThreadPool&&) noexcept = delete;
    ThreadPool& operator=(ThreadPool&&) noexcept = delete;


    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) {
        using return_type = std::invoke_result_t<F, Args...>; // Fix: Explicitly declare return_type
    
        auto task = std::make_shared<std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
    
        std::future<return_type> res = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            if (stop) {
                throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
            }
    
            tasks_.emplace([task]() { (*task)(); });
        }
        
        condition_.notify_one();
        return res;
    }
    


    [[nodiscard]] size_t thread_count() const noexcept { return threads_.size(); } //  get threads_ vector size
    [[nodiscard]] size_t queue_size() const { // get tasks queue size. must lock. if a thread grabs a task while we're reading, we can crash or corrupt our return value. 
        std::lock_guard<std::mutex> lock(mutex_); // smart lock, behaves like a unique or shared ptr, automatically lifted upon function termnination. 
        return tasks_.size();
    }

    void wait_for_tasks() { 
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return tasks_.empty() && active_workers_.load() == 0; });
    }
    
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop = true;
        }
        condition_.notify_all();
    }

private:
    
void worker() {
    while (true) { 
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { return stop.load() || !tasks_.empty(); });

            if (stop && tasks_.empty()) return; // Exit only when tasks are completely finished

            task = std::move(tasks_.front());
            tasks_.pop();
            active_workers_++; // Increment active workers
        } 
        task(); // Execute task
        active_workers_--; // Decrement active workers after execution
        condition_.notify_all(); // Notify that a task is done
    }
}





    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_; // a queue of tasks! 
    std::atomic<size_t> active_workers_{0};
    mutable std::mutex mutex_; // Locking a resource so no other threads can grab it. *** We should replace this with a more efficient method for concurrency ***
    std::condition_variable condition_; 
    /* std::condition_variable is a synchronization primitive that is used to safely make threads wait until a condition is met instead of busy-waiting. 
    Threads sleep and are awoken by API calls like notify_one() or notify_all() to 'wake' the threads and respond to a tasks or termination. 
     */
    std::atomic<bool> stop{false}; // this will be our terminate() flag, when this is true, all woken threads will check it and handle termination/joining.
};

#endif // THREAD_POOL_HPP
