// argtable3.h — a hand-written, dependency-free reimplementation of the
// argtable3 command-line parsing API.
//
// This header replaces the external argtable3 library. It provides the same
// API surface that modbox uses (the option constructors, the arg_* structs
// and their fields, arg_parse / arg_freetable / arg_print_errors) with the
// same observable behaviour, so no command source file had to change.
//
// Implemented surface (everything modbox actually uses):
//   types    arg_lit, arg_str, arg_int, arg_dbl, arg_file, arg_end
//   makers   arg_lit0/litn, arg_str0/str1/strn, arg_int0/intn,
//            arg_dbl0/dbln, arg_file0/file1/filen, arg_end
//   runtime  arg_parse, arg_freetable, arg_print_errors, arg_print_option
//   fields   count, sval[], ival[], dval[], filename[]
//   errors   end->error[] / end->argval[] with ARG_ELONGOPT / ARG_ARG
//
// Parsing rules, matching argtable3:
//   * "--name"        a long option; "--name=value" supplies the value
//   * "--name value"  the value is taken from the next argv entry
//   * "-n"            a short option, which may be clustered ("-abc")
//   * "-nvalue"       a short option with an attached value
//   * "--"            end of options; the rest are positional arguments
//   * optional-argument entries (arg_str0/arg_int0/arg_dbl0) consume the
//     next argv entry only when it does not itself look like an option
//   * a value that is a valid number is stored in ival/dval as well as sval
//
// License: the original argtable3 is BSD-licensed; this is an independent
// reimplementation of its API, written for modbox.

#ifndef ARGTABLE3_H
#define ARGTABLE3_H

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Error codes reported through arg_end::error[] ─────────────────────── */

#define ARG_ELONGOPT 1   /* an unrecognised "--option"            */
#define ARG_ELONGMISS 2   /* a long option missing its argument    */
#define ARG_EMISSOPT 3    /* an option missing its required argument */
#define ARG_ENOMISSOPT 9  /* a required positional entry was not given */
#define ARG_EBADNUM 10     /* a value that will not convert to its type */
#define ARG_EBADINT 4     /* an integer argument that will not parse */
#define ARG_EDOUBLE 5     /* a floating point argument that will not parse */
#define ARG_EMALLOC 6     /* out of memory                         */
#define ARG_EARG 7        /* an unknown option                     */
#define ARG_ARG 8         /* an unrecognised positional argument  */

/* ── Data types ────────────────────────────────────────────────────────── */

#define ARG_NONE 0
#define ARG_INT 1
#define ARG_DBL 2
#define ARG_STR 3
#define ARG_LIT 4
#define ARG_FILE 5
#define ARG_END 20

struct arg_hdr {
    char flag;
    const char* shortopts;
    const char* longopts;
    const char* datatype;
    const char* glossary;
    int mincount;
    int maxcount;
    void* parent;
    void* errorfn;
    void* errorfndata;
    void* priv;
};

struct arg_lit {
    struct arg_hdr hdr;
    int count;
};

struct arg_str {
    struct arg_hdr hdr;
    int count;
    const char** sval;
};

struct arg_int {
    struct arg_hdr hdr;
    int count;
    const char** sval;
    int* ival;
};

struct arg_dbl {
    struct arg_hdr hdr;
    int count;
    const char** sval;
    double* dval;
};

struct arg_file {
    struct arg_hdr hdr;
    int count;
    const char** filename;
    const char** basenames;
    const char** values;
};

struct arg_end {
    int count;
    int* error;
    const char** argval;
};

/* ── Option constructors ───────────────────────────────────────────────── */

/* An entry that matches an option but records no value (arg_lit0). */
struct arg_lit* arg_lit0(const char* shortopts, const char* longopts,
                         const char* glossary);
struct arg_lit* arg_litn(const char* shortopts, const char* longopts,
                         int mincount, int maxcount, const char* glossary);

/* A single string value, optional (arg_str0/arg_str1). */
struct arg_str* arg_str0(const char* shortopts, const char* longopts,
                         const char* datatype, const char* glossary);
struct arg_str* arg_str1(const char* shortopts, const char* longopts,
                         const char* datatype, const char* glossary);

/* Up to maxcount string values. */
struct arg_str* arg_strn(const char* shortopts, const char* longopts,
                         const char* datatype, int mincount, int maxcount,
                         const char* glossary);

/* A single integer, optional. */
struct arg_int* arg_int0(const char* shortopts, const char* longopts,
                         const char* datatype, const char* glossary);
struct arg_int* arg_intn(const char* shortopts, const char* longopts,
                         const char* datatype, int mincount, int maxcount,
                         const char* glossary);

/* A single double, optional. */
struct arg_dbl* arg_dbl0(const char* shortopts, const char* longopts,
                         const char* datatype, const char* glossary);
struct arg_dbl* arg_dbln(const char* shortopts, const char* longopts,
                         const char* datatype, int mincount, int maxcount,
                         const char* glossary);

/* File arguments, optionally bounded. */
struct arg_file* arg_file0(const char* shortopts, const char* longopts,
                           const char* datatype, const char* glossary);
struct arg_file* arg_file1(const char* shortopts, const char* longopts,
                           const char* datatype, const char* glossary);
struct arg_file* arg_filen(const char* shortopts, const char* longopts,
                           const char* datatype, int mincount, int maxcount,
                           const char* glossary);

/* The table terminator; must be last in every table. */
struct arg_end* arg_end(int maxcount);

/* ── Runtime ───────────────────────────────────────────────────────────── */

/* Parse argv against the table. Returns 0 on success, or the number of
 * errors encountered (matching argtable3). `n` is the number of entries in
 * the table, the last of which is the arg_end entry. */
int arg_parse_n(int argc, char** argv, void** argtable, size_t n);

/* Same, when the table is NULL-terminated after the arg_end entry. */
int arg_parse(int argc, char** argv, void** argtable);

/* Release a table built with arg_end(). */
void arg_freetable(void** argtable, size_t n);

/* Release every entry in the table, including the arg_end entry. Used by
 * commands that exit early (--help, --version) without reaching the RAII
 * ArgTable destructor. The entry count is derived from the arg_end
 * terminator, so only the table is passed. */
void arg_free(void** argtable);

void arg_print_errors(FILE* fp, struct arg_end* end, const char* progname);
void arg_print_option(FILE* fp, const struct arg_hdr* opt,
                      const char* option, const char* value);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* ARGTABLE3_H */
