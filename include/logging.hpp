#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <iostream>
#include <source_location>
#include <format>
#include <string_view>

inline void log_message(std::string_view format,
                        const std::source_location& location = std::source_location::current()) {
    std::cerr << std::format("[{}:{}] - {}\n", location.file_name(), location.line(), format);
}

#endif
