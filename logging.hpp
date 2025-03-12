#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <iostream>
#include <string_view>
#include <chrono>
#include <ctime>
#include <source_location>
#include <format>

template<typename... Args>
void log_message_impl(std::string_view format, const std::source_location& location, const Args&... args) {
    auto time_t_val = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char time_str[20];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t_val));

    std::cerr << std::format("[{}] {}:{} - ", time_str, location.file_name(), location.line());
    std::cerr << std::vformat(format, std::make_format_args(args...)) << '\n';
}

template<typename... Args>
void log_message(std::string_view format, const Args&... args) {
    log_message_impl(format, std::source_location::current(), args...);
}

#endif // LOGGING_HPP




/*

Example Usage:
log_message("User {} has logged in from IP: {}", "Zelda", "192.168.1.100");
log_message("File {} could not be found. Error code: {}", "config.yaml", 404);

Output:
[2025-03-06 18:05:12] main.cpp:5 - User Zelda has logged in from IP: 192.168.1.100
[2025-03-06 18:05:12] main.cpp:6 - File config.yaml could not be found. Error code: 404

*/

