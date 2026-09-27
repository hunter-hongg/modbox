#ifndef ARG_UTIL_HPP
#define ARG_UTIL_HPP

#include <argtable3.h>
#include <cstdio>
#include <initializer_list>
#include <utility>
#include <vector>

// RAII wrapper around an argtable3 table. The arg_end entry MUST be the
// last initializer, per arg_parse requirements. Frees the table on scope
// exit, including early returns. Note: exit() bypasses destructors, which
// matches the pre-RAII leak-on-exit behavior.
class ArgTable {
public:
    ArgTable(std::initializer_list<void*> entries) : table_(entries) {}
    explicit ArgTable(std::vector<void*> entries) : table_(std::move(entries)) {}
    ~ArgTable() { arg_freetable(table_.data(), table_.size()); }
    ArgTable(const ArgTable&) = delete;
    ArgTable& operator=(const ArgTable&) = delete;

    int parse(int argc, char** argv) {
        return arg_parse_n(argc, argv, table_.data(), table_.size());
    }

    int print_errors(struct arg_end* end, const char* prog) {
        arg_print_errors(stderr, end, prog);
        return 1;
    }

private:
    std::vector<void*> table_;
};

// Print argtable3 parse errors in the modbox convention used across commands
// (e.g. getenforce, audit2allow): "prog: unrecognized option '...'" /
// "prog: unexpected argument '...'" plus a --help hint. Returns 1 so callers
// can `return print_arg_errors(end, argv[0]);`.
inline int print_arg_errors(struct arg_end* end, const char* prog) {
    for (int i = 0; i < end->count; i++) {
        const char* argval = end->argval[i] != nullptr ? end->argval[i] : "";
        if (end->error[i] == ARG_ELONGOPT) {
            (void)fprintf(stderr, "%s: unrecognized option '%s'\n", prog, argval);
        } else {
            (void)fprintf(stderr, "%s: unexpected argument '%s'\n", prog, argval);
        }
    }
    (void)fprintf(stderr, "Try '%s --help' for more information.\n", prog);
    return 1;
}

#endif
