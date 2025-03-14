#include <iostream>      // std::cerr, std::cout
#include <cstdint>       // uint16_t
#include <cstdlib>       // std::size_t, std::stoi
#include <csignal>       // std::signal, SIGINT
#include <exception>     // std::exception
#include "server.hpp"    // Server class

Server* global_server = nullptr;

void handle_signal(int) {
    if (global_server) {
        std::cout << "\nShutting down server gracefully...\n";
        global_server->stop();
    }
    std::exit(0);
}


int main(int argc, char* argv[]) {
    try {
        uint16_t port = 1234; 
        size_t thread_pool_size = 4; 

        if (argc > 1) {
            port = static_cast<uint16_t>(std::stoi(argv[1]));
            if (port < 1024 || port > 65535) {
                std::cerr << "Invalid port number. Use a port between 1024 and 65535.\n";
                return 1;
            }
        }
        if (argc > 2) {
            thread_pool_size = static_cast<size_t>(std::stoi(argv[2]));
            if (thread_pool_size == 0) {
                std::cerr << "Thread pool size must be greater than 0.\n";
                return 1;
            }
        }

        Server server(port, thread_pool_size);
        global_server = &server; 

        auto result = server.initialize();
        if (!result) {
            std::cerr << "Failed to initialize server: " << result.error().message() << "\n";
            return 1;
        }

        std::signal(SIGINT, handle_signal);

        std::cout << "Server running on port " << port << " with " << thread_pool_size << " threads.\n";
        server.run();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
