#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <vector>
#include <string>
#include <argtable3.h>
#include "commands/tr.hpp"

static unsigned char parse_escape(const char*& p)
{
    p++;
    switch (*p) {
    case '\\': p++; return '\\';
    case 'a':  p++; return '\a';
    case 'b':  p++; return '\b';
    case 'f':  p++; return '\f';
    case 'n':  p++; return '\n';
    case 'r':  p++; return '\r';
    case 't':  p++; return '\t';
    case 'v':  p++; return '\v';
    case '0': case '1': case '2': case '3':
    case '4': case '5': case '6': case '7': {
        int val = 0;
        int cnt = 0;
        while (cnt < 3 && *p >= '0' && *p <= '7') {
            val = val * 8 + (*p - '0');
            p++; cnt++;
        }
        return (unsigned char)val;
    }
    default:
        return (unsigned char)(*p++);
    }
}

static int parse_char(const char*& p)
{
    if (*p == '\\') return parse_escape(p);
    return (unsigned char)(*p++);
}

static bool prefix_match(const char* p, const char* pre)
{
    return strncmp(p, pre, strlen(pre)) == 0;
}

static bool is_char_class(unsigned char c, const char* name)
{
    if (strcmp(name, "alnum") == 0) return isalnum(c);
    if (strcmp(name, "alpha") == 0) return isalpha(c);
    if (strcmp(name, "blank") == 0) return c == ' ' || c == '\t';
    if (strcmp(name, "cntrl") == 0) return iscntrl(c);
    if (strcmp(name, "digit") == 0) return isdigit(c);
    if (strcmp(name, "graph") == 0) return isgraph(c);
    if (strcmp(name, "lower") == 0) return islower(c);
    if (strcmp(name, "print") == 0) return isprint(c);
    if (strcmp(name, "punct") == 0) return ispunct(c);
    if (strcmp(name, "space") == 0) return isspace(c);
    if (strcmp(name, "upper") == 0) return isupper(c);
    if (strcmp(name, "xdigit") == 0) return isxdigit(c);
    return false;
}

static bool is_valid_class(const char* name)
{
    static const char* names[] = {
        "alnum", "alpha", "blank", "cntrl", "digit", "graph", "lower",
        "print", "punct", "space", "upper", "xdigit", nullptr
    };
    for (int i = 0; names[i]; i++)
        if (strcmp(name, names[i]) == 0) return true;
    return false;
}

struct SetParse {
    std::vector<unsigned char> chars;
    bool has_fill = false;
    unsigned char fill_char = 0;
};

static int parse_set(const char* str, SetParse& out)
{
    out.chars.clear();
    out.has_fill = false;
    std::vector<bool> seen(256, false);
    const char* p = str;

    auto add_char = [&](unsigned char c) {
        if (!seen[c]) { seen[c] = true; out.chars.push_back(c); }
    };

    auto add_range = [&](unsigned char lo, unsigned char hi) {
        if (lo > hi) { fprintf(stderr, "tr: invalid range '%c-%c'\n", lo, hi); return -1; }
        for (int c = lo; c <= hi; c++) add_char((unsigned char)c);
        return 0;
    };

    while (*p) {
        if (*p == '[') {
            const char* cp = p + 1;
            if (*cp == ':') {
                const char* end = strstr(cp + 1, ":]");
                if (!end) { fprintf(stderr, "tr: missing closing ':]'\n"); return -1; }
                std::string cls(cp + 1, end - cp - 1);
                if (!is_valid_class(cls.c_str())) {
                    fprintf(stderr, "tr: unrecognized character class '%s'\n", cls.c_str());
                    return -1;
                }
                for (int c = 0; c < 256; c++)
                    if (is_char_class((unsigned char)c, cls.c_str())) add_char((unsigned char)c);
                p = end + 2;
                continue;
            }
            const char* star = nullptr;
            const char* end = p + 1;
            while (*end && *end != ']') {
                if (*end == '*') star = end;
                end++;
            }
            if (star && *end == ']') {
                std::string chstr(p + 1, star - p - 1);
                const char* cp2 = chstr.c_str();
                unsigned char ch;
                if (*cp2) { ch = (unsigned char)parse_char(cp2); }
                else { fprintf(stderr, "tr: missing character in repetition\n"); return -1; }
                const char* ns = star + 1;
                if (*ns >= '0' && *ns <= '9') {
                    int cnt = 0;
                    while (*ns >= '0' && *ns <= '9') { cnt = cnt * 10 + (*ns - '0'); ns++; }
                    for (int i = 0; i < cnt; i++) add_char(ch);
                } else {
                    out.has_fill = true;
                    out.fill_char = ch;
                    add_char(ch);
                }
                p = end + 1;
                continue;
            }
            add_char('[');
            p++;
            continue;
        }

        if (*p == '\\') {
            unsigned char c = (unsigned char)parse_escape(p);
            add_char(c);
            continue;
        }

        if (*(p + 1) == '-' && *(p + 2) != '\0') {
            unsigned char lo = (unsigned char)*p;
            unsigned char hi = (unsigned char)*(p + 2);
            if (add_range(lo, hi) == -1) return -1;
            p += 3;
            continue;
        }

        add_char((unsigned char)*p);
        p++;
    }
    return 0;
}

static std::vector<unsigned char> make_complement(const std::vector<unsigned char>& set)
{
    std::vector<bool> in(256, false);
    for (unsigned char c : set) in[(size_t)c] = true;
    std::vector<unsigned char> result;
    for (int i = 0; i < 256; i++)
        if (!in[i]) result.push_back((unsigned char)i);
    return result;
}

static void build_translate(unsigned char tbl[256],
                            const std::vector<unsigned char>& s1,
                            const std::vector<unsigned char>& s2)
{
    for (int i = 0; i < 256; i++) tbl[i] = (unsigned char)i;
    if (s2.empty()) return;
    for (size_t i = 0; i < s1.size(); i++) {
        size_t j = i < s2.size() ? i : s2.size() - 1;
        tbl[s1[i]] = s2[j];
    }
}

static void tr_translate(FILE* in, const unsigned char tbl[256])
{
    int c;
    while ((c = fgetc(in)) != EOF)
        fputc(tbl[(unsigned char)c], stdout);
}

static void tr_delete(FILE* in, const std::vector<unsigned char>& del_set, bool squeeze,
                      const std::vector<unsigned char>& sq_set)
{
    std::vector<bool> del(256, false);
    for (unsigned char c : del_set) del[(size_t)c] = true;

    std::vector<bool> sq(256, false);
    if (squeeze) for (unsigned char c : sq_set) sq[(size_t)c] = true;

    int last = -1;
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (del[(size_t)(unsigned char)c]) continue;
        if (squeeze && sq[(size_t)(unsigned char)c]) {
            if (c == last) continue;
            last = c;
        } else if (squeeze) {
            last = -1;
        }
        fputc(c, stdout);
    }
}

static void tr_squeeze(FILE* in, const std::vector<unsigned char>& sq_set)
{
    std::vector<bool> sq(256, false);
    for (unsigned char c : sq_set) sq[(size_t)c] = true;
    int last = -1;
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (sq[(size_t)(unsigned char)c]) {
            if (c == last) continue;
            last = c;
        } else {
            last = -1;
        }
        fputc(c, stdout);
    }
}

static void tr_translate_squeeze(FILE* in, const unsigned char tbl[256],
                                 const std::vector<unsigned char>& sq_set)
{
    std::vector<bool> sq(256, false);
    for (unsigned char c : sq_set) sq[(size_t)c] = true;
    int last = -1;
    int c;
    while ((c = fgetc(in)) != EOF) {
        unsigned char tc = tbl[(unsigned char)c];
        if (sq[tc]) {
            if (tc == (unsigned char)last) continue;
            last = tc;
        } else {
            last = -1;
        }
        fputc(tc, stdout);
    }
}

void tr_command(int argc, char** argv)
{
    struct arg_lit* complement_opt = arg_lit0("c", "complement", "use complement of SET1");
    struct arg_lit* delete_opt = arg_lit0("d", "delete", "delete characters in SET1, do not translate");
    struct arg_lit* squeeze_opt = arg_lit0("s", "squeeze-repeats", "replace repeated characters");
    struct arg_lit* truncate_opt = arg_lit0("t", "truncate-set1", "truncate SET1 to length of SET2");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_str* set1_arg = arg_strn(NULL, NULL, "SET1", 0, 1, "character set 1");
    struct arg_str* set2_arg = arg_strn(NULL, NULL, "SET2", 0, 1, "character set 2");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {complement_opt, delete_opt, squeeze_opt, truncate_opt, help_opt, set1_arg, set2_arg, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... SET1 [SET2]\n", argv[0]);
        printf("Translate, squeeze, or delete characters.\n");
        printf("\n");
        printf("  -c, --complement    use complement of SET1\n");
        printf("  -d, --delete        delete characters in SET1, do not translate\n");
        printf("  -s, --squeeze-repeats  replace each input sequence of a repeated\n");
        printf("                        character with a single occurrence\n");
        printf("  -t, --truncate-set1  first truncate SET1 to length of SET2\n");
        printf("  -h, --help          display this help and exit\n");
        printf("\n");
        printf("SETs are specified as strings of characters. Most represent themselves.\n");
        printf("Interpreted sequences:\n");
        printf("  \\NNN            character with octal value NNN (1 to 3 digits)\n");
        printf("  \\\\              backslash\n");
        printf("  \\a              alert (BEL)\n");
        printf("  \\b              backspace\n");
        printf("  \\f              form feed\n");
        printf("  \\n              new line\n");
        printf("  \\r              carriage return\n");
        printf("  \\t              horizontal tab\n");
        printf("  \\v              vertical tab\n");
        printf("  CHAR1-CHAR2     all characters from CHAR1 to CHAR2 in ascending order\n");
        printf("  [CHAR*N]        CHAR repeated N times\n");
        printf("  [CHAR*]         CHAR repeated to fill length of SET1\n");
        printf("  [:class:]       all characters in the class\n");
        printf("\n");
        printf("Character classes:\n");
        printf("  alnum   alpha    blank    cntrl    digit    graph    lower\n");
        printf("  print   punct    space    upper    xdigit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    bool cflag = (complement_opt->count > 0);
    bool dflag = (delete_opt->count > 0);
    bool sflag = (squeeze_opt->count > 0);
    bool tflag = (truncate_opt->count > 0);

    if (set1_arg->count == 0) {
        fprintf(stderr, "tr: missing operand\n");
        fprintf(stderr, "Try 'tr --help' for more information.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    const char* set1_str = set1_arg->sval[0];
    const char* set2_str = (set2_arg->count > 0) ? set2_arg->sval[0] : nullptr;

    if (dflag && tflag) {
        tflag = false;
    }

    SetParse sp1;
    if (parse_set(set1_str, sp1) == -1) {
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    SetParse sp2;
    if (set2_str) {
        if (parse_set(set2_str, sp2) == -1) {
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    if (sp1.chars.empty() && !cflag) {
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    std::vector<unsigned char> eff_set1 = sp1.chars;
    if (cflag) {
        eff_set1 = make_complement(sp1.chars);
    }

    std::vector<unsigned char> set2;
    if (set2_str) {
        set2 = sp2.chars;
        if (sp2.has_fill) {
            while (set2.size() < eff_set1.size())
                set2.push_back(sp2.fill_char);
        }
    }

    if (tflag && dflag) tflag = false;

    if (tflag && !set2.empty()) {
        if (eff_set1.size() > set2.size())
            eff_set1.resize(set2.size());
    }

    if (set2_str && set2.empty()) {
        if (!dflag) {
            fprintf(stderr, "tr: empty SET2\n");
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    if (dflag && sflag && set2.empty()) {
        set2 = eff_set1;
    }

    unsigned char tbl[256];

    if (dflag && !sflag) {
        tr_delete(stdin, eff_set1, false, set2);
    } else if (dflag && sflag) {
        tr_delete(stdin, eff_set1, true, set2);
    } else if (sflag && !set2_str) {
        tr_squeeze(stdin, eff_set1);
    } else if (sflag && set2_str) {
        if (set2.empty()) {
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        build_translate(tbl, eff_set1, set2);
        tr_translate_squeeze(stdin, tbl, set2);
    } else {
        if (set2.empty()) {
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
        build_translate(tbl, eff_set1, set2);
        tr_translate(stdin, tbl);
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
