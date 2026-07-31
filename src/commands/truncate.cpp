#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <argtable3.h>
#include "commands/truncate.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"

struct TruncateOptions {
    int no_create = 0;      // -c
    int io_blocks = 0;      // -o
    std::string reference;  // -r
    std::string size_str;   // -s
};

/* Parse a human-readable size string like "10K", "1M", "+100", "-1G", "<512", ">1024", "/64", "%32".
 * Returns the parsed value through `out` and true on success. */
static bool parse_size(const char* s, int64_t& out) {
    if (s == nullptr || *s == '\0') return false;

    const char* p = s;
    int modifier = 0;  // 0=none, 1=+, 2=-, 3=<, 4=>, 5=/, 6=%
    int64_t multiplier = 1;

    /* Detect modifier prefix */
    if (*p == '+' || *p == '-' || *p == '<' || *p == '>' || *p == '/' || *p == '%') {
        modifier = (*p == '+') ? 1 : (*p == '-') ? 2 : (*p == '<') ? 3 : (*p == '>') ? 4 : (*p == '/') ? 5 : 6;
        p++;
    }

    /* Skip whitespace */
    while (*p == ' ') p++;

    if (*p == '\0') return false;

    /* Parse number */
    char* end = nullptr;
    errno = 0;
    int64_t val = strtoll(p, &end, 10);
    if (errno != 0 || end == p || *end == '\0') {
        /* Try decimal */
        val = (int64_t)strtod(p, &end);
        if (errno != 0 || end == p) return false;
    }

    /* Parse optional unit suffix */
    if (*end != '\0') {
        char unit = *end;
        switch (unit) {
        case 'B': case 'b':
            end++;
            [[fallthrough]];
        case ' ': case '\t':
            break;
        case 'Y': case 'y': multiplier = 1024LL << 50; end++; break;
        case 'Z': case 'z': multiplier = 1024LL << 42; end++; break;
        case 'E': case 'e': multiplier = 1024LL << 34; end++; break;
        case 'P': case 'p': multiplier = 1024LL << 26; end++; break;
        case 'T': case 't': multiplier = 1024LL << 18; end++; break;
        case 'G': case 'g': multiplier = 1024LL << 10; end++; break;
        case 'M': case 'm': multiplier = 1024LL << 2; end++; break;
        case 'K': case 'k': multiplier = 1024LL;       end++; break;
        case 'R': case 'r': multiplier = 1024LL << 60; end++; break;
        case 'Q': case 'q': multiplier = 1024LL << 68; end++; break;
        default: return false;
        }
    }

    out = val * multiplier;
    return true;
}

/* Apply a modifier to an existing file size. */
static int64_t apply_modifier(int64_t current, int modifier, int64_t new_val) {
    switch (modifier) {
    case 0: return new_val;         // absolute
    case 1: return current + new_val;  // relative increase
    case 2: return current - new_val;  // relative decrease
    case 3: {                        // at most
        if (new_val > current) return current;
        return new_val;
    }
    case 4: {                        // at least
        if (new_val < current) return current;
        return new_val;
    }
    case 5: {                        // floor to multiple
        if (new_val == 0) return current;
        return (current / new_val) * new_val;
    }
    case 6: {                        // ceil to multiple
        if (new_val == 0) return current;
        int64_t r = current % new_val;
        return r == 0 ? current : current + (new_val - r);
    }
    default: return new_val;
    }
}

int truncate_command(int argc, char** argv) {
    struct arg_lit* no_create_opt = arg_lit0("c", "no-create", "do not create any files");
    struct arg_lit* io_blocks_opt = arg_lit0("o", "io-blocks", "treat SIZE as IO blocks");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_str* ref_opt = arg_str0("r", "reference", "REFERENCE", "base file for size");
    struct arg_str* size_opt = arg_str0("s", "size", "SIZE", "desired file size");
    struct arg_file* files_arg = arg_filen(NULL, NULL, "FILE", 1, 1000, "file(s) to truncate");
    struct arg_end* end = arg_end(20);

    ArgTable at({no_create_opt, io_blocks_opt, help_opt, ref_opt, size_opt, files_arg, end});

    int nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... FILE...\n", argv[0]);
        printf("Shrink or extend each FILE to the specified size.\n");
        printf("\n");
        printf("  -c, --no-create          do not create any files\n");
        printf("  -o, --io-blocks          treat SIZE as IO blocks\n");
        printf("  -r, --reference=FILE     base file for size\n");
        printf("  -s, --size=SIZE          desired file size\n");
        printf("      SIZE may be followed/preceded by a modifier:\n");
        printf("        '+' increase, '-' decrease, '<' at most, '>' at least,\n");
        printf("        '/' floor, '%%' ceiling (to a multiple of SIZE)\n");
        printf("      SIZE may have a suffix: K, M, G, T, P, E, Z, Y, R, Q\n");
        printf("        (1024^N) or KB, MB, ... (1000^N)\n");
        printf("  -h, --help               display this help and exit\n");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    if (files_arg->count == 0) {
        fprintf(stderr, "truncate: missing file operand\n");
        fprintf(stderr, "Try 'truncate --help' for more information.\n");
        return 0;
    }

    if (size_opt->count == 0 && ref_opt->count == 0) {
        fprintf(stderr, "truncate: missing operand\n");
        fprintf(stderr, "Try 'truncate --help' for more information.\n");
        return 0;
    }

    TruncateOptions opts;
    opts.no_create = (no_create_opt->count > 0);
    opts.io_blocks = (io_blocks_opt->count > 0);
    if (ref_opt->count > 0) opts.reference = ref_opt->sval[0];
    if (size_opt->count > 0) opts.size_str = size_opt->sval[0];

    for (int i = 0; i < files_arg->count; i++) {
        const char* filepath = files_arg->filename[i];

        /* Get reference file size if specified */
        int64_t target_size = 0;
        bool have_target = false;
        bool is_relative = false;

        if (!opts.reference.empty()) {
            struct stat st;
            if (stat(opts.reference.c_str(), &st) != 0) {
                fprintf(stderr, "truncate: cannot stat '%s': %s\n",
                        opts.reference.c_str(), strerror(errno));
                continue;
            }
            target_size = opts.io_blocks ? (st.st_blocks >> 1) : st.st_size;
            have_target = true;
        } else {
            if (!parse_size(opts.size_str.c_str(), target_size)) {
                fprintf(stderr, "truncate: invalid '%s': %s\n",
                        opts.size_str.c_str(), strerror(EINVAL));
                continue;
            }
            /* Check if it's a relative/modified size */
            const char* s = opts.size_str.c_str();
            if (*s == '+' || *s == '-' || *s == '<' || *s == '>' || *s == '/' || *s == '%') {
                is_relative = true;
            }
        }

        /* Create file if it doesn't exist and --no-create not set */
        struct stat st;
        bool exists = (stat(filepath, &st) == 0);

        if (!exists) {
            if (opts.no_create) {
                fprintf(stderr, "truncate: %s: No such file\n", filepath);
                continue;
            }
            /* Create empty file */
            FILE* f = fopen(filepath, "w");
            if (!f) {
                fprintf(stderr, "truncate: cannot open '%s': %s\n", filepath, strerror(errno));
                continue;
            }
            fclose(f);
            /* If target is negative (relative decrease from nonexistent), skip */
            if (is_relative && target_size < 0) {
                continue;
            }
            if (!is_relative) {
                target_size = target_size < 0 ? 0 : target_size;
            }
        }

        if (is_relative) {
            int64_t current_size = st.st_size;
            target_size = apply_modifier(current_size, 0, target_size);
            /* Re-parse the modifier to get the actual modifier type */
            const char* s = opts.size_str.c_str();
            int mod = 0;
            if (*s == '+') mod = 1;
            else if (*s == '-') mod = 2;
            else if (*s == '<') mod = 3;
            else if (*s == '>') mod = 4;
            else if (*s == '/') mod = 5;
            else if (*s == '%') mod = 6;

            /* Re-parse the numeric part */
            int64_t raw_val = 0;
            const char* num_start = s + 1;
            while (*num_start == ' ') num_start++;
            char* endptr = nullptr;
            errno = 0;
            raw_val = strtoll(num_start, &endptr, 10);

            int64_t multiplier = 1;
            if (*endptr != '\0') {
                char unit = *endptr;
                switch (unit) {
                case 'Y': case 'y': multiplier = 1024LL << 50; break;
                case 'Z': case 'z': multiplier = 1024LL << 42; break;
                case 'E': case 'e': multiplier = 1024LL << 34; break;
                case 'P': case 'p': multiplier = 1024LL << 26; break;
                case 'T': case 't': multiplier = 1024LL << 18; break;
                case 'G': case 'g': multiplier = 1024LL << 10; break;
                case 'M': case 'm': multiplier = 1024LL << 2; break;
                case 'K': case 'k': multiplier = 1024LL; break;
                default: break;
                }
            }
            int64_t adjusted = raw_val * multiplier;
            target_size = apply_modifier(current_size, mod, adjusted);
        }

        /* Clamp negative sizes to 0 */
        if (target_size < 0) target_size = 0;

        /* Apply truncate/ftruncate */
        int ret = 0;
        if (opts.io_blocks) {
            /* For io_blocks mode, use ftruncate with block-sized value */
            int fd = open(filepath, O_RDWR);
            if (fd < 0) {
                fprintf(stderr, "truncate: cannot open '%s': %s\n", filepath, strerror(errno));
                continue;
            }
            if (ftruncate(fd, target_size) != 0) {
                fprintf(stderr, "truncate: cannot truncate '%s': %s\n", filepath, strerror(errno));
                ret = 1;
            }
            close(fd);
        } else {
            if (truncate(filepath, target_size) != 0) {
                fprintf(stderr, "truncate: cannot truncate '%s': %s\n", filepath, strerror(errno));
                ret = 1;
            }
        }

        if (ret != 0) {
            // error already printed
        }
    }

    return 0;
}
REGISTER_COMMAND("truncate", truncate_command, "Shrink or extend files to the specified size");
