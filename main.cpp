

// Modern request parser using std::span
#include <iostream>      // std::cerr, std::cout
#include <cstdint>       // uint16_t
#include <cstdlib>       // std::size_t
#include <csignal>       // std::signal, SIGINT
#include <exception>     // std::exception


int main(int argc, char* argv[]) {
    try {
        uint16_t port = 1234;
        size_t thread_pool_size = 4;

        // Parse command line arguments if needed
        // ...

        Server server(port, thread_pool_size);
        
        auto result = server.initialize();
        if (!result) {
            std::cerr << "Failed to initialize server: " 
                     << result.error().message() << "\n";
            return 1;
        }

        // Set up signal handling for graceful shutdown
        std::signal(SIGINT, [](int) {
            // Signal handler code
        });

        server.run();
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}