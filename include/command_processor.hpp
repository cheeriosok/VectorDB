#ifndef COMMAND_PROCESSOR_HPP
#define COMMAND_PROCESSOR_HPP

#include <unordered_map>
#include <vector>
#include <string>

class CommandProcessor {
public:
    static void process_command(const std::vector<std::string>& args, std::vector<uint8_t>& response) {
        if (args.empty()) {
            response.push_back(0);
            return;
        }

        auto it = command_handlers.find(args[0]);
        if (it == command_handlers.end()) {
            response.push_back(1);
            return;
        }

        it->second(args, response);
    }

private:
    static inline const std::unordered_map<std::string, void(*)(const std::vector<std::string>&, std::vector<uint8_t>&)> command_handlers = {
        {"ping", [](const auto&, auto& resp) { resp.push_back(2); }},
        {"echo", [](const auto& args, auto& resp) { resp.insert(resp.end(), args[1].begin(), args[1].end()); }}
    };
};

#endif 
