#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <unistd.h>      // For close()
#include <fcntl.h>       // For fcntl(), O_NONBLOCK
#include <utility>       // For std::exchange
#include <system_error>  // For std::make_error_code, std::errc
#include <expected>      // For std::expected (if using C++23)

template<typename T>
using Result = std::expected<T, std::error_code>;

// socket class manages a file descriptor (fd) and ensures proper cleanup
class Socket {
public:
    // constructor takes an existing file descriptor
    explicit Socket(int fd) : fd_(fd) {}
    
    // destructor closes the file descriptor if it's valid
    ~Socket() { 
        if (fd_ != -1) {
            close(fd_);
        }
    }
    
    // delete copy constructor and assignment to prevent unintended copying
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    
    // move constructor/assignment transfers ownership of fd from another socket object
    Socket(Socket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) { // prevent self-assignment
            if (fd_ != -1) { // close existing fd before taking new ownership
                close(fd_);
            }
            fd_ = std::exchange(other.fd_, -1); // transfer ownership
        }
        return *this;
    }
    
    // returns the file descriptor
    [[nodiscard]] int get() const noexcept { return fd_; }
    
    // sets the socket to non-blocking mode
    Result<void> set_nonblocking() const {
        int flags = fcntl(fd_, F_GETFL, 0); // get current flags
        if (flags == -1) { // check for errors
            return std::unexpected(std::make_error_code(static_cast<std::errc>(errno)));
        }
        
        flags |= O_NONBLOCK; // enable non-blocking mode
        if (fcntl(fd_, F_SETFL, flags) == -1) { // apply new flags
            return std::unexpected(std::make_error_code(static_cast<std::errc>(errno)));
        }
        
        return {}; // success
    }

private:
    int fd_; // file descriptor for the socket
};

#endif