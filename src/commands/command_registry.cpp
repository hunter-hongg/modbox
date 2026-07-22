#include "commands/command_registry.hpp"

CommandRegistry &CommandRegistry::instance() {
    static CommandRegistry inst;
    return inst;
}

void CommandRegistry::add(const char *name, const char *help,
                          void (*run)(int, char **)) {
    entries_.push_back({name, help, run});
}

const std::vector<CommandEntry> &CommandRegistry::all() const {
    return entries_;
}

void CommandRegistry::for_each(std::function<void(const CommandEntry &)> fn) const {
    for (const auto &e : entries_) {
        fn(e);
    }
}
