#ifndef COMMAND_PROCESSOR_HPP
#define COMMAND_PROCESSOR_HPP

// Modern command processor with type-safe command handling
class CommandProcessor {
    public:
        struct CommandContext {
            const std::vector<std::string>& args;
            std::vector<uint8_t>& response;
            HMap& db;
            std::vector<HeapItem>& heap;
        };
    
        static void process_command(CommandContext ctx) {
            if (ctx.args.empty()) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "Empty command");
            }
    
            const auto& cmd = ctx.args[0];
            const auto command_it = command_handlers.find(
                std::string(to_lower(cmd)));
                
            if (command_it == command_handlers.end()) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_UNKNOWN, "Unknown command");
            }
    
            command_it->second(ctx);
        }
    
    private:
        static constexpr auto to_lower = [](std::string_view str) {
            std::string result;
            result.reserve(str.size());
            std::ranges::transform(str, std::back_inserter(result), 
                                 [](char c) { return std::tolower(c); });
            return result;
        };
    
        // Command handlers
        static void handle_get(CommandContext ctx) {
            if (ctx.args.size() != 2) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "GET requires exactly one key");
            }
    
            auto entry = lookup_entry(ctx.db, ctx.args[1]);
            if (!entry) {
                return ResponseSerializer::serialize_nil(ctx.response);
            }
    
            if (entry->type != EntryType::String) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_TYPE, "Key holds wrong type");
            }
    
            ResponseSerializer::serialize_string(ctx.response, entry->value);
        }
    
        static void handle_set(CommandContext ctx) {
            if (ctx.args.size() != 3) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "SET requires key and value");
            }
    
            auto [entry, created] = get_or_create_entry(ctx.db, ctx.args[1]);
            if (!created && entry->type != EntryType::String) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_TYPE, "Key holds wrong type");
            }
    
            entry->type = EntryType::String;
            entry->value = ctx.args[2];
            ResponseSerializer::serialize_nil(ctx.response);
        }
    
        static void handle_zadd(CommandContext ctx) {
            if (ctx.args.size() != 4) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "ZADD requires key, score and member");
            }
    
            double score;
            if (!parse_double(ctx.args[2], score)) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "Invalid score value");
            }
    
            auto [entry, created] = get_or_create_entry(ctx.db, ctx.args[1]);
            if (!created) {
                if (entry->type != EntryType::ZSet) {
                    return ResponseSerializer::serialize_error(
                        ctx.response, ERR_TYPE, "Key holds wrong type");
                }
            } else {
                entry->type = EntryType::ZSet;
                entry->zset = std::make_unique<ZSet>();
            }
    
            bool added = entry->zset->add(ctx.args[3], score);
            ResponseSerializer::serialize_integer(ctx.response, added ? 1 : 0);
        }
    
        static void handle_zquery(CommandContext ctx) {
            if (ctx.args.size() != 6) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "ZQUERY requires key, score, name, offset, limit");
            }
    
            double score;
            if (!parse_double(ctx.args[2], score)) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "Invalid score value");
            }
    
            int64_t offset, limit;
            if (!parse_int(ctx.args[4], offset) || !parse_int(ctx.args[5], limit)) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "Invalid offset or limit");
            }
    
            auto entry = lookup_entry(ctx.db, ctx.args[1]);
            if (!entry) {
                ResponseSerializer::serialize_array(ctx.response, 0);
                return;
            }
    
            if (entry->type != EntryType::ZSet) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_TYPE, "Key holds wrong type");
            }
    
            if (limit <= 0) {
                ResponseSerializer::serialize_array(ctx.response, 0);
                return;
            }
    
            auto results = entry->zset->query(score, ctx.args[3], offset, limit);
            ResponseSerializer::serialize_array(ctx.response, results.size() * 2);
            
            for (const auto& result : results) {
                ResponseSerializer::serialize_string(ctx.response, result.name);
                ResponseSerializer::serialize_double(ctx.response, result.score);
            }
        }
    
        // TTL handling
        static void handle_pexpire(CommandContext ctx) {
            if (ctx.args.size() != 3) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "PEXPIRE requires key and milliseconds");
            }
    
            int64_t ttl_ms;
            if (!parse_int(ctx.args[2], ttl_ms)) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "Invalid TTL value");
            }
    
            auto entry = lookup_entry(ctx.db, ctx.args[1]);
            if (!entry) {
                ResponseSerializer::serialize_integer(ctx.response, 0);
                return;
            }
    
            set_entry_ttl(*entry, ttl_ms, ctx.heap);
            ResponseSerializer::serialize_integer(ctx.response, 1);
        }
    
        static void handle_pttl(CommandContext ctx) {
            if (ctx.args.size() != 2) {
                return ResponseSerializer::serialize_error(
                    ctx.response, ERR_ARG, "PTTL requires key");
            }
    
            auto entry = lookup_entry(ctx.db, ctx.args[1]);
            if (!entry) {
                ResponseSerializer::serialize_integer(ctx.response, -2);
                return;
            }
    
            if (entry->heap_idx == static_cast<size_t>(-1)) {
                ResponseSerializer::serialize_integer(ctx.response, -1);
                return;
            }
    
            auto expire_at = ctx.heap[entry->heap_idx].val;
            auto now = get_monotonic_usec();
            auto ttl = expire_at > now ? (expire_at - now) / 1000 : 0;
            ResponseSerializer::serialize_integer(ctx.response, ttl);
        }
    
        // Command map using string_view for efficiency
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
    
        // Helper functions
        static bool parse_double(const std::string& str, double& out) {
            try {
                size_t pos;
                out = std::stod(str, &pos);
                return pos == str.size() && !std::isnan(out);
            } catch (...) {
                return false;
            }
        }
    
        static bool parse_int(const std::string& str, int64_t& out) {
            try {
                size_t pos;
                out = std::stoll(str, &pos);
                return pos == str.size();
            } catch (...) {
                return false;
            }
        }
    
        static std::uint64_t get_monotonic_usec() {
            using namespace std::chrono;
            return duration_cast<microseconds>(
                steady_clock::now().time_since_epoch()).count();
        }
    };

#endif