#ifndef RG_HPP
#define RG_HPP

#include <string>
#include <vector>

enum class RgMode { BASIC, EXTENDED, FIXED };
enum class RgColor { NEVER, ALWAYS, AUTO };

struct RgOptions {
    int line_number = 1;          // -n (default: 1)
    int no_line_number = 0;       // -N (suppress line numbers)
    int ignore_case = 0;          // -i
    int smart_case = 1;           // -S (default: 1)
    int case_sensitive = 0;       // -s (force case-sensitive)
    int invert_match = 0;         // -v
    int word_regexp = 0;          // -w
    int line_regexp = 0;          // -x
    int only_matching = 0;        // -o
    int files_with_matches = 0;   // -l
    int count_only = 0;           // -c
    int fixed_strings = 0;        // -F
    int hidden = 0;               // --hidden
    int no_ignore = 0;            // --no-ignore (implies --hidden)
    int max_count = 0;            // -m (0 = unlimited)
    int max_depth = -1;           // --max-depth (-1 = unlimited)
    int context_before = 0;       // -B / --before-context
    int context_after = 0;        // -A / --after-context
    int context = 0;              // -C / --context (both sides)
    RgMode mode = RgMode::BASIC;
    RgColor color_mode = RgColor::AUTO;
    std::string pattern;
    std::vector<std::string> glob_patterns; // -g / --glob patterns
};

void rg_command(int argc, char** argv);

#endif
