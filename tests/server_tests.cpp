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
// #include <gtest/gtest.h>
// #include <vector>

// class EntryManagerTest : public ::testing::Test {
// protected:
//     ThreadPool pool;
//     BinaryHeap<uint64_t> heap;
//     EntryManager entry_manager;

//     EntryManagerTest() : pool(2), heap(std::less<uint64_t>()), entry_manager() {}
// };

// TEST_F(EntryManagerTest, CreateEntry) {
//     auto entry = entry_manager.create_entry("test_key", std::string("test_value"));

//     EXPECT_NE(entry, nullptr);  // Ensure entry is created
//     EXPECT_EQ(entry->key, "test_key");

//     auto str_entry = std::dynamic_pointer_cast<Entry<std::string>>(entry);
//     ASSERT_NE(str_entry, nullptr);  // Ensure the dynamic cast succeeds
//     EXPECT_EQ(str_entry->value, "test_value");
//     EXPECT_EQ(entry->heap_idx, static_cast<size_t>(-1));  // Not in heap initially
// }

// TEST_F(EntryManagerTest, DestroyEntry) {
//     entry_manager.create_entry("test_key", std::string("test_value"));
//     entry_manager.delete_entry("test_key");

//     auto entry = entry_manager.find_entry("test_key");
//     EXPECT_EQ(entry, nullptr);  // Entry should be deleted
// }

// TEST_F(EntryManagerTest, DeleteEntryAsync) {
//     entry_manager.create_entry("test_key", std::string("test_value"));
//     entry_manager.delete_entry_async("test_key");

//     pool.wait_for_tasks();
//     EXPECT_EQ(entry_manager.find_entry("test_key"), nullptr);
// }

// TEST_F(EntryManagerTest, SetEntryTTL_AddToHeap) {
//     auto entry = entry_manager.create_entry("test_key", std::string("test_value"));
//     entry_manager.set_entry_ttl(*entry, 5000);

//     EXPECT_NE(entry->heap_idx, static_cast<size_t>(-1));  // Entry should have a valid heap index
//     EXPECT_GT(entry_manager.get_expiry_time(*entry), 0);  // TTL should be set
// }

// TEST_F(EntryManagerTest, SetEntryTTL_RemoveFromHeap) {
//     auto entry = entry_manager.create_entry("test_key", std::string("test_value"));
//     entry_manager.set_entry_ttl(*entry, 5000);
//     ASSERT_GT(entry_manager.get_expiry_time(*entry), 0);

//     entry_manager.set_entry_ttl(*entry, -1);  // Setting TTL to -1 should remove the entry
//     EXPECT_EQ(entry->heap_idx, static_cast<size_t>(-1));
// }

// TEST_F(EntryManagerTest, SetEntryTTL_UpdateHeap) {
//     auto entry = entry_manager.create_entry("test_key", std::string("test_value"));
//     entry_manager.set_entry_ttl(*entry, 5000);

//     uint64_t old_ttl = entry_manager.get_expiry_time(*entry);

//     entry_manager.set_entry_ttl(*entry, 10000);
//     uint64_t new_ttl = entry_manager.get_expiry_time(*entry);

//     EXPECT_NE(old_ttl, new_ttl);  // TTL should have been updated
// }

// TEST_F(EntryManagerTest, PerformanceTest_InsertManyEntries) {
//     const size_t num_entries = 100000;
//     std::vector<std::shared_ptr<EntryBase>> entries;
//     entries.reserve(num_entries);

//     uint64_t start_time = get_monotonic_usec();

//     for (size_t i = 0; i < num_entries; i++) {
//         auto entry = entry_manager.create_entry("key" + std::to_string(i), "value" + std::to_string(i));
//         entries.push_back(entry);
//         entry_manager.set_entry_ttl(*entry, ((i % 1000) + 1) * 10);
//     }

//     uint64_t end_time = get_monotonic_usec();
//     double time_taken = (end_time - start_time) / 1000.0;  // Convert to milliseconds

//     EXPECT_EQ(entries.size(), num_entries);
//     std::cout << "[Performance] Inserted " << num_entries << " entries in " << time_taken << " ms\n";
// }

// TEST_F(EntryManagerTest, TTLUpdateOnDuplicateKeys) {
//     auto entry = entry_manager.create_entry("test_key", std::string("test_value"));
//     entry_manager.set_entry_ttl(*entry, 5000);
//     uint64_t initial_ttl = entry_manager.get_expiry_time(*entry);

//     // Update TTL for the same key
//     entry_manager.set_entry_ttl(*entry, 10000);
//     uint64_t updated_ttl = entry_manager.get_expiry_time(*entry);

//     EXPECT_NE(initial_ttl, updated_ttl);
// }

// TEST_F(EntryManagerTest, ExpiredTTLShouldBeRemoved) {
//     auto entry1 = entry_manager.create_entry("key1", std::string("value1"));
//     auto entry2 = entry_manager.create_entry("key2", std::string("value2"));

//     entry_manager.set_entry_ttl(*entry1, 1000);
//     entry_manager.set_entry_ttl(*entry2, 5000);
    
//     EXPECT_GT(entry_manager.get_expiry_time(*entry1), 0);
//     EXPECT_GT(entry_manager.get_expiry_time(*entry2), 0);

//     entry_manager.set_entry_ttl(*entry1, -1);  // Expire first entry
//     EXPECT_EQ(entry_manager.get_expiry_time(*entry1), 0);
//     EXPECT_GT(entry_manager.get_expiry_time(*entry2), 0);
// }

// TEST_F(EntryManagerTest, TTLSetToZeroExpiresImmediately) {
//     auto entry = entry_manager.create_entry("test_key", std::string("test_value"));
//     entry_manager.set_entry_ttl(*entry, 0);

//     EXPECT_EQ(entry->heap_idx, static_cast<size_t>(-1));  // Entry should not be in heap
//     EXPECT_EQ(entry_manager.get_expiry_time(*entry), 0);
// }

// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }


// #include "../command_processor.hpp"

// class CommandProcessorTest : public ::testing::Test {
// protected:
//     EntryManager entry_manager;
//     std::mutex db_mutex;

//     void executeCommand(const std::vector<std::string>& args, std::vector<uint8_t>& response) {
//         CommandProcessor::CommandContext ctx{args, response, entry_manager, db_mutex};
//         CommandProcessor::process_command(ctx);
//     }
// };

// // ✅ **Test: SET & GET**
// TEST_F(CommandProcessorTest, SetAndGet) {
//     std::vector<uint8_t> response;
//     executeCommand({"set", "test_key", "test_value"}, response);

//     response.clear();
//     executeCommand({"get", "test_key"}, response);
    
//     EXPECT_EQ(ResponseSerializer::deserialize_string(response), "test_value");
// }

// // ✅ **Test: GET Missing Key**
// TEST_F(CommandProcessorTest, GetMissingKey) {
//     std::vector<uint8_t> response;
//     executeCommand({"get", "missing_key"}, response);
    
//     EXPECT_EQ(ResponseSerializer::deserialize_nil(response), true);
// }

// // ✅ **Test: EXISTS**
// TEST_F(CommandProcessorTest, Exists) {
//     std::vector<uint8_t> response;

//     executeCommand({"set", "test_key", "value"}, response);
//     response.clear();

//     executeCommand({"exists", "test_key"}, response);
//     EXPECT_EQ(ResponseSerializer::deserialize_integer(response), 1);

//     response.clear();
//     executeCommand({"exists", "missing_key"}, response);
//     EXPECT_EQ(ResponseSerializer::deserialize_integer(response), 0);
// }

// // ✅ **Test: DEL**
// TEST_F(CommandProcessorTest, Delete) {
//     std::vector<uint8_t> response;

//     executeCommand({"set", "test_key", "value"}, response);
//     response.clear();

//     executeCommand({"del", "test_key"}, response);
//     EXPECT_EQ(ResponseSerializer::deserialize_integer(response), 1);

//     response.clear();
//     executeCommand({"exists", "test_key"}, response);
//     EXPECT_EQ(ResponseSerializer::deserialize_integer(response), 0);
// }

// // ✅ **Test: PEXPIRE & PTTL**
// TEST_F(CommandProcessorTest, PExpireAndPTTL) {
//     std::vector<uint8_t> response;

//     executeCommand({"set", "temp_key", "expiring"}, response);
//     response.clear();

//     executeCommand({"pexpire", "temp_key", "5000"}, response);
//     EXPECT_EQ(ResponseSerializer::deserialize_integer(response), 1);

//     response.clear();
//     executeCommand({"pttl", "temp_key"}, response);
//     EXPECT_GT(ResponseSerializer::deserialize_integer(response), 0);
// }

// // ✅ **Test: ZADD & ZREM**
// TEST_F(CommandProcessorTest, ZAddAndZRem) {
//     std::vector<uint8_t> response;

//     executeCommand({"zadd", "myzset", "10", "member1"}, response);
//     EXPECT_EQ(ResponseSerializer::deserialize_integer(response), 1);

//     response.clear();
//     executeCommand({"zrem", "myzset", "member1"}, response);
//     EXPECT_EQ(ResponseSerializer::deserialize_integer(response), 1);

//     response.clear();
//     executeCommand({"zrem", "myzset", "non_existent_member"}, response);
//     EXPECT_EQ(ResponseSerializer::deserialize_integer(response), 0);
// }

// // ✅ **Test: FLUSHALL**
// TEST_F(CommandProcessorTest, FlushAll) {
//     std::vector<uint8_t> response;

//     executeCommand({"set", "key1", "value1"}, response);
//     executeCommand({"set", "key2", "value2"}, response);
    
//     response.clear();
//     executeCommand({"flushall"}, response);
    
//     response.clear();
//     executeCommand({"exists", "key1"}, response);
//     EXPECT_EQ(ResponseSerializer::deserialize_integer(response), 0);

//     response.clear();
//     executeCommand({"exists", "key2"}, response);
//     EXPECT_EQ(ResponseSerializer::deserialize_integer(response), 0);
// }

// // ✅ **Test: Invalid Command**
// TEST_F(CommandProcessorTest, InvalidCommand) {
//     std::vector<uint8_t> response;
//     executeCommand({"invalid_cmd"}, response);
    
//     EXPECT_EQ(ResponseSerializer::deserialize_error(response), "unknown command");
// }

// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
#include "../server.hpp"
#include "../socket.hpp"
#include "../entry_manager.hpp"
#include "../command_processor.hpp"

class ServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        server = std::make_unique<Server>(4321, 4);
    }
    void TearDown() override {
        server->stop();
    }
    std::unique_ptr<Server> server;
    EntryManager entry_manager_;
    std::shared_mutex connections_mutex_;
    CommandProcessor command_processor_;
};

TEST_F(ServerTest, InitializeServer) {
    auto result = server->initialize();
    EXPECT_TRUE(result.has_value()) << "Server initialization failed: " << result.error().message();
}

// Instead of testing `create_listen_socket()`, test `initialize()`, which calls it
TEST_F(ServerTest, ServerInitializationCreatesSocket) {
    auto result = server->initialize();
    EXPECT_TRUE(result.has_value());
    EXPECT_NE(server->get_listen_socket_fd(), -1) << "Listening socket should be valid after initialization";
}


// Fix: Use proper connection creation
TEST_F(ServerTest, AddAndRemoveConnection) {
    int fake_fd = 100;
    auto fake_socket = std::make_unique<Socket>(fake_fd);
    
    // Create a connection using `std::unique_ptr<Socket>` instead of extracting raw `Socket`
    auto conn = std::make_unique<Connection>(
        std::move(*fake_socket),  // Extract and move the raw `Socket` object
        entry_manager_,
        connections_mutex_,
        command_processor_
    );
    

    server->add_connection(std::move(conn));
    EXPECT_EQ(server->connections_.size(), 1) << "Connection should be added";

    server->remove_connection(fake_fd);
    EXPECT_EQ(server->connections_.size(), 0) << "Connection should be removed";
}

// Fix: Ensure it does not crash, but do not attempt an actual connection
TEST_F(ServerTest, AcceptNewConnection) {
    pollfd fake_pollfd{};
    fake_pollfd.revents = POLLIN;
    
    // Should not crash when attempting to accept connections
    server->accept_new_connections(fake_pollfd);
    
    SUCCEED() << "Server should handle accept_new_connections gracefully";
}

// Fix: Ensure timeout logic is correct
TEST_F(ServerTest, CalculateNextTimeout) {
    auto timeout = server->calculate_next_timeout();
    EXPECT_GE(timeout.count(), 0) << "Timeout should be non-negative";
}

// Fix: Ensure `prepare_poll_args` properly populates the vector
TEST_F(ServerTest, PreparePollArgs) {
    std::vector<pollfd> poll_args;
    server->prepare_poll_args(poll_args);
    EXPECT_FALSE(poll_args.empty()) << "Poll arguments should be populated";
}
