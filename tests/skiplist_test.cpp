#include <gtest/gtest.h>
#include "../src/skiplist.hpp"
#include <random>
#include <set>
#include <unordered_set>
#include <mutex>
#include <thread>
#include <atomic>

/* ///////////////////
SINGLE-THREADED TESTING 
*/ ///////////////////

TEST(SkipListTest, BasicConcurrency) {
    SkipList<int, std::string> list;

    constexpr int THREADS = 4;
    constexpr int INSERTS_PER_THREAD = 2;

    auto worker = [&](int tid) {
        for (int i = 0; i < INSERTS_PER_THREAD; ++i) {
            int key = tid * INSERTS_PER_THREAD + i;
            bool stfu = list.add(key, "val_" + std::to_string(key));
            ASSERT_TRUE(list.contains(key));
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < THREADS; ++i)
        threads.emplace_back(worker, i);

    for (auto& t : threads)
        t.join();
}




int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
