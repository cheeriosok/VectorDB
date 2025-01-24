#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <span>
#include <expected>
#include <system_error>
#include <format>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <source_location>
#include <ranges>

// System headers
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

// Project headers
#include "hashtable.hpp"
#include "zset.hpp"
#include "list.hpp"
#include "heap.hpp"
#include "thread_pool.hpp"
#include "common.hpp"

namespace {

// Modern logging with source_location
template<typename... Args>
void log_message(std::string_view format, 
                const Args&... args,
                const std::source_location& location = std::source_location::current()) {
    auto timestamp = std::chrono::system_clock::now();
    std::cerr << std::format("[{}] {}:{} - ", 
                            timestamp, 
                            location.file_name(), 
                            location.line());
    std::cerr << std::vformat(format, std::make_format_args(args...)) << '\n';
}

// Modern error handling with std::expected
template<typename T>
using Result = std::expected<T, std::error_code>;

// Constants
static constexpr size_t MAX_MSG_SIZE = 4096;
static constexpr auto IDLE_TIMEOUT = std::chrono::milliseconds(5000);
static constexpr uint16_t SERVER_PORT = 1234;

// Connection states as strongly typed enum
enum class ConnectionState : uint8_t {
    Request,
    Response,
    End
};

// Modern RAII wrapper for sockets
class Socket {
public:
    explicit Socket(int fd) : fd_(fd) {}
    ~Socket() { 
        if (fd_ != -1) {
            close(fd_);
        }
    }
    
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (fd_ != -1) {
                close(fd_);
            }
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }
    
    [[nodiscard]] int get() const noexcept { return fd_; }
    
    Result<void> set_nonblocking() const {
        int flags = fcntl(fd_, F_GETFL, 0);
        if (flags == -1) {
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }
        
        flags |= O_NONBLOCK;
        if (fcntl(fd_, F_SETFL, flags) == -1) {
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }
        
        return {};
    }

private:
    int fd_;
};

// Modern connection class with RAII
class Connection {
public:
    explicit Connection(Socket socket) 
        : socket_(std::move(socket))
        , state_(ConnectionState::Request)
        , idle_start_(std::chrono::steady_clock::now()) {
        rbuf_.reserve(MAX_MSG_SIZE);
        wbuf_.reserve(MAX_MSG_SIZE);
    }
    
    [[nodiscard]] int fd() const noexcept { return socket_.get(); }
    [[nodiscard]] ConnectionState state() const noexcept { return state_; }
    [[nodiscard]] auto idle_duration() const noexcept {
        return std::chrono::steady_clock::now() - idle_start_;
    }
    
    void update_idle_time() noexcept {
        idle_start_ = std::chrono::steady_clock::now();
    }
    
    Result<void> process_io();
    
private:
    Socket socket_;
    ConnectionState state_;
    std::chrono::steady_clock::time_point idle_start_;
    std::vector<uint8_t> rbuf_;
    std::vector<uint8_t> wbuf_;
    size_t wbuf_sent_{0};
    
    Result<void> handle_request();
    Result<void> handle_response();
    Result<bool> try_fill_buffer();
    Result<bool> try_flush_buffer();
    Result<bool> try_process_request();
};

// Global state with modern synchronization
class ServerState {
public:
    ServerState() = default;
    
    void add_connection(std::unique_ptr<Connection> conn) {
        std::unique_lock lock(mutex_);
        connections_[conn->fd()] = std::move(conn);
    }
    
    std::unique_ptr<Connection> remove_connection(int fd) {
        std::unique_lock lock(mutex_);
        auto it = connections_.find(fd);
        if (it == connections_.end()) {
            return nullptr;
        }
        auto conn = std::move(it->second);
        connections_.erase(it);
        return conn;
    }
    
    [[nodiscard]] std::vector<pollfd> get_poll_fds() const {
        std::shared_lock lock(mutex_);
        std::vector<pollfd> poll_fds;
        poll_fds.reserve(connections_.size() + 1);
        
        // Add listening socket
        poll_fds.push_back({
            .fd = listen_socket_.get(),
            .events = POLLIN,
            .revents = 0
        });
        
        // Add connection sockets
        for (const auto& [fd, conn] : connections_) {
            poll_fds.push_back({
                .fd = fd,
                .events = conn->state() == ConnectionState::Request ? POLLIN : POLLOUT,
                .revents = 0
            });
        }
        
        return poll_fds;
    }
    
    Result<void> initialize(uint16_t port);
    
private:
    mutable std::shared_mutex mutex_;
    Socket listen_socket_{-1};
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    HMap db_;
    std::vector<HeapItem> heap_;
    ThreadPool thread_pool_{4};
};

// Global state instance
ServerState g_server;

// Modern request parser using std::span
class RequestParser {
public:
    static Result<std::vector<std::string>> parse(std::span<const uint8_t> data) {
        if (data.size() < sizeof(uint32_t)) {
            return std::unexpected(std::make_error_code(std::errc::message_size));
        }

        uint32_t len;
        std::memcpy(&len, data.data(), sizeof(uint32_t));
        
        if (len > MAX_MSG_SIZE) {
            return std::unexpected(std::make_error_code(std::errc::message_size));
        }
        
        if (sizeof(uint32_t) + len > data.size()) {
            return std::unexpected(std::make_error_code(std::errc::message_size));
        }

        std::vector<std::string> cmd;
        const uint8_t* pos = data.data() + sizeof(uint32_t);
        const uint8_t* end = pos + len;
        
        while (pos < end) {
            if (end - pos < sizeof(uint32_t)) {
                return std::unexpected(std::make_error_code(std::errc::bad_message));
            }
            
            uint32_t str_len;
            std::memcpy(&str_len, pos, sizeof(uint32_t));
            pos += sizeof(uint32_t);
            
            if (str_len > static_cast<uint32_t>(end - pos)) {
                return std::unexpected(std::make_error_code(std::errc::bad_message));
            }
            
            cmd.emplace_back(reinterpret_cast<const char*>(pos), str_len);
            pos += str_len;
        }
        
        return cmd;
    }
};

// Modern response serializer
class ResponseSerializer {
public:
    template<typename T>
    static void serialize(std::vector<uint8_t>& buffer, const T& data);
    
    static void serialize_nil(std::vector<uint8_t>& buffer) {
        buffer.push_back(static_cast<uint8_t>(SerializationType::Nil));
    }
    
    static void serialize_error(std::vector<uint8_t>& buffer, 
                              int32_t code, 
                              std::string_view msg) {
        buffer.push_back(static_cast<uint8_t>(SerializationType::Error));
        append_data(buffer, code);
        append_data(buffer, static_cast<uint32_t>(msg.size()));
        buffer.insert(buffer.end(), msg.begin(), msg.end());
    }
    
    static void serialize_string(std::vector<uint8_t>& buffer, 
                               std::string_view str) {
        buffer.push_back(static_cast<uint8_t>(SerializationType::String));
        append_data(buffer, static_cast<uint32_t>(str.size()));
        buffer.insert(buffer.end(), str.begin(), str.end());
    }
    
    static void serialize_integer(std::vector<uint8_t>& buffer, int64_t value) {
        buffer.push_back(static_cast<uint8_t>(SerializationType::Integer));
        append_data(buffer, value);
    }
    
    static void serialize_double(std::vector<uint8_t>& buffer, double value) {
        buffer.push_back(static_cast<uint8_t>(SerializationType::Double));
        append_data(buffer, value);
    }

private:
    template<typename T>
    static void append_data(std::vector<uint8_t>& buffer, const T& data) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&data);
        buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
    }
};

// Implementation of Connection member functions
Result<void> Connection::process_io() {
    update_idle_time();
    
    switch (state_) {
        case ConnectionState::Request:
            return handle_request();
        case ConnectionState::Response:
            return handle_response();
        case ConnectionState::End:
            return std::unexpected(std::make_error_code(std::errc::connection_aborted));
        default:
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
}

Result<void> Connection::handle_request() {
    while (true) {
        auto result = try_fill_buffer();
        if (!result) {
            return std::unexpected(result.error());
        }
        if (!*result) {
            break;
        }
    }
    return {};
}

Result<void> Connection::handle_response() {
    while (true) {
        auto result = try_flush_buffer();
        if (!result) {
            return std::unexpected(result.error());
        }
        if (!*result) {
            break;
        }
    }
    return {};
}

Result<bool> Connection::try_fill_buffer() {
    assert(rbuf_.size() < MAX_MSG_SIZE);
    
    rbuf_.resize(MAX_MSG_SIZE);
    ssize_t rv;
    do {
        size_t capacity = rbuf_.size() - rbuf_.size();
        rv = read(socket_.get(), rbuf_.data() + rbuf_.size(), capacity);
    } while (rv < 0 && errno == EINTR);
    
    if (rv < 0) {
        if (errno == EAGAIN) {
            return false;
        }
        return std::unexpected(std::make_error_code(std::errc::io_error));
    }
    
    if (rv == 0) {
        state_ = ConnectionState::End;
        return false;
    }
    
    rbuf_.resize(rbuf_.size() + rv);
    
    while (try_process_request()) {}
    
    return true;
}

Result<bool> Connection::try_process_request() {
    if (rbuf_.size() < sizeof(uint32_t)) {
        return false;
    }
    
    auto parse_result = RequestParser::parse(std::span(rbuf_));
    if (!parse_result) {
        state_ = ConnectionState::End;
        return false;
    }
    
    auto& cmd = *parse_result;
    
    // Process command and generate response
    std::vector<uint8_t> response;
    process_command(cmd, response);
    
    // Prepare write buffer
    uint32_t wlen = static_cast<uint32_t>(response.size());
    wbuf_.clear();
    wbuf_.reserve(sizeof(wlen) + response.size());
    ResponseSerializer::append_data(wbuf_, wlen);
    wbuf_.insert(wbuf_.end(), response.begin(), response.end());
    
    // Update state
    state_ = ConnectionState::Response;
    wbuf_sent_ = 0;
    
    // Handle remaining data in read buffer
    size_t consumed = sizeof(uint32_t) + cmd.size();
    if (consumed < rbuf_.size()) {
        std::copy(rbuf_.begin() + consumed, rbuf_.end(), rbuf_.begin());
        rbuf_.resize(rbuf_.size() - consumed);
    } else {
        rbuf_.clear();
    }
    
    return rbuf_.size() >= sizeof(uint32_t);
}

Result<bool> Connection::try_flush_buffer() {
    while (wbuf_sent_ < wbuf_.size()) {
        ssize_t rv;
        do {
            size_t remain = wbuf_.size() - wbuf_sent_;
            rv = write(socket_.get(), 
                      wbuf_.data() + wbuf_sent_, 
                      remain);
        } while (rv < 0 && errno == EINTR);
        
        if (rv < 0) {
            if (errno == EAGAIN) {
                return false;
            }
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }
        
        wbuf_sent_ += rv;
    }
    
    if (wbuf_sent_ == wbuf_.size()) {
        state_ = ConnectionState::Request;
        wbuf_sent_ = 0;
        wbuf_.clear();
        return false;
    }
    
    return true;
}


// Modern command processor with type-safe command handling
class CommandProcessor {
public:
    struct CommandContext {
        const std::vector<std::string>& args;
        std::vector<uint8_t>& response;
        HMap& db;
        std::vector<HeapItem>& heap;
    };

    static void process_command(CommandContext ctx) {
        if (ctx.args.empty()) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "Empty command");
        }

        const auto& cmd = ctx.args[0];
        const auto command_it = command_handlers.find(
            std::string(to_lower(cmd)));
            
        if (command_it == command_handlers.end()) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_UNKNOWN, "Unknown command");
        }

        command_it->second(ctx);
    }

private:
    static constexpr auto to_lower = [](std::string_view str) {
        std::string result;
        result.reserve(str.size());
        std::ranges::transform(str, std::back_inserter(result), 
                             [](char c) { return std::tolower(c); });
        return result;
    };

    // Command handlers
    static void handle_get(CommandContext ctx) {
        if (ctx.args.size() != 2) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "GET requires exactly one key");
        }

        auto entry = lookup_entry(ctx.db, ctx.args[1]);
        if (!entry) {
            return ResponseSerializer::serialize_nil(ctx.response);
        }

        if (entry->type != EntryType::String) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_TYPE, "Key holds wrong type");
        }

        ResponseSerializer::serialize_string(ctx.response, entry->value);
    }

    static void handle_set(CommandContext ctx) {
        if (ctx.args.size() != 3) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "SET requires key and value");
        }

        auto [entry, created] = get_or_create_entry(ctx.db, ctx.args[1]);
        if (!created && entry->type != EntryType::String) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_TYPE, "Key holds wrong type");
        }

        entry->type = EntryType::String;
        entry->value = ctx.args[2];
        ResponseSerializer::serialize_nil(ctx.response);
    }

    static void handle_zadd(CommandContext ctx) {
        if (ctx.args.size() != 4) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "ZADD requires key, score and member");
        }

        double score;
        if (!parse_double(ctx.args[2], score)) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "Invalid score value");
        }

        auto [entry, created] = get_or_create_entry(ctx.db, ctx.args[1]);
        if (!created) {
            if (entry->type != EntryType::ZSet) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_TYPE, "Key holds wrong type");
            }
        } else {
            entry->type = EntryType::ZSet;
            entry->zset = std::make_unique<ZSet>();
        }

        bool added = entry->zset->add(ctx.args[3], score);
        ResponseSerializer::serialize_integer(ctx.response, added ? 1 : 0);
    }

    static void handle_zquery(CommandContext ctx) {
        if (ctx.args.size() != 6) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "ZQUERY requires key, score, name, offset, limit");
        }

        double score;
        if (!parse_double(ctx.args[2], score)) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "Invalid score value");
        }

        int64_t offset, limit;
        if (!parse_int(ctx.args[4], offset) || !parse_int(ctx.args[5], limit)) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "Invalid offset or limit");
        }

        auto entry = lookup_entry(ctx.db, ctx.args[1]);
        if (!entry) {
            ResponseSerializer::serialize_array(ctx.response, 0);
            return;
        }

        if (entry->type != EntryType::ZSet) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_TYPE, "Key holds wrong type");
        }

        if (limit <= 0) {
            ResponseSerializer::serialize_array(ctx.response, 0);
            return;
        }

        auto results = entry->zset->query(score, ctx.args[3], offset, limit);
        ResponseSerializer::serialize_array(ctx.response, results.size() * 2);
        
        for (const auto& result : results) {
            ResponseSerializer::serialize_string(ctx.response, result.name);
            ResponseSerializer::serialize_double(ctx.response, result.score);
        }
    }

    // TTL handling
    static void handle_pexpire(CommandContext ctx) {
        if (ctx.args.size() != 3) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "PEXPIRE requires key and milliseconds");
        }

        int64_t ttl_ms;
        if (!parse_int(ctx.args[2], ttl_ms)) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "Invalid TTL value");
        }

        auto entry = lookup_entry(ctx.db, ctx.args[1]);
        if (!entry) {
            ResponseSerializer::serialize_integer(ctx.response, 0);
            return;
        }

        set_entry_ttl(*entry, ttl_ms, ctx.heap);
        ResponseSerializer::serialize_integer(ctx.response, 1);
    }

    static void handle_pttl(CommandContext ctx) {
        if (ctx.args.size() != 2) {
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "PTTL requires key");
        }

        auto entry = lookup_entry(ctx.db, ctx.args[1]);
        if (!entry) {
            ResponseSerializer::serialize_integer(ctx.response, -2);
            return;
        }

        if (entry->heap_idx == static_cast<size_t>(-1)) {
            ResponseSerializer::serialize_integer(ctx.response, -1);
            return;
        }

        auto expire_at = ctx.heap[entry->heap_idx].val;
        auto now = get_monotonic_usec();
        auto ttl = expire_at > now ? (expire_at - now) / 1000 : 0;
        ResponseSerializer::serialize_integer(ctx.response, ttl);
    }

    // Command map using string_view for efficiency
    static inline const std::unordered_map<std::string, 
        std::function<void(CommandContext)>> command_handlers = {
        {"get", handle_get},
        {"set", handle_set},
        {"zadd", handle_zadd},
        {"zquery", handle_zquery},
        {"pexpire", handle_pexpire},
        {"pttl", handle_pttl}
        // Add other commands...
    };

    // Helper functions
    static bool parse_double(const std::string& str, double& out) {
        try {
            size_t pos;
            out = std::stod(str, &pos);
            return pos == str.size() && !std::isnan(out);
        } catch (...) {
            return false;
        }
    }

    static bool parse_int(const std::string& str, int64_t& out) {
        try {
            size_t pos;
            out = std::stoll(str, &pos);
            return pos == str.size();
        } catch (...) {
            return false;
        }
    }

    static std::uint64_t get_monotonic_usec() {
        using namespace std::chrono;
        return duration_cast<microseconds>(
            steady_clock::now().time_since_epoch()).count();
    }
};

// Modern entry managment with RAII
class EntryManager {
public:
    static void destroy_entry(Entry* entry) {
        if (!entry) return;

        entry->value.clear();
        if (entry->zset) {
            entry->zset->clear();
            entry->zset.reset();
        }
        delete entry;
    }

    static void delete_entry_async(Entry* entry, ThreadPool& pool) {
        if (!entry) return;

        pool.enqueue([entry]() {
            destroy_entry(entry);
        });
    }

    static void set_entry_ttl(Entry& entry, 
                            int64_t ttl_ms, 
                            std::vector<HeapItem>& heap) {
        if (ttl_ms < 0) {
            if (entry.heap_idx != static_cast<size_t>(-1)) {
                remove_from_heap(entry, heap);
            }
            return;
        }

        auto expire_at = get_monotonic_usec() + 
                        static_cast<uint64_t>(ttl_ms) * 1000;
        
        if (entry.heap_idx == static_cast<size_t>(-1)) {
            add_to_heap(entry, expire_at, heap);
        } else {
            update_heap(entry, expire_at, heap);
        }
    }

private:
    static void remove_from_heap(Entry& entry, 
                               std::vector<HeapItem>& heap) {
        size_t pos = entry.heap_idx;
        heap[pos] = heap.back();
        heap.pop_back();
        
        if (pos < heap.size()) {
            heap_update(heap.data(), pos, heap.size());
        }
        entry.heap_idx = -1;
    }

    static void add_to_heap(Entry& entry, 
                          uint64_t expire_at, 
                          std::vector<HeapItem>& heap) {
        HeapItem item{expire_at, &entry.heap_idx};
        heap.push_back(item);
        heap_update(heap.data(), heap.size() - 1, heap.size());
    }

    static void update_heap(Entry& entry, 
                          uint64_t expire_at, 
                          std::vector<HeapItem>& heap) {
        heap[entry.heap_idx].val = expire_at;
        heap_update(heap.data(), entry.heap_idx, heap.size());
    }
};
} // Namedspace
class Server {
public:
    Server(uint16_t port, size_t thread_pool_size) 
        : port_(port)
        , thread_pool_(thread_pool_size) {
    }

    Result<void> initialize() {
        // Create listening socket
        auto socket_result = create_listen_socket();
        if (!socket_result) {
            return std::unexpected(socket_result.error());
        }
        listen_socket_ = std::move(*socket_result);

        dlist_init(&idle_list_);
        return {};
    }

    void run() {
        std::vector<pollfd> poll_args;
        
        while (!should_stop_) {
            prepare_poll_args(poll_args);
            
            int timeout_ms = static_cast<int>(calculate_next_timeout());
            int rv = poll(poll_args.data(), poll_args.size(), timeout_ms);
            
            if (rv < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw std::system_error(
                    std::make_error_code(std::errc::io_error),
                    "Poll failed"
                );
            }

            process_active_connections(poll_args);
            process_timers();
            accept_new_connections(poll_args[0]);
        }
    }

    void stop() {
        should_stop_ = true;
    }

private:
    Result<Socket> create_listen_socket() {
        Socket sock(socket(AF_INET, SOCK_STREAM, 0));
        if (sock.get() < 0) {
            return std::unexpected(
                std::make_error_code(std::errc::bad_file_descriptor)
            );
        }

        // Enable address reuse
        int val = 1;
        if (setsockopt(sock.get(), SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
            return std::unexpected(
                std::make_error_code(std::errc::invalid_argument)
            );
        }

        // Bind socket
        sockaddr_in addr{
            .sin_family = AF_INET,
            .sin_port = htons(port_),
            .sin_addr = {.s_addr = htonl(INADDR_ANY)}
        };

        if (bind(sock.get(), 
                reinterpret_cast<sockaddr*>(&addr), 
                sizeof(addr)) < 0) {
            return std::unexpected(
                std::make_error_code(std::errc::address_in_use)
            );
        }

        // Listen
        if (listen(sock.get(), SOMAXCONN) < 0) {
            return std::unexpected(
                std::make_error_code(std::errc::connection_refused)
            );
        }

        // Set non-blocking mode
        auto result = sock.set_nonblocking();
        if (!result) {
            return std::unexpected(result.error());
        }

        return std::move(sock);
    }

    void prepare_poll_args(std::vector<pollfd>& poll_args) {
        poll_args.clear();
        
        // Add listening socket
        poll_args.push_back({
            .fd = listen_socket_.get(),
            .events = POLLIN,
            .revents = 0
        });

        // Add connection sockets
        std::shared_lock lock(connections_mutex_);
        for (const auto& [fd, conn] : connections_) {
            poll_args.push_back({
                .fd = fd,
                .events = conn->state() == ConnectionState::Request 
                    ? POLLIN : POLLOUT,
                .events = POLLERR,
                .revents = 0
            });
        }
    }

    std::chrono::milliseconds calculate_next_timeout() {
        using namespace std::chrono;
        auto now = steady_clock::now();
        auto next_timeout = now + hours(24); // Default timeout

        // Check idle connections
        if (!dlist_empty(&idle_list_)) {
            auto* next = container_of(idle_list_.next, Connection, idle_list);
            auto idle_timeout = next->idle_start() + IDLE_TIMEOUT;
            next_timeout = std::min(next_timeout, idle_timeout);
        }

        // Check TTL heap
        if (!heap_.empty()) {
            auto ttl_timeout = time_point<steady_clock>(
                microseconds(heap_[0].val)
            );
            next_timeout = std::min(next_timeout, ttl_timeout);
        }

        return duration_cast<milliseconds>(next_timeout - now);
    }

    void process_active_connections(const std::vector<pollfd>& poll_args) {
        for (size_t i = 1; i < poll_args.size(); ++i) {
            if (poll_args[i].revents == 0) {
                continue;
            }

            std::unique_lock lock(connections_mutex_);
            auto it = connections_.find(poll_args[i].fd);
            if (it == connections_.end()) {
                continue;
            }

            auto& conn = it->second;
            try {
                auto result = conn->process_io();
                if (!result) {
                    remove_connection(conn->fd());
                }
            } catch (const std::exception& e) {
                log_message("Connection processing error: {}", e.what());
                remove_connection(conn->fd());
            }
        }
    }

    void process_timers() {
        using namespace std::chrono;
        auto now = steady_clock::now();

        // Process idle connections
        while (!dlist_empty(&idle_list_)) {
            auto* next = container_of(idle_list_.next, Connection, idle_list);
            if (now - next->idle_start() < IDLE_TIMEOUT) {
                break;
            }
            remove_connection(next->fd());
        }

        // Process TTL heap
        const size_t max_ttl_ops = 2000;
        size_t ttl_ops = 0;
        
        while (!heap_.empty() && 
               microseconds(heap_[0].val) <= now.time_since_epoch() && 
               ttl_ops < max_ttl_ops) 
        {
            auto* entry = container_of(heap_[0].ref, Entry, heap_idx);
            auto node = hm_pop(&db_, &entry->node, 
                             [](HNode* a, HNode* b) { return a == b; });
            
            assert(node == &entry->node);
            EntryManager::delete_entry_async(entry, thread_pool_);
            ttl_ops++;
        }
    }

    void accept_new_connections(const pollfd& listen_poll) {
        if (!(listen_poll.revents & POLLIN)) {
            return;
        }

        while (true) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            
            int client_fd = accept(listen_socket_.get(),
                                 reinterpret_cast<sockaddr*>(&client_addr),
                                 &addr_len);

            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                throw std::system_error(
                    std::make_error_code(std::errc::io_error),
                    "Accept failed"
                );
            }

            try {
                Socket client_socket(client_fd);
                auto result = client_socket.set_nonblocking();
                if (!result) {
                    continue;
                }

                auto conn = std::make_unique<Connection>(std::move(client_socket));
                add_connection(std::move(conn));
                
            } catch (const std::exception& e) {
                log_message("Failed to create connection: {}", e.what());
                close(client_fd);
            }
        }
    }

    void add_connection(std::unique_ptr<Connection> conn) {
        std::unique_lock lock(connections_mutex_);
        int fd = conn->fd();
        connections_.emplace(fd, std::move(conn));
        dlist_insert_before(&idle_list_, &connections_[fd]->idle_list);
    }

    void remove_connection(int fd) {
        std::unique_lock lock(connections_mutex_);
        auto it = connections_.find(fd);
        if (it != connections_.end()) {
            dlist_detach(&it->second->idle_list);
            connections_.erase(it);
        }
    }

    uint16_t port_;
    Socket listen_socket_{-1};
    ThreadPool thread_pool_;
    std::atomic<bool> should_stop_{false};
    
    mutable std::shared_mutex connections_mutex_;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    
    DList idle_list_;
    HMap db_;
    std::vector<HeapItem> heap_;
};

int main(int argc, char* argv[]) {
    try {
        uint16_t port = 1234;
        size_t thread_pool_size = 4;

        // Parse command line arguments if needed
        // ...

        Server server(port, thread_pool_size);
        
        auto result = server.initialize();
        if (!result) {
            std::cerr << "Failed to initialize server: " 
                     << result.error().message() << "\n";
            return 1;
        }

        // Set up signal handling for graceful shutdown
        std::signal(SIGINT, [](int) {
            // Signal handler code
        });

        server.run();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}