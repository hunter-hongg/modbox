#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <vector>
#include <string>
#include <argtable3.h>

#include "commands/fmt.hpp"
#include "commands/command_macros.hpp"

static void emit_paragraph(const std::vector<std::string>& words, int width,
  FILE* out) {
  int col = 0;
  for (size_t i = 0; i < words.size(); i++) {
    const std::string& w = words[i];
    int len = (int)w.size();
    if (i > 0) {
      if (col + 1 + len > width) {
        fputc('\n', out);
        fputs(w.c_str(), out);
        col = len;
      } else {
        fputc(' ', out);
        fputs(w.c_str(), out);
        col += 1 + len;
      }
    } else {
      fputs(w.c_str(), out);
      col = len;
    }
  }
  fputc('\n', out);
}

static void fmt_file(FILE* fp, int width, FILE* out) {
  std::vector<std::string> para;
  char* buf = nullptr;
  size_t cap = 0;
  ssize_t n;

  while ((n = getline(&buf, &cap, fp)) != -1) {
    if (n > 0 && buf[n - 1] == '\n') {
      buf[--n] = '\0';
    }
    if (n == 0) {
      if (!para.empty()) {
        emit_paragraph(para, width, out);
        para.clear();
      }
      fputc('\n', out);
      continue;
    }
    size_t start = 0;
    while (start < (size_t)n) {
      while (start < (size_t)n && buf[start] == ' ') start++;
      size_t end = start;
      while (end < (size_t)n && buf[end] != ' ') end++;
      std::string w(buf + start, end - start);
      if (!w.empty()) {
        para.push_back(w);
      }
      start = end;
      while (start < (size_t)n && buf[start] == ' ') start++;
    }
  }

  if (!para.empty()) {
    emit_paragraph(para, width, out);
  }
  free(buf);
}

void fmt_command(int argc, char** argv) {
  struct arg_int* width_opt = arg_int0("w", "width", "N", "maximum output width (default 72)");
  struct arg_lit* uniform_opt = arg_lit0("u", "uniform-spacing",
    "one space between words, two between sentences");
  struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
  struct arg_file* files_arg = arg_filen(NULL, NULL, "FILE", 0, 100, "input file(s)");
  struct arg_end* end = arg_end(20);

  void* argtable[] = {width_opt, uniform_opt, help_opt, files_arg, end};
  int nerrors = arg_parse(argc, argv, argtable);

  if (help_opt->count > 0) {
    printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
    printf("Reformat each paragraph to fit within a fixed width.\n");
    printf("\n");
    printf(" -w, --width=N   maximum output width (default 72)\n");
    printf(" -u, --uniform-spacing  one space between words, two between sentences\n");
    printf(" -h, --help      display this help and exit\n");
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  if (nerrors > 0) {
    arg_print_errors(stderr, end, argv[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return;
  }

  int width = 72;
  if (width_opt->count > 0) {
    width = width_opt->ival[0];
  }

  if (files_arg->count == 0) {
    fmt_file(stdin, width, stdout);
  } else {
    for (int i = 0; i < files_arg->count; i++) {
      const char* fname = files_arg->filename[i];
      if (strcmp(fname, "-") == 0) {
        fmt_file(stdin, width, stdout);
      } else {
        FILE* fp = fopen(fname, "r");
        if (fp == NULL) {
          fprintf(stderr, "fmt: %s: %s\n", fname, strerror(errno));
          continue;
        }
        fmt_file(fp, width, stdout);
        fclose(fp);
      }
    }
  }

  arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

REGISTER_COMMAND("fmt", fmt_command, "Reformat paragraph text");
