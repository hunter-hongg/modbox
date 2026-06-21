#include <argtable3.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands/tac.h"

#define ARG_END_SIZE 20

typedef struct {
    const guint8* data;
    size_t len;
} TacData;

typedef struct {
    GByteArray* bytes;
    GByteArray* sep_bytes;
} TacRecord;

static void free_records(GPtrArray* records) {
    for (size_t i = 0; i < records->len; i++) {
        TacRecord* rec = (TacRecord*)g_ptr_array_index(records, i);
        g_byte_array_free(rec->bytes, TRUE);
        g_byte_array_free(rec->sep_bytes, TRUE);
        g_free(rec);
    }
    g_ptr_array_free(records, TRUE);
}

static void write_records(GPtrArray* records, FILE* out) {
    for (gint i = (gint)records->len - 1; i >= 0; i--) {
        TacRecord* rec = (TacRecord*)g_ptr_array_index(records, i);
        if (rec->sep_bytes && rec->sep_bytes->len > 0) {
            if (fwrite(rec->sep_bytes->data, 1, rec->sep_bytes->len, out) != rec->sep_bytes->len) {
                fprintf(stderr, "tac: write error\n");
                return;
            }
        }
        if (fwrite(rec->bytes->data, 1, rec->bytes->len, out) != rec->bytes->len) {
            fprintf(stderr, "tac: write error\n");
            return;
        }
    }
}

static GPtrArray* split_custom(const guint8* data, size_t total_len,
                               const char* sep, int before_mode, int regex_mode) {
    GPtrArray* records = g_ptr_array_new();
    size_t sep_len = strlen(sep);

    if (regex_mode) {
        regex_t regex;
        if (regcomp(&regex, sep, REG_EXTENDED) != 0) {
            fprintf(stderr, "tac: invalid regex: %s\n", sep);
            g_ptr_array_free(records, TRUE);
            return NULL;
        }

        size_t pos = 0;
        GByteArray* pending_sep = g_byte_array_new();
        while (pos < total_len) {
            regmatch_t match;
            int rc = regexec(&regex, (const char*)data + pos, 1, &match, 0);
            if (rc != 0 || match.rm_so == -1) {
                break;
            }

            size_t abs_match_start = pos + (size_t)match.rm_so;
            size_t abs_match_end = pos + (size_t)match.rm_eo;
            size_t cur_match_len = abs_match_end - abs_match_start;

            if (before_mode) {
                if (abs_match_start > pos) {
                    TacRecord* rec = g_malloc(sizeof(TacRecord));
                    rec->bytes = g_byte_array_sized_new(abs_match_start - pos);
                    g_byte_array_append(rec->bytes, data + pos, abs_match_start - pos);
                    rec->sep_bytes = g_byte_array_sized_new(pending_sep->len);
                    g_byte_array_append(rec->sep_bytes, pending_sep->data, pending_sep->len);
                    g_ptr_array_add(records, rec);
                } else if (records->len == 0) {
                    TacRecord* rec = g_malloc(sizeof(TacRecord));
                    rec->bytes = g_byte_array_new();
                    rec->sep_bytes = g_byte_array_sized_new(pending_sep->len);
                    g_byte_array_append(rec->sep_bytes, pending_sep->data, pending_sep->len);
                    g_ptr_array_add(records, rec);
                }
                g_byte_array_set_size(pending_sep, 0);
                g_byte_array_append(pending_sep, data + abs_match_start, cur_match_len);
                pos = abs_match_end;
            } else {
                TacRecord* rec = g_malloc(sizeof(TacRecord));
                rec->bytes = g_byte_array_sized_new(cur_match_len + (abs_match_start - pos));
                g_byte_array_append(rec->bytes, data + pos, abs_match_start - pos);
                g_byte_array_append(rec->bytes, data + abs_match_start, cur_match_len);
                rec->sep_bytes = g_byte_array_new();
                g_ptr_array_add(records, rec);
                pos = abs_match_end;
            }
        }

        if (pos < total_len) {
            TacRecord* rec = g_malloc(sizeof(TacRecord));
            rec->bytes = g_byte_array_sized_new(total_len - pos);
            g_byte_array_append(rec->bytes, data + pos, total_len - pos);
            if (before_mode) {
                rec->sep_bytes = g_byte_array_sized_new(pending_sep->len);
                g_byte_array_append(rec->sep_bytes, pending_sep->data, pending_sep->len);
            } else {
                rec->sep_bytes = g_byte_array_new();
            }
            g_ptr_array_add(records, rec);
        } else if (records->len == 0) {
            TacRecord* rec = g_malloc(sizeof(TacRecord));
            rec->bytes = g_byte_array_new();
            rec->sep_bytes = g_byte_array_new();
            g_ptr_array_add(records, rec);
        }
        g_byte_array_free(pending_sep, TRUE);

        regfree(&regex);
    } else {
        size_t pos = 0;
        GByteArray* pending_sep = g_byte_array_new();
        while (pos < total_len) {
            size_t found = total_len;
            for (size_t i = pos; i + sep_len <= total_len; i++) {
                if (memcmp(data + i, sep, sep_len) == 0) {
                    found = i;
                    break;
                }
            }

            if (found == total_len) {
                break;
            }

            if (before_mode) {
                if (found > pos) {
                    TacRecord* rec = g_malloc(sizeof(TacRecord));
                    rec->bytes = g_byte_array_sized_new(found - pos);
                    g_byte_array_append(rec->bytes, data + pos, found - pos);
                    rec->sep_bytes = g_byte_array_sized_new(pending_sep->len);
                    g_byte_array_append(rec->sep_bytes, pending_sep->data, pending_sep->len);
                    g_ptr_array_add(records, rec);
                } else if (records->len == 0) {
                    TacRecord* rec = g_malloc(sizeof(TacRecord));
                    rec->bytes = g_byte_array_new();
                    rec->sep_bytes = g_byte_array_sized_new(pending_sep->len);
                    g_byte_array_append(rec->sep_bytes, pending_sep->data, pending_sep->len);
                    g_ptr_array_add(records, rec);
                }
                g_byte_array_set_size(pending_sep, 0);
                g_byte_array_append(pending_sep, data + found, sep_len);
                pos = found + sep_len;
            } else {
                TacRecord* rec = g_malloc(sizeof(TacRecord));
                rec->bytes = g_byte_array_sized_new(found - pos + sep_len);
                g_byte_array_append(rec->bytes, data + pos, found - pos + sep_len);
                rec->sep_bytes = g_byte_array_new();
                g_ptr_array_add(records, rec);
                pos = found + sep_len;
            }
        }

        if (pos < total_len) {
            TacRecord* rec = g_malloc(sizeof(TacRecord));
            rec->bytes = g_byte_array_sized_new(total_len - pos);
            g_byte_array_append(rec->bytes, data + pos, total_len - pos);
            if (before_mode) {
                rec->sep_bytes = g_byte_array_sized_new(pending_sep->len);
                g_byte_array_append(rec->sep_bytes, pending_sep->data, pending_sep->len);
            } else {
                rec->sep_bytes = g_byte_array_new();
            }
            g_ptr_array_add(records, rec);
        } else if (records->len == 0) {
            TacRecord* rec = g_malloc(sizeof(TacRecord));
            rec->bytes = g_byte_array_new();
            rec->sep_bytes = g_byte_array_new();
            g_ptr_array_add(records, rec);
        }
        g_byte_array_free(pending_sep, TRUE);
    }

    return records;
}

static GPtrArray* split_by_newline(const guint8* data, size_t total_len) {
    GPtrArray* records = g_ptr_array_new();
    size_t pos = 0;

    while (pos < total_len) {
        const guint8* nl_ptr = memchr(data + pos, '\n', total_len - pos);
        if (!nl_ptr) {
            TacRecord* rec = g_malloc(sizeof(TacRecord));
            rec->bytes = g_byte_array_sized_new(total_len - pos);
            if (total_len > pos) {
                g_byte_array_append(rec->bytes, data + pos, total_len - pos);
            }
            rec->sep_bytes = g_byte_array_new();
            g_ptr_array_add(records, rec);
            break;
        }
        size_t chunk_len = (size_t)(nl_ptr - (const guint8*)data) - pos + 1;
        TacRecord* rec = g_malloc(sizeof(TacRecord));
        rec->bytes = g_byte_array_sized_new(chunk_len);
        g_byte_array_append(rec->bytes, data + pos, chunk_len);
        rec->sep_bytes = g_byte_array_new();
        g_ptr_array_add(records, rec);
        pos = (size_t)(nl_ptr - (const guint8*)data) + 1;
    }

    return records;
}

static GPtrArray* split_input(const guint8* data, size_t total_len,
                              const char* sep, int before_mode, int regex_mode) {
    if (!sep || strlen(sep) == 0) {
        return split_by_newline(data, total_len);
    }
    return split_custom(data, total_len, sep, before_mode, regex_mode);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void tac_command(gint argc, gchar** argv) {
    struct arg_lit* before_opt = arg_lit0("b", "before",
        "put separator before each chunk");
    struct arg_lit* regex_opt = arg_lit0("r", "regex",
        "treat separator as a regex");
    struct arg_str* sep_opt = arg_str0("s", "separator", "STR",
        "use STR instead of newline");
    struct arg_lit* help_opt = arg_lit0("h", "help",
        "display this help and exit");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 100,
        "file to reverse");
    struct arg_end* end = arg_end(ARG_END_SIZE);

    void* argtable[] = { before_opt, regex_opt, sep_opt, help_opt,
                         file_arg, end };

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]...\n", argv[0]);
        printf("Reverse a file line by line.\n");
        printf("\n");
        printf("  -b, --before          put separator before each chunk\n");
        printf("  -r, --regex           treat separator as a regex\n");
        printf("  -s, --separator=STR   use STR instead of newline\n");
        printf("  -h, --help            display this help and exit\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    int before_mode = (before_opt->count > 0);
    int regex_mode = (regex_opt->count > 0);
    const char* sep = sep_opt->count > 0 ? sep_opt->sval[0] : NULL;

    int from_stdin = (file_arg->count == 0);

    if (from_stdin) {
        GByteArray* buf = g_byte_array_new();
        char tmp[4096];
        size_t n;
        while ((n = fread(tmp, 1, sizeof(tmp), stdin)) > 0) {
            g_byte_array_append(buf, (guint8*)tmp, n);
        }
        if (buf->len > 0) {
            GPtrArray* records = split_input(buf->data, buf->len,
                                             sep, before_mode, regex_mode);
            write_records(records, stdout);
            free_records(records);
        }
        g_byte_array_free(buf, FALSE);
    } else {
        for (int i = 0; i < file_arg->count; i++) {
            FILE* fp = fopen(file_arg->filename[i], "rb");
            if (!fp) {
                fprintf(stderr, "tac: %s: No such file or directory\n",
                        file_arg->filename[i]);
                continue;
            }

            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            if (fsize > 0) {
                char* data = g_malloc((size_t)fsize);
                size_t nread = fread(data, 1, (size_t)fsize, fp);
                fclose(fp);

                GPtrArray* records = split_input((const guint8*)data, nread,
                                                 sep, before_mode, regex_mode);
                g_free(data);
                write_records(records, stdout);
                free_records(records);
            } else {
                fclose(fp);
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}
