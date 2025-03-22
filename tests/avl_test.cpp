#include <gtest/gtest.h>
#include <random>
#include <set>
#include <thread>
#include <atomic>
#include <vector>
#include <random>
#include <unordered_set>
#include <mutex>
#include "../src/avl_lockfree.hpp"

/* ///////////////////
SINGLE-THREADED TESTING 
*/ ///////////////////

TEST(AVLTreeTest, SetAndGetTest) {
    AVLTree<int,int> tree;
    tree.set(10, 100);
    tree.set(20, 200);
    tree.set(30, 300);
    EXPECT_EQ(tree.get(10), 100);
    EXPECT_EQ(tree.get(20), 200);
    EXPECT_EQ(tree.get(30), 300);
    EXPECT_EQ(tree.get(40), std::nullopt); 
}

TEST(AVLTreeTest, DeleteTest) {
    AVLTree<int,int> tree;
    tree.set(10, 100);
    tree.set(20, 200);
    tree.set(30, 300);
    EXPECT_TRUE(tree.exists(20)); 
    tree.del(20);
    EXPECT_FALSE(tree.exists(20)); 
    EXPECT_EQ(tree.get(20), std::nullopt);
}

TEST(AVLTreeTest, AVLBalanceLLTest) {
    AVLTree<int,int> tree;    
    tree.set(3, 300);
    tree.set(2, 200);
    tree.set(1, 100); 
    EXPECT_EQ(tree.get(1), 100);
    EXPECT_EQ(tree.get(2), 200);
    EXPECT_EQ(tree.get(3), 300);
}

TEST(AVLTreeTest, AVLBalanceRRTest) {
    AVLTree<int,int> tree;    
    tree.set(1, 100);
    tree.set(2, 200);
    tree.set(3, 300); 
    EXPECT_EQ(tree.get(1), 100);
    EXPECT_EQ(tree.get(2), 200);
    EXPECT_EQ(tree.get(3), 300);
}

TEST(AVLTreeTest, AVLBalanceLRTest) {
    AVLTree<int,int> tree;
    
    tree.set(3, 300);
    tree.set(1, 100);
    tree.set(2, 200);

    EXPECT_EQ(tree.get(1), 100);
    EXPECT_EQ(tree.get(2), 200);
    EXPECT_EQ(tree.get(3), 300);
}

TEST(AVLTreeTest, AVLBalanceRLTest) {
    AVLTree<int,int> tree;
    
    tree.set(1, 100);
    tree.set(3, 300);
    tree.set(2, 200);

    EXPECT_EQ(tree.get(1), 100);
    EXPECT_EQ(tree.get(2), 200);
    EXPECT_EQ(tree.get(3), 300);
    
    // The tree should now be balanced:
    //     2
    //    / \
    //   1   3
}

TEST(AVLTreeTest, ExistsTest) {
    AVLTree<int,int> tree;
    
    tree.set(5, 500);
    tree.set(10, 1000);
    tree.set(15, 1500);

    EXPECT_TRUE(tree.exists(10));
    EXPECT_FALSE(tree.exists(20));
}

TEST(AVLTreeTest, UpdateValueTest) {
    AVLTree<int,int> tree;
    
    tree.set(10, 100);
    EXPECT_EQ(tree.get(10), 100);

    tree.set(10, 200); 
    EXPECT_EQ(tree.get(10), 200);
}

TEST(AVLTreeTest, ComplexInsertTest) {
    AVLTree<int,int> tree;
    
    tree.set(50, 500);
    tree.set(30, 300);
    tree.set(70, 700);
    tree.set(20, 200);
    tree.set(40, 400);
    tree.set(60, 600);
    tree.set(80, 800);

    EXPECT_EQ(tree.get(50), 500);
    EXPECT_EQ(tree.get(30), 300);
    EXPECT_EQ(tree.get(70), 700);
    EXPECT_EQ(tree.get(20), 200);
    EXPECT_EQ(tree.get(40), 400);
    EXPECT_EQ(tree.get(60), 600);
    EXPECT_EQ(tree.get(80), 800);

    // The tree should be balanced:
    //          50
    //        /    \
    //      30      70
    //     /  \    /  \
    //   20   40  60  80
}

TEST(AVLTreeTest, DeleteAndBalanceTest) {
    AVLTree<int,int> tree;
    
    tree.set(50, 500);
    tree.set(30, 300);
    tree.set(70, 700);
    tree.set(20, 200);
    tree.set(40, 400);
    tree.set(60, 600);
    tree.set(80, 800);

    tree.del(20);
    tree.del(30);
    tree.del(40);

    EXPECT_FALSE(tree.exists(20));
    EXPECT_FALSE(tree.exists(30));
    EXPECT_FALSE(tree.exists(40));

    // Tree should still be balanced:
    //        60
    //       /  \
    //      50   70
    //            \
    //             80
}

TEST(AVLTreeTest, DeleteSingleNodeTest) {
    AVLTree<int, int> tree;
    tree.set(10, 100);
    tree.del(10);
    EXPECT_EQ(tree.get(10), std::nullopt);
}
TEST(AVLTreeTest, DeleteRootOneChildTest) {
    AVLTree<int, int> tree;
    tree.set(10, 100);
    tree.set(5, 50);
    tree.del(10);
    EXPECT_EQ(tree.get(10), std::nullopt);
    EXPECT_EQ(tree.get(5), 50);
}
TEST(AVLTreeTest, DeleteWithTwoChildrenTest) {
    AVLTree<int, int> tree;
    tree.set(10, 100);
    tree.set(5, 50);
    tree.set(15, 150);
    tree.set(12, 120); 
    tree.del(10);
    EXPECT_EQ(tree.get(10), std::nullopt);
    EXPECT_EQ(tree.get(12), 120); 
}
TEST(AVLTreeTest, DuplicateInsertTest) {
    AVLTree<int, int> tree;
    tree.set(42, 100);
    tree.set(42, 999); 
    EXPECT_EQ(tree.get(42), 999);
}
TEST(AVLTreeTest, DegenerateInputBalanceTest) {
    AVLTree<int, int> tree;
    for (int i = 1; i <= 100; ++i)
        tree.set(i, i * 10);
    for (int i = 1; i <= 100; ++i)
        EXPECT_EQ(tree.get(i), i * 10);
}
TEST(AVLTreeTest, LargeDeleteCascadeTest) {
    AVLTree<int, int> tree;
    for (int i = 0; i < 50; ++i)
        tree.set(i, i + 1);
    for (int i = 0; i < 50; ++i)
        tree.del(i);
    for (int i = 0; i < 50; ++i)
        EXPECT_EQ(tree.get(i), std::nullopt);
}
TEST(AVLTreeTest, BalanceCheckTest) {
    AVLTree<int, int> tree;
    for (int i = 0; i < 1000; ++i)
        tree.set(i, i);
}
TEST(AVLTreeTest, GetOnEmptyTreeTest) {
    AVLTree<int, int> tree;
    EXPECT_EQ(tree.get(42), std::nullopt);
}
TEST(AVLTreeTest, SetDelSetAgainTest) {
    AVLTree<int, int> tree;
    tree.set(99, 123);
    tree.del(99);
    EXPECT_EQ(tree.get(99), std::nullopt);

    tree.set(99, 456); 
    EXPECT_EQ(tree.get(99), 456);
}
TEST(AVLTreeTest, ReverseSortedInputTest) {
    AVLTree<int, int> tree;
    for (int i = 100; i >= 1; --i)
        tree.set(i, i * 10);
    for (int i = 100; i >= 1; --i)
        EXPECT_EQ(tree.get(i), i * 10);
}
TEST(AVLTreeTest, RandomInsertDeleteFuzzTest) {
    AVLTree<int, int> tree;
    std::set<int> inserted;
    std::mt19937 rng(42); // consistent seed
    std::uniform_int_distribution<int> dist(1, 1000);
    while (inserted.size() < 500) {
        int key = dist(rng);
        inserted.insert(key);
        tree.set(key, key * 10);
    }
    for (int key : inserted)
        EXPECT_EQ(tree.get(key), key * 10);
    int count = 0;
    for (int key : inserted) {
        if (count++ % 2 == 0) {
            tree.del(key);
            EXPECT_EQ(tree.get(key), std::nullopt);
        } else {
            EXPECT_EQ(tree.get(key), key * 10);
        }
    }
}

TEST(AVLTreeTest, MultithreadedInsertGetDelete) {
    AVLTree<int, int> tree;
    constexpr int NUM_THREADS = 64;
    constexpr int OPS_PER_THREAD = 1000;

    std::atomic<int> inserted{0};
    std::mutex ground_truth_mutex;
    std::unordered_set<int> ground_truth;

    auto worker = [&](int tid) {
        std::mt19937 rng(tid * 100 + 42);
        std::uniform_int_distribution<int> dist(1, 1000);

        for (int i = 0; i < OPS_PER_THREAD; ++i) {
            int op = rng() % 6;
            int key = dist(rng);
            int val = key * 10;

            switch (op) {
                case 0:
                case 1:
                case 2:  // 3/6 = 50% chance insert
                    tree.set(key, val);
                    {
                        std::lock_guard<std::mutex> lock(ground_truth_mutex);
                        ground_truth.insert(key);
                    }
                    inserted++;
                    break;
                case 3:
                case 4:  // 2/6 = ~33% chance get
                    {
                        auto got = tree.get(key);
                        if (got) {
                            EXPECT_EQ(*got, key * 10);
                        }
                    }
                    break;
                case 5:  // 1/6 = ~17% chance delete
                    tree.del(key);
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

    for (auto& t : threads) t.join();

    for (int key : ground_truth) {
        EXPECT_TRUE(tree.exists(key));
        auto got = tree.get(key);
        EXPECT_TRUE(got.has_value());
        EXPECT_EQ(*got, key * 10);
    }

    std::cout << "[TEST] Total inserted: " << inserted << std::endl;
}

int main(int argc, char **argv) {

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();


}
