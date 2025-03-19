#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <cstdint>        
#include <vector>         
#include <chrono>         
#include <system_error>   
#include <expected>       
#include <cassert>       
#include <span>           
#include <algorithm>      
#include <unistd.h>       
#include <cerrno>         
#include <mutex>          
#include "socket.hpp"               
#include "request_parser.hpp"       
#include "response_serializer.hpp"  
#include "command_processor.hpp"   
#include "entry_manager.hpp"        

static constexpr size_t MAX_MSG_SIZE = 4096; 
static constexpr auto IDLE_TIMEOUT = std::chrono::milliseconds(5000); 
static constexpr uint16_t SERVER_PORT = 4321; 


enum class ConnectionState : uint8_t {
    Request,  
    Response, 
    End       
};

template<typename T>
using Result = std::expected<T, std::error_code>;

class Connection {
    public:
        Connection(Socket socket, EntryManager& entry_manager, CommandProcessor& processor)
            : socket_(std::move(socket)), 
              entry_manager_(entry_manager), 
              command_processor_(processor),
              state_(ConnectionState::Request),
              idle_start_(std::chrono::steady_clock::now()) {
            rbuf_.reserve(MAX_MSG_SIZE);
            wbuf_.reserve(MAX_MSG_SIZE);
        }
    

    [[nodiscard]] int fd() const noexcept { return socket_.get(); }  
    [[nodiscard]] ConnectionState state() const noexcept { return state_; }  
    [[nodiscard]] auto idle_start() const noexcept { return idle_start_; }  
    
    void update_idle_time() noexcept { idle_start_ = std::chrono::steady_clock::now(); } 
    Result<void> process_io();

private:
    Socket socket_;  
    EntryManager& entry_manager_;  
    CommandProcessor& command_processor_;  
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

Result<void> Connection::process_io() {
    char buffer[1024] = {0};
    ssize_t bytes_read = read(socket_.get(), buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';  
        std::string request(buffer);
        std::cout << " Received command: " << request << std::endl;

        std::vector<std::string> args;
        std::istringstream iss(request);
        std::string word;
        while (iss >> word) {
            args.push_back(word);
        }

        std::vector<uint8_t> response;
        CommandProcessor::CommandContext ctx{args, response, entry_manager_};  
        command_processor_.process_command(ctx);

        std::string response_str(response.begin(), response.end());
        ssize_t bytes_sent = write(socket_.get(), response_str.c_str(), response.size());

        if (bytes_sent < 0) {
            std::cerr << " Write failed: " << strerror(errno) << std::endl;
        } else {
            std::cout << " Sent response: " << response_str << std::endl;
        }

        return {};
    } else if (bytes_read == 0) {
        std::cerr << "Client disconnected.\n";
        return std::unexpected(std::make_error_code(std::errc::connection_reset));
    } else {
        std::cerr << " Read error: " << strerror(errno) << std::endl;
        return std::unexpected(std::make_error_code(std::errc::io_error));
    }
}




inline Result<void> Connection::handle_request() {
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

inline Result<void> Connection::handle_response() {
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

inline Result<bool> Connection::try_fill_buffer() {
    assert(rbuf_.size() < MAX_MSG_SIZE); 
    
    ssize_t rv;
    do {
        size_t capacity = MAX_MSG_SIZE - rbuf_.size();
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

inline Result<bool> Connection::try_process_request() {
    if (rbuf_.size() < sizeof(uint32_t)) {
        return false; 
    }
    
    auto parse_result = RequestParser::parse(std::span(rbuf_));
    if (!parse_result) {
        state_ = ConnectionState::End; 
        return false;
    }
    
    auto& cmd = *parse_result;
    
    std::vector<uint8_t> response;
    CommandProcessor::CommandContext ctx{
        cmd, response, entry_manager_
    };
    
    command_processor_.process_command(ctx);
    
    uint32_t wlen = static_cast<uint32_t>(response.size());
    wbuf_.clear();
    wbuf_.reserve(sizeof(wlen) + response.size());
    ResponseSerializer::append_data(wbuf_, wlen);
    wbuf_.insert(wbuf_.end(), response.begin(), response.end());

    state_ = ConnectionState::Response;
    wbuf_sent_ = 0;
    
    size_t consumed = sizeof(uint32_t) + cmd.size();
    if (consumed < rbuf_.size()) {
        std::copy(rbuf_.begin() + consumed, rbuf_.end(), rbuf_.begin());
        rbuf_.resize(rbuf_.size() - consumed);
    } else {
        rbuf_.clear();
    }
    
    return rbuf_.size() >= sizeof(uint32_t);
}

inline Result<bool> Connection::try_flush_buffer() {
    while (wbuf_sent_ < wbuf_.size()) {
        ssize_t rv;
        do {
            size_t remain = wbuf_.size() - wbuf_sent_;
            rv = write(socket_.get(), wbuf_.data() + wbuf_sent_, remain);
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

#endif // CONNECTION_HPP
