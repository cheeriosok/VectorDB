#ifndef COMMAND_PROCESSOR_HPP
#define COMMAND_PROCESSOR_HPP

// modern command processor with type-safe command handling
class CommandProcessor {
    public:
        // structure to hold command context, including arguments, response buffer, database, and heap
        struct CommandContext {
            const std::vector<std::string>& args; // reference to command arguments
            std::vector<uint8_t>& response; // reference to response buffer
            HMap& db; // reference to database
            std::vector<HeapItem>& heap; // reference to heap
        };
    
        // function to process commands by looking up handlers and executing them
        static void process_command(CommandContext ctx) {
            if (ctx.args.empty()) { // check if no command is provided
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "Empty command");
            }
    
            const auto& cmd = ctx.args[0]; // extract command name
            const auto command_it = command_handlers.find(
                std::string(to_lower(cmd))); // convert command to lowercase and find handler
                
            if (command_it == command_handlers.end()) { // check if command exists
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_UNKNOWN, "Unknown command");
            }
    
            command_it->second(ctx); // call the command handler function
        }
    
    private:
        // lambda function to convert a string to lowercase
        static constexpr auto to_lower = [](std::string_view str) {
            std::string result;
            result.reserve(str.size()); // reserve memory for efficiency
            std::ranges::transform(str, std::back_inserter(result), 
                                 [](char c) { return std::tolower(c); }); // convert each character to lowercase
            return result;
        };
    
        // function to handle get command
        static void handle_get(CommandContext ctx) {
            if (ctx.args.size() != 2) { // validate argument count
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "GET requires exactly one key");
            }
    
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
    
            auto [entry, created] = get_or_create_entry(ctx.db, ctx.args[1]); // get or create entry
            if (!created) {
                if (entry->type != EntryType::ZSet) { // check entry type
                    return ResponseSerializer::serialize_error(
                        ctx.response, ERR_TYPE, "Key holds wrong type");
                }
            } else {
                entry->type = EntryType::ZSet;
                entry->zset = std::make_unique<ZSet>(); // initialize zset if new entry
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
            if (!parse_int(ctx.args[4], offset) || !parse_int(ctx.args[5], limit)) { // validate offset and limit
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "Invalid offset or limit");
            }
    
            auto entry = lookup_entry(ctx.db, ctx.args[1]); // find entry in database
            if (!entry) { // check if entry exists
                ResponseSerializer::serialize_array(ctx.response, 0);
                return;
            }
    
            if (entry->type != EntryType::ZSet) { // ensure entry is of type zset
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_TYPE, "Key holds wrong type");
            }
    
            if (limit <= 0) { // validate limit value
                ResponseSerializer::serialize_array(ctx.response, 0);
                return;
            }
    
            auto results = entry->zset->query(score, ctx.args[3], offset, limit); // perform zset query
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
        if (!parse_int(ctx.args[2], ttl_ms)) { // validate ttl value
            return ResponseSerializer::serialize_error(
                ctx.response, ERR_ARG, "Invalid TTL value");
        }

        auto entry = lookup_entry(ctx.db, ctx.args[1]); // find entry in database
        if (!entry) {
            ResponseSerializer::serialize_integer(ctx.response, 0);
            return;
        }

        set_entry_ttl(*entry, ttl_ms, ctx.heap); // set expiration time
        ResponseSerializer::serialize_integer(ctx.response, 1); // confirm success
    }

    // command map mapping string commands to function handlers
    static inline const std::unordered_map<std::string, 
        std::function<void(CommandContext)>> command_handlers = {
        {"get", handle_get},
        {"set", handle_set},
        {"zadd", handle_zadd},
        {"pexpire", handle_pexpire}
        // more commands coming soon!!! :/ (hell no)
    };
    
        // function to handle pttl command
        static void handle_pttl(CommandContext ctx) {
            if (ctx.args.size() != 2) { // validate argument count
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "PTTL requires key");
            }
    
            auto entry = lookup_entry(ctx.db, ctx.args[1]); // find entry in database
            if (!entry) { // check if entry exists
                ResponseSerializer::serialize_integer(ctx.response, -2);
                return;
            }
    
            if (entry->heap_idx == static_cast<size_t>(-1)) { // check if key has an expiration time
                ResponseSerializer::serialize_integer(ctx.response, -1);
                return;
            }
    
            auto expire_at = ctx.heap[entry->heap_idx].val; // get expiration time
            auto now = get_monotonic_usec(); // get current time
            auto ttl = expire_at > now ? (expire_at - now) / 1000 : 0; // calculate remaining ttl in milliseconds
            ResponseSerializer::serialize_integer(ctx.response, ttl); // return ttl value
        }
    
        // command map using string_view for efficiency
        static inline const std::unordered_map<std::string, 
            std::function<void(CommandContext)>> command_handlers = {
            {"get", handle_get},
            {"set", handle_set},
            {"zadd", handle_zadd},
            {"zquery", handle_zquery},
            {"pexpire", handle_pexpire},
            {"pttl", handle_pttl}
            // Add other commands...
        };
    
        // helper function to parse double values safely w. try catch blocks.
        static bool parse_double(const std::string& str, double& out) {
            try {
                size_t pos;
                out = std::stod(str, &pos);
                return pos == str.size() && !std::isnan(out);
            } catch (...) {
                return false;
            }
        }
    
        // helper function to parse int values safely w. try catch blocks.
        static bool parse_int(const std::string& str, int64_t& out) {
            try {
                size_t pos;
                out = std::stoll(str, &pos);
                return pos == str.size();
            } catch (...) {
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

#endif