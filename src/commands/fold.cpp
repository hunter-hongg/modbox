#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <cctype>
#include <vector>
#include <argtable3.h>

#include "commands/fold.hpp"
#include "commands/command_macros.hpp"

#define MAX_LINE 1048576

static void print_help(const char* prog) {
  printf("Usage: %s [OPTION]... [FILE]...\n", prog);
  printf("Wrap input lines in each FILE (or stdin), writing to stdout.\n");
  printf("\n");
  printf("  -b, --bytes=MAX          use MAX bytes\n");
  printf("  -s, --spaces             break at spaces, not in the middle of words\n");
  printf("  -w, --width=WIDTH        use WIDTH instead of 80\n");
  printf("      --help               display this help and exit\n");
  printf("      --version            output version information and exit\n");
}

static void process_file(FILE* fp, int width, bool break_spaces) {
  char line_buf[MAX_LINE];
  int has_nl;
  while (fgets(line_buf, sizeof(line_buf), fp)) {
    size_t len = strlen(line_buf);
    if (len > 0 && line_buf[len - 1] == '\n') {
      line_buf[--len] = '\0';
      has_nl = 1;
    } else {
      has_nl = 0;
    }
    if (len == 0) {
      printf("\n");
      continue;
    }
    if (break_spaces) {
      size_t pos = 0;
      while (pos < len) {
        size_t remaining = len - pos;
        size_t chunk = (remaining < (size_t)width) ? remaining : (size_t)width;
        if (pos + chunk < len && line_buf[pos + chunk] != ' ') {
          size_t adj = chunk;
          while (adj > 0 && line_buf[pos + adj] != ' ') adj--;
          if (adj == 0) adj = chunk;
          chunk = adj;
        }
        for (size_t i = pos; i < pos + chunk; i++) fputc(line_buf[i], stdout);
        pos += chunk;
        if (pos < len) {
          printf("\n");
          while (pos < len && line_buf[pos] == ' ') pos++;
        }
      }
    } else {
      for (size_t pos = 0; pos < len; pos += (size_t)width) {
        size_t chunk = (len - pos < (size_t)width) ? len - pos : (size_t)width;
        for (size_t i = pos; i < pos + chunk; i++) fputc(line_buf[i], stdout);
        printf("\n");
      }
    }
  }
}

void fold_command(int argc, char** argv) {
  int width = 80;
  bool break_spaces = false;
  std::vector<const char*> files;

  int i = 1;
  for (; i < argc; i++) {
    const char* a = argv[i];
    if (strcmp(a, "--help") == 0) { print_help(argv[0]); return; }
    if (strcmp(a, "--version") == 0) { printf("fold (modbox) 1.0\n"); return; }
    if (strncmp(a, "-w", 2) == 0) {
      if (a[2]) width = std::atoi(a + 2);
      else { i++; width = std::atoi(argv[i]); }
      continue;
    }
    if (strcmp(a, "-b") == 0 && i + 1 < argc) { i++; width = std::atoi(argv[i]); continue; }
    if (strcmp(a, "--bytes") == 0 && i + 1 < argc) { i++; width = std::atoi(argv[i]); continue; }
    if (strcmp(a, "--width") == 0 && i + 1 < argc) { i++; width = std::atoi(argv[i]); continue; }
    if (strcmp(a, "-s") == 0 || strcmp(a, "--spaces") == 0) { break_spaces = true; continue; }
    files.push_back(a);
  }
  for (; i < argc; i++) {
    files.push_back(argv[i]);
  }

  for (size_t k = 0; k < files.size(); k++) {
    FILE* fp = (strcmp(files[k], "-") == 0) ? stdin : fopen(files[k], "r");
    if (!fp) {
      fprintf(stderr, "fold: %s: %s\n", files[k], strerror(errno));
      continue;
    }
    process_file(fp, width, break_spaces);
    if (fp != stdin) fclose(fp);
  }
}

REGISTER_COMMAND("fold", fold_command, "Wrap lines to fit width");
