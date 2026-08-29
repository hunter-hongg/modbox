#ifndef PAGER_HPP
#define PAGER_HPP

#include <vector>
#include <string>

// A single view: one file (or stdin) rendered as a set of lines.
struct PagerView {
    std::string name;                 // file name, or "(stdin)"
    std::vector<std::string> lines;
};

// Configurable behaviour for the pager. All fields default to the minimal
// behaviour expected by `cat --less` callers (no numbers, short prompt,
// no search, no startup positioning).
struct PagerOptions {
    bool line_numbers = false;        // -N
    bool ignore_case = false;         // -i / -I (search)
    bool long_prompt = false;         // -M
    bool quit_at_eof = false;         // -E
    bool quit_if_one_screen = false;  // -F
    bool no_init = false;             // -X
    bool chop_long_lines = false;     // -S
    std::string pattern;              // -p / +/pat (initial search)
    long start_line = 1;              // 1-based initial row; <=0 means end
};

// Legacy single-buffer entry point (used by cat --less). Behaviour is
// unchanged: non-TTY passthrough, raw-mode paging with j/k/q.
void pager_run(const std::vector<std::string>& lines);

// Configurable pager entry point (used by the less command).
void pager_run(const std::vector<PagerView>& views, const PagerOptions* cfg);

#endif
