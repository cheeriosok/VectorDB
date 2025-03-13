#ifndef SERVER_HPP
#define SERVER_HPP

#include <memory>         
#include <vector>         
#include <unordered_map>  
#include <cstdint>        
#include <shared_mutex>   
#include <expected>      
#include <system_error>   
#include <poll.h>     
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <atomic>
#include "logging.hpp"
#include "socket.hpp"
#include "connection.hpp"
#include "command_processor.hpp"
#include "src/thread_pool.hpp"
#include "entry_manager.hpp"
#include "src/list.hpp"  

template<typename T>
using Result = std::expected<T, std::error_code>;

class Server {
private:
    uint16_t port_;
    Socket listen_socket_{-1};
    ThreadPool thread_pool_;
    CommandProcessor command_processor_;
    EntryManager entry_manager_;
    std::atomic<bool> should_stop_;
public:
    Server(uint16_t port, size_t thread_pool_size)
        : port_(port), thread_pool_(thread_pool_size), 
          command_processor_(), entry_manager_(), should_stop_(false) {}

    Result<void> initialize();
    void run();
    void stop();

    [[nodiscard]] int get_listen_socket_fd() const {
        return listen_socket_.get();
    }

    mutable std::shared_mutex connections_mutex_;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
    DoublyLinkedList<Connection*> idle_list_;

    Result<Socket> create_listen_socket();
    void prepare_poll_args(std::vector<pollfd>& poll_args);
    std::chrono::milliseconds calculate_next_timeout();
    void process_active_connections(const std::vector<pollfd>& poll_args);
    void process_timers();
    void accept_new_connections(const pollfd& listen_poll);
    void add_connection(std::unique_ptr<Connection> conn);
    void remove_connection(int fd);
};

Result<void> Server::initialize() {
    auto listen_result = create_listen_socket();
    if (!listen_result) {
        return std::unexpected(listen_result.error());
    }

    listen_socket_ = std::move(*listen_result);
    return {};
}

inline Result<Socket> Server::create_listen_socket() {
    Socket sock(socket(AF_INET, SOCK_STREAM, 0));
    if (sock.get() < 0) {
        return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
    }

    int val = 1;
    if (setsockopt(sock.get(), SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        return std::unexpected(std::make_error_code(std::errc::address_in_use));
    }

    if (listen(sock.get(), SOMAXCONN) < 0) {
        return std::unexpected(std::make_error_code(std::errc::connection_refused));
    }

    auto result = sock.set_nonblocking();
    if (!result) {
        return std::unexpected(result.error());
    }

    return std::move(sock);
}

inline void Server::prepare_poll_args(std::vector<pollfd>& poll_args) {
    poll_args.clear();
    poll_args.push_back({listen_socket_.get(), POLLIN, 0});

    std::shared_lock lock(connections_mutex_);
    for (const auto& [fd, conn] : connections_) {
        poll_args.push_back({fd, static_cast<short>(conn->state() == ConnectionState::Request ? POLLIN : POLLOUT), 0});
    }
}

inline std::chrono::milliseconds Server::calculate_next_timeout() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto next_timeout = now + hours(24);

    if (!idle_list_.empty()) {
        auto next = *idle_list_.begin();  // Get the first idle connection
        auto idle_timeout = next->idle_start() + IDLE_TIMEOUT;
        next_timeout = std::min(next_timeout, idle_timeout);
    }

    return duration_cast<milliseconds>(next_timeout - now);
}

inline void Server::process_active_connections(const std::vector<pollfd>& poll_args) {
    for (size_t i = 1; i < poll_args.size(); ++i) {
        if (poll_args[i].revents == 0) continue;

        std::unique_lock lock(connections_mutex_);
        auto it = connections_.find(poll_args[i].fd);
        if (it == connections_.end()) continue;

        auto& conn = it->second;
        try {
            auto result = conn->process_io();
            if (!result) remove_connection(conn->fd());
        } catch (const std::exception& e) {
            log_message("Connection error: {}", e.what());
            remove_connection(conn->fd());
        }
    }
}

inline void Server::process_timers() {
    using namespace std::chrono;
    auto now = steady_clock::now();

    while (!idle_list_.empty()) {
        auto next = *idle_list_.begin();
        if (now - next->idle_start() < IDLE_TIMEOUT) {
            break;
        }
        remove_connection(next->fd());
    }
}

inline void Server::accept_new_connections(const pollfd& listen_poll) {
    if (!(listen_poll.revents & POLLIN)) return;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(listen_socket_.get(), reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

        if (client_fd < 0) break;

        try {
            Socket client_socket(client_fd);
            auto result = client_socket.set_nonblocking();
            if (!result) continue;

            auto conn = std::make_unique<Connection>(
                std::move(client_socket),
                entry_manager_,
                connections_mutex_,  
                command_processor_
            );
                      

            add_connection(std::move(conn));
        } catch (...) {
            close(client_fd);
        }
    }
}

inline void Server::add_connection(std::unique_ptr<Connection> conn) {
    std::unique_lock lock(connections_mutex_);
    int fd = conn->fd();
    connections_.emplace(fd, std::move(conn));
}

inline void Server::remove_connection(int fd) {
    std::unique_lock lock(connections_mutex_);
    connections_.erase(fd);
}

void Server::run() {
    std::cout << "Server is running on port " << port_ << "...\n";
    
    while (!should_stop_) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); \
    }

    std::cout << "Server shutting down...\n";
}

inline void Server::stop() {
    should_stop_ = true;
}

#endif // SERVER_HPP
