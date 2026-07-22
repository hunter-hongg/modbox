#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>
#include "commands/command_registry.hpp"
#include "commands/help.hpp"
#include "commands/command_macros.hpp"

static void print_sorted(const char* argv0, const std::vector<const CommandEntry*>& sorted) {
    for (const auto* e : sorted) {
        printf(" %-16s %s\n", e->name.c_str(), e->help.c_str());
    }
    printf("\n");
    printf("Run \"%s help <command>\" or \"%s <command> --help\" for detailed help on a specific command.\n",
           argv0, argv0);
}

void output_help(const char* argv0, const char* progname) {
    if (strcmp(progname, "modbox") == 0) {
        printf("Usage: %s <command> [options]\n", argv0);
        printf("\n");
        printf("Available commands:\n");
        std::vector<const CommandEntry*> sorted;
        for (const auto& e : CommandRegistry::instance().all()) {
            sorted.push_back(&e);
        }
        std::sort(sorted.begin(), sorted.end(),
                  [](const CommandEntry* a, const CommandEntry* b) { return a->name < b->name; });
        print_sorted(argv0, sorted);
    } else {
        printf("Usage: %s [options]\n", argv0);
    }
}

void help_command(int argc, char** argv) {
    (void)argc;
    (void)argv;
    output_help("modbox", "modbox");
}

REGISTER_COMMAND("help", help_command, "Display this help message");
