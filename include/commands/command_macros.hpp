#ifndef COMMAND_MACROS_HPP
#define COMMAND_MACROS_HPP

#include "commands/command_registry.hpp"

#define REGISTER_COMMAND(n, f, h)                                                   \
    namespace {                                                                     \
        const bool _##f##_reg = []{                                                  \
            CommandRegistry::instance().add(n, h, f);                               \
            return true;                                                             \
        }();                                                                        \
    }

#endif
