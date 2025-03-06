#ifndef CONNECTION_HPP
#define CONNECTION_HPP


// Connection states as strongly typed enum
enum class ConnectionState : uint8_t {
    Request,
    Response,
    End
};
// Modern error handling with std::expected
template<typename T>
using Result = std::expected<T, std::error_code>;

// Constants
static constexpr size_t MAX_MSG_SIZE = 4096;
static constexpr auto IDLE_TIMEOUT = std::chrono::milliseconds(5000);
static constexpr uint16_t SERVER_PORT = 1234;

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