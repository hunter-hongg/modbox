#include <argtable3.h>
#include <dirent.h>
#include <glib.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "commands/ls.h"

/* Named constants for printable ASCII range used by print_escaped_filename */
#define ASCII_SPACE 0x20
#define ASCII_TILDE 0x7e

typedef enum { COLOR_NEVER, COLOR_ALWAYS, COLOR_AUTO } color_mode_t;

static int should_color(color_mode_t mode) {
    switch (mode) {
    case COLOR_ALWAYS:
        return 1;
    case COLOR_AUTO:
        return isatty(STDOUT_FILENO);
    default:
        return 0;
    }
}

static const char* get_file_color(struct stat* st) {
    if (S_ISDIR(st->st_mode)) {
        return "01;34";
    }
    if (S_ISLNK(st->st_mode)) {
        return "01;36";
    }
    if (st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
        return "01;32";
    }
    return NULL;
}

static void print_escaped_filename(const char* filename) {
    for (const unsigned char* p = (const unsigned char*)filename; *p != '\0'; p++) {
        // Tab is printed as-is; printable ASCII is printed as-is
        if (*p == '\t' || (*p >= ASCII_SPACE && *p <= ASCII_TILDE)) {
            putchar((int)*p);
        } else {
            // All other bytes are escaped as \ooo (3-digit octal)
            printf("\\%03o", (int)*p);
        }
    }
}

static void print_file_info(const char* filename, int show_details,
                            int show_author, int escape_mode,
                            color_mode_t color_mode, unsigned long block_size,
                            char size_suffix) {
    int use_color = should_color(color_mode);

    if (!show_details && !use_color) {
        if (escape_mode) {
            print_escaped_filename(filename);
        } else {
            printf("%s", filename);
        }
        printf("  ");
        return;
    }

    struct stat st;
    if (lstat(filename, &st) == -1) {
        if (!show_details) {
            if (escape_mode) {
                print_escaped_filename(filename);
            } else {
                printf("%s", filename);
            }
            printf("  ");
        }
        return;
    }

    const char* color_code = NULL;
    if (use_color) {
        color_code = get_file_color(&st);
        use_color = (color_code != NULL);
    }

    if (!show_details) {
        if (use_color) {
            printf("\033[%sm", color_code);
        }
        if (escape_mode) {
            print_escaped_filename(filename);
        } else {
            printf("%s", filename);
        }
        if (use_color) {
            printf("\033[0m");
        }
        printf("  ");
        return;
    }

    printf("%c%c%c%c%c%c%c%c%c ", S_ISDIR(st.st_mode) ? 'd' : '-',
           st.st_mode & S_IRUSR ? 'r' : '-', st.st_mode & S_IWUSR ? 'w' : '-',
           st.st_mode & S_IXUSR ? 'x' : '-', st.st_mode & S_IRGRP ? 'r' : '-',
           st.st_mode & S_IWGRP ? 'w' : '-', st.st_mode & S_IXGRP ? 'x' : '-',
           st.st_mode & S_IROTH ? 'r' : '-', st.st_mode & S_IWOTH ? 'w' : '-');

    // NOLINTNEXTLINE(bugprone-narrowing-conversions)
    printf("%4ld ", (long)st.st_nlink);

    struct passwd* pwd = getpwuid(st.st_uid);
    struct group* grp = getgrgid(st.st_gid);
    if (show_author) {
        printf("%s %s %s ", pwd ? pwd->pw_name : "-", pwd ? pwd->pw_name : "-",
               grp ? grp->gr_name : "-");
    } else {
        printf("%s %s ", pwd ? pwd->pw_name : "-", grp ? grp->gr_name : "-");
    }

    unsigned long display_size =
        block_size > 0
            ? (st.st_size + ((off_t)block_size / 2)) / (off_t)block_size
            : (unsigned long)st.st_size;
    if (size_suffix != '\0') {
        printf("%7lu%c ", display_size, size_suffix);
    } else {
        printf("%8lu ", display_size);
    }

    char time_buf[20];
    // NOLINTNEXTLINE(bugprone-unused-return-value)
    (void)strftime(time_buf, sizeof(time_buf), "%b %d %H:%M",
                   localtime(&st.st_mtime));
    printf("%s ", time_buf);

    if (use_color) {
        printf("\033[%sm", color_code);
    }
    if (escape_mode) {
        print_escaped_filename(filename);
    } else {
        printf("%s", filename);
    }
    if (use_color) {
        printf("\033[0m");
    }
    printf("\n");
}

static unsigned long parse_block_size(const char* str) {
    char* endptr;
    unsigned long val = strtoul(str, &endptr, 10);

    if (endptr == str) {
        // No digits consumed; treat the whole string as a bare suffix (e.g. "K")
        val = 1;
    } else if (val == 0) {
        // NOLINTNEXTLINE(bugprone-unused-return-value,
        // clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr,
                      "ls: --block-size argument must be a positive integer\n");
        return 1;
    }

    // Apply any trailing suffix
    while (*endptr != '\0') {
        switch (*endptr) {
        case 'K':
        case 'k':
            val *= 1024UL;
            break;
        case 'M':
        case 'm':
            val *= 1024UL * 1024;
            break;
        case 'G':
        case 'g':
            val *= 1024UL * 1024 * 1024;
            break;
        case 'T':
        case 't':
            val *= 1024UL * 1024 * 1024 * 1024;
            break;
        case 'P':
        case 'p':
            val *= 1024UL * 1024 * 1024 * 1024 * 1024;
            break;
        case 'E':
        case 'e':
            val *= 1024UL * 1024 * 1024 * 1024 * 1024 * 1024;
            break;
        default:
            // NOLINTNEXTLINE(bugprone-unused-return-value,
            // clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(
                stderr,
                "ls: unrecognized block size suffix '%c' in '%s'\nValid suffixes: "
                "K, M, G, T, P, E\n",
                *endptr, str);
            return 1;
        }
        endptr++;
    }

    return val > 0 ? val : 1;
}

/** Get the terminal width in columns, falling back to the COLUMNS env var,
 *  then to a default of 80. */
static int get_terminal_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return (int)ws.ws_col;
    }
    const char* cols = getenv("COLUMNS");
    if (cols != NULL) {
        char* endptr;
        long val = strtol(cols, &endptr, 10);
        if (endptr != cols && val > 0 && val < 10000) {
            return (int)val;
        }
    }
    return 80;
}

/** Compute the display width of a filename when printed via
 *  print_escaped_filename() — printable chars count as 1, escaped
 *  non-printable bytes count as 4 ( "\ooo" ). */
static int escaped_display_width(const char* s) {
    int width = 0;
    for (const unsigned char* p = (const unsigned char*)s; *p != '\0'; p++) {
        if (*p == '\t' || (*p >= ASCII_SPACE && *p <= ASCII_TILDE)) {
            width++;
        } else {
            width += 4;
        }
    }
    return width;
}

/** Compute the visual width of a plain (non-escaped) filename. */
static int plain_display_width(const char* s) {
    return (int)strlen(s);
}

/** Print a list of filenames in vertically-sorted columns, like GNU ls -C.
 *
 *  The list is assumed to already be sorted.  Terminal width is detected
 *  via ioctl / $COLUMNS / default 80.
 */
static void print_columns(GList* files, int escape_mode, color_mode_t color_mode) {
    int use_color = should_color(color_mode);
    int term_width = get_terminal_width();
    int count = g_list_length(files);

    if (count == 0) {
        return;
    }

    /* Find the maximum display width among filenames */
    int max_width = 0;
    for (GList* iter = files; iter != NULL; iter = iter->next) {
        int w = escape_mode ? escaped_display_width((const char*)iter->data)
                            : plain_display_width((const char*)iter->data);
        if (w > max_width) {
            max_width = w;
        }
    }

    const int gap = 2;
    int col_width = max_width + gap;
    if (col_width > term_width) {
        col_width = term_width;
    }
    int num_cols = term_width / col_width;
    if (num_cols < 1) {
        num_cols = 1;
    }
    if (num_cols > count) {
        num_cols = count;
    }
    int num_rows = (count + num_cols - 1) / num_cols;

    /* Build an array of filename pointers for indexed access */
    const char** names = (const char**)g_malloc(sizeof(char*) * (size_t)count);
    {
        int idx = 0;
        for (GList* iter = files; iter != NULL; iter = iter->next) {
            names[idx++] = (const char*)iter->data;
        }
    }

    /* Print in vertical-sorted order (down columns) */
    for (int row = 0; row < num_rows; row++) {
        for (int col = 0; col < num_cols; col++) {
            int index = col * num_rows + row;
            if (index >= count) {
                continue;
            }

            const char* name = names[index];
            int name_display_w = escape_mode ? escaped_display_width(name)
                                             : plain_display_width(name);

            /* Determine color if needed */
            const char* color_code = NULL;
            if (use_color) {
                struct stat st;
                if (lstat(name, &st) == 0) {
                    color_code = get_file_color(&st);
                }
            }

            if (color_code != NULL) {
                printf("\033[%sm", color_code);
            }
            if (escape_mode) {
                print_escaped_filename(name);
            } else {
                printf("%s", name);
            }
            if (color_code != NULL) {
                printf("\033[0m");
            }

            /* Pad to column width (not needed for the last column) */
            if (col < num_cols - 1) {
                int padding = col_width - name_display_w;
                if (padding < 0) {
                    padding = 1;
                }
                for (int p = 0; p < padding; p++) {
                    putchar(' ');
                }
            }
        }
        printf("\n");
    }

    g_free(names);
}

/**
 * Sort a GList of filename strings, print them (either as columns or
 * individually via print_file_info), then free all associated memory.
 *
 * The caller should not reference @a files (or its elements) after this call
 * — ownership is transferred.
 */
static void sort_and_output_files(GList* files, int show_details,
                                  int show_author, int escape_mode,
                                  color_mode_t color_mode,
                                  unsigned long block_size, char size_suffix,
                                  int use_columns) {
    if (files == NULL) {
        return;
    }

    files = g_list_sort(files, (GCompareFunc)strcmp);

    if (use_columns) {
        print_columns(files, escape_mode, color_mode);
    } else {
        for (GList* iter = files; iter != NULL; iter = iter->next) {
            print_file_info((const char*)iter->data, show_details, show_author,
                            escape_mode, color_mode, block_size, size_suffix);
        }
        if (!show_details) {
            printf("\n");
        }
    }

    for (GList* iter = files; iter != NULL; iter = iter->next) {
        g_free(iter->data);
    }
    g_list_free(files);
}

void ls_command(gint argc, gchar** argv) {
    int show_all = 0;
    int show_almost_all = 0;
    int show_details = 0;
    int show_author = 0;
    int escape_mode = 0;
    int ignore_backups = 0;
    int list_dir_contents = 1; /* 1 = list contents of dirs; 0 = list dirs themselves */
    color_mode_t color_mode = COLOR_NEVER;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--color") == 0) {
            argv[i] = "--color=always";
            break;
        }
    }

    struct arg_lit* all_opt =
        arg_lit0("a", "all", "do not ignore entries starting with .");
    struct arg_lit* almost_all_opt =
        arg_lit0("A", "almost-all", "do not list implied . and ..");
    struct arg_lit* long_opt = arg_lit0("l", "long", "use a long listing format");
    struct arg_lit* author_opt =
        arg_lit0(NULL, "author", "with -l, print the author of each file");
    struct arg_lit* escape_opt =
        arg_lit0("b", "escape", "print octal escapes for non-graphic characters");
    struct arg_str* color_opt =
        arg_str0(NULL, "color", "WHEN",
                 "colorize the output; WHEN can be 'always', 'auto', or 'never'");
    struct arg_lit* ignore_backups_opt =
        arg_lit0("B", "ignore-backups",
                 "do not list entries ending with ~");
    struct arg_str* block_size_opt =
        arg_str0(NULL, "block-size", "SIZE",
                 "scale sizes by SIZE when printing them; "
                 "e.g., 'K' for KiB, 'M' for MiB");
    struct arg_lit* directory_opt =
        arg_lit0("d", "directory",
                 "list directories themselves, not their contents");
    struct arg_lit* columns_opt =
        arg_lit0("C", NULL, "list entries by columns");
    struct arg_file* dir_arg =
        arg_filen(NULL, NULL, "DIR", 0, 100, "directory to list");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {all_opt,        almost_all_opt,     long_opt,
                        author_opt,     escape_opt,         color_opt,
                        ignore_backups_opt, block_size_opt, directory_opt,
                        columns_opt,    dir_arg, end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (nerrors > 0) {
        arg_print_errors(stdout, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    int show_columns = (columns_opt->count > 0) && (!show_details);
    show_all = (all_opt->count > 0) && (!show_almost_all); // -A overrides -a
    show_almost_all = (almost_all_opt->count > 0);
    show_details = (long_opt->count > 0);
    show_author = (author_opt->count > 0);
    escape_mode = (escape_opt->count > 0);
    ignore_backups = (ignore_backups_opt->count > 0);
    if (directory_opt->count > 0) {
        list_dir_contents = 0;
    }

    if (color_opt->count > 0) {
        const char* val = color_opt->sval[0];
        if (strcmp(val, "always") == 0) {
            color_mode = COLOR_ALWAYS;
        } else if (strcmp(val, "auto") == 0) {
            color_mode = COLOR_AUTO;
        } else if (strcmp(val, "never") == 0) {
            color_mode = COLOR_NEVER;
        } else {
            // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
            (void)fprintf(stderr,
                          "ls: invalid argument '%s' for --color\nValid arguments: "
                          "always, auto, never\n",
                          val);
        }
    }

    unsigned long block_size = 0;
    char size_suffix = '\0';

    if (block_size_opt->count > 0) {
        block_size = parse_block_size(block_size_opt->sval[0]);
        /* Extract the trailing suffix character for display (e.g. "K" from "1K") */
        const char* bs_str = block_size_opt->sval[0];
        size_t bs_len = strlen(bs_str);
        for (size_t j = bs_len; j > 0; j--) {
            if ((bs_str[j - 1] >= 'A' && bs_str[j - 1] <= 'Z') ||
                (bs_str[j - 1] >= 'a' && bs_str[j - 1] <= 'z')) {
                size_suffix = bs_str[j - 1];
                /* Convert to uppercase for consistent display */
                if (size_suffix >= 'a' && size_suffix <= 'z') {
                    size_suffix -= 32;
                }
                break;
            }
        }
    }

    if (dir_arg->count == 0) {
        dir_arg->count = 1;
        dir_arg->filename[0] = ".";
    }

    if (list_dir_contents) {
        /* Normal mode: open each directory and list its contents */
        for (int i = 0; i < dir_arg->count; i++) {
            DIR* dir = opendir(dir_arg->filename[i]);
            if (dir == NULL) {
                (void)fprintf(stderr, "ls: %s: No such file or directory\n", dir_arg->filename[i]);
                continue;
            }

            struct dirent* entry;
            GList* files = NULL;

            while ((entry = readdir(dir)) != NULL) {
                if (!show_all && entry->d_name[0] == '.') {
                    if (!show_almost_all) {
                        continue;
                    }
                    if (strcmp(entry->d_name, ".") == 0 ||
                        strcmp(entry->d_name, "..") == 0) {
                        continue;
                    }
                }
                if (ignore_backups) {
                    size_t dlen = strlen(entry->d_name);
                    if (dlen > 0 && entry->d_name[dlen - 1] == '~') {
                        continue;
                    }
                }
                files = g_list_append(files, g_strdup(entry->d_name));
            }

            sort_and_output_files(files, show_details, show_author,
                                  escape_mode, color_mode,
                                  block_size, size_suffix,
                                  show_columns);
            closedir(dir);
        }
    } else {
        /* -d mode: list the directory entries themselves, not their contents */
        GList* files = NULL;
        for (int i = 0; i < dir_arg->count; i++) {
            struct stat st;
            if (lstat(dir_arg->filename[i], &st) == -1) {
                (void)fprintf(stderr,
                              "ls: cannot access '%s': No such file or directory\n",
                              dir_arg->filename[i]);
                continue;
            }
            files = g_list_append(files, g_strdup(dir_arg->filename[i]));
        }

        sort_and_output_files(files, show_details, show_author,
                              escape_mode, color_mode,
                              block_size, size_suffix,
                              show_columns || !show_details);
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
