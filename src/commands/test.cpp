#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "commands/test.hpp"
#include "commands/command_macros.hpp"

namespace {

struct Ctx {
    char** args;
    int nargs;
    int pos;
    const char* prog;
    bool error;

    bool at_end() const { return pos >= nargs; }
    const char* cur() const { return args[pos]; }
};

bool is_unary(const char* s) {
    static const char* ops[] = {
        "-b", "-c", "-d", "-e", "-f", "-g", "-h", "-k", "-L", "-n", "-N",
        "-O", "-p", "-r", "-s", "-S", "-t", "-u", "-w", "-x", "-z", "-G",
        "-a", "-o", nullptr
    };
    for (int i = 0; ops[i]; i++)
        if (strcmp(s, ops[i]) == 0) return true;
    return false;
}

bool is_binary(const char* s) {
    return strcmp(s, "=") == 0 || strcmp(s, "==") == 0 || strcmp(s, "!=") == 0 ||
           strcmp(s, "-eq") == 0 || strcmp(s, "-ne") == 0 || strcmp(s, "-gt") == 0 ||
           strcmp(s, "-ge") == 0 || strcmp(s, "-lt") == 0 || strcmp(s, "-le") == 0 ||
           strcmp(s, "-nt") == 0 || strcmp(s, "-ot") == 0 || strcmp(s, "-ef") == 0;
}

bool do_stat(const char* path, struct stat* st, bool follow) {
    if (follow) return stat(path, st) == 0;
    return lstat(path, st) == 0;
}

bool unary_check(const char* op, const char* arg, const char* prog) {
    struct stat st;
    if (strcmp(op, "-b") == 0) return do_stat(arg, &st, true) && S_ISBLK(st.st_mode);
    if (strcmp(op, "-c") == 0) return do_stat(arg, &st, true) && S_ISCHR(st.st_mode);
    if (strcmp(op, "-d") == 0) return do_stat(arg, &st, true) && S_ISDIR(st.st_mode);
    if (strcmp(op, "-e") == 0) return do_stat(arg, &st, true);
    if (strcmp(op, "-f") == 0) return do_stat(arg, &st, true) && S_ISREG(st.st_mode);
    if (strcmp(op, "-g") == 0) return do_stat(arg, &st, true) && (st.st_mode & S_ISGID);
    if (strcmp(op, "-h") == 0 || strcmp(op, "-L") == 0)
        return do_stat(arg, &st, false) && S_ISLNK(st.st_mode);
    if (strcmp(op, "-k") == 0) return do_stat(arg, &st, true) && (st.st_mode & S_ISVTX);
    if (strcmp(op, "-n") == 0) return strlen(arg) > 0;
    if (strcmp(op, "-p") == 0) return do_stat(arg, &st, true) && S_ISFIFO(st.st_mode);
    if (strcmp(op, "-r") == 0) return do_stat(arg, &st, true) && access(arg, R_OK) == 0;
    if (strcmp(op, "-s") == 0) return do_stat(arg, &st, true) && st.st_size > 0;
    if (strcmp(op, "-S") == 0) return do_stat(arg, &st, true) && S_ISSOCK(st.st_mode);
    if (strcmp(op, "-u") == 0) return do_stat(arg, &st, true) && (st.st_mode & S_ISUID);
    if (strcmp(op, "-w") == 0) return do_stat(arg, &st, true) && access(arg, W_OK) == 0;
    if (strcmp(op, "-x") == 0) return do_stat(arg, &st, true) && access(arg, X_OK) == 0;
    if (strcmp(op, "-O") == 0) return do_stat(arg, &st, true) && st.st_uid == geteuid();
    if (strcmp(op, "-G") == 0) return do_stat(arg, &st, true) && st.st_gid == getegid();
    if (strcmp(op, "-N") == 0)
        return do_stat(arg, &st, true) && st.st_mtime > st.st_atime;
    if (strcmp(op, "-z") == 0) return strlen(arg) == 0;
    if (strcmp(op, "-t") == 0) return isatty(atoi(arg));
    if (strcmp(op, "-a") == 0) return do_stat(arg, &st, true);
    if (strcmp(op, "-o") == 0) return false;
    fprintf(stderr, "%s: unknown unary operator '%s'\n", prog, op);
    return false;
}

bool parse_int(const char* s, long long& v) {
    char* end = nullptr;
    errno = 0;
    long long r = strtoll(s, &end, 10);
    if (end == s || *end != '\0') return false;
    v = r;
    return true;
}

bool newer_than(const char* a, const char* b) {
    struct stat sa, sb;
    if (!do_stat(a, &sa, true) || !do_stat(b, &sb, true)) return false;
    return sa.st_mtime > sb.st_mtime;
}

bool same_file(const char* a, const char* b) {
    struct stat sa, sb;
    if (!do_stat(a, &sa, true) || !do_stat(b, &sb, true)) return false;
    return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

bool binary_check(const char* op, const char* a, const char* b, Ctx& c) {
    if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) return strcmp(a, b) == 0;
    if (strcmp(op, "!=") == 0) return strcmp(a, b) != 0;
    if (strcmp(op, "-nt") == 0) return newer_than(a, b);
    if (strcmp(op, "-ot") == 0) return newer_than(b, a);
    if (strcmp(op, "-ef") == 0) return same_file(a, b);
    long long v1, v2;
    if (!parse_int(a, v1) || !parse_int(b, v2)) {
        fprintf(stderr, "%s: integer expression expected\n", c.prog);
        c.error = true;
        return false;
    }
    if (strcmp(op, "-eq") == 0) return v1 == v2;
    if (strcmp(op, "-ne") == 0) return v1 != v2;
    if (strcmp(op, "-gt") == 0) return v1 > v2;
    if (strcmp(op, "-ge") == 0) return v1 >= v2;
    if (strcmp(op, "-lt") == 0) return v1 < v2;
    if (strcmp(op, "-le") == 0) return v1 <= v2;
    fprintf(stderr, "%s: unknown binary operator '%s'\n", c.prog, op);
    c.error = true;
    return false;
}

bool parse_expr(Ctx& c);
bool parse_expr_and(Ctx& c);
bool parse_not(Ctx& c);
bool parse_primary(Ctx& c);

bool parse_or(Ctx& c) {
    bool v = parse_expr_and(c);
    while (!c.error && !c.at_end()) {
        if (strcmp(c.cur(), "-o") == 0) {
            c.pos++;
            bool r = parse_expr_and(c);
            v = v || r;
        } else {
            break;
        }
    }
    return v;
}

bool parse_expr_and(Ctx& c) {
    bool v = parse_not(c);
    while (!c.error && !c.at_end()) {
        if (strcmp(c.cur(), "-a") == 0) {
            c.pos++;
            bool r = parse_not(c);
            v = v && r;
        } else {
            break;
        }
    }
    return v;
}

bool parse_not(Ctx& c) {
    if (!c.at_end() && strcmp(c.cur(), "!") == 0) {
        c.pos++;
        return !parse_not(c);
    }
    return parse_primary(c);
}

bool parse_primary(Ctx& c) {
    if (c.at_end()) {
        fprintf(stderr, "%s: syntax error: missing operand\n", c.prog);
        c.error = true;
        return false;
    }
    const char* tok = c.cur();
    if (strcmp(tok, "(") == 0) {
        c.pos++;
        bool v = parse_expr(c);
        if (c.at_end() || strcmp(c.cur(), ")") != 0) {
            fprintf(stderr, "%s: ')' expected\n", c.prog);
            c.error = true;
            return false;
        }
        c.pos++;
        return v;
    }
    if (is_unary(tok)) {
        c.pos++;
        if (c.at_end()) {
            fprintf(stderr, "%s: %s: unary operator expected\n", c.prog, tok);
            c.error = true;
            return false;
        }
        const char* arg = c.cur();
        c.pos++;
        return unary_check(tok, arg, c.prog);
    }
    const char* arg = c.cur();
    c.pos++;
    if (!c.at_end() && is_binary(c.cur())) {
        const char* op = c.cur();
        c.pos++;
        if (c.at_end()) {
            fprintf(stderr, "%s: %s: binary operator expected\n", c.prog, op);
            c.error = true;
            return false;
        }
        const char* arg2 = c.cur();
        c.pos++;
        return binary_check(op, arg, arg2, c);
    }
    return strlen(arg) > 0;
}

bool parse_expr(Ctx& c) {
    return parse_or(c);
}

void print_help(const char* prog) {
    printf("Usage: %s EXPRESSION\n", prog);
    printf("  or:  %s [ EXPRESSION ]\n", prog);
    printf("Exit status is 0 if EXPRESSION is true, 1 if false, 2 if EXPRESSION is invalid.\n");
    printf("\n");
    printf("  ( EXPRESSION )               EXPRESSION is true\n");
    printf("  ! EXPRESSION                 EXPRESSION is false\n");
    printf("  EXPR1 -a EXPR2               both EXPR1 and EXPR2 are true\n");
    printf("  EXPR1 -o EXPR2               either EXPR1 or EXPR2 is true\n");
    printf("\n");
    printf("  -n STRING                    the length of STRING is nonzero\n");
    printf("  STRING                       equivalent to -n STRING\n");
    printf("  -z STRING                    the length of STRING is zero\n");
    printf("  STRING1 = STRING2            the strings are equal\n");
    printf("  STRING1 != STRING2           the strings are not equal\n");
    printf("\n");
    printf("  INTEGER1 -eq INTEGER2        INTEGER1 is equal to INTEGER2\n");
    printf("  INTEGER1 -ge INTEGER2        INTEGER1 is greater than or equal to INTEGER2\n");
    printf("  INTEGER1 -gt INTEGER2        INTEGER1 is greater than INTEGER2\n");
    printf("  INTEGER1 -le INTEGER2        INTEGER1 is less than or equal to INTEGER2\n");
    printf("  INTEGER1 -lt INTEGER2        INTEGER1 is less than INTEGER2\n");
    printf("  INTEGER1 -ne INTEGER2        INTEGER1 is not equal to INTEGER2\n");
    printf("\n");
    printf("  FILE1 -ef FILE2              FILE1 and FILE2 have the same device and inode numbers\n");
    printf("  FILE1 -nt FILE2              FILE1 is newer (modification time) than FILE2\n");
    printf("  FILE1 -ot FILE2              FILE1 is older than FILE2\n");
    printf("\n");
    printf("  -b FILE                      FILE exists and is block special\n");
    printf("  -c FILE                      FILE exists and is character special\n");
    printf("  -d FILE                      FILE exists and is a directory\n");
    printf("  -e FILE                      FILE exists\n");
    printf("  -f FILE                      FILE exists and is a regular file\n");
    printf("  -g FILE                      FILE exists and is set-group-ID\n");
    printf("  -G FILE                      FILE exists and is owned by the effective group ID\n");
    printf("  -h FILE                      FILE exists and is a symbolic link (same as -L)\n");
    printf("  -k FILE                      FILE exists and has its sticky bit set\n");
    printf("  -L FILE                      FILE exists and is a symbolic link (same as -h)\n");
    printf("  -N FILE                      FILE exists and has been modified since it was last read\n");
    printf("  -O FILE                      FILE exists and is owned by the effective user ID\n");
    printf("  -p FILE                      FILE exists and is a named pipe\n");
    printf("  -r FILE                      FILE exists and read permission is granted\n");
    printf("  -s FILE                      FILE exists and has a size greater than zero\n");
    printf("  -S FILE                      FILE exists and is a socket\n");
    printf("  -t FD                        file descriptor FD is opened on a terminal\n");
    printf("  -u FILE                      FILE exists and its set-user-ID bit is set\n");
    printf("  -w FILE                      FILE exists and write permission is granted\n");
    printf("  -x FILE                      FILE exists and execute permission is granted\n");
    printf("\n");
    printf("  --help                       display this help and exit\n");
    printf("  --version                    output version information and exit\n");
    printf("\n");
    printf("NOTE: Binary -a and -o have lower precedence than unary and comparison operators.\n");
    printf("Your shell may have its own version of %s which may differ.\n", prog);
}

}  // namespace

void test_command(int argc, char** argv) {
    const char* prog = argv[0];
    const char* base = strrchr(prog, '/');
    base = base ? base + 1 : prog;
    const bool is_bracket = (strcmp(base, "[") == 0);

    char** args = argv + 1;
    int nargs = argc - 1;

    if (nargs >= 1 && strcmp(args[0], "--help") == 0) {
        print_help(prog);
        exit(0);
    }
    if (nargs >= 1 && strcmp(args[0], "--version") == 0) {
        printf("test (modbox) 1.0\n");
        exit(0);
    }

    if (is_bracket) {
        if (nargs >= 1 && strcmp(args[nargs - 1], "]") == 0) {
            nargs -= 1;
        } else {
            (void)fprintf(stderr, "%s: missing ']'\n", prog);
            exit(2);
        }
    }

    if (nargs == 0) {
        exit(1);
    }

    if (nargs == 1) {
        exit(strlen(args[0]) > 0 ? 0 : 1);
    }

    Ctx c;
    c.args = args;
    c.nargs = nargs;
    c.pos = 0;
    c.prog = prog;
    c.error = false;

    const bool result = parse_expr(c);

    if (c.error) {
        exit(2);
    }
    if (!c.at_end()) {
        (void)fprintf(stderr, "%s: syntax error: extra arguments\n", prog);
        exit(2);
    }
    exit(result ? 0 : 1);
}

REGISTER_COMMAND("test", test_command, "Evaluate conditional expression");
namespace {
const bool _bracket_reg = []{
    CommandRegistry::instance().add("[", "Evaluate conditional expression with brackets", test_command);
    return true;
}();
}
