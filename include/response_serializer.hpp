#ifndef RESPONSE_SERIALIZER_HPP
#define RESPONSE_SERIALIZER_HPP

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
            append_data(buffer, value);
        }
    
        static void serialize_double(std::vector<uint8_t>& buffer, double value) {
            buffer.push_back(static_cast<uint8_t>(SerializationType::Double));
            append_data(buffer, value);
        }
    
    private:
        template<typename T>
        static void append_data(std::vector<uint8_t>& buffer, const T& data) {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&data);
            buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
        }
    };

#endif 
