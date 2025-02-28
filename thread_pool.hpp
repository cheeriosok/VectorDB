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

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_); // destructor secures a lock
            stop = true; // set stop condition to true to let workers know to leave
        }
        condition_.notify_all(); // ^^ 
        /*
        Joining before ending reasons:
        If ThreadPool is destroyed without joining threads, the threads:
        Might try to access memory that no longer exists.
        Could cause undefined behavior or crashes.
        */
        for (auto& thread : threads_) { // ^^
            if (thread.joinable()) { // ^^ Is it joinable?
                thread.join(); // ^^ Then we join.  Must check joinable before join to prevent crashes.
            }
        }
    }
    // delete copy constructor 3 reasons - we're dealing with shared states, threads cant be copied and concurrency makes copying unsafe
    ThreadPool(const ThreadPool&) = delete; . 
    ThreadPool& operator=(const ThreadPool&) = delete;
    // default move constructor - if necessary to transfer ownership we default this behavior! 
    ThreadPool(ThreadPool&&) noexcept = default;
    ThreadPool& operator=(ThreadPool&&) noexcept = default;

    template<typename F, typename... Args>  // Template for function
    auto enqueue(F&& f, Args&&... args) // Templated function signature - we accept a function and its arguments respetively. notice they're all RValues.
        -> std::future<typename std::result_of<F, Args...>::type>  // the return type is wrapped in future && result_of to retrieve the result / return type asynchonously
    {
        using return_type = typename std::result_of<F, Args...>::type; // extracts the return type of our templated argument and assigns to return_type with using directive

        auto task = std::make_shared<std::packaged_task<return_type()> >( // make_shared pointer for our packaged_task with expected return_type. 
            std::bind(std::forward<F>(f), std::forward<Args>(args)...) // we create a callable object that stores F and its arguments to ensure asynchronous execution by a worker thread.
        ); // using shared to easily move the task queue and execute later without ownnership issues. 

        /*
        Retrieves a std::future<return_type> from the packaged_task.
        The future allows the caller to retrieve the result later.
        */
        std::future<return_type> res = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_); // lock task queue to prevent race conditions
            if (stop) { // if stopped (error or manual), throw runtime_error
                throw std::runtime_error("Cannot enqueue on stopped ThreadPool");
            }

            tasks_.emplace([task]() { (*task)(); }); // nasty looking lambda function - when the worker threads retrives the task, t executes task. emplace to construct directly in the queue.
        } 
        condition_.notify_one();
        return res;
    }

    [[nodiscard]] size_t thread_count() const noexcept { return threads_.size(); } //  get threads_ vector size
    [[nodiscard]] size_t queue_size() const { // get tasks queue size. must lock. if a thread grabs a task while we're reading, we can crash or corrupt our return value. 
        std::lock_guard<std::mutex> lock(mutex_); // smart lock, behaves like a unique or shared ptr, automatically lifted upon function termnination. 
        return tasks_.size();
    }

    void wait_for_tasks() { // Blocks the calling thread until all takss are processed
        std::unique_lock<std::mutex> lock(mutex_); // only one thread can access the resource at a tim 
        // Note this is a lambda function -  void wait(std::unique_lock<std::mutex>& lock, Predicate pred); where predicate is a lambda function that returns a boolean. 
        condition_.wait(lock, [this] { // suspend the calling thread until a notification is received by condition
            return tasks_.empty(); // condition_.wait() will keep waiting until tasks.empty() is true. 
        });
    }

private:
    void worker() {
        while (true) { 
            std::function<void()> task; // Declare a function object to hold a task that the worker will execute
            {
                std::unique_lock<std::mutex> lock(mutex_); // create a lock on the mutex to ensure exclusive access to the task queue
                
                condition_.wait(lock, [this] { // put the thread to sleep unless 'stop' is true or there is at least one task in the queue.
                    return stop || !tasks_.empty();
                });
        
                if (stop && tasks_.empty()) { // if 'stop' is true and there are no remaining tasks...
                    return; //exit the worker thread.
                }
        
                task = std::move(tasks_.front()); // move the first task from the queue into 'task' (avoids unnecessary copying)
                tasks_.pop(); // remove the retrieved task from the queue
            } // mutex automatically unlocks here as 'lock' goes out of scope
        
            task(); // execute the task.
        }
        
    }

    std::vector<std::thread> threads_; // a vector of std::threads
    std::queue<std::function<void()>> tasks_; // a queue of tasks! 

    mutable std::mutex mutex_; // Locking a resource so no other threads can grab it. *** We should replace this with a more efficient method for concurrency ***
    std::condition_variable condition_; /
    /* std::condition_variable is a synchronization primitive that is used to safely make threads wait until a condition is met instead of busy-waiting. 
    Threads sleep and are awoken by API calls like notify_one() or notify_all() to 'wake' the threads and respond to a tasks or termination. 
     */
    bool stop{false}; // this will be our terminate() flag, when this is true, all woken threads will check it and handle termination/joining.
};

} // namespace threading

#endif // THREAD_POOL_HPP
