#ifndef COMMON_HPP
#define COMMON_HPP

#include <cstddef>       
#include <cstdint>      
#include <type_traits>   
#include <string>        
#include <string_view>   

/* fairly convoluted but container_of can help you move "backwards" in inheritance hierarchies or class containment to find the parent class or container.

 example use cases:
 - inheritance: recover the derived class object when you only have a pointer to a base class member.
 - containment: recover the container object when you only have a pointer to one of its member

*/

// GNU C Macro - derived from Linux's Macro and now using reinterpret_cast to safely cast back to the parent structure

template<typename Parent, typename Member>
Parent* container_of(Member* ptr, std::size_t offset) {
    return reinterpret_cast<Parent*>(reinterpret_cast<char*>(ptr) - offset);
}


enum class SerializationType : uint8_t {
    Nil     = 0,
    Error   = 1,
    String  = 2,
    Integer = 3,
    Double  = 4
};

template<typename T>
constexpr SerializationType get_serialization_type() {
    if constexpr (std::is_integral_v<T> && !std::is_same_v<T, char> && !std::is_same_v<T, signed char> && !std::is_same_v<T, unsigned char>) {
        return SerializationType::Integer;
    } else if constexpr (std::is_floating_point_v<T>) {
        return SerializationType::Double;
    } else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>) {
        return SerializationType::String;
    } else {
        return SerializationType::Nil;
    }
}


#endif 
