#include <gtest/gtest.h>

/*
AVL TREE TESTS
*/

// #include "../avl.hpp"

// TEST(AVLTreeTest, SetAndGetTest) {
//     AVLTree<int,int> tree;
    
//     tree.set(10, 100);
//     tree.set(20, 200);
//     tree.set(30, 300);

//     EXPECT_EQ(tree.get(10), 100);
//     EXPECT_EQ(tree.get(20), 200);
//     EXPECT_EQ(tree.get(30), 300);
//     EXPECT_EQ(tree.get(40), std::nullopt); // Non-existent key
// }

// TEST(AVLTreeTest, DeleteTest) {
//     AVLTree<int,int> tree;
//     tree.set(10, 100);
//     tree.set(20, 200);
//     tree.set(30, 300);
    
//     EXPECT_TRUE(tree.exists(20)); 
//     tree.del(20);
//     EXPECT_FALSE(tree.exists(20)); 
//     EXPECT_EQ(tree.get(20), std::nullopt); 
// }

// TEST(AVLTreeTest, AVLBalanceTest) {
//     AVLTree<int,int> tree;
    
//     tree.set(3, 300);
//     tree.set(2, 200);
//     tree.set(1, 100); 

//     EXPECT_EQ(tree.get(1), 100);
//     EXPECT_EQ(tree.get(2), 200);
//     EXPECT_EQ(tree.get(3), 300);
// }

// TEST(AVLTreeTest, ExistsTest) {
//     AVLTree<int,int> tree;
    
//     tree.set(5, 500);
//     tree.set(10, 1000);
//     tree.set(15, 1500);

//     EXPECT_TRUE(tree.exists(10));
//     EXPECT_FALSE(tree.exists(20));
// }

// /*
// HASH TABLE TESTS
// */

// #include "../hashtable.hpp"

// // Test HTable insert and retrieval
// TEST(HTableTest, InsertAndGet) {
//     HTable<int, std::string> table;

//     table.insert(1, "one");
//     table.insert(2, "two");
//     table.insert(3, "three");

//     ASSERT_NE(table.lookup(1), nullptr);
//     EXPECT_EQ(table.lookup(1)->value_, "one");

//     ASSERT_NE(table.lookup(2), nullptr);
//     EXPECT_EQ(table.lookup(2)->value_, "two");

//     ASSERT_NE(table.lookup(3), nullptr);
//     EXPECT_EQ(table.lookup(3)->value_, "three");

//     EXPECT_EQ(table.lookup(4), nullptr);  // Key 4 should not exist
// }

// // Test HTable removal
// TEST(HTableTest, Remove) {
//     HTable<int, std::string> table;

//     table.insert(10, "ten");
//     table.insert(20, "twenty");
//     table.insert(30, "thirty");

//     ASSERT_NE(table.lookup(20), nullptr);
//     table.remove(20);
//     EXPECT_EQ(table.lookup(20), nullptr);
// }

// // Test HTable resizing
// TEST(HTableTest, ResizeTest) {
//     HTable<int, std::string> table;

//     for (int i = 0; i < 100; i++) {
//         table.insert(i, "num" + std::to_string(i));
//     }

//     for (int i = 0; i < 100; i++) {
//         ASSERT_NE(table.lookup(i), nullptr);
//         EXPECT_EQ(table.lookup(i)->value_, "num" + std::to_string(i));
//     }
// }

// // Test HMap insert and retrieval
// TEST(HMapTest, InsertAndGet) {
//     HMap<int, std::string> map;

//     map.insert(1, "apple");
//     map.insert(2, "banana");
//     map.insert(3, "cherry");

//     ASSERT_NE(map.find(1), nullptr);
//     EXPECT_EQ(*map.find(1), "apple");  // FIXED

//     ASSERT_NE(map.find(2), nullptr);
//     EXPECT_EQ(*map.find(2), "banana");  // FIXED

//     ASSERT_NE(map.find(3), nullptr);
//     EXPECT_EQ(*map.find(3), "cherry");  // FIXED

//     EXPECT_EQ(map.find(4), nullptr);  // Key 4 should not exist
// }

// // Test HMap removal
// TEST(HMapTest, RemoveTest) {
//     HMap<int, std::string> map;

//     map.insert(10, "ten");
//     map.insert(20, "twenty");
//     map.insert(30, "thirty");

//     ASSERT_NE(map.find(20), nullptr);
//     auto removed = map.remove(20);
//     EXPECT_TRUE(removed.has_value());
//     EXPECT_EQ(removed.value(), "twenty");
//     EXPECT_EQ(map.find(20), nullptr);
// }

// // Test HMap resizing
// TEST(HMapTest, ResizeTest) {
//     HMap<int, std::string> map;

//     for (int i = 0; i < 100; i++) {
//         map.insert(i, "num" + std::to_string(i));
//     }

//     for (int i = 0; i < 100; i++) {
//         ASSERT_NE(map.find(i), nullptr);
//         EXPECT_EQ(*map.find(i), "num" + std::to_string(i));  // FIXED
//     }
// }

// /*
//     Heap Test
// */

// #include "../heap.hpp"

// TEST(HeapItemTest, ConstructorAndAccessTest) {
//     HeapItem<int> item(42);
//     EXPECT_EQ(item.value(), 42);
// }

// TEST(HeapItemTest, MoveConstructorTest) {
//     HeapItem<int> item1(100);
//     HeapItem<int> item2(std::move(item1));
//     EXPECT_EQ(item2.value(), 100);
// }

// TEST(HeapItemTest, ValueModificationTest) {
//     HeapItem<int> item(50);
//     item.value() = 75;
//     EXPECT_EQ(item.value(), 75);
// }

// TEST(HeapItemTest, PositionReferenceSetTest) {
//     HeapItem<int> item(50);
//     std::size_t position = 3;
//     item.set_position(&position);
//     EXPECT_NE(item.position(), nullptr); // Using getter instead of direct access
// }

// TEST(BinaryHeapTest, PushAndTopTest) {
//     BinaryHeap<int> heap(std::less<int>{});
    
//     heap.push(HeapItem<int>(10));
//     heap.push(HeapItem<int>(20));
//     heap.push(HeapItem<int>(5));
    
//     EXPECT_EQ(heap.top().value(), 5); // MinHeap property: 5 is smallest
// }

// TEST(BinaryHeapTest, PopTest) {
//     BinaryHeap<int> heap(std::less<int>{});
    
//     heap.push(HeapItem<int>(15));
//     heap.push(HeapItem<int>(10));
//     heap.push(HeapItem<int>(30));
    
//     EXPECT_EQ(heap.pop().value(), 10); // Smallest element should be removed
//     EXPECT_EQ(heap.pop().value(), 15);
//     EXPECT_EQ(heap.pop().value(), 30);
//     EXPECT_TRUE(heap.empty());
// }

// TEST(BinaryHeapTest, ExceptionHandlingTest) {
//     BinaryHeap<int> heap(std::less<int>{});

//     EXPECT_THROW(
//         {
//             [[maybe_unused]] const auto& val = heap.top();  // Capture as a reference to avoid copying
//         }, 
//         std::out_of_range
//     );

//     EXPECT_THROW(heap.pop(), std::out_of_range);
// }


// /*
//     Thread Pool Test
// */

// #include <chrono>
// #include <thread>
// #include <vector>
// #include <atomic>

// #include "../thread_pool.hpp"

// TEST(ThreadPoolTest, ConstructionAndDestruction) {
//     EXPECT_NO_THROW(ThreadPool pool(4));
// }

// TEST(ThreadPoolTest, BasicTaskExecution) {
//     ThreadPool pool(2);
    
//     auto future = pool.enqueue([] { return 42; });
//     EXPECT_EQ(future.get(), 42);
// }

// TEST(ThreadPoolTest, MultipleTasksExecution) {
//     ThreadPool pool(4);
//     std::vector<std::future<int>> futures;
    
//     for (int i = 0; i < 10; ++i) {
//         futures.push_back(pool.enqueue([i] { return i * i; }));
//     }
    
//     for (int i = 0; i < 10; ++i) {
//         EXPECT_EQ(futures[i].get(), i * i);
//     }
// }

// TEST(ThreadPoolTest, WaitForAllTasks) {
//     ThreadPool pool(2);
    
//     for (int i = 0; i < 5; ++i) {
//         pool.enqueue([] { std::this_thread::sleep_for(std::chrono::milliseconds(100)); });
//     }
    
//     EXPECT_NO_THROW(pool.wait_for_tasks());
// }

// TEST(ThreadPoolTest, ExceptionOnZeroThreads) {
//     EXPECT_THROW(ThreadPool pool(0), std::invalid_argument);
// }

// TEST(ThreadPoolTest, EnqueueAfterShutdown) {
//     ThreadPool pool(2);
//     pool.shutdown();
//     EXPECT_THROW(pool.enqueue([] { return 1; }), std::runtime_error);
// }


// TEST(ThreadPoolTest, ThreadPoolHandlesLargeNumberOfTasks) {
//     ThreadPool pool(4);
//     std::atomic<int> counter{0};
//     const int task_count = 100;
    
//     for (int i = 0; i < task_count; ++i) {
//         pool.enqueue([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
//     }
    
//     pool.wait_for_tasks();
//     EXPECT_EQ(counter.load(), task_count);
// }

// TEST(ThreadPoolTest, VerifyTaskOrderingIsIrrelevant) {
//     ThreadPool pool(2);
//     std::vector<std::future<int>> futures;
    
//     for (int i = 0; i < 10; ++i) {
//         futures.push_back(pool.enqueue([i] { return i; }));
//     }
    
//     std::set<int> results;
//     for (auto& future : futures) {
//         results.insert(future.get());
//     }
    
//     for (int i = 0; i < 10; ++i) {
//         EXPECT_NE(results.find(i), results.end());
//     }
// }

// TEST(ThreadPoolTest, MultipleThreadsExecutingTasks) {
//     ThreadPool pool(4);
//     std::atomic<int> counter{0};
    
//     for (int i = 0; i < 20; ++i) {
//         pool.enqueue([&counter] {
//             std::this_thread::sleep_for(std::chrono::milliseconds(50));
//             counter.fetch_add(1, std::memory_order_relaxed);
//         });
//     }
    
//     pool.wait_for_tasks();
//     EXPECT_EQ(counter.load(), 20);
// }

// TEST(ThreadPoolTest, TasksRunInParallel) {
//     ThreadPool pool(4);
//     std::atomic<int> active_tasks{0};
    
//     for (int i = 0; i < 10; ++i) {
//         pool.enqueue([&active_tasks] {
//             active_tasks.fetch_add(1, std::memory_order_relaxed);
//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//             active_tasks.fetch_sub(1, std::memory_order_relaxed);
//         });
//     }
    
//     std::this_thread::sleep_for(std::chrono::milliseconds(50));
//     EXPECT_GT(active_tasks.load(), 1);
//     pool.wait_for_tasks();
// }

// #include "../list.hpp"

// TEST(DoublyLinkedListTest, PushFrontTest) {
//     DoublyLinkedList<int> list;
//     ListNode<int> node1(10);
//     ListNode<int> node2(20);
//     ListNode<int> node3(30);

//     list.push_front(node1);
//     list.push_front(node2);
//     list.push_front(node3);
    
//     auto it = list.begin();
//     EXPECT_EQ(*it, 30);
//     ++it;
//     EXPECT_EQ(*it, 20);
//     ++it;
//     EXPECT_EQ(*it, 10);
// }

// TEST(DoublyLinkedListTest, PushBackTest) {
//     DoublyLinkedList<int> list;
//     ListNode<int> node1(10);
//     ListNode<int> node2(20);
//     ListNode<int> node3(30);

//     list.push_back(node1);
//     list.push_back(node2);
//     list.push_back(node3);
    
//     auto it = list.begin();
//     EXPECT_EQ(*it, 10);
//     ++it;
//     EXPECT_EQ(*it, 20);
//     ++it;
//     EXPECT_EQ(*it, 30);
// }

// TEST(DoublyLinkedListTest, UnlinkTest) {
//     DoublyLinkedList<int> list;
//     ListNode<int> node1(10);
//     ListNode<int> node2(20);
//     ListNode<int> node3(30);

//     list.push_back(node1);
//     list.push_back(node2);
//     list.push_back(node3);

//     node2.unlink();
    
//     auto it = list.begin();
//     EXPECT_EQ(*it, 10);
//     ++it;
//     EXPECT_EQ(*it, 30);
//     ++it;
//     EXPECT_EQ(it, list.end());
// }

// TEST(DoublyLinkedListTest, EmptyTest) {
//     DoublyLinkedList<int> list;
//     EXPECT_TRUE(list.empty());
    
//     ListNode<int> node(42);
//     list.push_front(node);
//     EXPECT_FALSE(list.empty());
// }

// TEST(DoublyLinkedListTest, IterationTest) {
//     DoublyLinkedList<int> list;
//     ListNode<int> node1(1);
//     ListNode<int> node2(2);
//     ListNode<int> node3(3);

//     list.push_back(node1);
//     list.push_back(node2);
//     list.push_back(node3);

//     std::vector<int> values;
//     for (auto it = list.begin(); it != list.end(); ++it) {
//         values.push_back(*it);
//     }
//     EXPECT_EQ(values, (std::vector<int>{1, 2, 3}));
// }

// TEST(DoublyLinkedListTest, InsertBeforeTest) {
//     DoublyLinkedList<int> list;
//     ListNode<int> node1(10);
//     ListNode<int> node2(20);
//     ListNode<int> node3(30);

//     list.push_back(node1);
//     list.push_back(node3);
//     node3.insert_before(node2);
    
//     auto it = list.begin();
//     EXPECT_EQ(*it, 10);
//     ++it;
//     EXPECT_EQ(*it, 20);
//     ++it;
//     EXPECT_EQ(*it, 30);
// }

// TEST(DoublyLinkedListTest, InsertAfterTest) {
//     DoublyLinkedList<int> list;
//     ListNode<int> node1(10);
//     ListNode<int> node2(20);
//     ListNode<int> node3(30);

//     list.push_back(node1);
//     list.push_back(node2);
//     node1.insert_after(node3);
    
//     auto it = list.begin();
//     EXPECT_EQ(*it, 10);
//     ++it;
//     EXPECT_EQ(*it, 30);
//     ++it;
//     EXPECT_EQ(*it, 20);
// }

// TEST(DoublyLinkedListTest, BidirectionalIterationTest) {
//     DoublyLinkedList<int> list;
//     ListNode<int> node1(1);
//     ListNode<int> node2(2);
//     ListNode<int> node3(3);

//     list.push_back(node1);
//     list.push_back(node2);
//     list.push_back(node3);
    
//     auto it = list.end();
//     --it;
//     EXPECT_EQ(*it, 3);
//     --it;
//     EXPECT_EQ(*it, 2);
//     --it;
//     EXPECT_EQ(*it, 1);
// }

// TEST(DoublyLinkedListTest, MultipleUnlinkTest) {
//     DoublyLinkedList<int> list;
//     ListNode<int> node1(10);
//     ListNode<int> node2(20);
//     ListNode<int> node3(30);
//     ListNode<int> node4(40);

//     list.push_back(node1);
//     list.push_back(node2);
//     list.push_back(node3);
//     list.push_back(node4);

//     node2.unlink();
//     node3.unlink();
    
//     auto it = list.begin();
//     EXPECT_EQ(*it, 10);
//     ++it;
//     EXPECT_EQ(*it, 40);
//     ++it;
//     EXPECT_EQ(it, list.end());
// }

#include "../zset.hpp"
#include <future>
#include <thread>
#include <vector>

TEST(ZSetTest, AddTest) {
    ZSet zset;

    auto future = zset.add("Alice", 10.5);
    EXPECT_TRUE(future.get());

    ZNode* node = nullptr;
    for (int i = 0; i < 10 && !(node = zset.lookup("Alice")); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->get_value(), 10.5);

    auto future2 = zset.add("Alice", 15.0);
    EXPECT_FALSE(future2.get()); // Expect false because Alice already exists
    EXPECT_EQ(zset.lookup("Alice")->get_value(), 15.0);
}

TEST(ZSetTest, LookupNonExistentKey) {
    ZSet zset;
    EXPECT_EQ(zset.lookup("Unknown"), nullptr);
}
// Concurrent Testing - try after AVL and Hashtable are thread-safe

// TEST(ZSetTest, LargeScaleInsertion) {
//     ZSet zset;
//     const int num_entries = 10000;
//     std::vector<std::future<bool>> futures;
    
//     for (int i = 0; i < num_entries; ++i) {
//         futures.push_back(zset.add("Key" + std::to_string(i), i * 1.0));
//     }
    
//     for (auto& f : futures) {
//         EXPECT_TRUE(f.get());
//     }
    
//     for (int i = 0; i < num_entries; ++i) {
//         auto node = zset.lookup("Key" + std::to_string(i));
//         ASSERT_NE(node, nullptr);
//         EXPECT_EQ(node->get_value(), i * 1.0);
//     }
// }
TEST(ZSetTest, LargeScaleInsertion) {
    ZSet zset;
    const int num_entries = 10000;

    // Sequential insertion (No concurrency)
    for (int i = 0; i < num_entries; ++i) {
        EXPECT_TRUE(zset.add("Key" + std::to_string(i), i * 1.0).get());
    }

    for (int i = 0; i < num_entries; ++i) {
        auto node = zset.lookup("Key" + std::to_string(i));
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->get_value(), i * 1.0);
    }
}

TEST(ZSetTest, ConcurrentAddTest) {
    ZSet zset;
    const int num_threads = 10;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&zset, i] {
            zset.add("ThreadedKey" + std::to_string(i), i * 2.0).get();
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    for (int i = 0; i < num_threads; ++i) {
        auto node = zset.lookup("ThreadedKey" + std::to_string(i));
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->get_value(), i * 2.0);
    }
}

TEST(ZSetTest, PopTest) {
    ZSet zset;
    zset.add("Charlie", 8.8).get();
    
    ZNode* node = zset.pop("Charlie").get();
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->get_value(), 8.8);
    EXPECT_EQ(zset.lookup("Charlie"), nullptr);
}

TEST(ZSetTest, PopNonExistentKey) {
    ZSet zset;
    EXPECT_EQ(zset.pop("Unknown").get(), nullptr);
}

TEST(ZSetTest, UpdateScoreTest) {
    ZSet zset;
    zset.add("Dave", 12.0).get();
    
    ZNode* node = zset.lookup("Dave");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->get_value(), 12.0);
    
    zset.update_score(node, 18.5);
    EXPECT_EQ(node->get_value(), 18.5);
}

TEST(ZSetTest, UpdateScoreNonExistent) {
    ZSet zset;
    ZNode dummy("Fake", 99.0);

    bool result = zset.update_score(&dummy, 88.0);  
    EXPECT_FALSE(result);  
    EXPECT_EQ(dummy.get_value(), 99.0); 
}


TEST(ZSetTest, QueryTest) {
    ZSet zset;
    zset.add("Eve", 20.0).get();
    
    ZNode* node = zset.query(20.0, "Eve", 0);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->get_value(), 20.0);
}

TEST(ZSetTest, QueryNonExistent) {
    ZSet zset;
    EXPECT_EQ(zset.query(99.0, "Ghost", 0), nullptr);
}
