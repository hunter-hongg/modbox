#include <cstdio>
#include <cstring>
#include <cctype>

#include "commands/cat/highlight.hpp"

#define ANSI_RESET   "\033[0m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_GRAY    "\033[90m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_MAGENTA "\033[35m"
#define WORD_BUF_LEN 256

static const char* c_keywords[] = {
    "auto", "break", "case", "const", "continue", "default", "do",
    "else", "enum", "extern", "for", "goto", "if", "register",
    "return", "signed", "sizeof", "static", "struct", "switch",
    "typedef", "union", "unsigned", "volatile", "while", NULL
};

static const char* c_types[] = {
    "int", "char", "float", "double", "void", "long", "short",
    "size_t", "ssize_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "int8_t", "int16_t", "int32_t", "int64_t", "FILE", "bool", NULL
};

static const char* py_keywords[] = {
    "and", "as", "assert", "async", "await", "break", "class",
    "continue", "def", "del", "elif", "else", "except", "finally",
    "for", "from", "global", "if", "import", "in", "is", "lambda",
    "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield", "self", "True", "False", "None", NULL
};

static const char* rs_keywords[] = {
    "as", "break", "const", "continue", "crate", "else", "enum",
    "extern", "false", "fn", "for", "if", "impl", "in", "let",
    "loop", "match", "mod", "move", "mut", "pub", "ref", "return",
    "self", "static", "struct", "super", "trait", "true", "type",
    "unsafe", "use", "where", "while", "async", "await", "dyn", NULL
};

static const char* rs_types[] = {
    "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64",
    "f32", "f64", "bool", "char", "str", "String", "Vec", "Option",
    "Result", "Box", "Rc", "Arc", "HashMap", "HashSet", NULL
};

static const char* go_keywords[] = {
    "break", "case", "chan", "const", "continue", "default", "defer",
    "else", "fallthrough", "for", "func", "go", "goto", "if",
    "import", "interface", "map", "package", "range", "return",
    "select", "struct", "switch", "type", "var", "nil", "true",
    "false", NULL
};

static const char* go_types[] = {
    "bool", "byte", "complex64", "complex128", "error", "float32",
    "float64", "int", "int8", "int16", "int32", "int64",
    "rune", "string", "uint", "uint8", "uint16", "uint32", "uint64",
    "uintptr", NULL
};

static const char* js_keywords[] = {
    "break", "case", "catch", "class", "const", "continue", "debugger",
    "default", "delete", "do", "else", "export", "extends", "finally",
    "for", "function", "if", "import", "in", "instanceof", "let",
    "new", "of", "return", "static", "super", "switch", "this",
    "throw", "try", "typeof", "var", "void", "while", "with",
    "yield", "async", "await", "null", "undefined", "true", "false",
    NULL
};

static const char* sh_keywords[] = {
    "if", "then", "else", "elif", "fi", "case", "esac", "for",
    "while", "do", "done", "in", "function", "return", "exit",
    "break", "continue", "until", "select", "time", NULL
};

static int is_c_comment_start(const char* s) {
    if (!s || !s[0] || !s[1]) { return 0; }
    return s[0] == '/' && s[1] == '*';
}

static int is_c_comment_end(const char* s) {
    if (!s || !s[0] || !s[1]) { return 0; }
    return s[0] == '*' && s[1] == '/';
}

static int is_cpp_comment(const char* s) {
    if (!s || !s[0] || !s[1]) { return 0; }
    return s[0] == '/' && s[1] == '/';
}

static int is_shell_comment(const char* s) {
    return s[0] == '#';
}

typedef struct {
    const char* ext;
    const char** keywords;
    const char** types;
    int line_comment;
    int block_comment;
    int has_preprocessor;
    int shell_style_comment;
    int has_hash_comment;
} LangDef;

static int str_in_list(const char* word, const char** list) {
    for (int i = 0; list[i]; i++) {
        if (strcmp(word, list[i]) == 0) { return 1; }
    }
    return 0;
}

static const LangDef* detect_language(const char* ext) {
    static const LangDef langs[] = {
        {"c",     c_keywords, c_types,     1, 1, 1, 0, 0},
        {"h",     c_keywords, c_types,     1, 1, 1, 0, 0},
        {"py",    py_keywords, NULL,         0, 0, 0, 0, 1},
        {"rs",    rs_keywords, rs_types,    1, 0, 0, 0, 0},
        {"go",    go_keywords, go_types,    1, 1, 0, 0, 0},
        {"js",    js_keywords, NULL,         1, 1, 0, 0, 0},
        {"ts",    js_keywords, NULL,         1, 1, 0, 0, 0},
        {"json",  NULL,       NULL,          0, 0, 0, 0, 0},
        {"yaml",  NULL,       NULL,          0, 0, 0, 0, 1},
        {"yml",   NULL,       NULL,          0, 0, 0, 0, 1},
        {"toml",  NULL,       NULL,          0, 0, 0, 0, 1},
        {"md",    NULL,       NULL,          0, 0, 0, 0, 0},
        {"sh",    sh_keywords, NULL,         0, 0, 0, 1, 0},
        {"bash",  sh_keywords, NULL,         0, 0, 0, 1, 0},
        {NULL,    NULL,       NULL,          0, 0, 0, 0, 0},
    };
    for (int i = 0; langs[i].ext; i++) {
        if (strcmp(ext, langs[i].ext) == 0) { return &langs[i]; }
    }
    return NULL;
}

const char* get_file_extension(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot || dot == path) { return ""; }
    return dot + 1;
}

static void print_token(FILE* out, const char* token, int len, const char* color) {
    if (color) {
        fprintf(out, "%s", color);
    }
    fwrite(token, 1, (size_t)len, out);
    if (color) {
        fprintf(out, "%s", ANSI_RESET);
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void print_highlighted(const char* line, const char* ext, FILE* out) {
    const LangDef* lang = detect_language(ext);
    if (!lang) {
        fputs(line, out);
        return;
    }

    size_t len = strlen(line);
    int has_newline = (len > 0 && line[len - 1] == '\n');
    size_t content_len = has_newline ? len - 1 : len;

    int in_block_comment = 0;
    int in_string = 0;
    char string_delim = 0;
    int i = 0;

    while (i < (int)content_len) {
        if (in_block_comment) {
            if (i + 1 < (int)content_len && is_c_comment_end(&line[i])) {
                print_token(out, "*/", 2, ANSI_GRAY);
                i += 2;
                in_block_comment = 0;
            } else {
                print_token(out, &line[i], 1, ANSI_GRAY);
                i++;
            }
            continue;
        }

        if (in_string) {
            if (line[i] == '\\' && i + 1 < (int)content_len) {
                print_token(out, &line[i], 2, ANSI_GREEN);
                i += 2;
            } else if (line[i] == string_delim) {
                print_token(out, &line[i], 1, ANSI_GREEN);
                i++;
                in_string = 0;
            } else {
                print_token(out, &line[i], 1, ANSI_GREEN);
                i++;
            }
            continue;
        }

        if (!in_string && !in_block_comment) {
            if (lang->block_comment && i + 1 < (int)content_len && is_c_comment_start(&line[i])) {
                print_token(out, "/*", 2, ANSI_GRAY);
                i += 2;
                in_block_comment = 1;
                continue;
            }

            if (lang->line_comment && i + 1 < (int)content_len && is_cpp_comment(&line[i])) {
                print_token(out, &line[i], (int)content_len - i, ANSI_GRAY);
                i = (int)content_len;
                continue;
            }

            if (lang->shell_style_comment && line[i] == '#') {
                print_token(out, &line[i], (int)content_len - i, ANSI_GRAY);
                i = (int)content_len;
                continue;
            }

            if (lang->has_hash_comment && i == 0 && line[i] == '#') {
                print_token(out, &line[i], (int)content_len - i, ANSI_GRAY);
                i = (int)content_len;
                continue;
            }

            if (line[i] == '"' || line[i] == '\'' || line[i] == '`') {
                in_string = 1;
                string_delim = line[i];
                print_token(out, &line[i], 1, ANSI_GREEN);
                i++;
                continue;
            }

            if (lang->has_preprocessor && line[i] == '#' && i == 0) {
                int j = i;
                while (j < (int)content_len && line[j] != '\n' && line[j] != '\r') { j++; }
                print_token(out, &line[i], j - i, ANSI_MAGENTA);
                i = j;
                continue;
            }

            if (isdigit((unsigned char)line[i]) || (line[i] == '-' && i + 1 < (int)content_len && isdigit((unsigned char)line[i + 1]))) {
                int j = i;
                if (line[j] == '-') { j++; }
                while (j < (int)content_len && (isdigit((unsigned char)line[j]) || line[j] == '.' || line[j] == 'x' || line[j] == 'X' || line[j] == 'a' || line[j] == 'b' || line[j] == 'c' || line[j] == 'd' || line[j] == 'e' || line[j] == 'f' || line[j] == 'A' || line[j] == 'B' || line[j] == 'C' || line[j] == 'D' || line[j] == 'E' || line[j] == 'F')) { j++; }
                print_token(out, &line[i], j - i, ANSI_YELLOW);
                i = j;
                continue;
            }

            if (isalpha((unsigned char)line[i]) || line[i] == '_') {
                int j = i;
                while (j < (int)content_len && (isalnum((unsigned char)line[j]) || line[j] == '_')) { j++; }

                char word[WORD_BUF_LEN];
                int word_len = j - i;
                if (word_len < WORD_BUF_LEN) {
                    strncpy(word, &line[i], (size_t)word_len);
                    word[word_len] = '\0';

                    if (lang->keywords && str_in_list(word, lang->keywords)) {
                        print_token(out, &line[i], word_len, ANSI_BLUE);
                    } else if (lang->types && str_in_list(word, lang->types)) {
                        print_token(out, &line[i], word_len, ANSI_CYAN);
                    } else {
                        fwrite(&line[i], 1, (size_t)word_len, out);
                    }
                } else {
                    fwrite(&line[i], 1, (size_t)word_len, out);
                }
                i = j;
                continue;
            }
        }

        fputc(line[i], out);
        i++;
    }

    if (has_newline) { fputc('\n', out); }
}
