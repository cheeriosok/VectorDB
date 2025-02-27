#ifndef SERVER_STATE_HPP
#define SERVER_STATE_HPP

#include "socket.hpp"
#include "connection.hpp"
#include <unordered_map>
#include <shared_mutex>

class ServerState {
public:
    void add_connection(std::unique_ptr<Connection> conn) {
        std::unique_lock lock(mutex_);
        connections_[conn->fd()] = std::move(conn);
    }

    std::unique_ptr<Connection> remove_connection(int fd) {
        std::unique_lock lock(mutex_);
        auto it = connections_.find(fd);
        if (it == connections_.end()) return nullptr;
        auto conn = std::move(it->second);
        connections_.erase(it);
        return conn;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<int, std::unique_ptr<Connection>> connections_;
};

#endif 
