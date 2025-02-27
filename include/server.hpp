#ifndef SERVER_HPP
#define SERVER_HPP

#include <unordered_map>
#include <memory>
#include <thread>
#include <shared_mutex>
#include <poll.h>
#include "socket.hpp"
#include "connection.hpp"
#include "server_state.hpp"
#include "command_processor.hpp"

class Server {
public:
    Server(uint16_t port, size_t thread_pool_size) 
        : port_(port), thread_pool_(thread_pool_size) {}

    Result<void> initialize() {
        listen_socket_ = Socket(socket(AF_INET, SOCK_STREAM, 0));
        if (listen_socket_.get() < 0) {
            return std::unexpected(std::make_error_code(std::errc::bad_file_descriptor));
        }
        return {};
    }

    void run() {
        std::vector<pollfd> poll_args;
        while (!should_stop_) {
            prepare_poll_args(poll_args);
            poll(poll_args.data(), poll_args.size(), 5000);
            process_active_connections(poll_args);
        }
    }

    void stop() { should_stop_ = true; }

private:
    uint16_t port_;
    Socket listen_socket_{-1};
    std::atomic<bool> should_stop_{false};
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
};

#endif 
