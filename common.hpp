#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace ds {

template<typename Derived, typename Base, typename Member>
constexpr Derived* container_of(Base* ptr, Member Base::*member) noexcept {
    return static_cast<Derived*>(
        reinterpret_cast<char*>(ptr) - 
        offsetof(Derived, member)
    );
}

[[nodiscard]] constexpr std::uint64_t hash_string(std::string_view str) noexcept {
    constexpr std::uint32_t INITIAL = 0x811C9DC5;
    constexpr std::uint32_t MULTIPLIER = 0x01000193;
    
    std::uint32_t hash = INITIAL;
    for (unsigned char c : str) {
        hash = (hash + c) * MULTIPLIER;
    }
    return hash;
}

enum class SerializationType : std::uint8_t {
    Nil = 0,
    Error = 1,
    String = 2,
    Integer = 3,
    Double = 4,
    Array = 5
};

template<typename T>
struct SerializationTraits {
    static constexpr SerializationType type = SerializationType::Nil;
};

template<>
struct SerializationTraits<std::string> {
    static constexpr SerializationType type = SerializationType::String;
};

template<typename T>
struct SerializationTraits<T, std::enable_if_t<std::is_integral_v<T>>> {
    static constexpr SerializationType type = SerializationType::Integer;
};

template<typename T>
struct SerializationTraits<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    static constexpr SerializationType type = SerializationType::Double;
};

} // namespace ds

#endif // COMMON_HPP