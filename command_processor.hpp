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
#include <mutex>         // std::mutex for thread safety
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
    // structure to hold command context, including arguments, response buffer, database, heap, and mutex
    struct CommandContext {
        const std::vector<std::string>& args; // reference to command arguments
        std::vector<uint8_t>& response;       // reference to response buffer
        HMap<std::string, Entry>& db;                             // reference to database
        BinaryHeap<uint64_t>& heap;            // reference to heap
        std::mutex& db_mutex;                 // mutex for thread-safe operations
    };

    // function to process commands by looking up handlers and executing them
    static void process_command(CommandContext ctx) {
        if (ctx.args.empty()) { // check if no command is provided
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "empty command");
        }
        
            std::string command_key = ctx.args[0]; // Copy to a mutable variable
        std::transform(command_key.begin(), command_key.end(), command_key.begin(), ::tolower);


        auto it = command_handlers.find(command_key);
        if (it == command_handlers.end()) { // check if command exists
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_UNKNOWN, "unknown command");
        }

        it->second(ctx); // execute the corresponding command handler
    }

private:
    // helper function to convert a string to lowercase safely
    static inline std::string to_lower(std::string_view str) {
        std::string result;
        result.reserve(str.size()); // reserve memory for efficiency
        std::transform(str.begin(), str.end(), std::back_inserter(result),
                       [](unsigned char c) { return std::tolower(c); }); // convert each character to lowercase
        return result;
    }

    // function to handle get command
    static void handle_get(CommandContext ctx) {
        if (ctx.args.size() != 2) { // validate argument count
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "GET requires exactly one key");
        }

        std::lock_guard<std::mutex> lock(ctx.db_mutex); // lock database for thread safety
        auto entry = lookup_entry(ctx.db, ctx.args[1]); // find entry in database
        if (!entry) { // check if entry exists
            return ResponseSerializer::serialize_nil(ctx.response);
        }

        if (entry->type != EntryType::String) { // check if entry type is correct
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_TYPE, "Key holds wrong type");
        }

        ResponseSerializer::serialize_string(ctx.response, entry->value); // return the stored value
    }

    // function to handle set command
    static void handle_set(CommandContext ctx) {
        if (ctx.args.size() != 3) { // validate argument count
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "SET requires key and value");
        }

        std::lock_guard<std::mutex> lock(ctx.db_mutex); // lock database for thread safety
        auto [entry, created] = get_or_create_entry(ctx.db, ctx.args[1]); // get or create entry
        if (!created && entry->type != EntryType::String) { // check entry type
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_TYPE, "Key holds wrong type");
        }

        entry->type = EntryType::String; // set entry type
        entry->value = ctx.args[2]; // store value
        ResponseSerializer::serialize_nil(ctx.response); // acknowledge command success
    }

    // function to handle zadd command
    static void handle_zadd(CommandContext ctx) {
        if (ctx.args.size() != 4) { // validate argument count
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "ZADD requires key, score and member");
        }

        double score;
        if (!parse_double(ctx.args[2], score)) { // validate score value
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "Invalid score value");
        }

        std::lock_guard<std::mutex> lock(ctx.db_mutex); // lock database for thread safety
        auto [entry, created] = get_or_create_entry(ctx.db, ctx.args[1]); // get or create entry
        if (!created && entry->type != EntryType::ZSet) { // check entry type
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_TYPE, "Key holds wrong type");
        }

        if (!entry->zset) { // ensure zset is initialized
            entry->zset = std::make_unique<ZSet>();
        }

        bool added = entry->zset->add(ctx.args[3], score); // add member to sorted set
        ResponseSerializer::serialize_integer(ctx.response, added ? 1 : 0); // return success flag
    }

        // function to handle zquery command
    static void handle_zquery(CommandContext ctx) {
        if (ctx.args.size() != 6) { // validate argument count
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "ZQUERY requires key, score, name, offset, limit");
        }

        double score;
        if (!parse_double(ctx.args[2], score)) { // validate score value
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "Invalid score value");
        }

        int64_t offset, limit;
        if (!parse_int(ctx.args[4], offset) || !parse_int(ctx.args[5], limit) || offset < 0 || limit <= 0) { // validate offset & limit
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "Invalid offset or limit");
        }

        std::lock_guard<std::mutex> lock(ctx.db_mutex); // lock database for thread safety
        auto entry = lookup_entry(ctx.db, ctx.args[1]); // find entry in database
        if (!entry) { // if entry does not exist
            ResponseSerializer::serialize_array(ctx.response, 0);
            return;
        }

        if (entry->type != EntryType::ZSet || !entry->zset) { // ensure entry is a valid ZSet
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_TYPE, "Key holds wrong type");
        }

        auto results = entry->zset->query(score, ctx.args[3], offset, limit); // perform ZSet query

        ResponseSerializer::serialize_array(ctx.response, results.size() * 2); // serialize results count
        
        for (const auto& result : results) { // iterate over results
            ResponseSerializer::serialize_string(ctx.response, result.name); // serialize name
            ResponseSerializer::serialize_double(ctx.response, result.score); // serialize score
        }
    }
    
       // function to handle pexpire command for setting ttl
    static void handle_pexpire(CommandContext ctx) {
        if (ctx.args.size() != 3) { // validate argument count
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "PEXPIRE requires key and milliseconds");
        }

        int64_t ttl_ms;
        if (!parse_int(ctx.args[2], ttl_ms) || ttl_ms < 0) { // validate TTL value
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "Invalid TTL value");
        }

        std::lock_guard<std::mutex> lock(ctx.db_mutex); // lock database for thread safety
        auto entry = lookup_entry(ctx.db, ctx.args[1]); // find entry in database
        if (!entry) { // if entry does not exist
            ResponseSerializer::serialize_integer(ctx.response, 0);
            return;
        }

        set_entry_ttl(*entry, ttl_ms, ctx.heap); // set expiration time in milliseconds
        ResponseSerializer::serialize_integer(ctx.response, 1); // confirm success
    }

    // command map mapping string commands to function handlers
    static const std::unordered_map<std::string, std::function<void(CommandContext)>> command_handlers;
    
        // function to handle pttl command
    static void handle_pttl(CommandContext ctx) {
        if (ctx.args.size() != 2) { // validate argument count
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "PTTL requires key");
        }

        std::lock_guard<std::mutex> lock(ctx.db_mutex); // lock database for thread safety
        auto entry = lookup_entry(ctx.db, ctx.args[1]); // find entry in database
        if (!entry) { // check if entry exists
            ResponseSerializer::serialize_integer(ctx.response, -2);
            return;
        }

        if (entry->heap_idx == static_cast<size_t>(-1) || entry->heap_idx >= ctx.heap.size()) { // validate heap index
            ResponseSerializer::serialize_integer(ctx.response, -1);
            return;
        }

        auto expire_at = ctx.heap[entry->heap_idx].val; // get expiration time
        auto now = get_monotonic_usec(); // get current time
        auto ttl = expire_at > now ? (expire_at - now) / 1000 : 0; // calculate remaining ttl in milliseconds
        ResponseSerializer::serialize_integer(ctx.response, ttl); // return ttl value
    }
        // command map using string_view for efficienc
    
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
        {"zadd", handle_zadd},
        {"zquery", handle_zquery},
        {"pexpire", handle_pexpire},
        {"pttl", handle_pttl}
};

#endif