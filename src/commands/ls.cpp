#include <argtable3.h>
#include <dirent.h>
#include <cerrno>
#include <grp.h>
#include <climits>
#include <pwd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctime>
#include <unistd.h>
#include <vector>
#include <string>
#include <algorithm>

#include "commands/ls.hpp"

/* Named constants for printable ASCII range used by print_escaped_filename */
#define ASCII_SPACE 0x20
#define ASCII_TILDE 0x7e

/* Maximum allowed COLUMNS env value (sanity limit) */
#define MAX_COLUMNS_ENV 10000

/* Default terminal width when neither ioctl nor COLUMNS env is available */
#define DEFAULT_TERMINAL_WIDTH 80

/* Gap between columns in columnar output (spaces) */
#define COLUMN_GAP 2

/* Size thresholds for colorful size display */
#define ONE_GIB 1073741824L
#define ONE_MIB 1048576L

/* File-age thresholds (seconds) for colorful date display */
#define AGE_12H 43200
#define AGE_24H 86400
#define AGE_72H 259200

static int should_color(ColorMode mode) {
  switch (mode) {
  case ColorMode::ALWAYS:
    return 1;
  case ColorMode::AUTO:
    return isatty(STDOUT_FILENO);
  default:
    return 0;
  }
}

static const char *get_file_color(const struct stat *st) {
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

static const char *get_classify_indicator(struct stat *st) {
  if (S_ISLNK(st->st_mode)) { return "@"; }
  if (S_ISDIR(st->st_mode)) { return "/"; }
  if (st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) { return "*"; }
  return "";
}

/** Display width of one icon (in terminal columns).
 *  Nerd Font icons are monospace; typically 1 column wide. */
#define ICON_DISPLAY_WIDTH 1

/** Return a UTF-8 Nerd Font icon string for the given file stat,
 *  matching lsd's icon set exactly. */
static const char *get_file_icon(const struct stat *st) {
  if (S_ISDIR(st->st_mode)) {
    return "\xEF\x84\x95"; //  folder
  }
  if (S_ISLNK(st->st_mode)) {
    return "\xEF\x92\x82"; //  symlink
  }
  if (S_ISSOCK(st->st_mode)) {
    return "\xF3\xB0\x86\xA8"; // 󰆨 socket
  }
  if (S_ISFIFO(st->st_mode)) {
    return "\xF3\xB0\x88\xB2"; // 󰈲 pipe
  }
  if (S_ISBLK(st->st_mode)) {
    return "\xF3\xB0\x9C\xAB"; // 󰜫 block device
  }
  if (S_ISCHR(st->st_mode)) {
    return "\xEE\x98\x81"; //  char device
  }
  if (st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
    return "\xEF\x92\x89"; //  executable
  }
  return "\xEF\x80\x96"; //  regular file
}

/** Extract the display name (basename) from a possibly-full path.
 *  Returns a pointer into the original string (no allocation). */
static const char *display_name_of(const char *path) {
  const char *slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

static void print_escaped_filename(const char *filename) {
  for (const unsigned char *p = (const unsigned char *)filename; *p != '\0';
       p++) {
    // Tab is printed as-is; printable ASCII is printed as-is
    if (*p == '\t' || (*p >= ASCII_SPACE && *p <= ASCII_TILDE)) {
      putchar((int)*p);
    } else {
      // All other bytes are escaped as \ooo (3-digit octal)
      printf("\\%03o", (int)*p);
    }
  }
}

static char file_type_char(mode_t mode) {
  if (S_ISDIR(mode)) { return 'd'; }
  if (S_ISLNK(mode)) { return 'l'; }
  if (S_ISBLK(mode)) { return 'b'; }
  if (S_ISCHR(mode)) { return 'c'; }
  if (S_ISSOCK(mode)) { return 's'; }
  if (S_ISFIFO(mode)) { return 'p'; }
  return '-';
}

static const char *file_type_color(mode_t mode) {
  if (S_ISDIR(mode)) { return "\033[1;34m"; }
  if (S_ISLNK(mode)) { return "\033[1;36m"; }
  if (S_ISBLK(mode) || S_ISCHR(mode)) { return "\033[1;33m"; }
  if (S_ISSOCK(mode)) { return "\033[35m"; }
  if (S_ISFIFO(mode)) { return "\033[33m"; }
  return "";
}

static void print_colored_triad(mode_t mode, int bit_r, int bit_w, int bit_x,
                                int special_bit, char special_set_char,
                                char special_unset_char) {
  printf("\033[37m%c\033[0m", mode & bit_r ? 'r' : '-');
  printf("\033[33m%c\033[0m", mode & bit_w ? 'w' : '-');
  if (mode & special_bit) {
    printf("\033[1;31m%c\033[0m", mode & bit_x ? special_set_char : special_unset_char);
  } else {
    printf("\033[1;32m%c\033[0m", mode & bit_x ? 'x' : '-');
  }
}

static void print_colorful_permissions(const struct stat *st) {
  printf("%s", file_type_color(st->st_mode));
  putchar(file_type_char(st->st_mode));
  printf("\033[0m");
  print_colored_triad(st->st_mode, S_IRUSR, S_IWUSR, S_IXUSR, S_ISUID, 's', 'S');
  print_colored_triad(st->st_mode, S_IRGRP, S_IWGRP, S_IXGRP, S_ISGID, 's', 'S');
  print_colored_triad(st->st_mode, S_IROTH, S_IWOTH, S_IXOTH, S_ISVTX, 't', 'T');
  printf(" ");
}

static void print_plain_permissions(const struct stat *st) {
  printf("%c%c%c%c%c%c%c%c%c ", S_ISDIR(st->st_mode) ? 'd' : '-',
         st->st_mode & S_IRUSR ? 'r' : '-', st->st_mode & S_IWUSR ? 'w' : '-',
         st->st_mode & S_IXUSR ? 'x' : '-', st->st_mode & S_IRGRP ? 'r' : '-',
         st->st_mode & S_IWGRP ? 'w' : '-', st->st_mode & S_IXGRP ? 'x' : '-',
         st->st_mode & S_IROTH ? 'r' : '-', st->st_mode & S_IWOTH ? 'w' : '-');
}

static void print_owner_group(const struct stat *st, const LsOptions *opts) {
  int colorful = opts->colorful;
  struct passwd *pwd = getpwuid(st->st_uid);
  struct group *grp = getgrgid(st->st_gid);
  if (opts->show_author) {
    if (colorful) {
      printf("\033[1;33m");
    }
    printf("%s %s ", pwd ? pwd->pw_name : "-", pwd ? pwd->pw_name : "-");
    if (colorful) {
      printf("\033[0m");
    }
    if (colorful) {
      printf("\033[1;36m");
    }
    printf("%s ", grp ? grp->gr_name : "-");
    if (colorful) {
      printf("\033[0m");
    }
  } else {
    if (colorful) {
      printf("\033[1;33m");
    }
    printf("%s ", pwd ? pwd->pw_name : "-");
    if (colorful) {
      printf("\033[0m");
    }
    if (colorful) {
      printf("\033[1;36m");
    }
    printf("%s ", grp ? grp->gr_name : "-");
    if (colorful) {
      printf("\033[0m");
    }
  }
}

static void print_size_with_color(unsigned long raw, unsigned long display_size,
                                   const LsOptions *opts) {
  if (opts->colorful) {
    if (raw > ONE_GIB) {
      printf("\033[1;31m");
    } else if (raw > ONE_MIB) {
      printf("\033[33m");
    } else {
      printf("\033[37m");
    }
  }
  if (opts->size_suffix != '\0') {
    printf("%7lu%c ", display_size, opts->size_suffix);
  } else {
    printf("%8lu ", display_size);
  }
  if (opts->colorful) {
    printf("\033[0m");
  }
}

static void print_date_with_color(const struct stat *st) {
  time_t now = time(NULL);
  double diff = difftime(now, st->st_mtime);
  if (diff < AGE_12H) {
    printf("\033[1;32m");
  } else if (diff < AGE_24H) {
    printf("\033[32m");
  } else if (diff < AGE_72H) {
    printf("\033[36m");
  } else {
    printf("\033[2;37m");
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static void print_long_format(const char *display_name, const struct stat *st,
                                const LsOptions *opts,
                                const char *color_code,
                                const char *classify_suffix) {
  int colorful = opts->colorful;

  // ── Permissions ──
  if (colorful) {
    print_colorful_permissions(st);
  } else {
    print_plain_permissions(st);
  }

  // ── Link count ──
  if (colorful) {
    printf("\033[33m");
  }
  // NOLINTNEXTLINE(bugprone-narrowing-conversions)
  printf("%4ld ", (long)st->st_nlink);
  if (colorful) {
    printf("\033[0m");
  }

  // ── Owner & Group ──
  print_owner_group(st, opts);

  // ── Size ──
  unsigned long raw = (unsigned long)st->st_size;
  unsigned long display_size =
      opts->block_size > 0
          ? (unsigned long)((st->st_size + ((off_t)opts->block_size / 2))
                            / (off_t)opts->block_size)
          : raw;
  print_size_with_color(raw, display_size, opts);

  // ── Date ──
  char time_buf[20];
  // NOLINTNEXTLINE(bugprone-unused-return-value)
  (void)strftime(time_buf, sizeof(time_buf), "%b %d %H:%M",
                 localtime(&st->st_mtime));
  if (colorful) {
    print_date_with_color(st);
  }
  printf("%s ", time_buf);
  if (colorful) {
    printf("\033[0m");
  }

  // ── Filename ──
  if (opts->show_icons) {
    printf("%s ", get_file_icon(st));
  }
  if (color_code != NULL) {
    printf("\033[%sm", color_code);
  }
  if (opts->escape_mode) {
    print_escaped_filename(display_name);
  } else {
    printf("%s", display_name);
  }
  printf("%s", classify_suffix);
  if (color_code != NULL) {
    printf("\033[0m");
  }
  printf("\n");
}

static void print_file_info(const char *filename, const LsOptions *opts) {
  int use_color = should_color(opts->color_mode);
  const char *display_name = display_name_of(filename);
  const char *classify_suffix = "";

  if (!opts->show_details && !use_color) {
    struct stat st;
    int have_stat = 0;
    if (opts->classify || opts->show_icons) {
      have_stat = (lstat(filename, &st) == 0);
      if (have_stat && opts->classify) {
        classify_suffix = get_classify_indicator(&st);
      }
    }
    if (opts->show_icons && have_stat) {
      printf("%s ", get_file_icon(&st));
    }
    if (opts->escape_mode) {
      print_escaped_filename(display_name);
    } else {
      printf("%s", display_name);
    }
    printf("%s%s", classify_suffix, opts->show_one_column ? "\n" : "  ");
    return;
  }

  struct stat st;
  if (lstat(filename, &st) == -1) {
    if (opts->show_details) {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr, "ls: cannot access '%s': %s\n", filename,
                    strerror(errno));
    } else {
      if (opts->escape_mode) {
        print_escaped_filename(display_name);
      } else {
        printf("%s", display_name);
      }
      printf("%s", opts->show_one_column ? "\n" : "  ");
    }
    return;
  }

  if (opts->classify) {
    classify_suffix = get_classify_indicator(&st);
  }

  const char *color_code = NULL;
  if (use_color) {
    color_code = get_file_color(&st);
    use_color = (color_code != NULL);
  }

  if (!opts->show_details) {
    if (opts->show_icons) {
      printf("%s ", get_file_icon(&st));
    }
    if (use_color) {
      printf("\033[%sm", color_code);
    }
    if (opts->escape_mode) {
      print_escaped_filename(display_name);
    } else {
      printf("%s", display_name);
    }
    printf("%s", classify_suffix);
    if (use_color) {
      printf("\033[0m");
    }
    printf("%s", opts->show_one_column ? "\n" : "  ");
    return;
  }

  print_long_format(display_name, &st, opts, color_code, classify_suffix);
}

static unsigned long parse_block_size(const char *str) {
  char *endptr;
  unsigned long val = strtoul(str, &endptr, 10);

  if (endptr == str) {
    // No digits consumed; treat the whole string as a bare suffix (e.g. "K")
    val = 1;
  } else if (val == 0) {
    // NOLINTNEXTLINE(bugprone-unused-return-value, clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    (void)fprintf(stderr,
                  "ls: --block-size argument must be a positive integer\n");
    return 1;
  }

  // Apply any trailing suffix
  while (*endptr != '\0') {
    switch (*endptr) {
    case 'K':
    case 'k':
      if (val > ULONG_MAX / 1024UL) { val = ULONG_MAX; break; }
      val *= 1024UL;
      break;
    case 'M':
    case 'm':
      if (val > ULONG_MAX / (1024UL * 1024)) { val = ULONG_MAX; break; }
      val *= 1024UL * 1024;
      break;
    case 'G':
    case 'g':
      if (val > ULONG_MAX / (1024UL * 1024 * 1024)) { val = ULONG_MAX; break; }
      val *= 1024UL * 1024 * 1024;
      break;
    case 'T':
    case 't':
      if (val > ULONG_MAX / (1024UL * 1024 * 1024 * 1024)) { val = ULONG_MAX; break; }
      val *= 1024UL * 1024 * 1024 * 1024;
      break;
    case 'P':
    case 'p':
      if (val > ULONG_MAX / (1024UL * 1024 * 1024 * 1024 * 1024)) { val = ULONG_MAX; break; }
      val *= 1024UL * 1024 * 1024 * 1024 * 1024;
      break;
    case 'E':
    case 'e':
      if (val > ULONG_MAX / (1024UL * 1024 * 1024 * 1024 * 1024 * 1024)) { val = ULONG_MAX; break; }
      val *= 1024UL * 1024 * 1024 * 1024 * 1024 * 1024;
      break;
    default:
      // NOLINTNEXTLINE(bugprone-unused-return-value, clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
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
  // NOLINTNEXTLINE(misc-include-cleaner)
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
    return (int)ws.ws_col;
  }
  const char *cols = getenv("COLUMNS");
  if (cols != NULL) {
    char *endptr;
    long val = strtol(cols, &endptr, 10);
    if (endptr != cols && val > 0 && val < MAX_COLUMNS_ENV) {
      return (int)val;
    }
  }
  return DEFAULT_TERMINAL_WIDTH;
}

/** Compute the display width of a filename when printed via
 *  print_escaped_filename() — printable chars count as 1, escaped
 *  non-printable bytes count as 4 ( "\ooo" ). */
static int escaped_display_width(const char *s) {
  int width = 0;
  for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
    if (*p == '\t' || (*p >= ASCII_SPACE && *p <= ASCII_TILDE)) {
      width++;
    } else {
      width += 4;
    }
  }
  return width;
}

/** Compute the visual width of a plain (non-escaped) filename. */
static int plain_display_width(const char *s) { return (int)strlen(s); }

/** Decide whether to use column layout.
 *  On terminal, unsorted output uses single-column layout (no wrap).
 *  When piped or redirected, preserve the configured column mode.
 *  -1 forces single-column output. */
static int should_use_columns(const LsOptions *opts) {
    if (opts->show_one_column) {
        return 0;
    }
    if (opts->unsorted && isatty(STDOUT_FILENO)) {
        return 0;
    }
    return opts->show_columns;
}

/** Print a list of filenames in vertically-sorted columns, like GNU ls -C.
 *
 *  The list is assumed to already be sorted.  Terminal width is detected
 *  via ioctl / $COLUMNS / default 80.
 */
static void print_one_entry(const char *name, int col, int num_cols,
                             int col_width, const LsOptions *opts,
                             int use_color) {
  const char *display_name = display_name_of(name);
  int name_display_w =
      opts->escape_mode ? escaped_display_width(display_name)
                        : plain_display_width(display_name);
  if (opts->classify) {
    name_display_w += 1;
  }
  if (opts->show_icons) {
    name_display_w += ICON_DISPLAY_WIDTH + 1; // icon + space
  }

  const char *color_code = NULL;
  const char *classify_suffix = "";
  struct stat st;
  int have_stat = 0;
  if (use_color || opts->classify || opts->show_icons) {
    have_stat = (lstat(name, &st) == 0);
    if (have_stat) {
      if (use_color) {
        color_code = get_file_color(&st);
      }
      if (opts->classify) {
        classify_suffix = get_classify_indicator(&st);
      }
    }
  }

  if (opts->show_icons && have_stat) {
    printf("%s ", get_file_icon(&st));
  }
  if (color_code != NULL) {
    printf("\033[%sm", color_code);
  }
  if (opts->escape_mode) {
    print_escaped_filename(display_name);
  } else {
    printf("%s", display_name);
  }
  printf("%s", classify_suffix);
  if (color_code != NULL) {
    printf("\033[0m");
  }

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

static void print_columns(const std::vector<std::string>& files, const LsOptions *opts) {
  int use_color = should_color(opts->color_mode);
  int term_width = get_terminal_width();
  int count = (int)files.size();

  if (count == 0) {
    return;
  }

  /* Find the maximum display width among filenames (basename only) */
  int max_width = 0;
  for (const auto& fname : files) {
    const char *display_name = display_name_of(fname.c_str());
    int w = opts->escape_mode ? escaped_display_width(display_name)
                              : plain_display_width(display_name);
    if (opts->classify) {
      w += 1;
    }
    if (opts->show_icons) {
      w += ICON_DISPLAY_WIDTH + 1; // icon + space
    }
    if (w > max_width) {
      max_width = w;
    }
  }

  const int gap = COLUMN_GAP;
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

  /* Build an array of C-string pointers for indexed access */
  const char **names = (const char **)malloc(sizeof(*names) * (size_t)count);
  {
    int idx = 0;
    for (const auto& fname : files) {
      names[idx++] = fname.c_str();
    }
  }

  /* Print in vertical-sorted order (down columns) */
  for (int row = 0; row < num_rows; row++) {
    for (int col = 0; col < num_cols; col++) {
      int index = (col * num_rows) + row;
      if (index >= count) {
        continue;
      }

      // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Assign, clang-analyzer-security.ArrayBound, clang-analyzer-core.CallAndMessage)
      print_one_entry(names[index], col, num_cols, col_width, opts, use_color);
    }
    printf("\n");
  }

  free((void*)names);
}

/**
 * Sort a vector of filename strings, print them (either as columns or
 * individually via print_file_info).
 */
static void sort_and_output_files(std::vector<std::string>& files, const LsOptions *opts) {
  if (files.empty()) {
    return;
  }

  if (!opts->unsorted) {
    std::sort(files.begin(), files.end());
  }

  if (opts->reverse_sort && !opts->unsorted) {
    std::reverse(files.begin(), files.end());
  }

  if (should_use_columns(opts)) {
    print_columns(files, opts);
  } else {
    for (const auto& fname : files) {
      print_file_info(fname.c_str(), opts);
    }
    if (!opts->show_details && !opts->show_one_column) {
      printf("\n");
    }
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ls_command(int argc, char **argv) {
  LsOptions opts = {0};
  opts.list_dir_contents = 1; /* default: list directory contents */
  opts.color_mode = ColorMode::NEVER;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--color") == 0) {
      argv[i] = "--color=always";
      break;
    }
  }

  struct arg_lit *all_opt =
      arg_lit0("a", "all", "do not ignore entries starting with .");
  struct arg_lit *almost_all_opt =
      arg_lit0("A", "almost-all", "do not list implied . and ..");
  struct arg_lit *long_opt = arg_lit0("l", "long", "use a long listing format");
  struct arg_lit *author_opt =
      arg_lit0(NULL, "author", "with -l, print the author of each file");
  struct arg_lit *escape_opt =
      arg_lit0("b", "escape", "print octal escapes for non-graphic characters");
  struct arg_str *color_opt =
      arg_str0(NULL, "color", "WHEN",
               "colorize the output; WHEN can be 'always', 'auto', or 'never'");
  struct arg_lit *ignore_backups_opt =
      arg_lit0("B", "ignore-backups", "do not list entries ending with ~");
  struct arg_str *block_size_opt =
      arg_str0(NULL, "block-size", "SIZE",
               "scale sizes by SIZE when printing them; "
               "e.g., 'K' for KiB, 'M' for MiB");
  struct arg_lit *directory_opt = arg_lit0(
      "d", "directory", "list directories themselves, not their contents");
  struct arg_lit *columns_opt = arg_lit0("C", NULL, "list entries by columns");
  struct arg_lit *reverse_opt = arg_lit0("r", "reverse", "reverse order when sorting");
  struct arg_lit *unsorted_opt = arg_lit0("U", NULL, "do not sort; list entries in directory order");
  struct arg_lit *one_column_opt = arg_lit0("1", NULL, "list one file per line");
  struct arg_lit *classify_opt =
      arg_lit0("F", "classify", "append indicator (one of */@) to entries");
  struct arg_lit *colorful_opt =
      arg_lit0(NULL, "colorful", "multi-color output (like eza/lsd)");
  struct arg_lit *icons_opt =
      arg_lit0(NULL, "icons", "display icons before file names (like lsd)");
  struct arg_lit *help_opt =
      arg_lit0("h", "help", "display this help and exit");
  struct arg_file *dir_arg =
      arg_filen(NULL, NULL, "DIR", 0, 100, "directory to list");
  struct arg_end *end = arg_end(20);

  void *argtable[] = {all_opt,
                      almost_all_opt,
                      long_opt,
                      author_opt,
                      escape_opt,
                      color_opt,
                      ignore_backups_opt,
                      block_size_opt,
                      directory_opt,
                      columns_opt,
                      reverse_opt,
                      unsorted_opt,
                      one_column_opt,
                      classify_opt,
                      colorful_opt,
                      icons_opt,
                      help_opt,
                      dir_arg,
                      end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]... [DIR]...\n", argv[0]);
    printf("List directory contents.\n");
    printf("\n");
    printf("  -a, --all             do not ignore entries starting with .\n");
    printf("  -A, --almost-all      do not list implied . and ..\n");
    printf("      --author          with -l, print the author of each file\n");
    printf("  -b, --escape          print octal escapes for non-graphic characters\n");
    printf("      --block-size=SIZE scale sizes by SIZE when printing them\n");
    printf("  -B, --ignore-backups  do not list entries ending with ~\n");
    printf("      --color=WHEN      colorize the output; WHEN can be always, auto, or never\n");
    printf("  -1                    list one file per line\n");
    printf("  -C                    list entries by columns\n");
    printf("  -d, --directory       list directories themselves, not their contents\n");
    printf("  -F, --classify        append indicator (one of */@) to entries\n");
    printf("      --colorful        multi-color output (like eza/lsd)\n");
    printf("      --icons           display icons before file names (like lsd)\n");
    printf("  -l, --long            use a long listing format\n");
    printf("  -r, --reverse         reverse order when sorting\n");
    printf("  -U                    do not sort; list entries in directory order\n");
    printf("  -h, --help            display this help and exit\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  opts.show_almost_all = (almost_all_opt->count > 0);
  opts.show_all = (all_opt->count > 0) && (!opts.show_almost_all); // -A overrides -a
  opts.show_details = (long_opt->count > 0);
  opts.show_columns = (columns_opt->count > 0) && (!opts.show_details);
  opts.show_one_column = (one_column_opt->count > 0);
  if (opts.show_one_column) {
    opts.show_columns = 0;
  }
  opts.classify = (classify_opt->count > 0);
  opts.colorful = (colorful_opt->count > 0);
  opts.show_icons = (icons_opt->count > 0);
  if (opts.colorful) {
    opts.color_mode = ColorMode::ALWAYS;
  }
  opts.reverse_sort = (reverse_opt->count > 0);
  opts.unsorted = (unsorted_opt->count > 0);
  opts.show_author = (author_opt->count > 0);
  opts.escape_mode = (escape_opt->count > 0);
  opts.ignore_backups = (ignore_backups_opt->count > 0);
  if (directory_opt->count > 0) {
    opts.list_dir_contents = 0;
  }

  if (color_opt->count > 0) {
    const char *val = color_opt->sval[0];
    if (strcmp(val, "always") == 0) {
      opts.color_mode = ColorMode::ALWAYS;
    } else if (strcmp(val, "auto") == 0) {
      opts.color_mode = ColorMode::AUTO;
    } else if (strcmp(val, "never") == 0) {
      opts.color_mode = ColorMode::NEVER;
    } else {
      // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
      (void)fprintf(stderr,
                    "ls: invalid argument '%s' for --color\nValid arguments: "
                    "always, auto, never\n",
                    val);
    }
  }

  if (block_size_opt->count > 0) {
    opts.block_size = parse_block_size(block_size_opt->sval[0]);
    /* Extract the trailing suffix character for display (e.g. "K" from "1K") */
    const char *bs_str = block_size_opt->sval[0];
    size_t bs_len = strlen(bs_str);
    for (size_t j = bs_len; j > 0; j--) {
      if ((bs_str[j - 1] >= 'A' && bs_str[j - 1] <= 'Z') ||
          (bs_str[j - 1] >= 'a' && bs_str[j - 1] <= 'z')) {
        opts.size_suffix = bs_str[j - 1];
        /* Convert to uppercase for consistent display */
        if (opts.size_suffix >= 'a' && opts.size_suffix <= 'z') {
          opts.size_suffix -= 32;
        }
        break;
      }
    }
  }

  if (dir_arg->count == 0) {
    dir_arg->count = 1;
    dir_arg->filename[0] = ".";
  }

  if (opts.list_dir_contents) {
    /* Normal mode: open each directory and list its contents */
    for (int i = 0; i < dir_arg->count; i++) {
      DIR *dir = opendir(dir_arg->filename[i]);
      if (dir == NULL) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr, "ls: %s: No such file or directory\n",
                      dir_arg->filename[i]);
        continue;
      }

      struct dirent *entry;
      std::vector<std::string> files;

      while ((entry = readdir(dir)) != NULL) {
        if (!opts.show_all && entry->d_name[0] == '.') {
          if (!opts.show_almost_all) {
            continue;
          }
          if (strcmp(entry->d_name, ".") == 0 ||
              strcmp(entry->d_name, "..") == 0) {
            continue;
          }
        }
        if (opts.ignore_backups) {
          size_t dlen = strlen(entry->d_name);
          if (dlen > 0 && entry->d_name[dlen - 1] == '~') {
            continue;
          }
        }
        /* Store full path so lstat works correctly for color and long format,
         * regardless of the current working directory. */
        char full_path[4096];
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)snprintf(full_path, sizeof(full_path), "%s/%s",
                       dir_arg->filename[i], entry->d_name);
        files.push_back(full_path);
      }

      sort_and_output_files(files, &opts);
      closedir(dir);
    }
  } else {
    /* -d mode: list the directory entries themselves, not their contents */
    std::vector<std::string> files;
    for (int i = 0; i < dir_arg->count; i++) {
      struct stat st;
      if (lstat(dir_arg->filename[i], &st) == -1) {
        // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
        (void)fprintf(stderr,
                      "ls: cannot access '%s': No such file or directory\n",
                      dir_arg->filename[i]);
        continue;
      }
      files.push_back(dir_arg->filename[i]);
    }

    opts.show_columns = opts.show_columns || !opts.show_details;
    sort_and_output_files(files, &opts);
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
