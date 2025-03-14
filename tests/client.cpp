#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int connect_to_server(const std::string& host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "failed to create socket.\n";
        return -1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "connection to " << host << ":" << port << " failed.\n";
        close(sock);
        return -1;
    }

    std::cout << "cnnected to " << host << ":" << port << "\n";
    return sock;
}
std::string send_command(int sock, const std::string& command) {//fuck
    std::string full_command = command + "\r\n";  
    std::cout << "sending command: [" << command << "]\n";

    if (send(sock, full_command.c_str(), full_command.size(), 0) < 0) {
        std::cerr << "failed to send command: " << command << " | Error: " << strerror(errno) << "\n";
        return "";
    }

    std::string response;
    char buffer[256];

    std::cout << "aiting for response...\n";

    int max_attempts = 5;  
    int attempts = 0;

    while (attempts < max_attempts) {
        ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            std::cerr << "recv() returned 0 or an error: " << strerror(errno) << "\n";
            break;
        }

        buffer[bytes_received] = '\0';  // Null-terminate buffer
        std::cout << "received chunk (raw): [" << buffer << "] | Bytes: " << bytes_received << "\n";

        response.append(buffer, bytes_received);

        if (!response.empty()) {
            break;
        }

        attempts++;
    }

    while (!response.empty() && (response.back() == '\r' || response.back() == '\n')) {
        response.pop_back();
    }

    std::cout << "🔹 Final Response: [" << response << "]\n";
    return response;
}



void run_tests(int sock) {
    struct {
        std::string command;
        std::string expected_response;
    } tests[] = {
        {"SET key1 hello", "OK"},
        {"GET key1", "hello"},
        {"EXISTS key1", "1"},
        {"DEL key1", "1"},
        {"EXISTS key1", "0"},
        {"SET key2 world", "OK"},
        {"GET key2", "world"},
        {"ZADD leaderboard 100 player1", "1"},
        {"ZREM leaderboard player1", "1"},
        {"PEXPIRE key2 1000", "1"},
        {"PTTL key2", ""},  
        {"FLUSHALL", "1"}
    };

    for (const auto& test : tests) {
        std::cout << "sending: [" << test.command << "]\n";
        std::string response = send_command(sock, test.command);
        std::cout << "response: [" << response << "]\n";

        if (!test.expected_response.empty() && response.find(test.expected_response) == std::string::npos) {
            std::cerr << "test failed for command: " << test.command << "\n";
        } else {
            std::cout << "test passed!\n";
        }
    }
}

void benchmark_response_time(int sock, const std::vector<std::string>& commands) {
    using namespace std::chrono;

    std::cout << "tunning response time benchmark with " << commands.size() << " commands...\n";

    std::vector<double> response_times;
    
    for (const auto& command : commands) {
        auto start_time = high_resolution_clock::now();
        std::string response = send_command(sock, command);
        auto end_time = high_resolution_clock::now();

        if (response.empty()) {
            std::cerr << 
            "No response for command: " << command << "\n";
            continue;
        }

        double elapsed_us = duration_cast<microseconds>(end_time - start_time).count();
        response_times.push_back(elapsed_us);

        std::cout << "🔹 [" << command << "] Response Time: " << elapsed_us << " µs\n";
    }

    double total_time = 0;
    for (double time : response_times) {
        total_time += time;
    }
    double avg_time = response_times.empty() ? 0 : total_time / response_times.size();

    std::cout << "avg response time: " << avg_time << " µs (" << avg_time / 1000.0 << " ms)\n";
}


int main() {
    int sock = connect_to_server("127.0.0.1", 1234);
    if (sock < 0) return 1;

    run_tests(sock);

    std::vector<std::string> commands = {
        "SET key1 hello",
        "GET key1",
        "EXISTS key1",
        "DEL key1",
        "SET key2 world",
        "GET key2",
        "ZADD leaderboard 100 player1",
        "ZREM leaderboard player1",
        "PEXPIRE key2 1000",
        "PTTL key2",
        "FLUSHALL"
    };

    benchmark_response_time(sock, commands);

    close(sock);
    return 0;
}
