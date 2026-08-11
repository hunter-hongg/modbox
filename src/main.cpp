#include <cstdio>
#include <string>
#include <filesystem>

#include "commands/help.hpp"
#include "commands/command_registry.hpp"

static int execute_command(const std::string& command, int argc, char** argv) {
    if (const auto* e = CommandRegistry::instance().find(command)) {
        return e->run(argc, argv);
    }
    std::string runname = std::filesystem::path(argv[0]).filename().string();
    (void)fprintf(stderr, "Unknown command: %s\n", command.c_str());
    output_help(argv[0], runname.c_str());
    return 1;
}

int main(int argc, char* argv[]) {
    std::string runname = std::filesystem::path(argv[0]).filename().string();
    if (runname == "modbox" && argc == 1) {
        output_help(argv[0], runname.c_str());
        return 0;
    }
    if (runname == "modbox") {
        return execute_command(argv[1], argc - 1, argv + 1);
    }
    return execute_command(runname, argc, argv);
}
