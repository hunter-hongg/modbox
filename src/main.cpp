#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <filesystem>

#include "commands/help.hpp"
#include "commands/command_registry.hpp"

using CommandFunc = void (*)(int, char**);

static void execute_command(const std::string& command, int argc, char** argv) {
    for (const auto& e : CommandRegistry::instance().all()) {
        if (e.name == command) {
            e.run(argc, argv);
            return;
        }
    }
    std::string runname = std::filesystem::path(argv[0]).filename().string();
    fprintf(stderr, "Unknown command: %s\n", command.c_str());
    output_help(argv[0], runname.c_str());
}

int main(int argc, char* argv[]) {
    std::string runname = std::filesystem::path(argv[0]).filename().string();
    if (runname == "modbox" && argc == 1) {
        output_help(argv[0], runname.c_str());
        return 0;
    }
    if (runname == "modbox") {
        execute_command(argv[1], argc - 1, argv + 1);
    } else {
        execute_command(runname, argc, argv);
    }
    return 0;
}
