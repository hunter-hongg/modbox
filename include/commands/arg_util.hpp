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
        return arg_parse(argc, argv, table_.data());
    }

    int print_errors(struct arg_end* end, const char* prog) {
        arg_print_errors(stderr, end, prog);
        return 1;
    }

private:
    std::vector<void*> table_;
};

#endif
