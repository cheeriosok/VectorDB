
static constexpr size_t MAX_MSG_SIZE = 4096; // Maximum message size (4KB)
static constexpr auto IDLE_TIMEOUT = std::chrono::milliseconds(5000); // 5-second timeout for idle connections
static constexpr uint16_t SERVER_PORT = 4321; // The port number the server will listen on

#include <memory>         
#include <vector>         
#include <unordered_map>  
#include <cstdint>        
#include <shared_mutex>   
#include <expected>      
#include <system_error>   
#include <poll.h>         
#include "logging.hpp"
#include "heap.hpp"
#include "socket.hpp"
#include "list.hpp"
#include "connection.hpp"
#include "hashtable.hpp"
#include "heap.hpp"
#include "thread_pool.hpp"

template<typename T>
using Result = std::expected<T, std::error_code>;

// server class - manages socket creation, polling, and connection handling
class Server {
    public:
    /// constructor: initializes the server with the given port and thread pool size
        Server(uint16_t port, size_t thread_pool_size) 
            : port_(port), thread_pool_(thread_pool_size) {}
    
        // initializes the server by setting up the listening socket (create_listening_socket dep)
        Result<void> initialize() {
            Result<Socket> socket_result = create_listen_socket(); // create a socket to listen for incoming connections
            if (!socket_result) { // check if there was an error in creating the listening socket
                return std::unexpected(socket_result.error());
            }
            listen_socket_ = std::move(*socket_result); // store the socket in a member variable
            /////////////////////////////////////////////////////////////////////////////////////
            dlist_init(&idle_list_); // !!!!!!PROBLEM: Dlst_init IS PSEUDOCODE FOR DOUBLE LINKED LIST - IMPLEMENT METHOD!!!!!!!
            /////////////////////////////////////////////////////////////////////////////////////
            
            return {};
        } // this was the complete initialization class 
    
        // runs the server loop, monitoring connections / events
        void run() {
            std::vector<pollfd> poll_args; // vector of file descriptors to monitor
            
            while (!should_stop_) { // keep running until `should_stop_` is set to true
                prepare_poll_args(poll_args); // prepare the list of file descriptors to monitor
                
                int timeout_ms = static_cast<int>(calculate_next_timeout()); // clculate the next timeout value
                int rv = poll(poll_args.data(), poll_args.size(), timeout_ms); // wait for events on file descriptors
                
                if (rv < 0) { // if poll() fails
                    if (errno == EINTR) { // if interrupted, continue
                        continue;
                    }
                    throw std::system_error( // else thow error
                        std::make_error_code(std::errc::io_error),
                        "Poll failed"
                    );
                }
    
                process_active_connections(poll_args); // handle active connections
                process_timers(); // process any timed events
                accept_new_connections(poll_args[0]); // accept new client connections
            }
        }
    
        // helper to stops the server by setting the `should_stop_` flag to true
        void stop() {
            should_stop_ = true;
        }
    
        // creates and configures the listening socket
        Result<Socket> create_listen_socket() { 
            Socket sock(socket(AF_INET, SOCK_STREAM, 0)); // create a TCP socket (IPv4)
            if (sock.get() < 0) { // Check for errors in socket creation
                return std::unexpected(
                    std::make_error_code(std::errc::bad_file_descriptor)
                );
            }
    
            // enable address reuse (allows quick restart of server without waiting for TIME_WAIT expiration)
            int val = 1;
            if (setsockopt(sock.get(), SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
                return std::unexpected(
                    std::make_error_code(std::errc::invalid_argument)
                );
            }
    
            // bind the socket to the specified port
            sockaddr_in addr{
                .sin_family = AF_INET, // IPv4
                .sin_port = htons(port_), // convert port number to network byte order
                .sin_addr = {.s_addr = htonl(INADDR_ANY)} // bind to all available network interfaces
            };
    
            if (bind(sock.get(), 
                    reinterpret_cast<sockaddr*>(&addr), 
                    sizeof(addr)) < 0) {
                return std::unexpected(
                    std::make_error_code(std::errc::address_in_use)
                );
            }
    
            // Start listening for incoming connections
            if (listen(sock.get(), SOMAXCONN) < 0) {
                return std::unexpected(
                    std::make_error_code(std::errc::connection_refused)
                );
            }
    
            // Set the socket to non-blocking mode
            auto result = sock.set_nonblocking();
            if (!result) {
                return std::unexpected(result.error());
            }
    
            return std::move(sock); // return the configured socket
        }
    
        // Prepares the list of sockets to be monitored by `poll()`
        void prepare_poll_args(std::vector<pollfd>& poll_args) {
            poll_args.clear();
            
            // Add the listening socket to the polling list
            poll_args.push_back({
                .fd = listen_socket_.get(),
                .events = POLLIN, // Monitor for incoming connections
                .revents = 0
            });
    
            // Add active client connections
            std::shared_lock lock(connections_mutex_);
            for (const auto& [fd, conn] : connections_) {
                poll_args.push_back({
                    .fd = fd,
                    .events = conn->state() == ConnectionState::Request 
                        ? POLLIN : POLLOUT, // Determine whether to read or write
                    .events = POLLERR, // Monitor for errors
                    .revents = 0
                });
            }
        }
    
        // calculates the next timeout duration for poll()
        std::chrono::milliseconds calculate_next_timeout() {
            using namespace std::chrono;
            auto now = steady_clock::now();
            auto next_timeout = now + hours(24); // Default timeout (24 hours if nothing else applies)
    
            // check if there are idle connections
            if (!dlist_empty(&idle_list_)) {
                auto* next = container_of(idle_list_.next, Connection, idle_list);
                auto idle_timeout = next->idle_start() + IDLE_TIMEOUT;
                next_timeout = std::min(next_timeout, idle_timeout);
            }
    
            // check TTL heap for expiring items
            if (!heap_.empty()) {
                auto ttl_timeout = time_point<steady_clock>(
                    microseconds(heap_[0].val)
                );
                next_timeout = std::min(next_timeout, ttl_timeout);
            }
    
            return duration_cast<milliseconds>(next_timeout - now);
        }
    
        // Processes active client connections
        void process_active_connections(const std::vector<pollfd>& poll_args) {
            for (size_t i = 1; i < poll_args.size(); ++i) { // Skip the listening socket
                if (poll_args[i].revents == 0) {
                    continue; // No event on this socket
                }
    
                std::unique_lock lock(connections_mutex_);
                auto it = connections_.find(poll_args[i].fd);
                if (it == connections_.end()) {
                    continue;
                }
    
                auto& conn = it->second;
                try {
                    auto result = conn->process_io(); // Handle input/output for this connection
                    if (!result) {
                        remove_connection(conn->fd()); // Remove connection on failure
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
            if (!(listen_poll.revents & POLLIN)) { // If there's no incoming connection, return
                return;
            }
    
            while (true) { // Keep accepting connections until none are left
                sockaddr_in client_addr{}; // Struct to store client address info
                socklen_t addr_len = sizeof(client_addr);
                
                int client_fd = accept(listen_socket_.get(),
                                     reinterpret_cast<sockaddr*>(&client_addr),
                                     &addr_len); // Accept a new client connection
    
                if (client_fd < 0) { // If accept() fails
                    if (errno == EAGAIN || errno == EWOULDBLOCK) { // No more connections to accept
                        break;
                    }
                    throw std::system_error(
                        std::make_error_code(std::errc::io_error),
                        "Accept failed"
                    );
                }
    
                try {
                    Socket client_socket(client_fd); // wrap the file descriptor in a `Socket` object
                    auto result = client_socket.set_nonblocking(); // set the client socket to non-blocking mode
                    if (!result) {
                        continue; // skip if non-blocking mode fails
                    }
    
                    auto conn = std::make_unique<Connection>(std::move(client_socket)); // create a new connection object
                    add_connection(std::move(conn)); // add it to the connection list
                    
                } catch (const std::exception& e) {
                    log_message("Failed to create connection: {}", e.what());
                    close(client_fd); // close the client socket if an exception occurs
                }
            }
        }
    
        // adds a new client connection to the connection list
        void add_connection(std::unique_ptr<Connection> conn) {
            std::unique_lock lock(connections_mutex_); // lock exists until connections_mutex_ goes out of scope
            int fd = conn->fd(); // get the file descriptor (locked to prevent race conditions or undefined behaviors)
            connections_.emplace(fd, std::move(conn)); // Store connection in the map
            dlist_insert_before(&idle_list_, &connections_[fd]->idle_list); // Track idle connections
        }
    
        // Removes a client connection from the connection list
        void remove_connection(int fd) {
            std::unique_lock lock(connections_mutex_);
            auto it = connections_.find(fd);
            if (it != connections_.end()) {
                dlist_detach(&it->second->idle_list); // remove from idle list
                connections_.erase(it); // erase from active connections map
            }
        }

    private:
        uint16_t port_; // the port number the server listens on
        Socket listen_socket_{-1}; // the main listening socket
        ThreadPool thread_pool_; // thread pool for handling client connections
        std::atomic<bool> should_stop_{false}; // flag to control the server loop
    
        mutable std::shared_mutex connections_mutex_; // synchronization for managing connections
        std::unordered_map<int, std::unique_ptr<Connection>> connections_; // active connections
    
        DoublyLinkedList idle_list_; // list of idle connections (for timeouts)
        HMap db_; // database (hash map) for managing server state
        BinaryHeap heap_; // heap for tracking time-based events
    };