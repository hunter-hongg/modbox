#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <string>

#include <argtable3.h>

#include "commands/less.hpp"
#include "commands/arg_util.hpp"
#include "commands/pager.hpp"
#include "commands/cmd_error.hpp"
#include "commands/version_util.hpp"
#include "commands/command_macros.hpp"

static std::vector<std::string> read_stream_to_lines(FILE* fp) {
    std::vector<std::string> lines;
    char* buf = nullptr;
    size_t cap = 0;
    ssize_t n;
    while ((n = getline(&buf, &cap, fp)) != -1) {
        if (n > 0 && buf[n - 1] == '\n') {
            buf[n - 1] = '\0';
        }
        lines.emplace_back(buf);
    }
    free(buf);
    return lines;
}

static bool fill_view_from_path(const char* path, PagerView& view, bool& had_error) {
    if (strcmp(path, "-") == 0) {
        view.name = "(stdin)";
        view.lines = read_stream_to_lines(stdin);
        return true;
    }
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        cmd_perror("less", path);
        had_error = true;
        return false;
    }
    view.name = path;
    view.lines = read_stream_to_lines(fp);
    (void)fclose(fp);
    return true;
}

int less_command(int argc, char** argv) {
    const char* prog = argv[0];

    // First pass: pull out "+cmd" startup directives (GNU less syntax),
    // since argtable3 cannot express a leading '+'.
    PagerOptions cfg;
    std::vector<char*> filtered;
    filtered.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (a[0] == '+' && a[1] != '\0') {
            const char* body = a + 1;
            if (strcmp(body, "G") == 0) {
                cfg.start_line = -1;
            } else if (body[0] == '/') {
                cfg.pattern = body + 1;
            } else {
                char* endp = nullptr;
                long ln = strtol(body, &endp, 10);
                if (endp != body && *endp == '\0' && ln >= 1) {
                    cfg.start_line = ln;
                } else {
                    cmd_error(prog, "invalid line number: '%s'", body);
                    return 2;
                }
            }
            continue;
        }
        filtered.push_back(argv[i]);
    }
    char** fargv = filtered.data();
    int fargc = (int)filtered.size();

    struct arg_lit* line_numbers_opt = arg_lit0("N", "LINE-NUMBERS", "display line numbers");
    struct arg_lit* ignore_case_opt = arg_lit0("i", NULL, "ignore case in searches");
    struct arg_lit* ignore_case_force_opt = arg_lit0("I", NULL, "force case-insensitive search");
    struct arg_lit* long_prompt_opt = arg_lit0("M", NULL, "verbose prompt");
    struct arg_lit* quit_at_eof_opt = arg_lit0("E", "QUIT-AT-EOF", "quit at end of file");
    struct arg_lit* quit_one_opt = arg_lit0("F", "quit-if-one-screen", "quit if one screen");
    struct arg_lit* no_init_opt = arg_lit0("X", NULL, "no screen init");
    struct arg_lit* chop_opt = arg_lit0("S", "chop-long-lines", "chop long lines");
    struct arg_str* pattern_opt = arg_str0("p", "pattern", "PATTERN", "start at PATTERN");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 1000, "file to read");
    struct arg_end* end = arg_end(20);

    ArgTable at({line_numbers_opt, ignore_case_opt, ignore_case_force_opt,
        long_prompt_opt, quit_at_eof_opt, quit_one_opt, no_init_opt, chop_opt,
        pattern_opt, help_opt, version_opt, file_arg, end});

    int nerrors = at.parse(fargc, fargv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", prog);
        printf("View FILE(s) or standard input one screen at a time.\n");
        printf("\n");
        printf("  -N, --LINE-NUMBERS      display line numbers\n");
        printf("  -i                       ignore case in searches\n");
        printf("  -I                       force case-insensitive search\n");
        printf("  -M                       verbose (long) prompt\n");
        printf("  -E, --QUIT-AT-EOF        quit at end of file\n");
        printf("  -F, --quit-if-one-screen quit if one screen\n");
        printf("  -X                       no screen init\n");
        printf("  -S, --chop-long-lines    chop long lines\n");
        printf("  -p, --pattern=PATTERN    start at PATTERN\n");
        printf("  -h, --help               display this help and exit\n");
        printf("      --version            output version information and exit\n");
        printf("\n");
        printf("  +G        start at end of file\n");
        printf("  +N        start at line N\n");
        printf("  +/PATTERN  start at PATTERN (like -p)\n");
        printf("  q         quit; SPACE/b forward; b back; j/k line; g/G top/bottom\n");
        printf("            / search; n/N next/prev match\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("less");
        return 0;
    }

    if (nerrors > 0) {
        at.print_errors(end, prog);
        (void)fprintf(stderr, "Try '%s --help' for more information.\n", prog);
        return 2;
    }

    cfg.line_numbers = (line_numbers_opt->count > 0);
    cfg.ignore_case = (ignore_case_opt->count > 0) || (ignore_case_force_opt->count > 0);
    cfg.long_prompt = (long_prompt_opt->count > 0);
    cfg.quit_at_eof = (quit_at_eof_opt->count > 0);
    cfg.quit_if_one_screen = (quit_one_opt->count > 0);
    cfg.no_init = (no_init_opt->count > 0);
    if (pattern_opt->count > 0) {
        cfg.pattern = pattern_opt->sval[0];
    }

    std::vector<PagerView> views;
    bool had_error = false;

    if (file_arg->count == 0) {
        PagerView v;
        v.name = "(stdin)";
        v.lines = read_stream_to_lines(stdin);
        views.push_back(std::move(v));
    } else {
        for (int i = 0; i < file_arg->count; i++) {
            PagerView v;
            if (fill_view_from_path(file_arg->filename[i], v, had_error)) {
                views.push_back(std::move(v));
            }
        }
    }

    pager_run(views, &cfg);
    return had_error ? 1 : 0;
}

REGISTER_COMMAND("less", less_command, "View files one screen at a time");
