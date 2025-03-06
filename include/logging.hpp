#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <iostream>
#include <source_location>
#include <format>
#include <string_view>

// Modern logging with source_location
template<typename... Args>
void log_message(std::string_view format, 
                const Args&... args,
                const std::source_location& location = std::source_location::current()) {
    auto timestamp = std::chrono::system_clock::now();
    std::cerr << std::format("[{}] {}:{} - ", 
                            timestamp, 
                            location.file_name(), 
                            location.line());
    std::cerr << std::vformat(format, std::make_format_args(args...)) << '\n';
}

// Modern error handling with std::expected
template<typename T>
using Result = std::expected<T, std::error_code>;

#endif
