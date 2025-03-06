

static constexpr size_t MAX_MSG_SIZE = 4096;
static constexpr auto IDLE_TIMEOUT = std::chrono::milliseconds(5000);
static constexpr uint16_t SERVER_PORT = 1234;


class Server {
    public:
        Server(uint16_t port, size_t thread_pool_size) 
            : port_(port)
            , thread_pool_(thread_pool_size) {
        }
    
        Result<void> initialize() {
            // Create listening socket
            auto socket_result = create_listen_socket();
            if (!socket_result) {
                return std::unexpected(socket_result.error());
            }
            listen_socket_ = std::move(*socket_result);
    
            dlist_init(&idle_list_);
            return {};
        }
    
        void run() {
            std::vector<pollfd> poll_args;
            
            while (!should_stop_) {
                prepare_poll_args(poll_args);
                
                int timeout_ms = static_cast<int>(calculate_next_timeout());
                int rv = poll(poll_args.data(), poll_args.size(), timeout_ms);
                
                if (rv < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    throw std::system_error(
                        std::make_error_code(std::errc::io_error),
                        "Poll failed"
                    );
                }
    
                process_active_connections(poll_args);
                process_timers();
                accept_new_connections(poll_args[0]);
            }
        }
    
        void stop() {
            should_stop_ = true;
        }
    
    private:
        Result<Socket> create_listen_socket() {
            Socket sock(socket(AF_INET, SOCK_STREAM, 0));
            if (sock.get() < 0) {
                return std::unexpected(
                    std::make_error_code(std::errc::bad_file_descriptor)
                );
            }
    
            // Enable address reuse
            int val = 1;
            if (setsockopt(sock.get(), SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
                return std::unexpected(
                    std::make_error_code(std::errc::invalid_argument)
                );
            }
    
            // Bind socket
            sockaddr_in addr{
                .sin_family = AF_INET,
                .sin_port = htons(port_),
                .sin_addr = {.s_addr = htonl(INADDR_ANY)}
            };
    
            if (bind(sock.get(), 
                    reinterpret_cast<sockaddr*>(&addr), 
                    sizeof(addr)) < 0) {
                return std::unexpected(
                    std::make_error_code(std::errc::address_in_use)
                );
            }
    
            // Listen
            if (listen(sock.get(), SOMAXCONN) < 0) {
                return std::unexpected(
                    std::make_error_code(std::errc::connection_refused)
                );
            }
    
            // Set non-blocking mode
            auto result = sock.set_nonblocking();
            if (!result) {
                return std::unexpected(result.error());
            }
    
            return std::move(sock);
        }
    
        void prepare_poll_args(std::vector<pollfd>& poll_args) {
            poll_args.clear();
            
            // Add listening socket
            poll_args.push_back({
                .fd = listen_socket_.get(),
                .events = POLLIN,
                .revents = 0
            });
    
            // Add connection sockets
            std::shared_lock lock(connections_mutex_);
            for (const auto& [fd, conn] : connections_) {
                poll_args.push_back({
                    .fd = fd,
                    .events = conn->state() == ConnectionState::Request 
                        ? POLLIN : POLLOUT,
                    .events = POLLERR,
                    .revents = 0
                });
            }
        }
    
        std::chrono::milliseconds calculate_next_timeout() {
            using namespace std::chrono;
            auto now = steady_clock::now();
            auto next_timeout = now + hours(24); // Default timeout
    
            // Check idle connections
            if (!dlist_empty(&idle_list_)) {
                auto* next = container_of(idle_list_.next, Connection, idle_list);
                auto idle_timeout = next->idle_start() + IDLE_TIMEOUT;
                next_timeout = std::min(next_timeout, idle_timeout);
            }
    
            // Check TTL heap
            if (!heap_.empty()) {
                auto ttl_timeout = time_point<steady_clock>(
                    microseconds(heap_[0].val)
                );
                next_timeout = std::min(next_timeout, ttl_timeout);
            }
    
            return duration_cast<milliseconds>(next_timeout - now);
        }
    
        void process_active_connections(const std::vector<pollfd>& poll_args) {
            for (size_t i = 1; i < poll_args.size(); ++i) {
                if (poll_args[i].revents == 0) {
                    continue;
                }
    
                std::unique_lock lock(connections_mutex_);
                auto it = connections_.find(poll_args[i].fd);
                if (it == connections_.end()) {
                    continue;
                }
    
                auto& conn = it->second;
                try {
                    auto result = conn->process_io();
                    if (!result) {
                        remove_connection(conn->fd());
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
            if (!(listen_poll.revents & POLLIN)) {
                return;
            }
    
            while (true) {
                sockaddr_in client_addr{};
                socklen_t addr_len = sizeof(client_addr);
                
                int client_fd = accept(listen_socket_.get(),
                                     reinterpret_cast<sockaddr*>(&client_addr),
                                     &addr_len);
    
                if (client_fd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }
                    throw std::system_error(
                        std::make_error_code(std::errc::io_error),
                        "Accept failed"
                    );
                }
    
                try {
                    Socket client_socket(client_fd);
                    auto result = client_socket.set_nonblocking();
                    if (!result) {
                        continue;
                    }
    
                    auto conn = std::make_unique<Connection>(std::move(client_socket));
                    add_connection(std::move(conn));
                    
                } catch (const std::exception& e) {
                    log_message("Failed to create connection: {}", e.what());
                    close(client_fd);
                }
            }
        }
    
        void add_connection(std::unique_ptr<Connection> conn) {
            std::unique_lock lock(connections_mutex_);
            int fd = conn->fd();
            connections_.emplace(fd, std::move(conn));
            dlist_insert_before(&idle_list_, &connections_[fd]->idle_list);
        }
    
        void remove_connection(int fd) {
            std::unique_lock lock(connections_mutex_);
            auto it = connections_.find(fd);
            if (it != connections_.end()) {
                dlist_detach(&it->second->idle_list);
                connections_.erase(it);
            }
        }
    
        uint16_t port_;
        Socket listen_socket_{-1};
        ThreadPool thread_pool_;
        std::atomic<bool> should_stop_{false};
        
        mutable std::shared_mutex connections_mutex_;
        std::unordered_map<int, std::unique_ptr<Connection>> connections_;
        
        DList idle_list_;
        HMap db_;
        std::vector<HeapItem> heap_;
    };