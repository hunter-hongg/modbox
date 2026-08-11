#ifndef COMMAND_REGISTRY_HPP
#define COMMAND_REGISTRY_HPP

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

struct CommandEntry {
    std::string name;
    std::string help;
    int (*run)(int, char **);
};

class CommandRegistry {
public:
    static CommandRegistry &instance();

    void add(const char *name, const char *help, int (*run)(int, char **));
    const std::vector<CommandEntry> &all() const;
    const CommandEntry* find(const std::string& name) const;

    void for_each(std::function<void(const CommandEntry &)> fn) const;

private:
    CommandRegistry() = default;
    std::vector<CommandEntry> entries_;
    mutable std::unordered_map<std::string, const CommandEntry*> index_;
};

#endif
