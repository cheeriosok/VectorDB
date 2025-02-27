#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdint>
#include <chrono>

constexpr size_t MAX_MSG_SIZE = 4096;
constexpr auto IDLE_TIMEOUT = std::chrono::milliseconds(5000);
constexpr uint16_t SERVER_PORT = 1234;

#endif 
