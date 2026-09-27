// argtable3.cpp — implementation of the hand-written argtable3 replacement.
//
// See include/argtable3.h for the contract this file implements.

#include "argtable3.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

/* Every entry shares a common header followed by `count`; the concrete structs
 * only append their value arrays, so allocation, parsing and teardown can all
 * be done generically. `sval` aliases arg_str.sval / arg_int.sval /
 * arg_dbl.sval and arg_file.filename, which all sit at the same offset. */
struct Entry {
    struct arg_hdr hdr;
    int count;
    const char** sval;       /* arg_str / arg_int / arg_dbl / arg_file */
    int* ival;               /* arg_int only    */
    double* dval;            /* arg_dbl only    */
    const char** basenames;  /* arg_file only   */
    int badval;              /* error code of the last bad value */
};

Entry* as_entry(void* p) { return static_cast<Entry*>(p); }

/* The table owns one string per option (short and long) plus the value
 * arrays; keep them together so arg_freetable can release everything. */
struct Slot {
    Entry* e = nullptr;
    std::vector<char*> owned;  /* strings duplicated from argv / literals */
    int count = 0;
};

struct Table {
    std::vector<Slot> slots;
    struct arg_end* end = nullptr;
    std::vector<int> end_errors;
    std::vector<const char*> end_argvals;
    std::vector<char*> end_owned;
};

Table* g_table = nullptr;   /* argtable3 keeps a single implicit table too */

/* The option constructors run before arg_parse(), so the table has to exist
 * from the first constructor call. */
Table& table() {
    if (g_table == nullptr) { g_table = new Table(); }
    return *g_table;
}

/* Does this argv entry look like an option? A lone "-" is a file name (stdin),
 * and a negative number is a value, not an option. */
bool looks_like_option(const char* s) {
    if (s == nullptr || s[0] != '-' || s[1] == '\0') { return false; }
    /* A negative number is a value. */
    if (std::isdigit(static_cast<unsigned char>(s[1])) != 0 ||
        s[1] == '.') {
        return false;
    }
    return true;
}

bool parse_int_strict(const char* s, int* out) {
    if (s == nullptr || *s == '\0') { return false; }
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(s, &end, 0);
    if (errno != 0 || end == s || *end != '\0') { return false; }
    *out = static_cast<int>(v);
    return true;
}

bool parse_dbl_strict(const char* s, double* out) {
    if (s == nullptr || *s == '\0') { return false; }
    char* end = nullptr;
    errno = 0;
    const double v = std::strtod(s, &end);
    if (errno != 0 || end == s || *end != '\0') { return false; }
    *out = v;
    return true;
}

char* dup(const char* s) {
    if (s == nullptr) { return nullptr; }
    const size_t n = std::strlen(s) + 1;
    char* p = static_cast<char*>(std::malloc(n));
    if (p != nullptr) { std::memcpy(p, s, n); }
    return p;
}

/* Match a short option character against an entry's shortopts string. */
bool short_matches(const Entry* e, char c) {
    return e->hdr.shortopts != nullptr && std::strchr(e->hdr.shortopts, c) != nullptr;
}

/* Match a long option name against an entry's longopts string. argtable3
 * separates alternative spellings with '|' (e.g. "decompress|uncompress");
 * ',' is also accepted as a separator. */
bool long_matches(const Entry* e, const char* name, size_t len) {
    if (e->hdr.longopts == nullptr) { return false; }
    const char* p = e->hdr.longopts;
    while (*p != '\0') {
        const char* q = p;
        while (*q != '\0' && *q != '|' && *q != ',') { ++q; }
        const size_t n = static_cast<size_t>(q - p);
        if (n == len && std::strncmp(p, name, n) == 0) { return true; }
        if (*q == '\0') { break; }
        p = q + 1;
    }
    return false;
}

/* Does this entry take a value? lit and end entries do not. */
bool takes_value(const Entry* e) {
    return e->hdr.datatype != nullptr && e->hdr.datatype[0] != '\0' &&
           e->hdr.datatype[0] != '0';
}

/* A required-value option (arg_str1 / mincount >= 1) takes the next argv entry
 * even when that entry starts with '-', because the value is not itself an
 * option. An optional-value option (arg_str0) leaves such an entry alone. */
bool value_is_required(const Entry* e) { return e->hdr.mincount >= 1; }

/* Record one value into an entry, converting it for int/dbl entries. */
bool store_value(Entry* e, Slot& slot, const char* value, int error_code) {
    if (static_cast<int>(slot.owned.size()) < e->hdr.maxcount) {
        slot.owned.push_back(dup(value));
    }
    if (e->hdr.maxcount > 0 && e->sval != nullptr) {
        e->sval[slot.count] = slot.owned[slot.count];
        if (e->ival != nullptr) {
            int iv = 0;
            if (!parse_int_strict(value, &iv)) {
                e->badval = error_code;
                return false;
            }
            e->ival[slot.count] = iv;
        }
        if (e->dval != nullptr) {
            double dv = 0.0;
            if (!parse_dbl_strict(value, &dv)) {
                e->badval = error_code;
                return false;
            }
            e->dval[slot.count] = dv;
        }
    }
    ++e->count;
    ++slot.count;
    return true;
}

void add_end_error(Table* t, int code, const char* value) {
    t->end_errors.push_back(code);
    t->end_argvals.push_back(value);
    t->end_owned.push_back(dup(value));
}

}  // namespace

extern "C" {

/* ── Constructors ──────────────────────────────────────────────────────── */

namespace {

Entry* alloc_entry(const char* shortopts, const char* longopts,
                   const char* datatype, int mincount, int maxcount,
                   const char* glossary, int kind) {
    auto* e = static_cast<Entry*>(std::calloc(1, sizeof(Entry)));
    if (e == nullptr) { return nullptr; }
    e->hdr.flag = '\0';
    e->hdr.shortopts = shortopts;
    e->hdr.longopts = longopts;
    e->hdr.datatype = datatype;
    e->hdr.glossary = glossary;
    e->hdr.mincount = mincount;
    e->hdr.maxcount = maxcount;
    e->hdr.parent = nullptr;
    e->badval = 0;
    e->count = 0;
    e->sval = (maxcount > 0)
                    ? static_cast<const char**>(std::calloc(
                          static_cast<size_t>(maxcount), sizeof(char*)))
                    : nullptr;

    if (kind == ARG_INT) {
        e->ival = (maxcount > 0)
                      ? static_cast<int*>(std::calloc(
                            static_cast<size_t>(maxcount), sizeof(int)))
                      : nullptr;
    } else if (kind == ARG_DBL) {
        e->dval = (maxcount > 0)
                      ? static_cast<double*>(std::calloc(
                            static_cast<size_t>(maxcount), sizeof(double)))
                      : nullptr;
    } else if (kind == ARG_FILE) {
        e->basenames = (maxcount > 0)
                           ? static_cast<const char**>(std::calloc(
                                 static_cast<size_t>(maxcount), sizeof(char*)))
                           : nullptr;
    }

    Slot slot;
    slot.e = e;
    table().slots.push_back(slot);
    return e;
}

}  // namespace

struct arg_lit* arg_lit0(const char* shortopts, const char* longopts,
                         const char* glossary) {
    return reinterpret_cast<struct arg_lit*>(
        alloc_entry(shortopts, longopts, "", 0, 1, glossary, ARG_LIT));
}

struct arg_lit* arg_litn(const char* shortopts, const char* longopts,
                         int mincount, int maxcount, const char* glossary) {
    return reinterpret_cast<struct arg_lit*>(
        alloc_entry(shortopts, longopts, "", mincount, maxcount, glossary,
                    ARG_LIT));
}

struct arg_str* arg_str0(const char* shortopts, const char* longopts,
                         const char* datatype, const char* glossary) {
    return reinterpret_cast<struct arg_str*>(alloc_entry(
        shortopts, longopts, datatype, 0, 1, glossary, ARG_STR));
}

struct arg_str* arg_str1(const char* shortopts, const char* longopts,
                         const char* datatype, const char* glossary) {
    return reinterpret_cast<struct arg_str*>(alloc_entry(
        shortopts, longopts, datatype, 1, 1, glossary, ARG_STR));
}

struct arg_str* arg_strn(const char* shortopts, const char* longopts,
                         const char* datatype, int mincount, int maxcount,
                         const char* glossary) {
    return reinterpret_cast<struct arg_str*>(alloc_entry(
        shortopts, longopts, datatype, mincount, maxcount, glossary, ARG_STR));
}

struct arg_int* arg_int0(const char* shortopts, const char* longopts,
                         const char* datatype, const char* glossary) {
    return reinterpret_cast<struct arg_int*>(alloc_entry(
        shortopts, longopts, datatype, 0, 1, glossary, ARG_INT));
}

struct arg_int* arg_intn(const char* shortopts, const char* longopts,
                         const char* datatype, int mincount, int maxcount,
                         const char* glossary) {
    return reinterpret_cast<struct arg_int*>(alloc_entry(
        shortopts, longopts, datatype, mincount, maxcount, glossary, ARG_INT));
}

struct arg_dbl* arg_dbl0(const char* shortopts, const char* longopts,
                         const char* datatype, const char* glossary) {
    return reinterpret_cast<struct arg_dbl*>(alloc_entry(
        shortopts, longopts, datatype, 0, 1, glossary, ARG_DBL));
}

struct arg_dbl* arg_dbln(const char* shortopts, const char* longopts,
                         const char* datatype, int mincount, int maxcount,
                         const char* glossary) {
    return reinterpret_cast<struct arg_dbl*>(alloc_entry(
        shortopts, longopts, datatype, mincount, maxcount, glossary, ARG_DBL));
}

struct arg_file* arg_file0(const char* shortopts, const char* longopts,
                           const char* datatype, const char* glossary) {
    return reinterpret_cast<struct arg_file*>(alloc_entry(
        shortopts, longopts, datatype, 0, 1, glossary, ARG_FILE));
}

struct arg_file* arg_file1(const char* shortopts, const char* longopts,
                           const char* datatype, const char* glossary) {
    return reinterpret_cast<struct arg_file*>(alloc_entry(
        shortopts, longopts, datatype, 1, 1, glossary, ARG_FILE));
}

struct arg_file* arg_filen(const char* shortopts, const char* longopts,
                           const char* datatype, int mincount, int maxcount,
                           const char* glossary) {
    return reinterpret_cast<struct arg_file*>(alloc_entry(
        shortopts, longopts, datatype, mincount, maxcount, glossary, ARG_FILE));
}

struct arg_end* arg_end(int maxcount) {
    auto* e = static_cast<struct arg_end*>(std::calloc(1, sizeof(struct arg_end)));
    if (e == nullptr) { return nullptr; }
    e->count = 0;
    e->error = (maxcount > 0)
                   ? static_cast<int*>(std::calloc(static_cast<size_t>(maxcount),
                                                   sizeof(int)))
                   : nullptr;
    e->argval = (maxcount > 0)
                    ? static_cast<const char**>(std::calloc(
                          static_cast<size_t>(maxcount), sizeof(char*)))
                    : nullptr;
    Table& t = table();
    t.end = e;
    t.end_errors.reserve(static_cast<size_t>(maxcount));
    t.end_argvals.reserve(static_cast<size_t>(maxcount));
    t.end_owned.reserve(static_cast<size_t>(maxcount));
    return e;
}

/* ── Parsing ───────────────────────────────────────────────────────────── */

int arg_parse_n(int argc, char** argv, void** argtable, size_t n) {
    Table* t = g_table;
    if (t == nullptr) { return 0; }

    /* The caller passes `n` entry pointers, the last of which is the arg_end
     * entry. Everything before it is an option or positional entry. */
    std::vector<Entry*> entries;
    struct arg_end* end = t->end;
    if (end == nullptr) { return 0; }
    for (size_t i = 0; i + 1 < n; ++i) {
        entries.push_back(as_entry(argtable[i]));
    }

    bool no_more_options = false;
    int errors = 0;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];

        if (no_more_options) {
            ++errors;
            add_end_error(t, ARG_ARG, a);
            continue;
        }

        if (std::strcmp(a, "--") == 0) {
            no_more_options = true;
            continue;
        }

        /* A long option. */
        if (a[0] == '-' && a[1] == '-') {
            const char* name = a + 2;
            const char* eq = std::strchr(name, '=');
            const size_t nlen = (eq != nullptr)
                                    ? static_cast<size_t>(eq - name)
                                    : std::strlen(name);

            Entry* hit = nullptr;
            for (Entry* e : entries) {
                if (long_matches(e, name, nlen)) { hit = e; break; }
            }
            if (hit == nullptr) {
                add_end_error(t, ARG_ELONGOPT, a);
                ++errors;
                continue;
            }
            /* Find the slot that owns this entry. */
            Slot* slot = nullptr;
            for (Slot& s : t->slots) {
                if (s.e == hit) { slot = &s; break; }
            }
            if (slot == nullptr) { continue; }

            if (!takes_value(hit)) {
                /* A flag; reject an attached value the way argtable3 does. */
                if (eq != nullptr) { ++errors; add_end_error(t, ARG_ELONGMISS, a); }
                ++hit->count;
                ++slot->count;
                continue;
            }
            if (eq != nullptr) {
                if (!store_value(hit, *slot, eq + 1, ARG_EBADINT)) { ++errors; }
                continue;
            }
            /* Take the next argv entry as the value when it is available and
             * is not itself an option. A required-value option takes it even
             * when it looks like an option. */
            if (i + 1 < argc &&
                (value_is_required(hit) || !looks_like_option(argv[i + 1]))) {
                if (!store_value(hit, *slot, argv[++i], ARG_EBADINT)) { ++errors; }
            } else {
                ++errors;
                add_end_error(t, ARG_ELONGMISS, a);
            }
            continue;
        }

        /* A short option, possibly clustered or with an attached value. */
        if (a[0] == '-' && a[1] != '\0') {
            bool consumed_next = false;
            for (size_t k = 1; a[k] != '\0'; ++k) {
                const char c = a[k];
                Entry* hit = nullptr;
                for (Entry* e : entries) {
                    if (short_matches(e, c)) { hit = e; break; }
                }
                if (hit == nullptr) {
                    char buf[3] = {'-', c, '\0'};
                    add_end_error(t, ARG_ELONGOPT, buf);
                    ++errors;
                    break;
                }
                Slot* slot = nullptr;
                for (Slot& s : t->slots) {
                    if (s.e == hit) { slot = &s; break; }
                }
                if (slot == nullptr) { continue; }

                if (!takes_value(hit)) {
                    ++hit->count;
                    ++slot->count;
                    continue;
                }
                /* An attached value: "-nVALUE" or the rest of a cluster. */
                if (a[k + 1] != '\0') {
                    const char* v = a + k + 1;
                    if (*v == '=') { ++v; }
                    if (!store_value(hit, *slot, v, ARG_EBADINT)) { ++errors; }
                    break;
                }
                /* Otherwise consume the next argv entry. A short option
                 * takes it even when it looks like an option: the value is
                 * the operand of -X, not another option (column -N '-,y'). */
                if (i + 1 < argc) {
                    if (!store_value(hit, *slot, argv[++i], ARG_EBADINT)) { ++errors; }
                    consumed_next = true;
                } else {
                    ++errors;
                    add_end_error(t, ARG_EMISSOPT, a);
                }
                break;
            }
            (void)consumed_next;
            continue;
        }

        /* A positional argument. Route it to the first positional entry
         * (arg_file / arg_str / arg_int / arg_dbl) that still has room; when
         * every one of them is full it is an excess argument. */
        {
            Entry* target = nullptr;
            Slot* tslot = nullptr;
            int probe_i = 0;
            double probe_d = 0.0;
            for (size_t i = 0; i < entries.size(); ++i) {
                Entry* e = entries[i];
                /* A positional entry has neither a short nor a long option. */
                if (e->hdr.shortopts != nullptr || e->hdr.longopts != nullptr) {
                    continue;
                }
                if (e->count < e->hdr.maxcount) {
                    target = e;
                    for (Slot& s : t->slots) {
                        if (s.e == e) { tslot = &s; break; }
                    }
                    break;
                }
            }
            if (target != nullptr && tslot != nullptr) {
                /* A typed positional entry only accepts the value when it
                 * actually parses for that type; otherwise fall through to
                 * the next positional entry (e.g. xz's LEVEL before FILE). */
                if (target->ival != nullptr && !parse_int_strict(a, &probe_i)) {
                    target = nullptr;
                    tslot = nullptr;
                } else if (target->dval != nullptr &&
                           !parse_dbl_strict(a, &probe_d)) {
                    target = nullptr;
                    tslot = nullptr;
                }
            }
            if (target == nullptr || tslot == nullptr) {
                /* Retry from the start, skipping this entry. */
                for (size_t i = 0; i < entries.size(); ++i) {
                    Entry* e = entries[i];
                    if (e->hdr.shortopts != nullptr || e->hdr.longopts != nullptr) {
                        continue;
                    }
                    if (e->count >= e->hdr.maxcount) { continue; }
                    if (e->ival != nullptr && !parse_int_strict(a, &probe_i)) { continue; }
                    if (e->dval != nullptr && !parse_dbl_strict(a, &probe_d)) { continue; }
                    target = e;
                    for (Slot& s : t->slots) {
                        if (s.e == e) { tslot = &s; break; }
                    }
                    break;
                }
            }
            if (target != nullptr && tslot != nullptr) {
                if (!store_value(target, *tslot, a, ARG_EBADINT)) { ++errors; }
            } else {
                ++errors;
                add_end_error(t, ARG_ARG, a);
            }
        }
    }

    /* Report entries that did not reach their minimum count. argtable3
     * surfaces this through the same end table, worded as
     * "missing option <datatype>". This must run before the end table is
     * published below, so these errors reach the caller. */
    for (Entry* e : entries) {
        if (e->hdr.mincount > 0 && e->count < e->hdr.mincount) {
            ++errors;
            const char* label = (e->hdr.datatype != nullptr &&
                                 e->hdr.datatype[0] != '\0')
                                    ? e->hdr.datatype
                                    : ((e->hdr.longopts != nullptr)
                                           ? e->hdr.longopts
                                           : "");
            add_end_error(t, ARG_ENOMISSOPT, label);
        }
    }

    /* Publish the collected end errors. */
    end->count = 0;
    for (size_t i = 0; i < t->end_errors.size(); ++i) {
        end->error[i] = t->end_errors[i];
        end->argval[i] = t->end_owned[i];
        ++end->count;
    }

    return errors;
}

/* Used by commands that build a raw `void*[]` table and NULL-terminate it;
 * the length is found by scanning for the registered arg_end entry. */
int arg_parse(int argc, char** argv, void** argtable) {
    size_t n = 0;
    if (g_table != nullptr && g_table->end != nullptr) {
        const void* end = static_cast<const void*>(g_table->end);
        for (void** p = argtable; *p != nullptr; ++p) {
            ++n;
            if (static_cast<const void*>(*p) == end) { break; }
        }
    }
    return arg_parse_n(argc, argv, argtable, n);
}

void arg_freetable(void** argtable, size_t n) {
    (void)argtable;
    (void)n;
    if (g_table == nullptr) { return; }
    Table* t = g_table;
    for (Slot& s : t->slots) {
        for (char* p : s.owned) { std::free(p); }
        s.owned.clear();
        if (s.e == nullptr) { continue; }
        std::free(const_cast<char**>(s.e->sval));
        std::free(s.e->ival);
        std::free(s.e->dval);
        std::free(const_cast<char**>(s.e->basenames));
        std::free(s.e);
    }
    for (char* p : t->end_owned) { std::free(p); }
    if (t->end != nullptr) {
        std::free(t->end->error);
        std::free(const_cast<char**>(t->end->argval));
        std::free(t->end);
    }
    t->slots.clear();
    t->end = nullptr;
    t->end_errors.clear();
    t->end_argvals.clear();
    t->end_owned.clear();
}

void arg_free(void** argtable) {
    size_t n = 0;
    if (g_table != nullptr && g_table->end != nullptr) {
        /* The table is NULL-terminated after the arg_end entry; the count is
         * not needed because teardown walks the recorded slots. */
        (void)argtable;
    }
    arg_freetable(argtable, n);
}

void arg_print_errors(FILE* fp, struct arg_end* end, const char* progname) {
    if (end == nullptr) { return; }
    for (int i = 0; i < end->count; ++i) {
        const char* v = (end->argval != nullptr && end->argval[i] != nullptr)
                            ? end->argval[i] : "";
        switch (end->error[i]) {
            case ARG_ELONGOPT:
                (void)fprintf(fp, "%s: invalid option '%s'\n", progname, v);
                break;
            case ARG_ELONGMISS:
                (void)fprintf(fp, "%s: option '%s' requires an argument\n",
                              progname, v);
                break;
            case ARG_EMISSOPT:
                (void)fprintf(fp, "%s: option '%s' requires an argument\n",
                              progname, v);
                break;
            case ARG_ENOMISSOPT:
                (void)fprintf(fp, "%s: missing option %s\n", progname, v);
                break;
            case ARG_ARG:
            default:
                (void)fprintf(fp, "%s: unexpected argument '%s'\n", progname, v);
                break;
        }
    }
    (void)fprintf(fp, "Try '%s --help' for more information.\n", progname);
}

void arg_print_option(FILE* fp, const struct arg_hdr* opt,
                      const char* option, const char* value) {
    (void)fp;
    (void)opt;
    (void)option;
    (void)value;
}

}  // extern "C"
