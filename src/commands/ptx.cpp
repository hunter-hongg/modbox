#include <cstdio>
#include <cstring>
#include <cctype>
#include <vector>
#include <string>
#include <algorithm>
#include <argtable3.h>

#include "commands/ptx.hpp"

#define PTX_MAX_LINE 4096
#define PTX_MAX_WORD 256

/* Structure to hold a keyword and its context */
struct KeywordEntry {
    std::string keyword;
    std::string left_context;
    std::string right_context;
    std::string reference;
    int line_number;
    
    bool operator<(const KeywordEntry& other) const {
        return keyword < other.keyword;
    }
};

/* Check if a character is a word constituent */
static int is_word_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '\'';
}

/* Extract words from a line and create keyword entries */
static void extract_keywords(const char* line, int line_num, const char* filename,
                             std::vector<KeywordEntry>* entries, const PtxOptions* opts) {
    char line_copy[PTX_MAX_LINE];
    strncpy(line_copy, line, PTX_MAX_LINE - 1);
    line_copy[PTX_MAX_LINE - 1] = '\0';
    
    char* word_start = nullptr;
    int in_word = 0;
    
    for (char* p = line_copy; *p; p++) {
        if (is_word_char(*p)) {
            if (!in_word) {
                word_start = p;
                in_word = 1;
            }
        } else {
            if (in_word) {
                *p = '\0';  /* Terminate the word */
                
                if (strlen(word_start) > 0) {
                    KeywordEntry entry;
                    entry.keyword = word_start;
                    entry.left_context = std::string(line_copy, word_start - line_copy);
                    entry.right_context = std::string(p + 1);
                    entry.line_number = line_num;
                    
                    if (opts->auto_reference) {
                        if (filename && strcmp(filename, "-") != 0) {
                            entry.reference = std::string(filename) + ":" + std::to_string(line_num);
                        } else {
                            entry.reference = std::to_string(line_num);
                        }
                    }
                    
                    entries->push_back(entry);
                }
                
                in_word = 0;
            }
        }
    }
    
    /* Handle word at end of line */
    if (in_word && word_start) {
        if (strlen(word_start) > 0) {
            KeywordEntry entry;
            entry.keyword = word_start;
            entry.left_context = std::string(line_copy, word_start - line_copy);
            entry.right_context = "";
            entry.line_number = line_num;
            
            if (opts->auto_reference) {
                if (filename && strcmp(filename, "-") != 0) {
                    entry.reference = std::string(filename) + ":" + std::to_string(line_num);
                } else {
                    entry.reference = std::to_string(line_num);
                }
            }
            
            entries->push_back(entry);
        }
    }
}

/* Format and output a keyword entry */
static void output_entry(const KeywordEntry& entry, const PtxOptions* opts, FILE* out_fp) {
    int context_width = (opts->width - opts->gap_size) / 2;
    
    std::string left = entry.left_context;
    std::string right = entry.right_context;
    std::string keyword = entry.keyword;
    
    /* Truncate contexts if needed */
    if ((int)left.length() > context_width) {
        left = left.substr(left.length() - context_width);
    }
    if ((int)right.length() > context_width) {
        right = right.substr(0, context_width);
    }
    
    /* Format output */
    if (opts->right_side_refs && !entry.reference.empty()) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(out_fp, "%s ", entry.reference.c_str());
    }
    
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(out_fp, "%-*s", context_width, left.c_str());
    
    /* Print gap */
    for (int i = 0; i < opts->gap_size; i++) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(out_fp, " ");
    }
    
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(out_fp, "%s", keyword.c_str());
    
    /* Print gap */
    for (int i = 0; i < opts->gap_size; i++) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(out_fp, " ");
    }
    
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(out_fp, "%s", right.c_str());
    
    if (!opts->right_side_refs && !entry.reference.empty()) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(out_fp, "  (%s)", entry.reference.c_str());
    }
    
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(out_fp, "\n");
}

/* Process a single file */
static void process_file(FILE* fp, const char* filename, std::vector<KeywordEntry>* entries, 
                        const PtxOptions* opts) {
    char line[PTX_MAX_LINE];
    int line_num = 0;
    
    while (fgets(line, PTX_MAX_LINE, fp)) {
        line_num++;
        
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        extract_keywords(line, line_num, filename, entries, opts);
    }
}

/* Main command implementation */
void ptx_command(int argc, char** argv) {
    PtxOptions opts = {0};
    
    struct arg_lit* auto_ref_opt = arg_lit0("A", "auto-reference", "generate automatic references");
    struct arg_lit* right_ref_opt = arg_lit0("R", "right-side-refs", "put references on right side");
    struct arg_lit* traditional_opt = arg_lit0("G", "traditional", "traditional mode");
    struct arg_lit* typeset_opt = arg_lit0("t", "typeset-mode", "typeset mode");
    struct arg_lit* refs_opt = arg_lit0("r", "references", "use input references");
    struct arg_int* width_opt = arg_int0("w", "width", "N", "output width");
    struct arg_int* gap_opt = arg_int0("g", "gap-size", "N", "gap size");
    struct arg_str* sentence_opt = arg_str0("S", "sentence-regexp", "REGEXP", "sentence regexp");
    struct arg_str* break_opt = arg_str0("b", "break-file", "FILE", "break file");
    struct arg_str* ignore_opt = arg_str0("i", "ignore-file", "FILE", "ignore file");
    struct arg_str* only_opt = arg_str0("o", "only-file", "FILE", "only file");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_args = arg_filen(NULL, NULL, "FILE", 0, 100, "input files");
    struct arg_end* end = arg_end(20);
    
    void* argtable[] = {
        auto_ref_opt, right_ref_opt, traditional_opt, typeset_opt, refs_opt,
        width_opt, gap_opt, sentence_opt, break_opt, ignore_opt, only_opt,
        help_opt, file_args, end
    };
    
    int nerrors = arg_parse(argc, argv, argtable);
    
    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Generate a permuted index (KWIC index) for each FILE.\n");
        printf("\n");
        printf("Options:\n");
        printf("  -A, --auto-reference   generate automatic references (filename:lineno)\n");
        printf("  -R, --right-side-refs  put references on right side of output\n");
        printf("  -G, --traditional      traditional mode (System V compatibility)\n");
        printf("  -t, --typeset-mode     typeset mode\n");
        printf("  -r, --references       use input references\n");
        printf("  -w, --width=N          output width (default 72)\n");
        printf("  -g, --gap-size=N       gap size between fields (default 3)\n");
        printf("  -S, --sentence-regexp=REGEXP  sentence regular expression\n");
        printf("  -b, --break-file=FILE  break file\n");
        printf("  -i, --ignore-file=FILE ignore file\n");
        printf("  -o, --only-file=FILE   only file\n");
        printf("  -h, --help            display this help and exit\n");
        printf("\n");
        printf("With no FILE, read standard input.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }
    
    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }
    
    opts.auto_reference = (auto_ref_opt->count > 0);
    opts.right_side_refs = (right_ref_opt->count > 0);
    opts.traditional = (traditional_opt->count > 0);
    opts.typeset_mode = (typeset_opt->count > 0);
    opts.references = (refs_opt->count > 0);
    opts.width = (width_opt->count > 0) ? width_opt->ival[0] : 72;
    opts.gap_size = (gap_opt->count > 0) ? gap_opt->ival[0] : 3;
    opts.sentence_regexp = (sentence_opt->count > 0) ? sentence_opt->sval[0] : nullptr;
    opts.break_file = (break_opt->count > 0) ? break_opt->sval[0] : nullptr;
    opts.ignore_file = (ignore_opt->count > 0) ? ignore_opt->sval[0] : nullptr;
    opts.only_file = (only_opt->count > 0) ? only_opt->sval[0] : nullptr;
    
    std::vector<KeywordEntry> entries;
    
    /* Process input files */
    if (file_args->count == 0) {
        process_file(stdin, "-", &entries, &opts);
    } else {
        for (int i = 0; i < file_args->count; i++) {
            const char* fname = file_args->filename[i];
            if (strcmp(fname, "-") == 0) {
                process_file(stdin, "-", &entries, &opts);
            } else {
                FILE* fp = fopen(fname, "r");
                if (fp == nullptr) {
                    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
                    (void)fprintf(stderr, "ptx: %s: No such file or directory\n", fname);
                    continue;
                }
                process_file(fp, fname, &entries, &opts);
                fclose(fp);
            }
        }
    }
    
    /* Sort entries by keyword */
    std::sort(entries.begin(), entries.end());
    
    /* Output entries */
    for (const auto& entry : entries) {
        output_entry(entry, &opts, stdout);
    }
    
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
