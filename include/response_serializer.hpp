#ifndef RESPONSE_SERIALIZER_HPP
#define RESPONSE_SERIALIZER_HPP

#include <vector>
#include <string_view>
#include <cstdint>

class ResponseSerializer {
public:
    template<typename T>
    static void serialize(std::vector<uint8_t>& buffer, const T& data) {
        buffer.push_back(static_cast<uint8_t>(SerializationType::Integer));
        append_data(buffer, static_cast<int64_t>(data));
    }

    static void serialize_string(std::vector<uint8_t>& buffer, std::string_view str) {
        buffer.push_back(static_cast<uint8_t>(SerializationType::String));
        append_data(buffer, static_cast<uint32_t>(str.size()));
        buffer.insert(buffer.end(), str.begin(), str.end());
    }

private:
    enum class SerializationType : uint8_t { Nil, Integer, String };

    template<typename T>
    static void append_data(std::vector<uint8_t>& buffer, const T& data) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&data);
        buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
    }
};

#endif 
