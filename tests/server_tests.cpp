#include <gtest/gtest.h>

/*

UNIT TESTS FOR:

- COMMON
- REQUEST_PARSER
- SOCKET
- RESPONSE SERIALIZER
- LOGGER
- CONNECTION
- ENTRY MANAGER
*/

// #include "../common.hpp"
// #include <cstddef>
// #include <string>

// // Test for container_of function
// struct Parent {
//     int a;
// };

// struct Child {
//     int b;
//     Parent parent;
// };

// TEST(CommonTests, ContainerOf) {
//     Child child;
//     Parent* recovered = container_of<Parent>(&child.parent, offsetof(Child, parent));
//     ASSERT_EQ(reinterpret_cast<void*>(recovered), reinterpret_cast<void*>(&child));
// }

// // Test for get_serialization_type function
// TEST(CommonTests, SerializationType) {
//     EXPECT_EQ(get_serialization_type<int>(), SerializationType::Integer);
//     EXPECT_EQ(get_serialization_type<double>(), SerializationType::Double);
//     EXPECT_EQ(get_serialization_type<std::string>(), SerializationType::String);
//     EXPECT_EQ(get_serialization_type<std::string_view>(), SerializationType::String);
//     EXPECT_EQ(get_serialization_type<char>(), SerializationType::Nil);
// }

// // Test for different integer types
// TEST(CommonTests, IntegerTypes) {
//     EXPECT_EQ(get_serialization_type<int>(), SerializationType::Integer);
//     EXPECT_EQ(get_serialization_type<unsigned int>(), SerializationType::Integer);
//     EXPECT_EQ(get_serialization_type<long>(), SerializationType::Integer);
//     EXPECT_EQ(get_serialization_type<long long>(), SerializationType::Integer);
// }

// // Test for different floating-point types
// TEST(CommonTests, FloatingPointTypes) {
//     EXPECT_EQ(get_serialization_type<float>(), SerializationType::Double);
//     EXPECT_EQ(get_serialization_type<double>(), SerializationType::Double);
//     EXPECT_EQ(get_serialization_type<long double>(), SerializationType::Double);
// }

// // Edge case test for non-standard types
// struct CustomType {};

// TEST(CommonTests, CustomType) {
//     EXPECT_EQ(get_serialization_type<CustomType>(), SerializationType::Nil);
// }

/*
SOCKET TESTS
*/

// #include <sys/socket.h>   // For socket(), AF_INET, SOCK_STREAM
// #include <cerrno>         // For errno
// #include "../socket.hpp"     // Include your Socket class

// TEST(SocketTest, OwnershipTransfer) {
//     int raw_fd = socket(AF_INET, SOCK_STREAM, 0);
//     ASSERT_NE(raw_fd, -1) << "Failed to create socket";

//     Socket sock1(raw_fd);
//     EXPECT_EQ(sock1.get(), raw_fd); // Verify ownership

//     Socket sock2(std::move(sock1)); // Move ownership
//     EXPECT_EQ(sock1.get(), -1); // sock1 should no longer own it
//     EXPECT_EQ(sock2.get(), raw_fd); // sock2 now owns it
// }

// TEST(SocketTest, SetNonBlocking) {
//     int raw_fd = socket(AF_INET, SOCK_STREAM, 0);
//     ASSERT_NE(raw_fd, -1) << "Failed to create socket";

//     Socket sock(raw_fd);
//     auto result = sock.set_nonblocking();
    
//     EXPECT_TRUE(result.has_value()) << "set_nonblocking() failed";
    
//     // Verify that the socket is actually non-blocking
//     int flags = fcntl(sock.get(), F_GETFL, 0);
//     EXPECT_NE(flags, -1) << "fcntl failed";
//     EXPECT_TRUE(flags & O_NONBLOCK) << "Socket is not in non-blocking mode";
// }

// TEST(SocketTest, InvalidFdHandling) {
//     Socket sock(-1); // Invalid file descriptor
//     auto result = sock.set_nonblocking();
    
//     EXPECT_FALSE(result.has_value()) << "set_nonblocking() should fail on an invalid fd";
// }

// TEST(SocketTest, CloseOnDestruction) {
//     int raw_fd = socket(AF_INET, SOCK_STREAM, 0);
//     ASSERT_NE(raw_fd, -1) << "Failed to create socket";

//     {
//         Socket sock(raw_fd);
//     } // `sock` should be destroyed here, closing the fd

//     // Try to set non-blocking mode, should fail if socket was closed
//     int flags = fcntl(raw_fd, F_GETFL, 0);
//     EXPECT_EQ(flags, -1) << "File descriptor should be closed after Socket destruction";
// }

/*
REQUEST PARSER TESTS
*/

// #include "../request_parser.hpp"

// TEST(RequestParserTest, ValidRequest) {
//     std::vector<uint8_t> valid_request = {
//         0x00, 0x00, 0x00, 0x09,  
//         0x00, 0x00, 0x00, 0x05, 
//         'H',  'e',  'l',  'l',  'o'
//     };    

//     auto result = RequestParser::parse(valid_request);
//     ASSERT_TRUE(result.has_value());
//     EXPECT_EQ(result->size(), 1);
//     EXPECT_EQ(result->at(0), "Hello");
// }


// TEST(RequestParserTest, TooShortRequest) {
//     std::vector<uint8_t> short_request = { 0x00, 0x00 }; // Not enough data for length field
//     auto result = RequestParser::parse(short_request);
//     EXPECT_FALSE(result.has_value()) << "Expected failure due to message being too short!";
// }

// TEST(RequestParserTest, MismatchedLength) {
//     std::vector<uint8_t> invalid_length = {
//         0x00, 0x00, 0x00, 0x10, // Declared length 16 (too large)
//         0x00, 0x00, 0x00, 0x03, 'A', 'B'
//     };

//     auto result = RequestParser::parse(invalid_length);
//     EXPECT_FALSE(result.has_value()) << "Expected failure due to mismatched length!";
// }

// TEST(RequestParserTest, StringLengthExceedsData) {
//     std::vector<uint8_t> bad_string_length = {
//         0x00, 0x00, 0x00, 0x08,  // Declared total length 8
//         0x00, 0x00, 0x00, 0x06,  // Declared string length 6 (but only 4 bytes remain)
//         'T',  'e',  's',  't'
//     };

//     auto result = RequestParser::parse(bad_string_length);
//     EXPECT_FALSE(result.has_value()) << "Expected failure due to excessive string length!";
// }

// TEST(RequestParserTest, EmptyRequest) {
//     std::vector<uint8_t> empty_request = { 0x00, 0x00, 0x00, 0x00 }; // Length = 0
//     auto result = RequestParser::parse(empty_request);
//     ASSERT_TRUE(result.has_value());
//     EXPECT_TRUE(result->empty());
// }

/*
LOGGER TESTS
*/


// #include <sstream>
// #include <iostream>
// #include "../logging.hpp"

// TEST(LoggingTest, LogMessageOutput) {
//     std::stringstream buffer;
//     std::streambuf* old = std::cerr.rdbuf(buffer.rdbuf()); // Redirect cerr

//     log_message("Test log: {}", 42);

//     std::cerr.rdbuf(old); // Restore cerr

//     std::string output = buffer.str();
//     EXPECT_NE(output.find("Test log: 42"), std::string::npos) << "Log message format incorrect!";
// }

/*
RESPONSE SERIALIZATION TESTS
*/

// #include "../response_serializer.hpp"

// TEST(ResponseSerializerTest, SerializeNil) {
//     std::vector<uint8_t> buffer;
//     ResponseSerializer::serialize_nil(buffer);
    
//     ASSERT_EQ(buffer.size(), 1);
//     EXPECT_EQ(buffer[0], static_cast<uint8_t>(SerializationType::Nil));
// }

// TEST(ResponseSerializerTest, SerializeInteger) {
//     std::vector<uint8_t> buffer;
//     ResponseSerializer::serialize_integer(buffer, 42);

//     ASSERT_EQ(buffer.size(), 1 + sizeof(int64_t)); // 1 byte type + 8 byte integer
//     EXPECT_EQ(buffer[0], static_cast<uint8_t>(SerializationType::Integer));

//     int64_t extracted_value;
//     std::memcpy(&extracted_value, &buffer[1], sizeof(int64_t));
//     EXPECT_EQ(extracted_value, 42);
// }

// TEST(ResponseSerializerTest, SerializeDouble) {
//     std::vector<uint8_t> buffer;
//     double value = 3.14159;
//     ResponseSerializer::serialize_double(buffer, value);

//     ASSERT_EQ(buffer.size(), 1 + sizeof(double)); // 1 byte type + 8 byte double
//     EXPECT_EQ(buffer[0], static_cast<uint8_t>(SerializationType::Double));

//     double extracted_value;
//     std::memcpy(&extracted_value, &buffer[1], sizeof(double));
//     EXPECT_DOUBLE_EQ(extracted_value, value);
// }

// TEST(ResponseSerializerTest, SerializeString) {
//     std::vector<uint8_t> buffer;
//     std::string test_str = "Hello";

//     ResponseSerializer::serialize_string(buffer, test_str);

//     ASSERT_EQ(buffer.size(), 1 + 4 + test_str.size()); // 1 byte type + 4 byte length + string data
//     EXPECT_EQ(buffer[0], static_cast<uint8_t>(SerializationType::String));

//     uint32_t extracted_len;
//     std::memcpy(&extracted_len, &buffer[1], sizeof(uint32_t));
//     EXPECT_EQ(extracted_len, test_str.size());

//     std::string extracted_str(buffer.begin() + 5, buffer.end());
//     EXPECT_EQ(extracted_str, test_str);
// }

// TEST(ResponseSerializerTest, SerializeError) {
//     std::vector<uint8_t> buffer;
//     int32_t error_code = -1;
//     std::string error_msg = "Not found";

//     ResponseSerializer::serialize_error(buffer, error_code, error_msg);

//     ASSERT_EQ(buffer.size(), 1 + 4 + 4 + error_msg.size()); // Type + 4-byte error code + 4-byte length + string data
//     EXPECT_EQ(buffer[0], static_cast<uint8_t>(SerializationType::Error));

//     int32_t extracted_code;
//     std::memcpy(&extracted_code, &buffer[1], sizeof(int32_t));
//     EXPECT_EQ(extracted_code, error_code);

//     uint32_t extracted_len;
//     std::memcpy(&extracted_len, &buffer[5], sizeof(uint32_t));
//     EXPECT_EQ(extracted_len, error_msg.size());

//     std::string extracted_msg(buffer.begin() + 9, buffer.end());
//     EXPECT_EQ(extracted_msg, error_msg);
// }

// TEST(ResponseSerializerTest, SerializeTemplateTypes) {
//     // Integer
//     {
//         std::vector<uint8_t> buffer;
//         ResponseSerializer::serialize(buffer, 100);
//         EXPECT_EQ(buffer[0], static_cast<uint8_t>(SerializationType::Integer));
//     }

//     // Double
//     {
//         std::vector<uint8_t> buffer;
//         ResponseSerializer::serialize(buffer, 3.14);
//         EXPECT_EQ(buffer[0], static_cast<uint8_t>(SerializationType::Double));
//     }

//     // String
//     {
//         std::vector<uint8_t> buffer;
//         ResponseSerializer::serialize(buffer, std::string_view("World"));
//         EXPECT_EQ(buffer[0], static_cast<uint8_t>(SerializationType::String));
//     }
// }

/*
CONNECTION TESTS
*/

// #include <gtest/gtest.h>
// #include "../connection.hpp"
// #include "../socket.hpp"
// #include "../request_parser.hpp"
// #include "../response_serializer.hpp"

// class MockSocket : public Socket {
// public:
//     MockSocket(int fd) : Socket(fd) {}
//     ssize_t read(void* buffer, size_t size) {
//         if (read_data.empty()) return 0;
//         size_t to_read = std::min(size, read_data.size());
//         memcpy(buffer, read_data.data(), to_read);
//         read_data.erase(read_data.begin(), read_data.begin() + to_read);
//         return to_read;
//     }
//     ssize_t write(const void* buffer, size_t size) {
//         write_data.insert(write_data.end(), (uint8_t*)buffer, (uint8_t*)buffer + size);
//         return size;
//     }
//     std::vector<uint8_t> read_data;
//     std::vector<uint8_t> write_data;
// };

// class ConnectionTest : public ::testing::Test {
// protected:
//     void SetUp() override {
//         mock_socket = std::make_unique<MockSocket>(1);
//         conn = std::make_unique<Connection>(std::move(*mock_socket));
//     }

//     std::unique_ptr<MockSocket> mock_socket;
//     std::unique_ptr<Connection> conn;
// };

// TEST_F(ConnectionTest, InitialState) {
//     EXPECT_EQ(conn->state(), ConnectionState::Request);
//     EXPECT_GE(conn->idle_duration().count(), 0);
// }

// TEST_F(ConnectionTest, ProcessEmptyRequest) {
//     mock_socket->read_data.clear();
//     auto result = conn->process_io();
//     EXPECT_FALSE(result.has_value());
//     EXPECT_EQ(result.error(), std::make_error_code(std::errc::connection_aborted));
// }

// TEST_F(ConnectionTest, HandleRequestValidData) {
//     mock_socket->read_data = {0x00, 0x01, 0x02, 0x03};
//     auto result = conn->process_io();
//     EXPECT_TRUE(result.has_value());
//     EXPECT_EQ(conn->state(), ConnectionState::Request);
// }

// TEST_F(ConnectionTest, HandleResponse) {
//     conn->process_io();
//     EXPECT_TRUE(conn->state() == ConnectionState::Request);
// }

// TEST_F(ConnectionTest, WriteBufferFlush) {
//     conn->process_io();
//     EXPECT_EQ(conn->state(), ConnectionState::Request);
// }

// TEST_F(ConnectionTest, InvalidStateHandling) {
//     conn->process_io();
//     EXPECT_EQ(conn->state(), ConnectionState::Request);
// }

// int main(int argc, char** argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }

/*
// ENTRY MANAGER TEST
*/

// #include "../entry_manager.hpp"
// #include <vector>

// class EntryManagerTest : public ::testing::Test {
// protected:
//     ThreadPool pool;
//     BinaryHeap<uint64_t> heap;

//     EntryManagerTest() : pool(2), heap(std::less<uint64_t>()) {}  
// };

// TEST_F(EntryManagerTest, CreateEntry) {
//     Entry entry("test_key", "test_value");

//     EXPECT_EQ(entry.key, "test_key");
//     EXPECT_EQ(entry.value, "test_value");
//     EXPECT_EQ(entry.heap_idx, static_cast<size_t>(-1));
//     EXPECT_FALSE(entry.zset);  // Ensure ZSet is not initialized
// }

// TEST_F(EntryManagerTest, DestroyEntry) {
//     Entry* entry = new Entry("test_key", "test_value");
//     EntryManager::destroy_entry(entry);
//     SUCCEED();  // No segfault = success
// }

// TEST_F(EntryManagerTest, DeleteEntryAsync) {
//     Entry* entry = new Entry("test_key", "test_value");
//     EntryManager::delete_entry_async(entry, pool);
//     pool.wait_for_tasks();  // Ensure all tasks complete before asserting
//     SUCCEED();
// }

// TEST_F(EntryManagerTest, SetEntryTTL_AddToHeap) {
//     Entry entry("test_key", "test_value");
//     EntryManager::set_entry_ttl(entry, 5000, heap);

//     EXPECT_NE(entry.heap_idx, static_cast<size_t>(-1));  // Entry should have a valid heap index
//     EXPECT_EQ(heap.size(), 1);
// }

// TEST_F(EntryManagerTest, SetEntryTTL_RemoveFromHeap) {
//     Entry entry("test_key", "test_value");
//     EntryManager::set_entry_ttl(entry, 5000, heap);
//     ASSERT_EQ(heap.size(), 1);

//     EntryManager::set_entry_ttl(entry, -1, heap);  // Setting TTL to -1 should remove the entry
//     EXPECT_EQ(entry.heap_idx, static_cast<size_t>(-1));
//     EXPECT_EQ(heap.size(), 0);
// }

// TEST_F(EntryManagerTest, SetEntryTTL_UpdateHeap) {
//     Entry entry("test_key", "test_value");
//     EntryManager::set_entry_ttl(entry, 5000, heap);

//     uint64_t old_ttl = heap.top().value(); 

//     EntryManager::set_entry_ttl(entry, 10000, heap);
//     uint64_t new_ttl = heap.top().value();

//     EXPECT_NE(old_ttl, new_ttl);  // TTL should have been updated
//     EXPECT_EQ(heap.size(), 1);  // Ensure no duplicate entries
// }

// TEST_F(EntryManagerTest, PerformanceTest_InsertManyEntries) {
//     const size_t num_entries = 100000;
//     std::vector<Entry> entries;
//     entries.reserve(num_entries);

//     uint64_t start_time = get_monotonic_usec();

//     for (size_t i = 0; i < num_entries; i++) {
//         entries.emplace_back("key" + std::to_string(i), "value" + std::to_string(i));
//         EntryManager::set_entry_ttl(entries[i], ((i % 1000) + 1) * 10, heap);
//     }

//     uint64_t end_time = get_monotonic_usec();
//     double time_taken = (end_time - start_time) / 1000.0;  // Convert to milliseconds

//     EXPECT_EQ(heap.size(), num_entries);
//     std::cout << "[Performance] Inserted " << num_entries << " entries in " << time_taken << " ms\n";
// }

// TEST_F(EntryManagerTest, TTLUpdateOnDuplicateKeys) {
//     Entry entry("test_key", "test_value");
//     EntryManager::set_entry_ttl(entry, 5000, heap);
//     uint64_t initial_ttl = heap.top().value();

//     // Update TTL for the same key
//     EntryManager::set_entry_ttl(entry, 10000, heap);
//     uint64_t updated_ttl = heap.top().value();

//     EXPECT_NE(initial_ttl, updated_ttl);
//     EXPECT_EQ(heap.size(), 1);  // Ensure heap doesn't have duplicate entries
// }

// TEST_F(EntryManagerTest, ExpiredTTLShouldBeRemoved) {
//     Entry entry1("key1", "value1");
//     Entry entry2("key2", "value2");

//     EntryManager::set_entry_ttl(entry1, 1000, heap);
//     EntryManager::set_entry_ttl(entry2, 5000, heap);
    
//     EXPECT_EQ(heap.size(), 2);

//     EntryManager::set_entry_ttl(entry1, -1, heap);  // Expire first entry
//     EXPECT_EQ(heap.size(), 1);
// }

// TEST_F(EntryManagerTest, TTLSetToZeroExpiresImmediately) {
//     Entry entry("test_key", "test_value");
//     EntryManager::set_entry_ttl(entry, 0, heap);

//     EXPECT_EQ(entry.heap_idx, static_cast<size_t>(-1));  // Entry should not be in heap
//     EXPECT_EQ(heap.size(), 0);
// }

// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
#include "../command_processor.hpp"

// Mock database and heap
class CommandProcessorTest : public ::testing::Test {
protected:
    Hashtable db;  
    BinaryHeap heap;
    std::vector<uint8_t> response;
    std::mutex db_mutex;

    void SetUp() override {
        response.clear();
    }

    std::string getResponseAsString() {
        return std::string(response.begin(), response.end());
    }
};

// Test SET command
TEST_F(CommandProcessorTest, SetCommand) {
    std::vector<std::string> args = {"SET", "key1", "value1"};
    CommandProcessor::CommandContext ctx{args, response, db, heap, db_mutex};

    CommandProcessor::process_command(ctx);
    EXPECT_EQ(getResponseAsString(), "+OK\r\n");

    std::lock_guard<std::mutex> lock(db_mutex);
    auto entry = lookup_entry(db, "key1");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->value, "value1");
}

// Test GET command
TEST_F(CommandProcessorTest, GetCommand) {
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        auto [entry, _] = get_or_create_entry(db, "key1");
        entry->type = EntryType::String;
        entry->value = "value1";
    }

    std::vector<std::string> args = {"GET", "key1"};
    CommandProcessor::CommandContext ctx{args, response, db, heap, db_mutex};

    CommandProcessor::process_command(ctx);
    EXPECT_EQ(getResponseAsString(), "$6\r\nvalue1\r\n");
}

// Test GET for a non-existent key
TEST_F(CommandProcessorTest, GetNonExistentKey) {
    std::vector<std::string> args = {"GET", "missing_key"};
    CommandProcessor::CommandContext ctx{args, response, db, heap, db_mutex};

    CommandProcessor::process_command(ctx);
    EXPECT_EQ(getResponseAsString(), "$-1\r\n");
}

// Test ZADD command
TEST_F(CommandProcessorTest, ZAddCommand) {
    std::vector<std::string> args = {"ZADD", "myzset", "10.5", "Alice"};
    CommandProcessor::CommandContext ctx{args, response, db, heap, db_mutex};

    CommandProcessor::process_command(ctx);
    EXPECT_EQ(getResponseAsString(), ":1\r\n");

    std::lock_guard<std::mutex> lock(db_mutex);
    auto entry = lookup_entry(db, "myzset");
    ASSERT_NE(entry, nullptr);
    ASSERT_NE(entry->zset, nullptr);
    EXPECT_TRUE(entry->zset->contains("Alice"));
}

// Test ZQUERY command
TEST_F(CommandProcessorTest, ZQueryCommand) {
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        auto [entry, _] = get_or_create_entry(db, "myzset");
        entry->type = EntryType::ZSet;
        entry->zset = std::make_unique<ZSet>();
        entry->zset->add("Alice", 10.5);
        entry->zset->add("Bob", 20.0);
    }

    std::vector<std::string> args = {"ZQUERY", "myzset", "10.0", "Alice", "0", "2"};
    CommandProcessor::CommandContext ctx{args, response, db, heap, db_mutex};

    CommandProcessor::process_command(ctx);
    EXPECT_EQ(getResponseAsString(), "*4\r\n$5\r\nAlice\r\n:10.5\r\n$3\r\nBob\r\n:20\r\n");
}

// Test PEXPIRE command
TEST_F(CommandProcessorTest, PExpireCommand) {
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        auto [entry, _] = get_or_create_entry(db, "key1");
        entry->type = EntryType::String;
        entry->value = "value1";
    }

    std::vector<std::string> args = {"PEXPIRE", "key1", "5000"};
    CommandProcessor::CommandContext ctx{args, response, db, heap, db_mutex};

    CommandProcessor::process_command(ctx);
    EXPECT_EQ(getResponseAsString(), ":1\r\n");
}

// Test PTTL command
TEST_F(CommandProcessorTest, PTTLCommand) {
    uint64_t expire_time = CommandProcessor::get_monotonic_usec() + 5000000; // 5 seconds TTL
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        auto [entry, _] = get_or_create_entry(db, "key1");
        entry->type = EntryType::String;
        entry->heap_idx = heap.size();
        heap.push(HeapItem<uint64_t>{expire_time});
    }

    std::vector<std::string> args = {"PTTL", "key1"};
    CommandProcessor::CommandContext ctx{args, response, db, heap, db_mutex};

    CommandProcessor::process_command(ctx);
    EXPECT_GT(std::stoi(getResponseAsString().substr(1)), 4900); // Ensure TTL is close to 5000 ms
}

// Test invalid command
TEST_F(CommandProcessorTest, InvalidCommand) {
    std::vector<std::string> args = {"INVALID"};
    CommandProcessor::CommandContext ctx{args, response, db, heap, db_mutex};

    CommandProcessor::process_command(ctx);
    EXPECT_EQ(getResponseAsString(), "-ERR unknown command\r\n");
}

