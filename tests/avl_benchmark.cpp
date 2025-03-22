

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
    AVLTree<int, int> tree;

    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& op : ops) {
        switch (op.type) {
            case SET: tree.set(op.key, op.val); break;
            case GET: tree.get(op.key); break;
            case DEL: tree.del(op.key); break;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "[Single-threaded] " << ops.size() << " ops in "
              << elapsed_ms << " ms (" << (ops.size() / (elapsed_ms / 1000)) << " ops/sec)\n";
}

void run_multi_threaded_benchmark(const std::vector<Op>& ops, int num_threads) {
    AVLTree<int, int> tree;
    std::atomic<int> total_ops = 0;
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    int ops_per_thread = ops.size() / num_threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            int begin = t * ops_per_thread;
            int end = (t == num_threads - 1) ? ops.size() : begin + ops_per_thread;

            for (int i = begin; i < end; ++i) {
                const auto& op = ops[i];
                switch (op.type) {
                    case SET: tree.set(op.key, op.val); break;
                    case GET: tree.get(op.key); break;
                    case DEL: tree.del(op.key); break;
                }
                total_ops++;
            }
        });
    }

    for (auto& t : threads) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "[Multi-threaded] " << total_ops << " ops in "
              << elapsed_ms << " ms (" << (total_ops / (elapsed_ms / 1000)) << " ops/sec)"
              << " with " << num_threads << " threads\n";
}

int main(int argc, char** argv) {
    const int THREADS = 8;

    std::cout << "\n--- AVL Tree Performance Benchmarks (500k Mixed Ops) ---\n\n";

    auto ops = generate_ops(TOTAL_OPS, 42);  // same ops for both modes

    run_single_threaded_benchmark(ops);
    run_multi_threaded_benchmark(ops, THREADS);

    return 0;
}


