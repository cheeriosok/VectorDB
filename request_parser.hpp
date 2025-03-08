#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

#include <vector>       // For std::vector
#include <string>       // For std::string
#include <cstdint>      // For uint8_t, uint32_t
#include <span>         // For std::span
#include <expected>     // For std::expected (C++23)
#include <system_error> // For std::error_code, std::errc, std::make_error_code
#include <cstring>      // For std::memcpy
#include "connection.hpp"

template<typename T>
using Result = std::expected<T, std::error_code>;

// request parser class responsible for parsing incoming data into command components
class RequestParser {
    public:
        /**
         *  Parses incoming binary data into a vector of command strings.
         * 
         *  A span of uint8_t containing the request data.
         *  Result<std::vector<std::string>> A vector of parsed command strings or an error code.
         *
         * This function extracts command components from a binary message.
         * The expected format of the data is:
         * - First 4 bytes (uint32_t): Total length of the command payload.
         * - Multiple entries of:
         *   - 4 bytes (uint32_t): Length of the next string.
         *   - Variable bytes: The actual string.
         */
        static Result<std::vector<std::string>> parse(std::span<const uint8_t> data) {
            // ensure message contains at least 4 bytes for the length field
            if (data.size() < sizeof(uint32_t)) {
                return std::unexpected(std::make_error_code(std::errc::message_size));
            }
    
            // extract total length of the command payload
            uint32_t len;
            std::memcpy(&len, data.data(), sizeof(uint32_t));
            
            // validate length constraints
            if (len > MAX_MSG_SIZE) {
                return std::unexpected(std::make_error_code(std::errc::message_size));
            }
            
            // ensure the total length specified does not exceed actual data size
            if (sizeof(uint32_t) + len > data.size()) {
                return std::unexpected(std::make_error_code(std::errc::message_size));
            }
    
            std::vector<std::string> cmd;
            const uint8_t* pos = data.data() + sizeof(uint32_t); // start of actual command data
            const uint8_t* end = pos + len;
            
            // iterate through the command data and extract strings
            while (pos < end) {
                // check if at least 4 bytes remain for the next string length field
                if (end - pos < sizeof(uint32_t)) {
                    return std::unexpected(std::make_error_code(std::errc::bad_message));
                }
                
                uint32_t str_len;
                std::memcpy(&str_len, pos, sizeof(uint32_t)); // extract length of next string
                pos += sizeof(uint32_t);
                
                // validate extracted string length
                if (str_len > static_cast<uint32_t>(end - pos)) {
                    return std::unexpected(std::make_error_code(std::errc::bad_message));
                }
                
                // extract and store the string
                cmd.emplace_back(reinterpret_cast<const char*>(pos), str_len);
                pos += str_len;
            }
            
            return cmd; // return parsed command components
        }
};

#endif