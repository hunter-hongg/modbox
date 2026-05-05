#include <argtable3.h>
#include <dirent.h>
#include <glib.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "commands/ls.h"

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

static const char *get_file_color(struct stat *st) {
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

static void print_file_info(const char *filename, int show_details,
                            int show_author,
                            color_mode_t color_mode) {
  int use_color = should_color(color_mode);

  if (!show_details && !use_color) {
    printf("%s  ", filename);
    return;
  }

  struct stat st;
  if (lstat(filename, &st) == -1) {
    if (!show_details) {
      printf("%s  ", filename);
    }
    return;
  }

  const char *color_code = NULL;
  if (use_color) {
    color_code = get_file_color(&st);
    use_color = (color_code != NULL);
  }

  if (!show_details) {
    if (use_color) {
      printf("\033[%sm", color_code);
    }
    printf("%s", filename);
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

  printf("%4ld ", (long)st.st_nlink);

  struct passwd *pwd = getpwuid(st.st_uid);
  struct group *grp = getgrgid(st.st_gid);
  if (show_author) {
    printf("%s %s %s ", pwd ? pwd->pw_name : "-", pwd ? pwd->pw_name : "-",
           grp ? grp->gr_name : "-");
  } else {
    printf("%s %s ", pwd ? pwd->pw_name : "-", grp ? grp->gr_name : "-");
  }

  printf("%8ld ", (long)st.st_size);

  char time_buf[20];
  (void)strftime(time_buf, sizeof(time_buf), "%b %d %H:%M",
                 localtime(&st.st_mtime));
  printf("%s ", time_buf);

  if (use_color) {
    printf("\033[%sm", color_code);
  }
  printf("%s", filename);
  if (use_color) {
    printf("\033[0m");
  }
  printf("\n");
}

void ls_command(gint argc, gchar **argv) {
  int show_all = 0;
  int show_almost_all = 0;
  int show_details = 0;
  int show_author = 0;
  color_mode_t color_mode = COLOR_NEVER;

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
  struct arg_str *color_opt =
      arg_str0(NULL, "color", "WHEN",
               "colorize the output; WHEN can be 'always', 'auto', or 'never'");
  struct arg_file *dir_arg =
      arg_filen(NULL, NULL, "DIR", 0, 100, "directory to list");
  struct arg_end *end = arg_end(20);

  void *argtable[] = {all_opt,   almost_all_opt, long_opt,  author_opt,
                      color_opt, dir_arg,        end};

  int nerrors = arg_parse(argc, argv, argtable);

  if (nerrors > 0) {
    arg_print_errors(stdout, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  show_almost_all = (almost_all_opt->count > 0);
  show_all = (all_opt->count > 0) && (!show_almost_all); // -A overrides -a
  show_details = (long_opt->count > 0);
  show_author = (author_opt->count > 0);

  if (color_opt->count > 0) {
    const char *val = color_opt->sval[0];
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

  if (dir_arg->count == 0) {
    dir_arg->count = 1;
    dir_arg->filename[0] = ".";
  }

  for (int i = 0; i < dir_arg->count; i++) {
    DIR *dir = opendir(dir_arg->filename[i]);
    if (dir == NULL) {
      printf("ls: %s: No such file or directory\n", dir_arg->filename[i]);
      continue;
    }

    struct dirent *entry;
    GList *files = NULL;

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
      files = g_list_append(files, g_strdup(entry->d_name));
    }

    files = g_list_sort(files, (GCompareFunc)strcmp);

    GList *iter = files;
    while (iter != NULL) {
      print_file_info((const char *)iter->data, show_details, show_author,
                       color_mode);
      g_free(iter->data);
      iter = iter->next;
    }

    if (!show_details) {
      printf("\n");
    }

    g_list_free(files);
    closedir(dir);
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
