#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <mutex>
#include "../src/skiplist.hpp"

constexpr int TOTAL_OPS = 500000;

enum Operation { SET = 0, GET = 1, DEL = 2 };

struct Op {
    Operation type;
    int key;
    int val;
};

std::vector<Op> generate_ops(int num_ops, int seed) {
    std::vector<Op> ops;
    ops.reserve(num_ops);

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist_key(1, 1'000'000);
    std::uniform_int_distribution<int> dist_op(0, 2);  // set, get, del

    for (int i = 0; i < num_ops; ++i) {
        int key = dist_key(rng);
        ops.push_back(Op{
            static_cast<Operation>(dist_op(rng)),
            key,
            key * 10
        });
    }
    return ops;
}

void run_single_threaded_benchmark(const std::vector<Op>& ops) {
    SkipList<int, int> sl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ops.size(); ++i) {
        const auto& op = ops[i];
        switch (op.type) {
            case SET: sl.set(op.key, op.val); break;
            case GET: {
                int dummy;
                sl.get(op.key, dummy);
                break;
            }
            case DEL: sl.del(op.key); break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "[DEBUG] ops address: " << static_cast<const void*>(ops.data()) << "\n";

    std::cout << "[Single-threaded] " << ops.size() << " ops in "
              << elapsed_ms << " ms (" << (ops.size() / (elapsed_ms / 1000)) << " ops/sec)\n";
}

void run_multi_threaded_benchmark(const std::vector<Op>& ops, int num_threads) {
    std::cout << "[DEBUG] Entered multi-threaded benchmark\n"; // <--- add this
    SkipList<int, int> sl;
    std::atomic<int> total_ops = 0;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    int ops_per_thread = ops.size() / num_threads;
    std::mutex outer_lock;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([=, &sl, &total_ops, &outer_lock]() {  // ← value-capture ops
            int begin = t * ops_per_thread;
            int end = (t == num_threads - 1) ? ops.size() : begin + ops_per_thread;
    
            for (int i = begin; i < end; ++i) {
                const auto& op = ops[i];
                std::lock_guard<std::mutex> lock(outer_lock);
                switch (op.type) {
                    case SET: sl.set(op.key, op.val); break;
                    case GET: { int dummy; sl.get(op.key, dummy); break; }
                    case DEL: sl.del(op.key); break;
                }
                total_ops++;
            }
        });
    }
    

    for (auto& t : threads) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "[DEBUG] ops address: " << static_cast<const void*>(ops.data()) << "\n";

    std::cout << "[Multi-threaded] " << total_ops << " ops in "
              << elapsed_ms << " ms (" << (total_ops / (elapsed_ms / 1000)) << " ops/sec) with "
              << num_threads << " threads\n";

    std::cout << "[DEBUG] Multi-threaded benchmark completed\n";
}

int main() {
    constexpr int THREADS = 8;

    std::cout << "\n--- SkipList Performance Benchmark (" << TOTAL_OPS << " Mixed Ops) ---\n\n";
    auto ops = generate_ops(TOTAL_OPS, 42);

    run_single_threaded_benchmark(ops);
    run_multi_threaded_benchmark(ops, THREADS);

    std::cout << "[DEBUG] Main exiting cleanly\n";
    return 0;
}
