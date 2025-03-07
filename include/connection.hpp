#ifndef CONNECTION_HPP
#define CONNECTION_HPP

// defines the different states a connection can be in
enum class ConnectionState : uint8_t {
    Request,  // Waiting for a request from the client
    Response, // Processing and sending response
    End       // Connection is closing
};

// modern error handling using std::expected for better error management
template<typename T>
using Result = std::expected<T, std::error_code>;

// constants defining buffer sizes and timeout settings
static constexpr size_t MAX_MSG_SIZE = 4096; // Maximum message size
static constexpr auto IDLE_TIMEOUT = std::chrono::milliseconds(5000); // Timeout for idle connections
static constexpr uint16_t SERVER_PORT = 1234; // Default server port

// connection class implementing RAII (Resource Acquisition Is Initialization) for socket management
class Connection {
    public:
        // constructor initializing socket, state, and idle start time
        explicit Connection(Socket socket) 
            : socket_(std::move(socket))
            , state_(ConnectionState::Request)
            , idle_start_(std::chrono::steady_clock::now()) {
            rbuf_.reserve(MAX_MSG_SIZE); // reserve space for read buffer
            wbuf_.reserve(MAX_MSG_SIZE); // rserve space for write buffer
        }
        
        // getter - returns the file descriptor of the socket
        [[nodiscard]] int fd() const noexcept { return socket_.get(); }
        
        // getter - returns the current connection state
        [[nodiscard]] ConnectionState state() const noexcept { return state_; }
        
        // getter - returns the duration the connection has been idle
        [[nodiscard]] auto idle_duration() const noexcept {
            return std::chrono::steady_clock::now() - idle_start_;
        }
        
        // setter- updates the idle start time to the current time
        void update_idle_time() noexcept {
            idle_start_ = std::chrono::steady_clock::now();
        }
        
        // declare process_io processes 
        Result<void> process_io();
        
    private:
        Socket socket_; // socket for communication
        ConnectionState state_; // current connection state
        std::chrono::steady_clock::time_point idle_start_; // last activity timestamp
        std::vector<uint8_t> rbuf_; // read buffer
        std::vector<uint8_t> wbuf_; // write buffer
        size_t wbuf_sent_{0}; // tracks the amount of data sent
        
        // handles request processing
        Result<void> handle_request();
        
        // handles response processing
        Result<void> handle_response();
        
        // attempts to fill the read buffer with incoming data
        Result<bool> try_fill_buffer();
        
        // attempts to flush the write buffer to the client
        Result<bool> try_flush_buffer();
        
        // attempts to process a request if enough data is available
        Result<bool> try_process_request();
};

// implementation of connection member functions
Result<void> Connection::process_io() {
    update_idle_time(); // update idle time to current time
    
    // switch on connection state to determine what to process
    switch (state_) {
        case ConnectionState::Request:
            return handle_request(); // Process client request
        case ConnectionState::Response:
            return handle_response(); // Process server response
        case ConnectionState::End:
            return std::unexpected(std::make_error_code(std::errc::connection_aborted));
        default:
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
}

// handles incoming client requests
Result<void> Connection::handle_request() {
    while (true) {
        auto result = try_fill_buffer(); // attempt to read data into buffer (see method below)
        if (!result) {
            return std::unexpected(result.error()); // return error if read fails
        }
        if (!*result) {
            break; // stop if no more data is available
        }
    }
    return {};
}

// handles sending responses back to the client
Result<void> Connection::handle_response() {
    while (true) {
        auto result = try_flush_buffer(); // attempt to write data to socket
        if (!result) {
            return std::unexpected(result.error()); // return error if write fails
        }
        if (!*result) {
            break; // stop if no more data to write
        }
    }
    return {};
}

// attempts to read data into the read buffer
Result<bool> Connection::try_fill_buffer() {
    assert(rbuf_.size() < MAX_MSG_SIZE); // throw an error if we've exceeded max_msg_size
    
    rbuf_.resize(MAX_MSG_SIZE); //
    ssize_t rv;
    do {
        size_t capacity = rbuf_.size() - rbuf_.size();
        rv = read(socket_.get(), rbuf_.data() + rbuf_.size(), capacity);
    } while (rv < 0 && errno == EINTR);
    
    if (rv < 0) {
        if (errno == EAGAIN) {
            return false; // no data available, try again later
        }
        return std::unexpected(std::make_error_code(std::errc::io_error)); // return error on failure
    }
    
    if (rv == 0) {
        state_ = ConnectionState::End; // mark connection as closed if no data read
        return false;
    }
    
    rbuf_.resize(rbuf_.size() + rv);
    
    while (try_process_request()) {} // process available requests
    
    return true;
}

// processes a request if the read buffer contains enough data
Result<bool> Connection::try_process_request() {
    if (rbuf_.size() < sizeof(uint32_t)) {
        return false; // not enough data to process a request
    }
    
    auto parse_result = RequestParser::parse(std::span(rbuf_));
    if (!parse_result) {
        state_ = ConnectionState::End; // nd connection if parsing fails
        return false;
    }
    
    auto& cmd = *parse_result;
    
    // process command and generate response
    std::vector<uint8_t> response;
    process_command(cmd, response);
    
    // prepare write buffer
    uint32_t wlen = static_cast<uint32_t>(response.size());
    wbuf_.clear();
    wbuf_.reserve(sizeof(wlen) + response.size());
    ResponseSerializer::append_data(wbuf_, wlen);
    wbuf_.insert(wbuf_.end(), response.begin(), response.end());
    
    // update state to response
    state_ = ConnectionState::Response;
    wbuf_sent_ = 0;
    
    // remove processed data from read buffer
    size_t consumed = sizeof(uint32_t) + cmd.size();
    if (consumed < rbuf_.size()) {
        std::copy(rbuf_.begin() + consumed, rbuf_.end(), rbuf_.begin());
        rbuf_.resize(rbuf_.size() - consumed);
    } else {
        rbuf_.clear();
    }
    
    return rbuf_.size() >= sizeof(uint32_t);
}

// attempts to flush data from the write buffer to the socket
Result<bool> Connection::try_flush_buffer() {
    while (wbuf_sent_ < wbuf_.size()) {
        ssize_t rv;
        do {
            size_t remain = wbuf_.size() - wbuf_sent_;
            rv = write(socket_.get(), wbuf_.data() + wbuf_sent_, remain);
        } while (rv < 0 && errno == EINTR);
        
        if (rv < 0) {
            if (errno == EAGAIN) {
                return false; // no data written, try again later
            }
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }
        
        wbuf_sent_ += rv;
    }
    
    if (wbuf_sent_ == wbuf_.size()) {
        state_ = ConnectionState::Request; // reset to request state after sending
        wbuf_sent_ = 0;
        wbuf_.clear();
        return false;
    }
    
    return true;
}
