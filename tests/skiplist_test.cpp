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

// TEST(SkipListTest, SetAndGet) {
//     SkipList<int, int> sl;
//     sl.set(10, 100);
//     sl.set(20, 200);
//     sl.set(30, 300);

//     int val;
//     EXPECT_TRUE(sl.get(10, val));
//     EXPECT_EQ(val, 100);
//     EXPECT_TRUE(sl.get(20, val));
//     EXPECT_EQ(val, 200);
//     EXPECT_TRUE(sl.get(30, val));
//     EXPECT_EQ(val, 300);
//     EXPECT_FALSE(sl.get(40, val));
// }

// TEST(SkipListTest, UpdateValue) {
//     SkipList<int, int> sl;
//     sl.set(42, 100);
//     int val;
//     EXPECT_TRUE(sl.get(42, val));
//     EXPECT_EQ(val, 100);
//     sl.set(42, 999);
//     EXPECT_TRUE(sl.get(42, val));
//     EXPECT_EQ(val, 999);
// }

// TEST(SkipListTest, DeleteExistingKey) {
//     SkipList<int, int> sl;
//     sl.set(1, 10);
//     sl.set(2, 20);
//     sl.set(3, 30);

//     sl.del(2);

//     int val;
//     EXPECT_FALSE(sl.get(2, val));
//     EXPECT_TRUE(sl.get(1, val));
//     EXPECT_EQ(val, 10);
//     EXPECT_TRUE(sl.get(3, val));
//     EXPECT_EQ(val, 30);
// }

// TEST(SkipListTest, DeleteNonExistentKey) {
//     SkipList<int, int> sl;
//     sl.set(1, 100);
//     sl.set(2, 200);
//     sl.del(3);  // Key doesn't exist, should do nothing
//     int val;
//     EXPECT_TRUE(sl.get(1, val));
//     EXPECT_TRUE(sl.get(2, val));
// }

// TEST(SkipListTest, LargeInsertGet) {
//     SkipList<int, int> sl;
//     for (int i = 0; i < 1000; ++i) {
//         sl.set(i, i * 10);
//     }
//     int val;
//     for (int i = 0; i < 1000; ++i) {
//         EXPECT_TRUE(sl.get(i, val));
//         EXPECT_EQ(val, i * 10);
//     }
// }

// TEST(SkipListTest, LargeDeleteTest) {
//     SkipList<int, int> sl;
//     for (int i = 0; i < 500; ++i)
//         sl.set(i, i + 1);
//         for (int i = 0; i < 500; ++i) {
//             std::cout << "[TEST] Deleting key: " << i << std::endl;
//             sl.del(i);
//         }
        
//     int val;
//     for (int i = 0; i < 500; ++i)
//         EXPECT_FALSE(sl.get(i, val));
// }

// TEST(SkipListTest, OverwriteManyTimes) {
//     SkipList<int, int> sl;
//     for (int i = 0; i < 100; ++i)
//         sl.set(42, i);
//     int val;
//     EXPECT_TRUE(sl.get(42, val));
//     EXPECT_EQ(val, 99);
// }

// TEST(SkipListTest, ReverseOrderInsert) {
//     SkipList<int, int> sl;
//     for (int i = 100; i >= 1; --i)
//         sl.set(i, i * 10);
//     int val;
//     for (int i = 100; i >= 1; --i) {
//         EXPECT_TRUE(sl.get(i, val));
//         EXPECT_EQ(val, i * 10);
//     }
// }

// TEST(SkipListTest, RandomInsertDeleteFuzz) {
//     SkipList<int, int> sl;
//     std::set<int> inserted;
//     std::mt19937 rng(42);
//     std::uniform_int_distribution<int> dist(1, 1000);

//     while (inserted.size() < 500) {
//         int key = dist(rng);
//         inserted.insert(key);
//         sl.set(key, key * 10);
//     }

//     int val;
//     for (int key : inserted)
//         EXPECT_TRUE(sl.get(key, val)) << "Missing key: " << key;

//     int count = 0;
//     for (int key : inserted) {
//         if (count++ % 2 == 0) {
//             sl.del(key);
//             EXPECT_FALSE(sl.get(key, val));
//         } else {
//             EXPECT_TRUE(sl.get(key, val));
//             EXPECT_EQ(val, key * 10);
//         }
//     }
// }

/* ///////////////////
MULTI-THREADED TESTING 
*/ ///////////////////

TEST(SkipListTest, MultithreadedSetGetDel) {
    SkipList<int, int> sl;
    constexpr int NUM_THREADS = 16;
    constexpr int OPS_PER_THREAD = 1000;

    std::atomic<int> inserted{0};
    std::mutex ground_truth_mutex;
    std::unordered_set<int> ground_truth;

    auto worker = [&](int tid) {
        std::mt19937 rng(tid * 100 + 123);
        std::uniform_int_distribution<int> dist(1, 1000);

        for (int i = 0; i < OPS_PER_THREAD; ++i) {
            int op = rng() % 6;
            int key = dist(rng);
            int val = key * 10;

            switch (op) {
                case 0:
                case 1:
                case 2:  // insert
                    sl.set(key, val);
                    {
                        std::lock_guard<std::mutex> lock(ground_truth_mutex);
                        ground_truth.insert(key);
                    }
                    inserted++;
                    break;
                case 3:
                case 4:  // get
                    {
                        int out;
                        auto result = sl.get(key, out);
                        if (result) EXPECT_EQ(out, key * 10);
                    }
                    break;
                case 5:  // delete
                    sl.del(key);
                    {
                        std::lock_guard<std::mutex> lock(ground_truth_mutex);
                        ground_truth.erase(key);
                    }
                    break;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t)
        threads.emplace_back(worker, t);

    for (auto& t : threads)
        t.join();

    int val;
    for (int key : ground_truth) {
        EXPECT_TRUE(sl.get(key, val));
        EXPECT_EQ(val, key * 10);
    }

    std::cout << "[TEST] Total inserted: " << inserted.load() << std::endl;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
