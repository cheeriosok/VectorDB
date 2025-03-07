#ifndef SERVER_STATE_HPP
#define SERVER_STATE_HPP

#include <thread>
#include "socket.hpp"
#include "thread_pool.hpp"
#include "heap.hpp"
#include "connection.hpp"
#include "socket.hpp"
#include "server.hpp"
#include "hashtable.hpp"
#include "heap.hpp"
#include "thread_pool.hpp"
#include <vector.hpp>
// server state manages all active connectons, db and background tasks.
class ServerState {
    public:
        // default constructor.
        ServerState() = default;
        
        // add a  new connection of type Connection to server's active connections.
        void add_connection(std::unique_ptr<Connection> conn) {
            std::unique_lock lock(mutex_); // lock to prevent race conditions
            connections_[conn->fd()] = std::move(conn); // store connection using its file descriptor as the key
        }
        
        std::unique_ptr<Connection> remove_connection(int fd) { // remove the connection by key
            std::unique_lock lock(mutex_); // lock to prevent race conditions
            auto it = connections_.find(fd); // ket's find the fd 
            if (it == connections_.end()) { // if it does not exsit, return nothing - our conn doesn't exsit
                return nullptr;
            }
            auto conn = std::move(it->second); // if it does exist, then lets move the fd to conn
            connections_.erase(it); // then erase the iterator
            return conn; // and return conn (unique_ptr, which will automatically be destroyed after return)
        }
        
        // retureves all file descriptors for polling (used in poll/select for event monitoring)
        [[nodiscard]] std::vector<pollfd> get_poll_fds() const { 
            std::shared_lock lock(mutex_); // lets prevent race conditions by locking - sicne we will have multiple owners, we'll used a shared_lock
            std::vector<pollfd> poll_fds; // initialize a vecotr of poll file descriptors 
            poll_fds.reserve(connections_.size() + 1); // preallcoate memory
            
            // add listening socket (used to accept new connections)
            poll_fds.push_back({
                .fd = listen_socket_.get(), //fd of listening socket
                .events = POLLIN, // wait for incoming conn. requests
                .revents = 0 // reset event flag
            });
            
            // add active connection sockets
            for (const auto& [fd, conn] : connections_) {
                poll_fds.push_back({
                    .fd = fd, // file descriptor for client socket
                    .events = conn->state() == ConnectionState::Request ? POLLIN : POLLOUT, // poll for r/w
                    .revents = 0 // reset event flag
                });
            }
            
            return poll_fds; // return the list of pollable file descriptors
        }
        
        Result<void> initialize(uint16_t port);
        
    private:
        mutable std::shared_mutex mutex_; // mutex to allow concurrent reads and exclusive writes
        Socket listen_socket_{-1}; // main listening socket for new connections, default init with arg -1
        std::unordered_map<int, std::unique_ptr<Connection>> connections_; // active client connections
        HMap db_; // map for database storage (k/v store)
        std::vector<HeapItem> heap_; // heap for managing scheduled tasks/timeouts
        ThreadPool thread_pool_{4}; // defaul init thread_pool to 4 worker threads.
    };
    
    // Global state instance
    ServerState g_server; // global instance of serverstate

#endif 
