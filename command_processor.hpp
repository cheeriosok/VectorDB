#ifndef COMMAND_PROCESSOR_HPP
#define COMMAND_PROCESSOR_HPP

#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <chrono>        // std::chrono::steady_clock
#include <cstdint>       // std::uint64_t
#include <algorithm>
#include <utility>
#include <mutex>        
#include "src/hashtable.hpp"
#include "src/heap.hpp"
#include "src/zset.hpp"
#include "entry_manager.hpp"
#include "response_serializer.hpp"

constexpr int ERR_ARG = -1;
constexpr int ERR_UNKNOWN = -2;
constexpr int ERR_TYPE = -3;

// modern command processor with type-safe command handling

class CommandProcessor {
public:
    struct CommandContext {
        const std::vector<std::string>& args;
        std::vector<uint8_t>& response;
        EntryManager& entry_manager;
    };

    static const std::unordered_map<std::string, std::function<void(CommandContext)>> command_handlers;

    static void process_command(CommandContext ctx) {
        if (ctx.args.empty()) { 
            return ResponseSerializer::serialize_error(ctx.response, ERR_ARG, "empty command\n");
        }
        
        std::string command_key = ctx.args[0]; 
        std::transform(command_key.begin(), command_key.end(), command_key.begin(), ::tolower);

        auto it = command_handlers.find(command_key);
        if (it == command_handlers.end()) { 
            return ResponseSerializer::serialize_error(ctx.response, ERR_UNKNOWN, "unknown command\n");
        }

        it->second(ctx);
    }

private:
    static void handle_get(CommandContext ctx) {
        if (ctx.args.size() != 2) { 
            return ResponseSerializer::serialize_error(ctx.response, ERR_ARG, "GET requires one key\n");
        }

        auto entry = ctx.entry_manager.find_entry(ctx.args[1]);
        if (!entry) { 
            return ResponseSerializer::serialize_nil(ctx.response);
        }

        if (auto str_entry = std::dynamic_pointer_cast<Entry<std::string>>(entry)) {
            ResponseSerializer::serialize_string(ctx.response, str_entry->value);
        } else {
            ResponseSerializer::serialize_error(ctx.response, ERR_TYPE, "Key holds wrong type\n");
        }
    }

    static void handle_set(CommandContext ctx) {
        if (ctx.args.size() != 3) { 
            return ResponseSerializer::serialize_error(ctx.response, ERR_ARG, "SET requires key and value\n");
        }
    
        ctx.entry_manager.create_entry(ctx.args[1], ctx.args[2]);
        ResponseSerializer::serialize_string(ctx.response, "OK");
    }    

    static void handle_del(CommandContext ctx) {
        if (ctx.args.size() != 2) { 
            return ResponseSerializer::serialize_error(ctx.response, ERR_ARG, "DEL requires key\n");
        }
    
        bool deleted = ctx.entry_manager.delete_entry(ctx.args[1]);
        ResponseSerializer::serialize_integer(ctx.response, deleted ? 1 : 0);
    }
    
    
    
    static void handle_exists(CommandContext ctx) {
        if (ctx.args.size() != 2) { 
            return ResponseSerializer::serialize_error(ctx.response, ERR_ARG, "EXISTS requires key\n");
        }
    
        auto entry = ctx.entry_manager.find_entry(ctx.args[1]);
        ResponseSerializer::serialize_integer(ctx.response, entry ? 1 : 0);
    }
    
    static void handle_flushall(CommandContext ctx) {
        bool cleared = ctx.entry_manager.clear_all();
        ResponseSerializer::serialize_integer(ctx.response, cleared ? 1 : 0);
    }      
    

    static void handle_zadd(CommandContext ctx) {
        if (ctx.args.size() != 4) { 
            return ResponseSerializer::serialize_error(ctx.response, ERR_ARG, "ZADD requires key, score, and member\n");
        }
    
        double score;
        if (!parse_double(ctx.args[2], score)) {
            return ResponseSerializer::serialize_error(ctx.response, ERR_ARG, "Invalid score value\n");
        }
    
        auto entry = ctx.entry_manager.find_entry(ctx.args[1]);
        std::shared_ptr<Entry<std::unique_ptr<ZSet>>> zset_entry;
    
        if (entry) {
            zset_entry = std::dynamic_pointer_cast<Entry<std::unique_ptr<ZSet>>>(entry);
            if (!zset_entry) {
                return ResponseSerializer::serialize_error(ctx.response, ERR_TYPE, "Key holds wrong type\n");
            }
        } else {
            auto new_entry = ctx.entry_manager.create_entry(ctx.args[1], std::make_unique<ZSet>());
            zset_entry = std::dynamic_pointer_cast<Entry<std::unique_ptr<ZSet>>>(new_entry);
        }
    
        bool added = zset_entry->value->add_internal(ctx.args[3], score);
        ResponseSerializer::serialize_integer(ctx.response, added ? 1 : 0);
    }
    
    

    static void handle_zrem(CommandContext ctx) {
        if (ctx.args.size() != 3) { 
            return ResponseSerializer::serialize_error(ctx.response, ERR_ARG, "ZREM requires key and member\n");
        }
    
        auto entry = ctx.entry_manager.find_entry(ctx.args[1]);
        if (!entry) { 
            ResponseSerializer::serialize_integer(ctx.response, 0);
            return;
        }
    
        auto zset_entry = std::dynamic_pointer_cast<Entry<std::unique_ptr<ZSet>>>(entry);
        if (!zset_entry) {
            return ResponseSerializer::serialize_error(ctx.response, ERR_TYPE, "Key holds wrong type\n");
        }
    
        bool removed = zset_entry->value->remove_internal(ctx.args[2]); 
        ResponseSerializer::serialize_integer(ctx.response, removed ? 1 : 0);
    }
    

    static void handle_pexpire(CommandContext ctx) {
        if (ctx.args.size() != 3) {
            return ResponseSerializer::serialize_error(ctx.response, ERR_ARG, "PEXPIRE requires key and TTL\n");
        }
    
        int64_t ttl_ms;
        if (!parse_int(ctx.args[2], ttl_ms) || ttl_ms < 0) {
            return ResponseSerializer::serialize_error(ctx.response, ERR_ARG, "Invalid TTL value\n");
        }
    
        auto entry = ctx.entry_manager.find_entry(ctx.args[1]);
        if (!entry) {
            ResponseSerializer::serialize_integer(ctx.response, 0);  // Key does not exist
            return;
        }
    
        bool success = ctx.entry_manager.set_entry_ttl(*entry, ttl_ms);
        ResponseSerializer::serialize_integer(ctx.response, success ? 1 : 0);
    }
    
    

    static void handle_pttl(CommandContext ctx) {
        if (ctx.args.size() != 2) {
            return ResponseSerializer::serialize_error(ctx.response, ERR_ARG, "PTTL requires key\n");
        }
    
        auto entry = ctx.entry_manager.find_entry(ctx.args[1]);
        if (!entry) {
            ResponseSerializer::serialize_integer(ctx.response, -2);  // Redis returns -2 if key doesn't exist
            return;
        }
    
        int64_t ttl = ctx.entry_manager.get_expiry_time(*entry);
        ResponseSerializer::serialize_integer(ctx.response, ttl);
    }
    
    
    

        // helper function to convert a string to lowercase safely
    static inline std::string to_lower(std::string_view str) {
        std::string result;
        result.reserve(str.size()); // reserve memory for efficiency
        std::transform(str.begin(), str.end(), std::back_inserter(result),
                        [](unsigned char c) { return std::tolower(c); }); // convert each character to lowercase
        return result;
    }

    // helper function to parse double values safely w. try catch blocks.
    static bool parse_double(const std::string& str, double& out) {
        try {
            size_t pos;
            out = std::stod(str, &pos);
            return pos == str.size() && !std::isnan(out);
        } catch (const std::invalid_argument& e) {
            return false;
        } catch (const std::out_of_range& e) {
            return false;
        }
    }
    
        // helper function to parse int values safely w. try catch blocks.
    static bool parse_int(const std::string& str, int64_t& out) {
        try {
            size_t pos;
            out = std::stoll(str, &pos);
            return pos == str.size() && !std::isnan(out);
        } catch (const std::invalid_argument& e) {
            return false;
        } catch (const std::out_of_range& e) {
            return false;
        }
    }
        // function to get current time in microseconds
    static std::uint64_t get_monotonic_usec() {
        using namespace std::chrono;
        return duration_cast<microseconds>(
            steady_clock::now().time_since_epoch()).count();
    }
};

const std::unordered_map<std::string, std::function<void(CommandProcessor::CommandContext)>>
CommandProcessor::command_handlers = {
    {"get", handle_get},
    {"set", handle_set},
    {"del", handle_del},
    {"exists", handle_exists},
    {"zadd", handle_zadd},
    {"zrem", handle_zrem},
    {"flushall", handle_flushall},
    {"pexpire", handle_pexpire},
    {"pttl", handle_pttl}        
};

#endif