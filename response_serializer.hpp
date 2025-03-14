#ifndef RESPONSE_SERIALIZER_HPP
#define RESPONSE_SERIALIZER_HPP

#include <vector>        
#include <cstdint>       
#include <string_view>  
#include <cstring>      
#include <string>      

#include "common.hpp"

class ResponseSerializer {
public:
    template<typename T>
    static void serialize(std::vector<uint8_t>& buffer, const T& data) {
        constexpr SerializationType type = get_serialization_type<T>();
        
        if constexpr (type == SerializationType::Nil) {
            serialize_nil(buffer);
        } else if constexpr (type == SerializationType::Integer) {
            serialize_integer(buffer, static_cast<int64_t>(data));
        } else if constexpr (type == SerializationType::Double) {
            serialize_double(buffer, static_cast<double>(data));
        } else if constexpr (type == SerializationType::String) {
            serialize_string(buffer, data);
        } else {
            static_assert("Unsupported type for serialization");
        }
    }

    static void serialize_nil(std::vector<uint8_t>& buffer) {
        buffer.push_back(static_cast<uint8_t>(SerializationType::Nil));
    }

    static void serialize_error(std::vector<uint8_t>& buffer, int32_t code, std::string_view msg) {
        buffer.push_back(static_cast<uint8_t>(SerializationType::Error));
        append_data(buffer, code);
        append_data(buffer, static_cast<uint32_t>(msg.size()));
        buffer.insert(buffer.end(), msg.begin(), msg.end());
    }

    static void serialize_string(std::vector<uint8_t>& buffer, std::string_view str) {
        buffer.push_back(static_cast<uint8_t>(SerializationType::String));
        append_data(buffer, static_cast<uint32_t>(str.size()));
        buffer.insert(buffer.end(), str.begin(), str.end());
    }

    static void serialize_integer(std::vector<uint8_t>& buffer, int64_t value) {
        buffer.push_back(static_cast<uint8_t>(SerializationType::Integer));
        std::string str_value = std::to_string(value) + "\r\n";  // Convert integer to string format
        buffer.insert(buffer.end(), str_value.begin(), str_value.end());
    }

    static void serialize_double(std::vector<uint8_t>& buffer, double value) {
        buffer.push_back(static_cast<uint8_t>(SerializationType::Double));
        append_data(buffer, value);
    }

    static std::string deserialize_string(const std::vector<uint8_t>& buffer) {
        if (buffer.empty() || buffer[0] != static_cast<uint8_t>(SerializationType::String)) {
            return "";
        }
        uint32_t size;
        std::memcpy(&size, buffer.data() + 1, sizeof(uint32_t));
        return std::string(buffer.begin() + 5, buffer.begin() + 5 + size);
    }

    static bool deserialize_nil(const std::vector<uint8_t>& buffer) {
        return !buffer.empty() && buffer[0] == static_cast<uint8_t>(SerializationType::Nil);
    }

    static int64_t deserialize_integer(const std::vector<uint8_t>& buffer) {
        if (buffer.empty() || buffer[0] != static_cast<uint8_t>(SerializationType::Integer)) {
            return -1; // Default error value
        }
    
        int64_t value;
        std::memcpy(&value, buffer.data() + 1, sizeof(int64_t));
    
        // Debugging Output
        std::cout << "🔍 Debug: Deserialized integer " << value << " from buffer [";
        for (auto byte : buffer) {
            std::cout << static_cast<int>(byte) << " ";
        }
        std::cout << "]" << std::endl;
    
        return value;
    }
    

    static std::string deserialize_error(const std::vector<uint8_t>& buffer) {
        if (buffer.empty() || buffer[0] != static_cast<uint8_t>(SerializationType::Error)) {
            return "No error";
        }
        uint32_t size;
        std::memcpy(&size, buffer.data() + 5, sizeof(uint32_t));
        return std::string(buffer.begin() + 9, buffer.begin() + 9 + size);
    }
    template<typename T>
    static void append_data(std::vector<uint8_t>& buffer, const T& data) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&data);
        buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
    }
};

#endif // RESPONSE_SERIALIZER_HPP
