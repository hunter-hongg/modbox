#ifndef CMP_HPP
#define CMP_HPP

#include <cstdint>

struct CmpOptions {
    int silent = 0;            // -s/--quiet/--silent: no output, status only
    int verbose = 0;           // -l/--verbose: list every differing byte
    int print_bytes = 0;       // -b/--print-bytes: show byte mnemonics
    int64_t limit = -1;        // -n/--bytes=LIMIT: compare at most LIMIT bytes
    int64_t ignore_initial1 = 0;  // -i/--ignore-initial=SKIP for FILE1
    int64_t ignore_initial2 = 0;  // second operand of --ignore-initial=SKIP1:SKIP2
};

int cmp_command(int argc, char** argv);

#endif
