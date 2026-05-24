#ifndef LS_H
#define LS_H

#include <glib.h>

typedef enum { COLOR_NEVER, COLOR_ALWAYS, COLOR_AUTO } color_mode_t;

typedef struct {
    int show_all;
    int show_almost_all;
    int show_details;
    int show_author;
    int escape_mode;
    int ignore_backups;
    int list_dir_contents;
    int show_columns;
    color_mode_t color_mode;
    unsigned long block_size;
    char size_suffix;
} LsOptions;

void ls_command(gint argc, gchar** argv);

#endif
