#ifndef SERVER_STATE_HPP
#define SERVER_STATE_HPP

class ServerState {
    public:
        ServerState() = default;
        
        void add_connection(std::unique_ptr<Connection> conn) {
            std::unique_lock lock(mutex_);
            connections_[conn->fd()] = std::move(conn);
        }
        
        std::unique_ptr<Connection> remove_connection(int fd) {
            std::unique_lock lock(mutex_);
            auto it = connections_.find(fd);
            if (it == connections_.end()) {
                return nullptr;
            }
            auto conn = std::move(it->second);
            connections_.erase(it);
            return conn;
        }
        
        [[nodiscard]] std::vector<pollfd> get_poll_fds() const {
            std::shared_lock lock(mutex_);
            std::vector<pollfd> poll_fds;
            poll_fds.reserve(connections_.size() + 1);
            
            // Add listening socket
            poll_fds.push_back({
                .fd = listen_socket_.get(),
                .events = POLLIN,
                .revents = 0
            });
            
            // Add connection sockets
            for (const auto& [fd, conn] : connections_) {
                poll_fds.push_back({
                    .fd = fd,
                    .events = conn->state() == ConnectionState::Request ? POLLIN : POLLOUT,
                    .revents = 0
                });
            }
            
            return poll_fds;
        }
        
        Result<void> initialize(uint16_t port);
        
    private:
        mutable std::shared_mutex mutex_;
        Socket listen_socket_{-1};
        std::unordered_map<int, std::unique_ptr<Connection>> connections_;
        HMap db_;
        std::vector<HeapItem> heap_;
        ThreadPool thread_pool_{4};
    };
    
    // Global state instance
    ServerState g_server;

#endif 
