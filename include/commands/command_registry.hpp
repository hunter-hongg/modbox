#ifndef COMMAND_REGISTRY_HPP
#define COMMAND_REGISTRY_HPP

#include <string>
#include <vector>
#include <functional>

struct CommandEntry {
    std::string name;
    std::string help;
    void (*run)(int, char **);
};

class CommandRegistry {
public:
    static CommandRegistry &instance();

    void add(const char *name, const char *help, void (*run)(int, char **));
    const std::vector<CommandEntry> &all() const;

    void for_each(std::function<void(const CommandEntry &)> fn) const;

private:
    CommandRegistry() = default;
    std::vector<CommandEntry> entries_;
};

#endif
