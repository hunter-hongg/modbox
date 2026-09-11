#ifndef COMMAND_MACROS_HPP
#define COMMAND_MACROS_HPP

#include "commands/command_registry.hpp"

// Registration may allocate; keep it non-throwing so the static initializers
// that call it cannot throw during startup (cert-err58-cpp).
inline bool modbox_register_command(const char* name, const char* help,
                                   int (*run)(int, char**)) noexcept {
    try {
        CommandRegistry::instance().add(name, help, run);
    } catch (...) {
        return false;
    }
    return true;
}

#define REGISTER_COMMAND(n, f, h)                                                   \
    namespace {                                                                     \
        const bool _##f##_reg = modbox_register_command(n, h, f);                   \
    }

#endif
