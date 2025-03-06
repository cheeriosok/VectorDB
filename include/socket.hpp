#ifndef SOCKET_HPP
#define SOCKET_HPP

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


#endif
