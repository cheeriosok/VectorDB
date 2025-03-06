#ifndef REQUEST_PARSER_HPP
#define REQUEST_PARSER_HPP

class RequestParser {
    public:
        static Result<std::vector<std::string>> parse(std::span<const uint8_t> data) {
            if (data.size() < sizeof(uint32_t)) {
                return std::unexpected(std::make_error_code(std::errc::message_size));
            }
    
            uint32_t len;
            std::memcpy(&len, data.data(), sizeof(uint32_t));
            
            if (len > MAX_MSG_SIZE) {
                return std::unexpected(std::make_error_code(std::errc::message_size));
            }
            
            if (sizeof(uint32_t) + len > data.size()) {
                return std::unexpected(std::make_error_code(std::errc::message_size));
            }
    
            std::vector<std::string> cmd;
            const uint8_t* pos = data.data() + sizeof(uint32_t);
            const uint8_t* end = pos + len;
            
            while (pos < end) {
                if (end - pos < sizeof(uint32_t)) {
                    return std::unexpected(std::make_error_code(std::errc::bad_message));
                }
                
                uint32_t str_len;
                std::memcpy(&str_len, pos, sizeof(uint32_t));
                pos += sizeof(uint32_t);
                
                if (str_len > static_cast<uint32_t>(end - pos)) {
                    return std::unexpected(std::make_error_code(std::errc::bad_message));
                }
                
                cmd.emplace_back(reinterpret_cast<const char*>(pos), str_len);
                pos += str_len;
            }
            
            return cmd;
        }
    };
    

#endif 
