#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <vector>
#include <chrono>
#include "socket.hpp"
#include "request_parser.hpp"
#include "response_serializer.hpp"

enum class ConnectionState : uint8_t { Request, Response, End };

class Connection {
public:
    explicit Connection(Socket socket)
        : socket_(std::move(socket)), state_(ConnectionState::Request),
          idle_start_(std::chrono::steady_clock::now()) {}

    [[nodiscard]] int fd() const noexcept { return socket_.get(); }
    
    Result<void> process_io() {
        update_idle_time();
        return (state_ == ConnectionState::Request) ? handle_request() : handle_response();
    }

private:
    Socket socket_;
    ConnectionState state_;
    std::chrono::steady_clock::time_point idle_start_;
    std::vector<uint8_t> rbuf_;
    std::vector<uint8_t> wbuf_;

    void update_idle_time() noexcept { idle_start_ = std::chrono::steady_clock::now(); }
    
    Result<void> handle_request() { return {}; }
    Result<void> handle_response() { return {}; }
};

#endif // CONNECTION_HPP
