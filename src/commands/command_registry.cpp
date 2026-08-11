#include "commands/command_registry.hpp"

CommandRegistry &CommandRegistry::instance() {
    static CommandRegistry inst;
    return inst;
}

void CommandRegistry::add(const char *name, const char *help,
                          int (*run)(int, char **)) {
    entries_.push_back({name, help, run});
}

const std::vector<CommandEntry> &CommandRegistry::all() const {
    return entries_;
}

const CommandEntry* CommandRegistry::find(const std::string& name) const {
    if (index_.empty()) {
        for (const auto& e : entries_) {
            index_[e.name] = &e;
        }
    }
    const auto it = index_.find(name);
    return it != index_.end() ? it->second : nullptr;
}

void CommandRegistry::for_each(std::function<void(const CommandEntry &)> fn) const {
    for (const auto &e : entries_) {
        fn(e);
    }
}
